#include "websocket/WebSocketSession.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "types/User.hpp"
#include "websocket/handlers/NotificationBroadcastManager.hpp"
#include "auth/Client.hpp"
#include "auth/AuthManager.hpp"
#include <iostream>
#include <regex>
#include <boost/beast/http.hpp>

namespace beast  = boost::beast;
namespace http   = beast::http;
namespace websocket = beast::websocket;
namespace asio   = boost::asio;
using tcp        = asio::ip::tcp;

namespace vh::websocket {

    WebSocketSession::WebSocketSession(const std::shared_ptr<WebSocketRouter>& router,
                                       const std::shared_ptr<NotificationBroadcastManager>& broadcastManager,
                                       const std::shared_ptr<vh::auth::AuthManager>& authManager)
            : authManager_(authManager),
              ws_(nullptr),
              router_(router),
              broadcastManager_(broadcastManager) {}

    WebSocketSession::~WebSocketSession() {
        if (broadcastManager_ && isRegistered_) {
            broadcastManager_->unregisterSession(shared_from_this());
            std::cout << "[WebSocketSession] Destructor called, session unregistered.\n";
        }
    }

    void WebSocketSession::run() {
        ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_->set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res) {
                    res.set(http::field::server, "Vaulthalla-WebSocket-Server");
                }));

        // 🔥 1-line change: drop the bogus request object
        ws_->async_accept(
                asio::bind_executor(
                        strand_,
                        [self = shared_from_this()](beast::error_code ec) {
                            if (ec) {
                                std::cerr << "[Session] Handshake failed: " << ec.message() << '\n';
                                return;
                            }

                            self->ipAddress_ =
                                    self->ws_->next_layer().remote_endpoint().address().to_string();
                            // user-agent isn't exposed here—grab it only if you go with manual mode.

                            self->broadcastManager_->registerSession(self);
                            self->isRegistered_ = true;
                            self->doRead();
                        }));
    }

    std::string WebSocketSession::getRefreshTokenFromCookie() const {
        auto it = handshakeRequest_.find(boost::beast::http::field::cookie);
        if (it != handshakeRequest_.end()) {
            std::string cookieHeader = it->value();
            std::smatch match;
            std::regex rgx("refreshToken=([^;\\s]+)");
            if (std::regex_search(cookieHeader, match, rgx) && match.size() > 1)
                return match[1].str();
        }
        return "";
    }

    void WebSocketSession::setRefreshTokenCookie(const std::string &token) { refreshToken_ = token; }
    void WebSocketSession::setHandshakeRequest(const RequestType &req) { handshakeRequest_ = req; }

    std::string WebSocketSession::getClientIp() const {
        try { return ws_->next_layer().remote_endpoint().address().to_string(); }
        catch (...) { return "unknown"; }
    }

    std::string WebSocketSession::getUserAgent() const {
        auto it = handshakeRequest_.find(boost::beast::http::field::user_agent);
        return it != handshakeRequest_.end() ? it->value() : "unknown";
    }

    std::string WebSocketSession::getRefreshToken() const { return refreshToken_; }

    void WebSocketSession::doRead() {
        ws_->async_read(buffer_,
                       asio::bind_executor(strand_,
                                           std::bind(&WebSocketSession::onRead, shared_from_this(),
                                                     std::placeholders::_1, std::placeholders::_2)));
    }

    void WebSocketSession::onRead(beast::error_code ec, std::size_t)
    {
        if (ec == websocket::error::closed) {
            std::cout << "[Session] Connection closed.\n";
            return;               // shared_ptr ref-count hits zero ⇒ auto-destruct
        }
        if (ec) {
            std::cerr << "[Session] Read error: " << ec.message() << '\n';
            return;
        }

        try {
            auto msg = json::parse(beast::buffers_to_string(buffer_.data()));
            router_->routeMessage(msg, *this);
        } catch (const std::exception& e) {
            std::cerr << "[Session] JSON error: " << e.what() << '\n';
        }

        buffer_.consume(buffer_.size());
        doRead();
    }

    void WebSocketSession::send(const json& message) {
        std::string msgStr = message.dump();

        asio::post(strand_, [self = shared_from_this(), msgStr]() {
            bool doWriteNow = false;

            {
                std::lock_guard<std::mutex> lock(self->writeQueueMutex_);
                self->writeQueue_.push(msgStr);
                if (!self->writingInProgress_) {
                    self->writingInProgress_ = true;
                    doWriteNow = true;
                }
            }

            if (doWriteNow) {
                self->doWrite();
            }
        });
    }

    void WebSocketSession::doWrite() {
        std::lock_guard<std::mutex> lock(writeQueueMutex_);
        const std::string& msgStr = writeQueue_.front();

        ws_->async_write(asio::buffer(msgStr),
                        asio::bind_executor(strand_,
                                            std::bind(&WebSocketSession::onWrite, shared_from_this(),
                                                      std::placeholders::_1, std::placeholders::_2)));
    }

    void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytesTransferred) {
        boost::ignore_unused(bytesTransferred);

        if (ec) {
            std::cerr << "[WebSocketSession] Write error: " << ec.message() << "\n";
            return;
        }

        bool writeMore = false;

        {
            std::lock_guard<std::mutex> lock(writeQueueMutex_);
            writeQueue_.pop();
            if (!writeQueue_.empty()) {
                writeMore = true;
            } else {
                writingInProgress_ = false;
            }
        }

        if (writeMore) {
            doWrite();
        }
    }

    void WebSocketSession::accept(tcp::socket&& socket)
    {
        // Wrap the raw socket in a Beast WebSocket stream
        ws_     = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
        strand_ = asio::make_strand(ws_->get_executor());

        auto self = shared_from_this();                               // keep session alive
        std::cout << "[Session] Accepting new WebSocket connection...\n";
        auto client = std::make_shared<vh::auth::Client>(self);
        std::cout << "[Session] Creating refresh token....\n";
        authManager_->createRefreshToken(client);
        std::cout << "[Session] Creating session for client...\n";
        authManager_->sessionManager()->createSession(client);
        std::cout << "[Session] Setting refresh token cookie...\n";
        setRefreshTokenCookie(client->getHashedRefreshToken());
        auto req  = std::make_shared<http::request<http::string_body>>(); // survives the async chain

        // 1) Read the incoming HTTP upgrade request ourselves
        http::async_read(
                ws_->next_layer(),     // underlying TCP socket
                tmpBuffer_,            // beast::flat_buffer (member) used ONLY for handshake
                *req,
                asio::bind_executor(strand_,
                                    [self, req](beast::error_code ec, std::size_t) {
                                        /* ---------- error handling while still in HTTP land ---------- */
                                        if (ec) {
                                            std::cerr << "[Session] Handshake read error: "
                                                      << ec.message() << '\n';
                                            return;                                         // destroys session
                                        }

                                        /* ---------- scrape whatever headers you need ---------- */
                                        self->ipAddress_ =
                                                self->ws_->next_layer().remote_endpoint().address().to_string();
                                        self->userAgent_ =
                                                (*req)[http::field::user_agent];   // may be empty
                                        self->setHandshakeRequest(*req);

                                        if (req->find(http::field::authorization) != req->end())
                                            self->refreshToken_ =
                                                    (*req)[http::field::authorization];

                                        /* ---------- push any auth / rate-limit logic here ---------- */
                                        // e.g. if(!self->verifyToken(self->accessToken_)) { /* reject */ }

                                        /* ---------- accept the upgrade ---------- */
                                        auto refreshToken = self->getRefreshToken();
                                        self->ws_->set_option(
                                                websocket::stream_base::timeout::suggested(beast::role_type::server));
                                        self->ws_->set_option(websocket::stream_base::decorator(
                                                [&refreshToken](websocket::response_type& res) {
                                                    res.set(http::field::server, "Vaulthalla");

                                                    // Send a blank refresh cookie
                                                    res.set(http::field::set_cookie,
                                                            "refresh=" + refreshToken + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=604800");
                                                }));

                                        self->ws_->async_accept(
                                                *req,
                                                asio::bind_executor(
                                                        self->strand_,
                                                        [self](beast::error_code ec) {
                                                            if (ec) {
                                                                std::cerr << "[Session] WebSocket accept error: "
                                                                          << ec.message() << '\n';
                                                                return;
                                                            }

                                                            /* ---------- session is live; register & read ---------- */
                                                            if (self->broadcastManager_)
                                                                self->broadcastManager_->registerSession(self);
                                                            self->isRegistered_ = true;

                                                            std::cout << "[Session] WS connected from "
                                                                      << self->ipAddress_
                                                                      << " UA:" << self->userAgent_ << '\n';

                                                            self->doRead();           // start the async read loop
                                                        }));
                                    }));
    }

    void WebSocketSession::close() {
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
        if (ec) std::cerr << "[WebSocketSession] Close error: " << ec.message() << "\n";
        else std::cout << "[WebSocketSession] Connection closed gracefully.\n";
    }

    std::shared_ptr<vh::types::User> WebSocketSession::getAuthenticatedUser() const {
        return authenticatedUser_;
    }

    void WebSocketSession::setAuthenticatedUser(std::shared_ptr<vh::types::User> user) {
        authenticatedUser_ = std::move(user);
    }

    void WebSocketSession::subscribeChannel(const std::string& channel) {
        std::lock_guard<std::mutex> lock(subscriptionMutex_);
        subscribedChannels_.insert(channel);
    }

    void WebSocketSession::unsubscribeChannel(const std::string& channel) {
        std::lock_guard<std::mutex> lock(subscriptionMutex_);
        subscribedChannels_.erase(channel);
    }

    bool WebSocketSession::isSubscribedTo(const std::string& channel) {
        std::lock_guard<std::mutex> lock(subscriptionMutex_);
        return subscribedChannels_.count(channel) > 0;
    }

    std::unordered_set<std::string> WebSocketSession::getSubscribedChannels() {
        std::lock_guard<std::mutex> lock(subscriptionMutex_);
        return subscribedChannels_;
    }

} // namespace vh::websocket
