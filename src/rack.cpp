#include "bananagrams/rack.hpp"

#include <stdexcept>
#include <cctype>

namespace bananagrams {

// TODO: Implement counts and A-Z validation; preserve state on mutation failure.
bool Rack::empty() const {
    return this->NUM_TILES == 0;
}
std::size_t Rack::size() const {
    return this->NUM_TILES;
}
int Rack::count(char letter) const {
    if (!std::isalpha(static_cast<unsigned char>(letter))) {
        return 0;
    }
    int idx = std::tolower(static_cast<unsigned char>(letter)) - 'a' + 1;
    return this->CURR_COUNTS[idx];
}
bool Rack::add(char letter) {
    if (!std::isalpha(static_cast<unsigned char>(letter))) {
        return false;
    }
    int idx = std::tolower(static_cast<unsigned char>(letter)) - 'a' + 1;
    this->CURR_COUNTS[idx] ++;
    return true;
}
bool Rack::remove(char letter) {
    if (!std::isalpha(static_cast<unsigned char>(letter))) {
        return false;
    }
    int idx = std::tolower(static_cast<unsigned char>(letter)) - 'a' + 1;
    if (this->CURR_COUNTS[idx] > 0) {
        this->CURR_COUNTS[idx] --;
        return true;
    }
    return false;
}
const std::array<int, 26>& Rack::counts() const {
    std::array<int, 26> res = this->CURR_COUNTS;
    return res;
}

} // namespace bananagrams
