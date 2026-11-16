#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace bananagrams {

// In-memory words; text-file loading stays outside the engine for future browser use.
// All methods, including construction, currently throw std::logic_error.
class Lexicon {
  private:
    std::unordered_set<std::string> LEXICON;

    bool isAllUpper(const std::string& word);
  public:
    // Require uppercase A-Z strings of length >= 2; reject the whole construction
    // with invalid_argument for any malformed entry. Duplicate words count once.
    explicit Lexicon(const std::vector<std::string>& words);


    // Exact membership query; malformed words return false without normalization.
    [[nodiscard]] bool contains(const std::string& word) const;
    // Return the number of unique words.
    [[nodiscard]] std::size_t size() const;

  private:
    std::unordered_set<std::string> words_;
};

} // namespace bananagrams
