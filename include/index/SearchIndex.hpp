#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vh::index {

class SearchIndex {
  public:
    // Add file content to index
    void addToIndex(const std::filesystem::path& path, const std::string& content);

    // Search for a term, returns list of paths that contain it
    std::vector<std::filesystem::path> search(const std::string& term) const;

  private:
    std::unordered_map<std::string, std::unordered_set<std::filesystem::path>> invertedIndex;

    std::vector<std::string> tokenize(const std::string& content) const;
};

} // namespace vh::index
