#pragma once

#include "http/PreviewResponse.hpp"

#include <boost/beast/http.hpp>
#include <memory>
#include <unordered_map>
#include <string>

namespace vh::storage { class StorageManager; }

namespace vh::http {

namespace http = boost::beast::http;

class ImagePreviewHandler {
public:
    explicit ImagePreviewHandler(const std::shared_ptr<storage::StorageManager>& storage) : storageManager_(storage) {}

    PreviewResponse handle(http::request<http::string_body>&& req,
                                          int vault_id,
                                          const std::string& rel_path,
                                          const std::unordered_map<std::string, std::string>& params) const;

private:
    std::shared_ptr<storage::StorageManager> storageManager_;
};

}
