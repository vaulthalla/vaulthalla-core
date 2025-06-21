#pragma once

#include <string>

namespace vh::database {
class FileQueries {
  public:
    FileQueries() = default;

    void createFile(const std::string& fileName, const std::string& mountName, const std::string& content);
    void deleteFile(const std::string& fileName, const std::string& mountName);
    bool fileMetaExists(const std::string& fileName, const std::string& mountName);
    std::string getFileMeta(const std::string& fileName, const std::string& mountName);
};
} // namespace vh::database