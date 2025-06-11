#include "websocket/WebSocketSession.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "types/User.hpp"
#include "websocket/handlers/NotificationBroadcastManager.hpp"
#include <iostream>

namespace vh::websocket {

    WebSocketSession::WebSocketSession(tcp::socket socket,
                                       const std::shared_ptr<WebSocketRouter>& router,
                                       const std::shared_ptr<NotificationBroadcastManager>& broadcastManager)
            : ws_(std::move(socket)),
              strand_(boost::asio::make_strand(ws_.get_executor())),
              router_(router),
              broadcastManager_(broadcastManager) {}

    WebSocketSession::~WebSocketSession() {
        if (broadcastManager_ && isRegistered_) {
            broadcastManager_->unregisterSession(shared_from_this());
            std::cout << "[WebSocketSession] Destructor called, session unregistered.\n";
        }
    }

    void WebSocketSession::run()
    {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res) {
                    res.set(boost::beast::http::field::server, "Vaulthalla-WebSocket-Server");
                }));

        // handshake THEN lambda fires
        ws_.async_accept(asio::bind_executor(
                strand_,
                [self = shared_from_this()](beast::error_code ec)
                {
                    if (ec) {
                        std::cerr << "[Session] Handshake failed: " << ec.message() << '\n';
                        return;
                    }
                    // safe to broadcast now
                    self->broadcastManager_->registerSession(self);
                    self->isRegistered_ = true;
                    self->doRead();
                }));
    }

    void WebSocketSession::doRead() {
        ws_.async_read(buffer_,
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

    void WebSocketSession::close() {
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
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
