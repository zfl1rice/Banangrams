#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/rack.hpp"

#include <array>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

using bananagrams::Rack;

// These must hold at compile time: callers cannot change counts through the API.
using CountsView = decltype(std::declval<const Rack&>().counts());
static_assert(std::is_same_v<CountsView, const std::array<int, 26>&>);
static_assert(!std::is_assignable_v<decltype(std::declval<CountsView>()[0]), int>);

namespace {

void check_rack(const Rack& rack, const std::array<int, 26>& expected) {
    CHECK(rack.counts() == expected);
    std::size_t expected_size = 0;
    long long counts_sum = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto letter = static_cast<char>('A' + static_cast<int>(i));
        INFO("letter := " << std::string(1, letter));
        CHECK(rack.count(letter) == expected[i]);
        CHECK(rack.count(letter) == rack.counts()[i]);
        CHECK(rack.counts()[i] >= 0);
        expected_size += static_cast<std::size_t>(expected[i]);
        counts_sum += rack.counts()[i];
    }
    CHECK(rack.size() == expected_size);
    CHECK(rack.empty() == (expected_size == 0));
    CHECK(rack.empty() == (rack.size() == 0));
    CHECK(counts_sum >= 0);
    if (counts_sum >= 0) {
        CHECK(rack.size() == static_cast<std::size_t>(counts_sum));
    }
}

} // namespace

TEST_CASE("Rack starts empty") {
    Rack rack{};
    CHECK(rack.empty());
    CHECK(rack.size() == 0);
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        INFO("letter := " << std::string(1, letter));
        CHECK(rack.count(letter) == 0);
    }
}

TEST_CASE("Rack tracks added tiles and becomes empty after removal") {
    Rack rack{};
    REQUIRE(rack.add('A'));
    CHECK_FALSE(rack.empty());
    CHECK(rack.size() == 1);
    CHECK(rack.count('A') == 1);

    REQUIRE(rack.add('A'));
    REQUIRE(rack.add('B'));
    CHECK(rack.size() == 3);
    CHECK(rack.count('A') == 2);
    CHECK(rack.count('B') == 1);

    REQUIRE(rack.remove('A'));
    CHECK(rack.count('A') == 1);
    CHECK(rack.size() == 2);
    CHECK_FALSE(rack.empty());

    REQUIRE(rack.remove('A'));
    REQUIRE(rack.remove('B'));
    CHECK(rack.empty());
    CHECK(rack.size() == 0);
    CHECK(rack.count('A') == 0);
    CHECK(rack.count('B') == 0);
}

TEST_CASE("Rack rejects unsupported characters without changing state") {
    // The API accepts one byte, not a Unicode character. High bytes include
    // bytes from UTF-8 encodings of accented and non-Latin characters.
    // Lowercase is unsupported too: the public contract specifies uppercase A-Z.
    for (bool populated : {false, true}) {
        for (unsigned int byte = 0; byte <= std::numeric_limits<unsigned char>::max(); ++byte) {
            if (byte >= 'A' && byte <= 'Z') {
                continue;
            }
            CAPTURE(populated);
            CAPTURE(static_cast<unsigned int>(byte));
            Rack rack{};
            if (populated) {
                REQUIRE(rack.add('A'));
                REQUIRE(rack.add('A'));
                REQUIRE(rack.add('B'));
            }
            const auto size_before = rack.size();
            const auto empty_before = rack.empty();
            std::array<int, 26> before{};
            for (int i = 0; i < 26; ++i) {
                before[static_cast<std::size_t>(i)] = rack.count(static_cast<char>('A' + i));
            }

            const auto check_unchanged = [&] {
                CHECK(rack.size() == size_before);
                CHECK(rack.empty() == empty_before);
                for (int i = 0; i < 26; ++i) {
                    const auto checked_letter = static_cast<char>('A' + i);
                    INFO("letter := " << std::string(1, checked_letter));
                    CHECK(rack.count(checked_letter) == before[static_cast<std::size_t>(i)]);
                }
            };
            const auto letter = static_cast<char>(byte);
            CHECK(rack.count(letter) == 0);
            check_unchanged();
            CHECK_FALSE(rack.add(letter));
            check_unchanged();
            CHECK_FALSE(rack.remove(letter));
            check_unchanged();
        }
    }
}

TEST_CASE("Rack counts provides a stable read only view") {
    Rack rack{};
    REQUIRE(rack.add('A'));
    REQUIRE(rack.add('A'));
    REQUIRE(rack.add('B'));
    const Rack& read_only = rack;
    const auto& view = read_only.counts();
    std::array<int, 26> expected{};
    expected[0] = 2;
    expected[1] = 1;
    CHECK(view == expected);
    CHECK(&read_only.counts() == &view);

    // Read operations leave the rack unchanged.
    static_cast<void>(read_only.empty());
    static_cast<void>(read_only.size());
    static_cast<void>(read_only.count('A'));
    CHECK(view == expected);
    CHECK(read_only.size() == 3);

    // It is a reference to the rack's counts, so valid mutations remain visible.
    REQUIRE(rack.add('B'));
    expected[1] = 2;
    CHECK(view == expected);
    REQUIRE(rack.remove('A'));
    expected[0] = 1;
    CHECK(view == expected);
}

