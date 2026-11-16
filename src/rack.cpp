#include "bananagrams/rack.hpp"

#include <stdexcept>

namespace bananagrams {

bool Rack::empty() const {
    return this->NUM_TILES == 0;
}
std::size_t Rack::size() const {
    return this->NUM_TILES;
}
int Rack::count(char letter) const {
    if (!(letter >= 'A' && letter <= 'Z')) {
        return 0;
    }
    int idx = letter - 'A';
    return this->CURR_COUNTS[idx];
}
bool Rack::add(char letter) {
    if (!(letter >= 'A' && letter <= 'Z')) {
        return false;
    }
    int idx = letter - 'A';
    this->CURR_COUNTS[idx] ++;
    this->NUM_TILES ++;
    return true;
}
bool Rack::remove(char letter) {
    if (!(letter >= 'A' && letter <= 'Z')) {
        return false;
    }
    int idx = letter - 'A';
    if (this->CURR_COUNTS[idx] > 0) {
        this->CURR_COUNTS[idx] --;
        this->NUM_TILES --;
        return true;
    }
    return false;
}
const std::array<int, 26>& Rack::counts() const {
    return this->CURR_COUNTS;
}

} // namespace bananagrams
