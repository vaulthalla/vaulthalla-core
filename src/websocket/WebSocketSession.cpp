// WebSocketSession.cpp — refactored
// Keeps implementation order in sync with the declaration order in WebSocketSession.hpp
// Author: (refactor by ChatGPT)
// -----------------------------------------------------------------------------
#include "websocket/WebSocketSession.hpp"

#include "auth/AuthManager.hpp"
#include "auth/Client.hpp"
#include "types/User.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "websocket/handlers/NotificationBroadcastManager.hpp"

#include <boost/beast/http.hpp>
#include <iostream>
#include <regex>

namespace {
    namespace beast     = boost::beast;
    namespace http      = beast::http;
    namespace websocket = beast::websocket;
    namespace asio      = boost::asio;

    using tcp  = asio::ip::tcp;
    using json = nlohmann::json;
} // namespace

namespace vh::websocket {

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ construction & destruction
// ──────────────────────────────────────────────────────────────────────────────
    WebSocketSession::WebSocketSession(const std::shared_ptr<WebSocketRouter>&          router,
                                       const std::shared_ptr<NotificationBroadcastManager>& broadcaster,
                                       const std::shared_ptr<auth::AuthManager>&       authManager)
            : authManager_{authManager},
              ws_{nullptr},
              router_{router},
              broadcastManager_{broadcaster} {}