TEST_CASE("Rack cannot remove a letter with zero count") {
    Rack rack{};
    CHECK_FALSE(rack.remove('A'));
    CHECK(rack.count('A') == 0);
    CHECK(rack.size() == 0);
    CHECK(rack.empty());

    REQUIRE(rack.add('B'));
    CHECK_FALSE(rack.remove('A'));
    CHECK(rack.count('A') == 0);
    CHECK(rack.count('B') == 1);
    CHECK(rack.size() == 1);

    REQUIRE(rack.remove('B'));
    CHECK_FALSE(rack.remove('B'));
    CHECK_FALSE(rack.remove('B'));
    CHECK(rack.count('B') == 0);
    CHECK(rack.size() == 0);
    CHECK(rack.empty());
}

TEST_CASE("Rack starts empty without brace initialization") {
    Rack rack;
    check_rack(rack, {});
}

TEST_CASE("Rack handles every uppercase letter in its own slot") {
    Rack rack{};
    std::array<int, 26> expected{};
    check_rack(rack, expected);

    // Include both ends of the alphabet and duplicate counts for every letter.
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = 0; i < expected.size(); ++i) {
            CAPTURE(pass);
            const auto letter = static_cast<char>('A' + static_cast<int>(i));
            INFO("letter := " << std::string(1, letter));
            REQUIRE(rack.add(letter));
            ++expected[i];
            check_rack(rack, expected);
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = expected.size(); i-- > 0;) {
            CAPTURE(pass);
            const auto letter = static_cast<char>('A' + static_cast<int>(i));
            INFO("letter := " << std::string(1, letter));
            REQUIRE(rack.remove(letter));
            --expected[i];
            check_rack(rack, expected);
        }
    }
}

TEST_CASE("Rack instances and copies have independent state") {
    Rack original{};
    REQUIRE(original.add('A'));
    REQUIRE(original.add('A'));
    REQUIRE(original.add('M'));
    std::array<int, 26> original_expected{};
    original_expected[0] = 2;
    original_expected[12] = 1;

    SUBCASE("Separately constructed rack") {
        Rack other{};
        check_rack(other, {});
        REQUIRE(other.add('B'));
        std::array<int, 26> other_expected{};
        other_expected[1] = 1;
        check_rack(other, other_expected);
        check_rack(original, original_expected);
    }
    SUBCASE("Copy construction") {
        Rack copy = original;
        check_rack(copy, original_expected);
        REQUIRE(copy.remove('A'));
        REQUIRE(copy.add('B'));
        auto copy_expected = original_expected;
        --copy_expected[0];
        ++copy_expected[1];
        check_rack(copy, copy_expected);
        check_rack(original, original_expected);

        REQUIRE(original.remove('M'));
        original_expected[12] = 0;
        check_rack(original, original_expected);
        check_rack(copy, copy_expected);
    }
    SUBCASE("Copy assignment replaces existing contents") {
        Rack copy{};
        REQUIRE(copy.add('Q'));
        copy = original;
        check_rack(copy, original_expected);
        REQUIRE(copy.remove('A'));
        auto copy_expected = original_expected;
        --copy_expected[0];
        check_rack(copy, copy_expected);
        check_rack(original, original_expected);
    }
}

TEST_CASE("Rack stays consistent through repeated mixed operations") {
    Rack rack{};
    std::array<int, 26> expected{};
    check_rack(rack, expected);
    for (int cycle = 0; cycle < 4; ++cycle) {
        CAPTURE(cycle);
        for (char letter : {'A', 'Z', 'M', 'A', 'Q', 'Z'}) {
            INFO("letter := " << std::string(1, letter));
            REQUIRE(rack.add(letter));
            ++expected[static_cast<std::size_t>(letter - 'A')];
            check_rack(rack, expected);
        }
        REQUIRE(rack.remove('A'));
        --expected[0];
        check_rack(rack, expected);
        REQUIRE(rack.add('B'));
        ++expected[1];
        check_rack(rack, expected);

        for (char letter : {'M', 'Z', 'A', 'B', 'Z', 'Q'}) {
            INFO("letter := " << std::string(1, letter));
            REQUIRE(rack.remove(letter));
            --expected[static_cast<std::size_t>(letter - 'A')];
            check_rack(rack, expected);
        }
        CHECK_FALSE(rack.remove('Z'));
        check_rack(rack, expected);
    }
}
