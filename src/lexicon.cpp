#include "bananagrams/lexicon.hpp"

#include <stdexcept>

namespace bananagrams {

// TODO: Reject malformed entries and store unique words without file I/O.
Lexicon::Lexicon(const std::vector<std::string>& valid_words) {
    for (std::string word : valid_words){
        if ((word.size() >= 2 && isAllUpper(word))) {
            this->LEXICON.insert(word);
        } else {
            throw std::invalid_argument("malformed lexicon entry");
        }
    }
}

bool Lexicon::isAllUpper(const std::string& s) {
    for (unsigned c: s) {
        if (!std::isupper(c)) {
            return false;
        }
    }
    return true;
}
bool Lexicon::contains(const std::string& word) const {
    return this->LEXICON.count(word) == 1;
}
std::size_t Lexicon::size() const {
    return this->LEXICON.size();
}

} // namespace bananagrams
