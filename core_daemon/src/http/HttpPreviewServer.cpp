#include "http/HttpPreviewServer.hpp"
#include "util/http.hpp"
#include "services/ServiceManager.hpp"
#include "auth/AuthManager.hpp"
#include "database/Queries/FileQueries.hpp"

#include <boost/beast/http/file_body.hpp>
#include <iostream>

namespace vh::http {

HttpPreviewServer::HttpPreviewServer(net::io_context& ioc, const tcp::endpoint& endpoint,
                                     const std::shared_ptr<services::ServiceManager>& serviceManager)
    : acceptor_(ioc), socket_(ioc), authManager_(serviceManager->authManager()), storageManager_(serviceManager->storageManager()) {
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) throw beast::system_error(ec);

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) throw beast::system_error(ec);

    acceptor_.bind(endpoint, ec);
    if (ec) throw beast::system_error(ec);

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) throw beast::system_error(ec);
}

void HttpPreviewServer::run() {
    do_accept();
}

void HttpPreviewServer::do_accept() {
    acceptor_.async_accept(
        socket_,
        [this](beast::error_code ec) {
            if (!ec) {
                std::thread{[self = shared_from_this()]() mutable {
                    self->handle_session(std::move(self->socket_));
                }}.detach();
            }
            do_accept();
        });
}

void HttpPreviewServer::handle_session(tcp::socket socket) const {
    beast::flat_buffer buffer;

    try {
        for (;;) {
            http::request<http::string_body> req;
            http::read(socket, buffer, req);

            if (req.method() != http::verb::get || !req.target().starts_with("/preview")) {
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(http::field::content_type, "text/plain");
                res.body() = "Invalid request";
                res.prepare_payload();
                http::write(socket, res);
                continue;
            }

            try {
                const auto refresh_token = util::extractCookie(req, "refresh");
                authManager_->validateRefreshToken(refresh_token);
            } catch (const std::exception& e) {
                std::cerr << "Authentication error: " << e.what() << std::endl;
                http::response<http::string_body> res{http::status::unauthorized, req.version()};
                res.set(http::field::content_type, "text/plain");
                res.body() = "Unauthorized: " + std::string(e.what());
                res.prepare_payload();
                http::write(socket, res);
                continue;
            }

            auto params = util::parse_query_params(req.target());

            const auto vault_it = params.find("vault_id");
            const auto path_it = params.find("path");
            if (vault_it == params.end() || path_it == params.end())
                throw std::runtime_error("Missing vault_id or path parameter");

            const auto vault_id = std::stoi(vault_it->second);
            const std::string rel_path = path_it->second;

            auto file_path = map_request_to_file(vault_id, rel_path);
            beast::error_code ec;
            http::file_body::value_type body;
            body.open(file_path.c_str(), beast::file_mode::scan, ec);

            if (ec) {
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.set(http::field::content_type, "text/plain");
                res.body() = "File not found";
                res.prepare_payload();
                http::write(socket, res);
                continue;
            }

            const auto mime_type = database::FileQueries::getMimeType(vault_id, {rel_path});

            http::response<http::file_body> res{
                std::piecewise_construct,
                std::make_tuple(std::move(body)),
                std::make_tuple(http::status::ok, req.version())
            };
            res.set(http::field::content_type, mime_type);
            res.content_length(body.size());
            res.keep_alive(req.keep_alive());

            http::write(socket, res);
        }
    } catch (const std::exception& e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}

std::string HttpPreviewServer::map_request_to_file(const unsigned int vault_id, const std::string& rel_path) const {
    // Sanitize rel_path if needed (e.g. remove ../)
    if (rel_path.find("..") != std::string::npos) throw std::runtime_error("Invalid path");

    const auto vault = storageManager_->getVault(vault_id);

    if (vault->type == types::VaultType::Local)
        return storageManager_->getLocalEngine(vault_id)->getAbsolutePath(rel_path).string();

    // TODO: Handle other vault types (e.g., S3, Azure, etc.)
    throw std::runtime_error("Unsupported vault type for preview: " + std::to_string(static_cast<int>(vault->type)));
}

} // namespace vh::http
