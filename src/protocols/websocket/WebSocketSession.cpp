#include "protocols/websocket/WebSocketSession.hpp"

#include "auth/AuthManager.hpp"
#include "logging/LogRegistry.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "identities/model/User.hpp"
#include "protocols/cookie.hpp"

#include <boost/beast/http.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/asio.hpp>

#include "config/ConfigRegistry.hpp"

namespace {
namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace asio      = boost::asio;
using json          = nlohmann::json;
} // namespace

using namespace vh::services;
using namespace vh::logging;
using namespace vh::identities::model;
using namespace vh::protocols;

namespace vh::websocket {

WebSocketSession::WebSocketSession(const std::shared_ptr<WebSocketRouter>& router)
    : uploadHandler_(std::make_shared<UploadHandler>(*this)), router_(router) {
    buffer_.max_size(65536);
}

WebSocketSession::~WebSocketSession() {
    close();
    LogRegistry::ws()->debug("[WebSocketSession] Session destroyed for IP: {}", getClientIp());
}

std::string WebSocketSession::getClientIp() const {
    try {
        if (!ws_ || !ws_->next_layer().is_open()) return "unknown";
        return ws_->next_layer().remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

std::string WebSocketSession::getUserAgent() const {
    const auto it = handshakeRequest_.find(http::field::user_agent);
    return it != handshakeRequest_.end() ? std::string(it->value()) : "unknown";
}

std::string WebSocketSession::getRefreshToken() const {
    return refreshToken_;
}

std::shared_ptr<User> WebSocketSession::getAuthenticatedUser() const {
    return authenticatedUser_;
}

void WebSocketSession::setAuthenticatedUser(std::shared_ptr<User> user) {
    authenticatedUser_ = std::move(user);
}

void WebSocketSession::setRefreshTokenCookie(const std::string& token) {
    refreshToken_ = token;
}

void WebSocketSession::setHandshakeRequest(const RequestType& req) {
    handshakeRequest_ = req;
}

void WebSocketSession::logFail(std::string_view where, const beast::error_code& ec) {
    if (!ec) return;
    LogRegistry::ws()->debug("[WebSocketSession] {}: {}", where, ec.message());
}

void WebSocketSession::accept(tcp::socket&& socket) {
    ws_ = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
    strand_ = asio::make_strand(ws_->get_executor());

    auto req = std::make_shared<RequestType>();
    auto self = shared_from_this();

    http::async_read(
        ws_->next_layer(),
        tmpBuffer_,
        *req,
        asio::bind_executor(
            strand_,
            [self, req](const beast::error_code& ec, std::size_t bytesRead) {
                self->onHeadersRead(req, ec, bytesRead);
            }));
}

void WebSocketSession::onHeadersRead(const std::shared_ptr<RequestType>& req, const beast::error_code& ec, std::size_t) {
    if (ec) return logFail("Error reading HTTP headers", ec);

    hydrateFromRequest(*req);

    // auth rehydrate/create *before* accept so we can set refresh cookie in decorator
    ServiceDepsRegistry::instance().authManager->rehydrateOrCreateClient(shared_from_this());

    installHandshakeDecorator();

    auto self = shared_from_this();
    ws_->async_accept(
        *req,
        asio::bind_executor(
            strand_,
            [self](const beast::error_code& ec2) {
                self->onHandshakeAccepted(ec2);
            }));
}

void WebSocketSession::hydrateFromRequest(const RequestType& req) {
    handshakeRequest_ = req;

    try {
        ipAddress_ = ws_->next_layer().remote_endpoint().address().to_string();
    } catch (...) {
        ipAddress_ = "unknown";
    }

    userAgent_ = std::string(req[http::field::user_agent]);
    refreshToken_ = extractCookie(req, "refresh");

    if (refreshToken_.empty())
        LogRegistry::ws()->debug("[Session] No refresh token found in Cookie header");
    else
        LogRegistry::ws()->debug("[Session] Refresh token found in Cookie header: {}", refreshToken_);
}

void WebSocketSession::installHandshakeDecorator() const {
    const auto token = refreshToken_;

    ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_->set_option(websocket::stream_base::decorator(
        [token](websocket::response_type& res) {
            res.set(http::field::server, "Vaulthalla");

            const bool isDev = config::ConfigRegistry::get().dev.enabled;
            const std::string sameSite = isDev ? "Lax" : "Lax"; // keep Lax unless *need* Strict
            const bool secure = true; // or: !isDev ? true : config says https-enabled

            std::string cookie = "refresh=" + token +
                "; Path=/" +
                "; HttpOnly" +
                "; SameSite=" + sameSite +
                "; Max-Age=604800";

            if (secure) cookie += "; Secure";

            // IMPORTANT: use insert to avoid clobbering other Set-Cookie headers
            res.insert(http::field::set_cookie, cookie);
        }
    ));
}

void WebSocketSession::onHandshakeAccepted(const beast::error_code& ec) {
    if (ec) return logFail("Handshake error", ec);

    LogRegistry::ws()->debug("[Session] Handshake accepted from IP: {}", getClientIp());
    startReadLoop();
}

void WebSocketSession::startReadLoop() {
    doRead();
}

void WebSocketSession::close() {
    if (closing_.exchange(true)) return;

    auto ws = ws_;
    if (!ws) {
        buffer_.consume(buffer_.size());
        return;
    }

    auto self = shared_from_this();
    asio::post(strand_, [self, ws]() mutable {
        boost::system::error_code ec;
        if (ws->is_open())
            ws->close(websocket::close_code::normal, ec);

        if (ec)
            LogRegistry::ws()->debug("[WebSocketSession] ws close error: {}", ec.message());

        self->ws_.reset();
        self->buffer_.consume(self->buffer_.size());

        LogRegistry::ws()->debug("[WebSocketSession] Closed session for IP: {}", self->getClientIp());
    });
}

void WebSocketSession::send(const json& message) {
    auto payload = message.dump();
    auto self = shared_from_this();

    asio::post(strand_, [self, payload = std::move(payload)]() mutable {
        self->writeQueue_.push_back(std::move(payload));
        self->maybeStartWrite();
    });
}

void WebSocketSession::maybeStartWrite() {
    if (writing_ || writeQueue_.empty()) return;
    writing_ = true;
    doWrite();
}

void WebSocketSession::doWrite() {
    if (!ws_) {
        writing_ = false;
        writeQueue_.clear();
        return;
    }

    ws_->async_write(
        asio::buffer(writeQueue_.front()),
        asio::bind_executor(
            strand_,
            [self = shared_from_this()](const beast::error_code& ec, std::size_t bytesWritten) {
                self->onWrite(ec, bytesWritten);
            }));
}

void WebSocketSession::onWrite(const beast::error_code& ec, std::size_t) {
    if (ec) {
        logFail("Write error", ec);
        writing_ = false;
        return;
    }

    writeQueue_.pop_front();
    if (writeQueue_.empty()) {
        writing_ = false;
        return;
    }

    doWrite();
}

void WebSocketSession::doRead() {
    if (!ws_) return;

    ws_->async_read(
        buffer_,
        asio::bind_executor(
            strand_,
            [self = shared_from_this()](const beast::error_code& ec, const std::size_t bytesRead) {
                self->onRead(ec, bytesRead);
            }));
}

void WebSocketSession::sendParseError(const std::string_view msg) {
    send({
        {"command", "error"},
        {"status", "parse_error"},
        {"message", std::string(msg)}
    });
}

void WebSocketSession::sendInternalError() {
    send({
        {"command", "error"},
        {"status", "internal_error"},
        {"message", "An internal error occurred while processing your request."}
    });
}

void WebSocketSession::onRead(const beast::error_code& ec, std::size_t) {
    if (ec == websocket::error::closed) {
        LogRegistry::ws()->debug("[Session] WebSocket closed gracefully by peer");
        return close();
    }
    if (ec == asio::error::eof) {
        LogRegistry::ws()->debug("[Session] WebSocket peer vanished (EOF)");
        return close();
    }
    if (ec) {
        logFail("Read error", ec);
        return close();
    }

    if (ws_->got_binary()) uploadHandler_->handleBinaryFrame(buffer_);
    else {
        try {
            const auto text = beast::buffers_to_string(buffer_.data());
            router_->routeMessage(json::parse(text), *this);
        } catch (const std::exception& ex) {
            LogRegistry::ws()->debug("[Session] Error parsing message: {}", ex.what());
            sendParseError(std::string("Failed to parse message: ") + ex.what());
            // NOTE: we still continue reading (keeps session alive after a bad frame)
        } catch (...) {
            LogRegistry::ws()->debug("[Session] Unknown error while processing message");
            sendInternalError();
        }
    }

    buffer_.consume(buffer_.size());
    doRead();
}

std::string WebSocketSession::generateUUIDv4() {
    static boost::uuids::random_generator generator;
    const boost::uuids::uuid uuid = generator();
    return boost::uuids::to_string(uuid);
}

} // namespace vh::websocket
