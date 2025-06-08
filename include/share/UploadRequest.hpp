#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace vh::share {

    struct UploadRequest {
        std::string linkId;
        std::string uploaderUsername; // optional if public upload
        std::string filename;
        std::size_t fileSize;

        nlohmann::json toJson() const {
            return {
                    {"linkId", linkId},
                    {"uploaderUsername", uploaderUsername},
                    {"filename", filename},
                    {"fileSize", fileSize}
            };
        }

        static UploadRequest fromJson(const nlohmann::json& j) {
            return UploadRequest{
                    j["linkId"].get<std::string>(),
                    j["uploaderUsername"].get<std::string>(),
                    j["filename"].get<std::string>(),
                    j["fileSize"].get<std::size_t>()
            };
        }
    };

} // namespace vh::share
