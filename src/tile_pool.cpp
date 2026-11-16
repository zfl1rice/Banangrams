#include "bananagrams/tile_pool.hpp"

#include <stdexcept>
#include <random>

namespace bananagrams {

// TODO: Validate supplied letters and initialize the pool and seeded RNG.
TilePool::TilePool(std::vector<char> init_letters, std::uint32_t seed) {
    this->LETTERS = init_letters;
    this->SEED = seed;
}
bool TilePool::empty() const {
    return this->LETTERS.size() == 0;
}
std::size_t TilePool::size() const {
    return this->LETTERS.size();
}
// TODO: Sample the current pool; define portable sampling later if replay needs it.
char TilePool::draw() {
    if (this->LETTERS.size() == 0){
        return NULL;
    }
    std::mt19937 gen(this->SEED);

    std::uniform_int_distribution<int> dist(0, this->LETTERS.size());

    int idx = dist(gen);

    char res = this->LETTERS[idx];

    std::swap(this->LETTERS[idx], this->LETTERS[this->LETTERS.size() - 1]);

    this->LETTERS.pop_back();

    return res;
}
std::vector<char> TilePool::draw_up_to(std::size_t count) {
    std::vector<char> res = {};
    for (int i = 0; i < count; i ++) {
        char next_letter = draw();
        if (next_letter != NULL) {
            res.push_back(next_letter);
        } else {
            return res;
        }
    }
    return res;
}
bool TilePool::return_tile(char letter) {
    if (!std::isalpha(static_cast<unsigned char>(letter))) {
        return false;
    }

    this->LETTERS.push_back(letter);
    return true;
}

} // namespace bananagrams
