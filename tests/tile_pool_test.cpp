#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/tile_pool.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace {

using bananagrams::TilePool;
using Counts = std::array<int, 26>;
constexpr std::uint32_t seed = 42;
constexpr std::size_t bag_size = 144;
// English Classic distribution, A through Z:
// https://www.bananagrammer.com/2009/07/letter-distributions-in-bananagrams-and.html
constexpr Counts distribution{
    13, 3, 3, 6, 18, 3, 4, 3, 12, 2, 2, 5, 3,
    8, 11, 3, 2, 9, 6, 9, 6, 3, 3, 2, 3, 2};
static_assert(std::accumulate(distribution.begin(), distribution.end(), 0) == bag_size);

TilePool standard_pool() {
    std::vector<char> letters;
    for (std::size_t i = 0; i < distribution.size(); ++i) {
        letters.insert(letters.end(), static_cast<std::size_t>(distribution[i]),
                       static_cast<char>('A' + static_cast<int>(i)));
    }
    return TilePool(letters, seed);
}

void check_state(const TilePool& pool, std::size_t remaining) {
    CHECK(pool.size() == remaining);
    CHECK(pool.empty() == (remaining == 0));
    CHECK(pool.empty() == (pool.size() == 0));
}

Counts tally(const std::vector<char>& letters) {
    Counts counts{};
    for (char letter : letters) {
        INFO("drawn byte := " << static_cast<unsigned int>(static_cast<unsigned char>(letter)));
        const bool valid = letter >= 'A' && letter <= 'Z';
        CHECK(valid);
        // Avoid indexing outside the test's array if the implementation fails.
        if (valid) {
            ++counts[static_cast<std::size_t>(letter - 'A')];
        }
    }
    return counts;
}

void check_distribution(const std::vector<char>& letters, const Counts& expected = distribution) {
    const auto actual = tally(letters);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("letter := " << std::string(1, static_cast<char>('A' + static_cast<int>(i))));
        CHECK(actual[i] == expected[i]);
    }
}

std::vector<char> draw_exactly(TilePool& pool, std::size_t count) {
    const auto before = pool.size();
    REQUIRE(count <= before);
    std::vector<char> drawn;
    for (std::size_t i = 0; i < count; ++i) {
        CAPTURE(i);
        const auto letter = pool.draw();
        REQUIRE(letter.has_value());
        drawn.push_back(*letter);
        check_state(pool, before - i - 1);
    }
    return drawn;
}

void check_exhausted(TilePool& pool) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        CAPTURE(attempt);
        CHECK(pool.draw() == std::nullopt);
        CHECK(pool.draw_up_to(1).empty());
        CHECK(pool.draw_up_to(bag_size + 1).empty());
        CHECK(pool.draw_up_to(0).empty());
        check_state(pool, 0);
    }
}

} // namespace

TEST_CASE("TilePool starts with the standard bag") {
    auto pool = standard_pool();
    check_state(pool, bag_size);
    CHECK(pool.draw_up_to(0).empty());
    check_state(pool, bag_size);
}

TEST_CASE("TilePool single draws preserve distribution and cannot overdraw") {
    auto pool = standard_pool();
    const auto drawn = draw_exactly(pool, bag_size);
    check_distribution(drawn);
    check_exhausted(pool);
}

TEST_CASE("TilePool supports every batch size through overdraw") {
    for (std::size_t count = 0; count <= bag_size + 1; ++count) {
        CAPTURE(count);
        auto pool = standard_pool();
        auto drawn = pool.draw_up_to(count);
        const auto expected_count = std::min(count, bag_size);
        REQUIRE(drawn.size() == expected_count);
        check_state(pool, bag_size - expected_count);

        const auto remaining = draw_exactly(pool, bag_size - expected_count);
        drawn.insert(drawn.end(), remaining.begin(), remaining.end());
        check_distribution(drawn);
        check_exhausted(pool);
    }
}

TEST_CASE("TilePool limits batches to the remaining tiles") {
    auto pool = standard_pool();
    auto drawn = draw_exactly(pool, bag_size - 3);
    CHECK(pool.draw_up_to(0).empty());
    check_state(pool, 3);
    const auto last = pool.draw_up_to(5);
    CHECK(last.size() == 3);
    check_state(pool, 0);
    drawn.insert(drawn.end(), last.begin(), last.end());
    check_distribution(drawn);
    check_exhausted(pool);
}

