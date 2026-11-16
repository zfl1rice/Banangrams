#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bananagrams/lexicon.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using bananagrams::Lexicon;

std::vector<std::string> malformed_words() {
    return {
        "", "A", "Z", "a", "cat", "Cat", "cAT", "CaT",
        "C4T", "42", "CAT!", "CAN'T", "CO-OP", " CAT", "CAT ",
        "C AT", "CA\tT", "CAT\n", "CAT\r\n", "\nCAT", "  ",
        std::string("CA\0T", 4), std::string("\0AT", 3),
        std::string("CAT\0", 4), std::string("\0\0", 2),
        // UTF-8 accented uppercase letters and non-Latin characters are not A-Z.
        "CAF\xC3\x89", "\xE7\x8C\xAB", std::string(2, static_cast<char>(0xFF))};
}

} // namespace

TEST_CASE("Lexicon accepts an empty in memory word list") {
    const Lexicon lexicon{std::vector<std::string>{}};
    CHECK(lexicon.size() == 0);
    CHECK_FALSE(lexicon.contains("AT"));
    CHECK_FALSE(lexicon.contains("QI"));
    CHECK_FALSE(lexicon.contains(""));
    CHECK(lexicon.size() == 0);
}

TEST_CASE("Lexicon stores and finds valid uppercase words") {
    const std::vector<std::string> words{
        "AT", "QI", "CAT", "CATS", "DOG", "SCAT",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ", std::string(256, 'Z')};
    const Lexicon lexicon{words};
    CHECK(lexicon.size() == words.size());
    for (const auto& word : words) {
        CAPTURE(word);
        CHECK(lexicon.contains(word));
    }
    CHECK(lexicon.size() == words.size());
}

TEST_CASE("Lexicon stores each duplicate word only once") {
    const std::vector<std::string> words{"CAT", "AT", "CAT", "QI", "AT", "CAT", "QI"};
    const Lexicon lexicon{words};
    CHECK(lexicon.size() == 3);
    CHECK(lexicon.contains("CAT"));
    CHECK(lexicon.contains("AT"));
    CHECK(lexicon.contains("QI"));

    const Lexicon repeated{std::vector<std::string>(100, "CAT")};
    CHECK(repeated.size() == 1);
    CHECK(repeated.contains("CAT"));
    CHECK_FALSE(repeated.contains("AT"));

    auto reversed = words;
    std::reverse(reversed.begin(), reversed.end());
    const Lexicon reordered{reversed};
    CHECK(reordered.size() == lexicon.size());
    for (const auto& word : words) {
        CAPTURE(word);
        CHECK(reordered.contains(word));
    }
}

TEST_CASE("Lexicon rejects construction for any malformed entry") {
    for (const auto& word : malformed_words()) {
        CAPTURE(word);
        CHECK_THROWS_AS((Lexicon{std::vector<std::string>{word}}), std::invalid_argument);
        // A bad entry anywhere must reject the entire construction, including
        // when valid duplicates or additional valid words surround it.
        for (std::size_t position = 0; position <= 3; ++position) {
            CAPTURE(position);
            std::vector<std::string> words{"AT", "CAT", "AT"};
            words.insert(words.begin() + static_cast<std::ptrdiff_t>(position), word);
            CHECK_THROWS_AS((Lexicon{words}), std::invalid_argument);
        }
    }
}

TEST_CASE("Lexicon rejects every byte outside uppercase A through Z") {
    const Lexicon lexicon{std::vector<std::string>{"CAT"}};
    for (unsigned int byte = 0; byte <= std::numeric_limits<unsigned char>::max(); ++byte) {
        if (byte >= 'A' && byte <= 'Z') {
            continue;
        }
        CAPTURE(byte);
        for (std::size_t position = 0; position < 3; ++position) {
            CAPTURE(position);
            std::string word = "CAT";
            word[position] = static_cast<char>(byte);
            CHECK_THROWS_AS((Lexicon{std::vector<std::string>{word}}), std::invalid_argument);
            CHECK_FALSE(lexicon.contains(word));
        }
    }
    CHECK(lexicon.size() == 1);
    CHECK(lexicon.contains("CAT"));
}

TEST_CASE("Lexicon lookups require exact stored words") {
    const Lexicon lexicon{std::vector<std::string>{"CAT", "CATS", "SCAT", "DOG"}};
    for (const std::string word : {"CA", "AT", "CATT", "ACT", "TAC", "ATCAT",
                                   "BAT", "OG", "DOGS", "SC", "CATCAT", "WORD"}) {
        CAPTURE(word);
        CHECK_FALSE(lexicon.contains(word));
    }
    for (const auto& word : malformed_words()) {
        CAPTURE(word);
        CHECK_FALSE(lexicon.contains(word));
    }
    // Repeated misses and hits must leave membership and size unchanged.
    CHECK(lexicon.size() == 4);
    for (const std::string word : {"CAT", "CATS", "SCAT", "DOG"}) {
        CAPTURE(word);
        CHECK(lexicon.contains(word));
        CHECK(lexicon.contains(word));
    }
    CHECK(lexicon.size() == 4);
}

TEST_CASE("Lexicon owns its words independently of the input vector") {
    std::vector<std::string> words{"AT", "CAT", "QI", "CAT"};
    const auto before = words;
    const Lexicon lexicon{words};
    CHECK(words == before);
    words[0] = "DOG";
    words.clear();
    CHECK(lexicon.size() == 3);
    CHECK(lexicon.contains("AT"));
    CHECK(lexicon.contains("CAT"));
    CHECK(lexicon.contains("QI"));
    CHECK_FALSE(lexicon.contains("DOG"));
}
