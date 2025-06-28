#include "websocket/WebSocketSession.hpp"

#include "types/db/User.hpp"
#include "auth/AuthManager.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "websocket/handlers/NotificationBroadcastManager.hpp"
#include <boost/beast/http.hpp>
#include <iostream>
#include <regex>
#include "websocket/handlers/UploadHandler.hpp"

namespace {
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;

using tcp = asio::ip::tcp;
using json = nlohmann::json;
} // namespace

namespace vh::websocket {

std::string extractCookie(const http::request<http::string_body>& req,
                          const std::string& key) {
    if (const auto it = req.find(http::field::cookie); it != req.end()) {
        const std::string cookieHeader = it->value();
        const std::regex cookieRegex(key + "=([^;]+)");
        std::smatch match;
        if (std::regex_search(cookieHeader, match, cookieRegex)) return match[1];
    }
    return "";
}

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ construction & destruction
// ──────────────────────────────────────────────────────────────────────────────
WebSocketSession::WebSocketSession(const std::shared_ptr<WebSocketRouter>& router,
                                   const std::shared_ptr<NotificationBroadcastManager>& broadcastManager,
                                   const std::shared_ptr<auth::AuthManager>& authManager)
    : authManager_{authManager}, ws_{nullptr}, router_{router}, uploadHandler_(std::make_shared<UploadHandler>(*this)),
      broadcastManager_{broadcastManager} {}

WebSocketSession::~WebSocketSession() {
    if (broadcastManager_ && isRegistered_) {
        broadcastManager_->unregisterSession(shared_from_this());
        std::cout << "[WebSocketSession] Destructor — session unregistered." << std::endl;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ basic information helpers
// ──────────────────────────────────────────────────────────────────────────────
std::string WebSocketSession::getClientIp() const {
    try {
        return ws_->next_layer().remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

std::string WebSocketSession::getUserAgent() const {
    const auto it = handshakeRequest_.find(http::field::user_agent);
    return it != handshakeRequest_.end() ? it->value() : "unknown";
}

std::string WebSocketSession::getRefreshToken() const {
    return refreshToken_;
}

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ session life‑cycle
// ──────────────────────────────────────────────────────────────────────────────
void WebSocketSession::accept(tcp::socket&& socket) {
    ws_ = std::make_shared<websocket::stream<tcp::socket> >(std::move(socket));
    strand_ = asio::make_strand(ws_->get_executor());

    auto self = shared_from_this();
    auto req = std::make_shared<http::request<http::string_body> >();

    // ── Lambda: decorate handshake response with refresh token cookie ──
    auto setHandshakeResponseHeaders = [self]() {
        const auto refreshTokenCopy = self->refreshToken_;
        self->ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        self->ws_->set_option(websocket::stream_base::decorator([refreshTokenCopy](websocket::response_type& res) {
            res.set(http::field::server, "Vaulthalla");
            res.set(http::field::set_cookie,
                    "refresh=" + refreshTokenCopy + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=604800;");
        }));
    };

    // ── Lambda: what happens once the WebSocket handshake is accepted ──
    auto onHandshakeAccepted = [self](beast::error_code ec) {
        if (ec) {
            std::cerr << "[Session] WebSocket accept error: " << ec.message() << std::endl;
            return;
        }

        if (self->broadcastManager_) self->broadcastManager_->registerSession(self);

        self->isRegistered_ = true;
        std::cout << "[Session] WS connected from " << self->ipAddress_ << " UA:" << self->userAgent_ << std::endl;

        self->doRead();
    };

    // ── Lambda: after HTTP headers are read, prepare session & auth ──
    auto onHeadersRead = [self, req, setHandshakeResponseHeaders, onHandshakeAccepted](beast::error_code ec,
        std::size_t) {
        if (ec) {
            std::cerr << "[Session] Handshake read error: " << ec.message() << std::endl;
            return;
        }

        // Metadata from HTTP request
        self->ipAddress_ = self->ws_->next_layer().remote_endpoint().address().to_string();
        self->userAgent_ = (*req)[http::field::user_agent];
        self->handshakeRequest_ = *req;
        self->refreshToken_ = extractCookie(*req, "refresh");

        if (!self->refreshToken_.empty()) std::cout << "[Session] Found refresh token in cookies" << std::endl;
        else std::cout << "[Session] No refresh token found in Cookie header" << std::endl;

        self->authManager_->rehydrateOrCreateClient(self);

        // Set response headers before async_accept
        setHandshakeResponseHeaders();

        self->ws_->async_accept(*req, asio::bind_executor(self->strand_, onHandshakeAccepted));
    };

    // ── Begin HTTP header read phase ──
    http::async_read(ws_->next_layer(), tmpBuffer_, *req, asio::bind_executor(strand_, onHeadersRead));
}

void WebSocketSession::close() {
    if (broadcastManager_ && isRegistered_) {
        // drop pub/sub ref
        broadcastManager_->unregisterSession(shared_from_this());
        isRegistered_ = false;
    }

    if (ws_ && ws_->is_open()) {
        // close websocket
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
        if (ec) std::cerr << "[Session] Close error: " << ec.message() << std::endl;
    }

    ws_.reset(); // release FD
    buffer_.consume(buffer_.size());
    std::cout << "[WebSocketSession] Session cleaned up" << std::endl;
}


// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ messaging I/O
// ──────────────────────────────────────────────────────────────────────────────
void WebSocketSession::send(const json& message) {
    const std::string msg = message.dump();
    asio::post(strand_, [self = shared_from_this(), msg]() {
        bool startWrite = false;
        self->writeQueue_.push(msg);
        if (!self->writingInProgress_) {
            self->writingInProgress_ = true;
            startWrite = true;
        }
        if (startWrite) self->doWrite();
    });
}

void WebSocketSession::doWrite() {
    const std::string& front = writeQueue_.front();
    auto self = shared_from_this();

    ws_->async_write(
        asio::buffer(front),
        asio::bind_executor(strand_,
                            [self](beast::error_code ec, std::size_t bytesWritten) {
                                self->onWrite(ec, bytesWritten);
                            }));
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytesWritten) {
    boost::ignore_unused(bytesWritten);
    if (ec) {
        std::cerr << "[WebSocketSession] Write error: " << ec.message() << std::endl;
        return;
    }

    writeQueue_.pop();
    if (writeQueue_.empty()) writingInProgress_ = false;
    else doWrite();
}

void WebSocketSession::doRead() {
    auto self = shared_from_this();

    ws_->async_read(
        buffer_,
        asio::bind_executor(strand_,
                            [self](beast::error_code ec, std::size_t bytesRead) {
                                self->onRead(ec, bytesRead);
                            }));
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t) {
    if (ec == websocket::error::closed) {
        // graceful close
        std::cout << "[Session] Peer sent CLOSE frame" << std::endl;
        close();
        return;
    }

    if (ec == asio::error::eof) {
        // ungraceful close
        std::cout << "[Session] Peer vanished (EOF)" << std::endl;
        close(); // <- **must** clean up
        return;
    }

    if (ec) {
        // any other read error
        std::cerr << "[Session] Read error: " << ec.message() << std::endl;
        close(); // defensive cleanup
        return;
    }

    if (ws_->got_binary()) {
        uploadHandler_->handleBinaryFrame(buffer_);
        buffer_.consume(buffer_.size());
    } else {
        try {
            router_->routeMessage(json::parse(beast::buffers_to_string(buffer_.data())), *this);
        } catch (const std::exception& ex) {
            std::cerr << "[Session] JSON error: " << ex.what() << std::endl;
        }
        buffer_.consume(buffer_.size());
    }

    buffer_.consume(buffer_.size());
    doRead();
}

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ authentication helpers
// ──────────────────────────────────────────────────────────────────────────────
std::shared_ptr<types::User> WebSocketSession::getAuthenticatedUser() const {
    return authenticatedUser_;
}

void WebSocketSession::setAuthenticatedUser(std::shared_ptr<types::User> user) {
    authenticatedUser_ = std::move(user);
}

void WebSocketSession::setRefreshTokenCookie(const std::string& token) {
    refreshToken_ = token;
}

void WebSocketSession::setHandshakeRequest(const RequestType& req) {
    handshakeRequest_ = req;
}

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ pub/sub channels
// ──────────────────────────────────────────────────────────────────────────────
void WebSocketSession::subscribeChannel(const std::string& channel) {
    subscribedChannels_.insert(channel);
}

void WebSocketSession::unsubscribeChannel(const std::string& channel) {
    subscribedChannels_.erase(channel);
}

bool WebSocketSession::isSubscribedTo(const std::string& channel) {
    return subscribedChannels_.contains(channel);
}

std::unordered_set<std::string> WebSocketSession::getSubscribedChannels() {
    return subscribedChannels_;
}

} // namespace vh::websocket