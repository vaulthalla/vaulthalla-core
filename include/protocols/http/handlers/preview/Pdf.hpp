#pragma once

#include "protocols/http/model/preview/Response.hpp"

#include <boost/beast/http.hpp>
#include <memory>

namespace vh::protocols::http::model::preview { struct Request; }

namespace vh::protocols::http::handlers::preview {

using request = boost::beast::http::request<boost::beast::http::string_body>;

struct Pdf {
    static Response handle(request&& req, const std::unique_ptr<Request>&& pr);
};

}
