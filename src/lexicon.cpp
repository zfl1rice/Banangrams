#include "bananagrams/lexicon.hpp"

#include <stdexcept>

namespace bananagrams {

// TODO: Reject malformed entries and store unique words without file I/O.
Lexicon::Lexicon(const std::vector<std::string>& valid_words) {
    for (std::string word : valid_words){
        this->LEXICON.insert(word);
    }
}
bool Lexicon::contains(const std::string& word) const {
    return this->LEXICON.count(word);
}
std::size_t Lexicon::size() const {
    return this->LEXICON.size();
}

} // namespace bananagrams
