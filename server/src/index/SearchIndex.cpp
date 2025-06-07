#include "index/SearchIndex.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace vh::index {

    void SearchIndex::addToIndex(const std::filesystem::path& path, const std::string& content) {
        auto tokens = tokenize(content);

        for (const auto& token : tokens) {
            invertedIndex[token].insert(path);
        }
    }

    std::vector<std::filesystem::path> SearchIndex::search(const std::string& term) const {
        auto it = invertedIndex.find(term);
        if (it == invertedIndex.end()) return {};

        return std::vector<std::filesystem::path>(it->second.begin(), it->second.end());
    }

    std::vector<std::string> SearchIndex::tokenize(const std::string& content) const {
        std::vector<std::string> tokens;
        std::istringstream stream(content);
        std::string word;

        while (stream >> word) {
            // Basic cleanup: lowercase, strip punctuation
            word.erase(std::remove_if(word.begin(), word.end(), [](unsigned char c) {
                return std::ispunct(c);
            }), word.end());

            std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) {
                return std::tolower(c);
            });

            if (!word.empty()) tokens.push_back(word);
        }

        return tokens;
    }

} // namespace vh::index
