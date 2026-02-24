#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

struct UploadContext {
    std::string path;
    uint64_t expectedSize = 0;
    uint64_t bytesReceived = 0;
    std::ofstream file;
};

using RequestType = boost::beast::http::request<boost::beast::http::string_body>;

namespace vh::identities::model { struct User; }

namespace vh::websocket {
class WebSocketRouter;
class UploadHandler;

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace asio      = boost::asio;
using tcp           = asio::ip::tcp;
using json          = nlohmann::json;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(const std::shared_ptr<WebSocketRouter>& router);
    ~WebSocketSession();

    void accept(tcp::socket&& socket);
    void send(const json& message);
    void close();

    const std::string& getUUID() const { return uuid_; }

    void setAuthenticatedUser(std::shared_ptr<identities::model::User> user);
    void setRefreshTokenCookie(const std::string& token);
    void setHandshakeRequest(const RequestType& req);

    std::shared_ptr<identities::model::User> getAuthenticatedUser() const;
    std::string getClientIp() const;
    std::string getUserAgent() const;
    std::string getRefreshToken() const;

    std::shared_ptr<UploadHandler> getUploadHandler() const { return uploadHandler_; }

    static std::string generateUUIDv4();

private:
    // ---- lifecycle / io
    void startReadLoop();
    void doRead();
    void onRead(const beast::error_code& ec, std::size_t bytesRead);

    void maybeStartWrite();
    void doWrite();
    void onWrite(const beast::error_code& ec, std::size_t bytesWritten);

    // ---- handshake helpers
    void onHeadersRead(const std::shared_ptr<RequestType>& req, const beast::error_code& ec, std::size_t bytesRead);
    void onHandshakeAccepted(const beast::error_code& ec);
    void hydrateFromRequest(const RequestType& req);
    void installHandshakeDecorator() const;

    // ---- misc helpers
    static void logFail(std::string_view where, const beast::error_code& ec);
    void sendParseError(std::string_view msg);
    void sendInternalError();

private:
    const std::string uuid_{generateUUIDv4()};

    std::shared_ptr<websocket::stream<tcp::socket>> ws_;
    asio::any_io_executor strand_;

    beast::flat_buffer buffer_{8192};
    beast::flat_buffer tmpBuffer_{4096}; // used during HTTP header read/handshake
    RequestType handshakeRequest_;

    std::shared_ptr<UploadHandler> uploadHandler_;
    std::shared_ptr<WebSocketRouter> router_;
    std::shared_ptr<identities::model::User> authenticatedUser_;

    std::string refreshToken_;
    std::string userAgent_;
    std::string ipAddress_;

    std::atomic_bool closing_{false};

    bool writing_ = false;                 // only touched on strand
    std::deque<std::string> writeQueue_;   // only touched on strand
};

} // namespace vh::websocket
