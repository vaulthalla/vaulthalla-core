#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <queue>
#include <mutex>
#include <unordered_set>

namespace vh::websocket {
    namespace auth {
        class User;
        class SessionManager;
    }

    class WebSocketRouter;
    class NotificationBroadcastManager;

    namespace beast = boost::beast;
    namespace websocket = beast::websocket;
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;
    using json = nlohmann::json;

    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    public:
        ~WebSocketSession();
        WebSocketSession(tcp::socket socket,
                         WebSocketRouter& router,
                         auth::SessionManager& sessionManager,
                         std::shared_ptr<NotificationBroadcastManager> broadcastManager);

        void run();

        void send(const json& message);

        // Accessors for session state
        std::shared_ptr<auth::User> getAuthenticatedUser() const;
        void setAuthenticatedUser(std::shared_ptr<auth::User> user);

        void subscribeChannel(const std::string& channel);
        void unsubscribeChannel(const std::string& channel);
        bool isSubscribedTo(const std::string& channel);
        std::unordered_set<std::string> getSubscribedChannels();

    private:
        websocket::stream<tcp::socket> ws_;
        beast::flat_buffer buffer_;
        asio::strand<boost::asio::any_io_executor> strand_;

        WebSocketRouter& router_;
        auth::SessionManager& sessionManager_;
        std::shared_ptr<auth::User> authenticatedUser_;

        std::mutex writeQueueMutex_;
        std::queue<std::string> writeQueue_;
        bool writingInProgress_ = false;

        std::unordered_set<std::string> subscribedChannels_;
        std::mutex subscriptionMutex_;
        std::shared_ptr<NotificationBroadcastManager> broadcastManager_;
        bool isRegistered_ = false;

        void doRead();
        void onRead(beast::error_code ec, std::size_t bytesTransferred);

        void doWrite();
        void onWrite(beast::error_code ec, std::size_t bytesTransferred);
    };

} // namespace vh::websocket