    WebSocketSession::~WebSocketSession() {
        if (broadcastManager_ && isRegistered_) {
            broadcastManager_->unregisterSession(shared_from_this());
            std::cout << "[WebSocketSession] Destructor — session unregistered.\n";
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

    std::string WebSocketSession::getRefreshToken() const { return refreshToken_; }

    std::string WebSocketSession::getRefreshTokenFromCookie() const {
        const auto it = handshakeRequest_.find(http::field::cookie);
        if (it == handshakeRequest_.end()) return {};

        static const std::regex rgx{"refreshToken=([^;\s]+)"};
        std::smatch        m;
        const std::string header = it->value();
        return std::regex_search(header, m, rgx) && m.size() > 1 ? m[1].str() : "";
    }

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ session life‑cycle
// ──────────────────────────────────────────────────────────────────────────────
    void WebSocketSession::accept(tcp::socket&& socket) {
        ws_     = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
        strand_ = asio::make_strand(ws_->get_executor());

        auto self   = shared_from_this();
        auto client = std::make_shared<auth::Client>(self);
        authManager_->sessionManager()->createSession(client);
        setRefreshTokenCookie(client->getHashedRefreshToken());

        auto req = std::make_shared<http::request<http::string_body>>();
        http::async_read(ws_->next_layer(), tmpBuffer_, *req,
                         asio::bind_executor(strand_,
                                             [self, req](beast::error_code ec, std::size_t) {
                                                 if (ec) {
                                                     std::cerr << "[Session] Handshake read error: " << ec.message() << '\n';
                                                     return;
                                                 }

                                                 // capture request metadata
                                                 self->ipAddress_  = self->ws_->next_layer().remote_endpoint().address().to_string();
                                                 self->userAgent_  = (*req)[http::field::user_agent];
                                                 self->handshakeRequest_ = *req;

                                                 if (const auto authHeader = req->find(http::field::authorization); authHeader != req->end())
                                                     self->refreshToken_ = (*req)[http::field::authorization];

                                                 // decorate the handshake response (cookie etc.)
                                                 const auto refreshTokenCopy = self->refreshToken_;
                                                 self->ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
                                                 self->ws_->set_option(websocket::stream_base::decorator(
                                                         [refreshTokenCopy](websocket::response_type& res) {
                                                             res.set(http::field::server, "Vaulthalla");
                                                             res.set(http::field::set_cookie,
                                                                     "refresh=" + refreshTokenCopy + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=604800");
                                                         }));

                                                 self->ws_->async_accept(*req, asio::bind_executor(self->strand_,
                                                                                                   [self](beast::error_code ec) {
                                                                                                       if (ec) {
                                                                                                           std::cerr << "[Session] WebSocket accept error: " << ec.message() << '\n';
                                                                                                           return;
                                                                                                       }

                                                                                                       if (self->broadcastManager_) self->broadcastManager_->registerSession(self);
                                                                                                       self->isRegistered_ = true;

                                                                                                       std::cout << "[Session] WS connected from " << self->ipAddress_ << " UA:" << self->userAgent_ << '\n';
                                                                                                       self->doRead();
                                                                                                   }));
                                             }));
    }

    void WebSocketSession::close() {
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
        if (ec)
            std::cerr << "[WebSocketSession] Close error: " << ec.message() << "\n";
        else
            std::cout << "[WebSocketSession] Connection closed gracefully.\n";
    }

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ messaging I/O
// ──────────────────────────────────────────────────────────────────────────────
    void WebSocketSession::send(const json& message) {
        const std::string msg = message.dump();
        asio::post(strand_, [self = shared_from_this(), msg]() {
            bool startWrite = false;
            {
                std::lock_guard lock(self->writeQueueMutex_);
                self->writeQueue_.push(msg);
                if (!self->writingInProgress_) {
                    self->writingInProgress_ = true;
                    startWrite              = true;
                }
            }
            if (startWrite) self->doWrite();
        });
    }

    void WebSocketSession::doWrite() {
        std::lock_guard lock(writeQueueMutex_);
        const std::string& front = writeQueue_.front();
        ws_->async_write(asio::buffer(front),
                         asio::bind_executor(strand_,
                                             std::bind(&WebSocketSession::onWrite, shared_from_this(),
                                                       std::placeholders::_1, std::placeholders::_2)));
    }

    void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytesWritten) {
        boost::ignore_unused(bytesWritten);
        if (ec) {
            std::cerr << "[WebSocketSession] Write error: " << ec.message() << '\n';
            return;
        }

        bool more;
        {
            std::lock_guard lock(writeQueueMutex_);
            writeQueue_.pop();
            more = !writeQueue_.empty();
            if (!more) writingInProgress_ = false;
        }
        if (more) doWrite();
    }

    void WebSocketSession::doRead() {
        ws_->async_read(buffer_,
                        asio::bind_executor(strand_,
                                            std::bind(&WebSocketSession::onRead, shared_from_this(),
                                                      std::placeholders::_1, std::placeholders::_2)));
    }

    void WebSocketSession::onRead(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) {
            std::cout << "[Session] Connection closed.\n";
            return;
        }
        if (ec) {
            std::cerr << "[Session] Read error: " << ec.message() << '\n';
            return;
        }

        try {
            router_->routeMessage(json::parse(beast::buffers_to_string(buffer_.data())), *this);
        } catch (const std::exception& ex) {
            std::cerr << "[Session] JSON error: " << ex.what() << '\n';
        }

        buffer_.consume(buffer_.size());
        doRead();
    }

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ authentication helpers
// ──────────────────────────────────────────────────────────────────────────────
    std::shared_ptr<types::User> WebSocketSession::getAuthenticatedUser() const { return authenticatedUser_; }
    void WebSocketSession::setAuthenticatedUser(std::shared_ptr<types::User> user) { authenticatedUser_ = std::move(user); }

    void WebSocketSession::setRefreshTokenCookie(const std::string& token) { refreshToken_ = token; }
    void WebSocketSession::setHandshakeRequest(const RequestType& req)     { handshakeRequest_ = req; }

// ──────────────────────────────────────────────────────────────────────────────
// ‑‑ pub/sub channels
// ──────────────────────────────────────────────────────────────────────────────
    void WebSocketSession::subscribeChannel(const std::string& channel) {
        std::lock_guard lock(subscriptionMutex_);
        subscribedChannels_.insert(channel);
    }

    void WebSocketSession::unsubscribeChannel(const std::string& channel) {
        std::lock_guard lock(subscriptionMutex_);
        subscribedChannels_.erase(channel);
    }

    bool WebSocketSession::isSubscribedTo(const std::string& channel) {
        std::lock_guard lock(subscriptionMutex_);
        return subscribedChannels_.contains(channel);
    }

    std::unordered_set<std::string> WebSocketSession::getSubscribedChannels() {
        std::lock_guard lock(subscriptionMutex_);
        return subscribedChannels_;
    }

} // namespace vh::websocket
