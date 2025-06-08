#include "websocket/WebSocketSession.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "auth/User.hpp"
#include "auth/SessionManager.hpp"
#include "websocket/handlers/NotificationBroadcastManager.hpp"
#include <iostream>

namespace vh::websocket {

    WebSocketSession::WebSocketSession(tcp::socket socket,
                                       WebSocketRouter& router,
                                       auth::SessionManager& sessionManager,
                                       std::shared_ptr<NotificationBroadcastManager> broadcastManager)
            : ws_(std::move(socket)),
              strand_(boost::asio::make_strand(ws_.get_executor())),
              router_(router),
              sessionManager_(sessionManager),
              broadcastManager_(std::move(broadcastManager)) {}

    WebSocketSession::~WebSocketSession() {
        if (broadcastManager_ && isRegistered_) {
            broadcastManager_->unregisterSession(shared_from_this());
            std::cout << "[WebSocketSession] Destructor called, session unregistered.\n";
        }
    }

    void WebSocketSession::run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(boost::beast::http::field::server, "Vaulthalla-WebSocket-Server");
        }));

        ws_.async_accept(asio::bind_executor(strand_,
                                             std::bind(&WebSocketSession::doRead, shared_from_this())));

        broadcastManager_->registerSession(shared_from_this());
    }

    void WebSocketSession::doRead() {
        ws_.async_read(buffer_,
                       asio::bind_executor(strand_,
                                           std::bind(&WebSocketSession::onRead, shared_from_this(),
                                                     std::placeholders::_1, std::placeholders::_2)));
    }

    void WebSocketSession::onRead(beast::error_code ec, std::size_t bytesTransferred) {
        if (!isRegistered_) {
            broadcastManager_->registerSession(shared_from_this());
            isRegistered_ = true;
        }

        boost::ignore_unused(bytesTransferred);

        if (ec == websocket::error::closed) {
            std::cout << "[WebSocketSession] Connection closed.\n";
            return;

            WebSocketSession::~WebSocketSession();
        }

        if (ec) {
            std::cerr << "[WebSocketSession] Read error: " << ec.message() << "\n";
            return;
        }

        try {
            std::string msgStr = beast::buffers_to_string(buffer_.data());
            json msg = json::parse(msgStr);

            // Route the message
            router_.routeMessage(msg, *this);
        } catch (const std::exception& e) {
            std::cerr << "[WebSocketSession] JSON parse or routing error: " << e.what() << "\n";
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

        ws_.async_write(asio::buffer(msgStr),
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

    std::shared_ptr<auth::User> WebSocketSession::getAuthenticatedUser() const {
        return authenticatedUser_;
    }

    void WebSocketSession::setAuthenticatedUser(std::shared_ptr<auth::User> user) {
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
