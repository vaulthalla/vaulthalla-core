#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <unordered_set>

using RequestType = boost::beast::http::request<boost::beast::http::string_body>;

namespace vh::auth {
class SessionManager;
class AuthManager;
} // namespace vh::auth

namespace vh::types {
class User;
}

namespace vh::websocket {
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
    WebSocketSession(const std::shared_ptr<WebSocketRouter>& router,
                     const std::shared_ptr<NotificationBroadcastManager>& broadcastManager,
                     const std::shared_ptr<vh::auth::AuthManager>& authManager);

    void send(const json& message);
    void accept(tcp::socket&& socket);
    void close();

    void subscribeChannel(const std::string& channel);
    void unsubscribeChannel(const std::string& channel);
    bool isSubscribedTo(const std::string& channel);
    std::unordered_set<std::string> getSubscribedChannels();

    const std::string& getUUID() const { return uuid; }

    // Accessors for session state
    std::shared_ptr<vh::types::User> getAuthenticatedUser() const;
    void setAuthenticatedUser(std::shared_ptr<vh::types::User> user);
    void setRefreshTokenCookie(const std::string& token);
    void setHandshakeRequest(const RequestType& req);
    std::string getClientIp() const;
    std::string getUserAgent() const;
    std::string getRefreshToken() const;

  private:
    std::shared_ptr<vh::auth::AuthManager> authManager_;
    const std::string uuid = generateUUIDv4();
    std::shared_ptr<websocket::stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_, tmpBuffer_;
    asio::any_io_executor strand_;
    RequestType handshakeRequest_;

    std::shared_ptr<WebSocketRouter> router_;
    std::shared_ptr<vh::types::User> authenticatedUser_;
    std::string refreshToken_;
    std::string userAgent_;
    std::string ipAddress_;

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

    static std::string generateUUIDv4() {
        static boost::uuids::random_generator generator;
        boost::uuids::uuid uuid = generator();
        return boost::uuids::to_string(uuid);
    }
};

} // namespace vh::websocket
