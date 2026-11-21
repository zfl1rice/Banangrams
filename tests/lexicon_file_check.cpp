// Standalone integration check: file loading stays outside the engine.
#include "bananagrams/lexicon.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: lexicon_file_check words.txt\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open word list");
        std::vector<std::string> words;
        for (std::string word; std::getline(input, word);) {
            if (word.size() < 2 || word.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
                throw std::runtime_error("Invalid word-list format");
            if (!words.empty() && words.back() >= word)
                throw std::runtime_error("Words must be sorted and unique");
            words.push_back(word);
        }
        const bananagrams::Lexicon lexicon(words);
        if (lexicon.size() != words.size()) throw std::runtime_error("Size mismatch");
        for (const auto& word : words)
            if (!lexicon.contains(word)) throw std::runtime_error("Missing loaded word");
        for (const auto* word : {"QI", "ROSE", "MARK", "MAY", "WILL", "TOAD", "CLOUDY", "BANJO", "DO", "SO", "GYM", "HERESY", "HERESIES", "THEOLOGY", "BISHOP", "NUN", "HYMN"})
            if (!lexicon.contains(word)) throw std::runtime_error("Missing required word");
        for (const auto* word : {"SH", "FA", "HM", "PFFT", "MI", "LA", "CD", "CM", "KB", "INC", "ED", "JESUS", "CHRIST", "ALLAH", "SATAN"})
            if (lexicon.contains(word)) throw std::runtime_error("Excluded spelling is playable");
        std::cout << "Loaded and checked " << lexicon.size() << " words through C++ Lexicon\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
