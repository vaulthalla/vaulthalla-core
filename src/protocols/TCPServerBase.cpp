#include "protocols/TcpServerBase.hpp"
#include "logging/LogRegistry.hpp"

#include <utility>

namespace vh::protocols {

TcpServerBase::TcpServerBase(asio::io_context& ioc,
                             const tcp::endpoint& endpoint,
                             const TcpServerOptions opts)
    : ioc_(ioc), acceptor_(ioc), opts_(opts) { init_acceptor(acceptor_, endpoint); }

void TcpServerBase::run() {
    logStart();

    const auto n = (opts_.acceptConcurrency == 0) ? 1u : opts_.acceptConcurrency;
    for (unsigned int i = 0; i < n; ++i) doAccept();
}

void TcpServerBase::onAcceptError(const beast::error_code& ec) {
    logger()->debug("[{}] accept error: {}", serverName(), ec.message());
}

std::shared_ptr<spdlog::logger> TcpServerBase::logger() const {
    using logging::LogRegistry;
    switch (opts_.channel) {
    case LogChannel::Http:      return LogRegistry::http();
    case LogChannel::WebSocket: return LogRegistry::ws();
    case LogChannel::General:
    default: return LogRegistry::vaulthalla();
    }
}

void TcpServerBase::logStart() const {
    logger()->info("[{}] Starting at {}", serverName(), endpointToString(acceptor_));
}

void TcpServerBase::doAccept() {
    auto self = shared_from_this();

    auto handler = [self](const beast::error_code& ec, tcp::socket socket) mutable {
        self->doAccept(); // re-arm ASAP

        if (ec) {
            if (ec == asio::error::operation_aborted) return; // shutting down
            self->onAcceptError(ec);
            return;
        }

        self->onAccept(std::move(socket));
    };

    if (opts_.useStrand) acceptor_.async_accept(asio::make_strand(ioc_), std::move(handler));
    else acceptor_.async_accept(std::move(handler));
}

}