TEST_CASE("TilePool redraws every returned letter after exhaustion") {
    auto pool = standard_pool();
    const auto drawn = pool.draw_up_to(bag_size);
    REQUIRE(drawn.size() == bag_size);
    check_state(pool, 0);
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        INFO("letter := " << std::string(1, letter));
        REQUIRE(pool.return_tile(letter));
        check_state(pool, 1);
        const auto redrawn = pool.draw();
        REQUIRE(redrawn.has_value());
        CHECK(*redrawn == letter);
        check_exhausted(pool);
    }
}

TEST_CASE("TilePool rejects invalid returns without changing state") {
    for (bool drained : {false, true}) {
        for (unsigned int byte = 0; byte <= std::numeric_limits<unsigned char>::max(); ++byte) {
            if (byte >= 'A' && byte <= 'Z') {
                continue;
            }
            CAPTURE(drained);
            CAPTURE(byte);
            auto pool = standard_pool();
            auto control = standard_pool();
            if (drained) {
                REQUIRE(pool.draw_up_to(bag_size).size() == bag_size);
                REQUIRE(control.draw_up_to(bag_size).size() == bag_size);
            }
            const auto remaining = drained ? std::size_t{0} : bag_size;
            check_state(pool, remaining);
            CHECK_FALSE(pool.return_tile(static_cast<char>(byte)));
            check_state(pool, remaining);
            const auto actual = pool.draw_up_to(bag_size + 1);
            const auto expected = control.draw_up_to(bag_size + 1);
            CHECK(actual == expected);
            CHECK(actual.size() == remaining);
            check_distribution(actual, drained ? Counts{} : distribution);
            check_state(pool, 0);
        }
    }
}

TEST_CASE("TilePool conserves tiles through mixed draws and returns") {
    auto pool = standard_pool();
    auto drawn = draw_exactly(pool, 5);
    Counts expected = distribution;
    const auto returned = drawn.front();
    REQUIRE(returned >= 'A');
    REQUIRE(returned <= 'Z');
    REQUIRE(pool.return_tile(returned));
    ++expected[static_cast<std::size_t>(returned - 'A')];
    check_state(pool, 140);

    const auto batch = pool.draw_up_to(17);
    REQUIRE(batch.size() == 17);
    drawn.insert(drawn.end(), batch.begin(), batch.end());
    check_state(pool, 123);
    REQUIRE(pool.return_tile('Z'));
    ++expected[25];
    REQUIRE(pool.return_tile('Z'));
    ++expected[25];
    check_state(pool, 125);

    const auto single = draw_exactly(pool, 1);
    drawn.insert(drawn.end(), single.begin(), single.end());
    const auto rest = pool.draw_up_to(bag_size);
    CHECK(rest.size() == 124);
    drawn.insert(drawn.end(), rest.begin(), rest.end());
    CHECK(drawn.size() == bag_size + 3);
    check_distribution(drawn, expected);
    check_exhausted(pool);
}

TEST_CASE("TilePool seed 42 reproduces identical operations") {
    auto first = standard_pool();
    auto second = standard_pool();
    for (int i = 0; i < 10; ++i) {
        CAPTURE(i);
        const auto a = first.draw();
        const auto b = second.draw();
        REQUIRE(a.has_value());
        REQUIRE(b.has_value());
        CHECK(a == b);
        check_state(first, bag_size - static_cast<std::size_t>(i) - 1);
        check_state(second, first.size());
    }
    CHECK(first.draw_up_to(0) == second.draw_up_to(0));
    const auto first_batch = first.draw_up_to(21);
    const auto second_batch = second.draw_up_to(21);
    CHECK(first_batch == second_batch);
    CHECK(first_batch.size() == 21);
    check_state(first, 113);
    check_state(second, 113);
    REQUIRE(first.return_tile('A'));
    REQUIRE(second.return_tile('A'));
    check_state(first, 114);
    check_state(second, 114);
    CHECK(draw_exactly(first, 114) == draw_exactly(second, 114));
    check_exhausted(first);
    check_exhausted(second);
}

TEST_CASE("TilePool batches match consecutive single draws") {
    auto singles = standard_pool();
    auto batches = standard_pool();
    for (std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{3},
                              std::size_t{20}, std::size_t{120}}) {
        CAPTURE(count);
        const auto expected = draw_exactly(singles, count);
        CHECK(batches.draw_up_to(count) == expected);
        check_state(batches, singles.size());
    }
    check_exhausted(singles);
    check_exhausted(batches);
}
