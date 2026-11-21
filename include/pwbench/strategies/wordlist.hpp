#ifndef PWBENCH_STRATEGIES_WORDLIST_HPP
#define PWBENCH_STRATEGIES_WORDLIST_HPP

#include "pwbench/types.hpp"

#include <filesystem>
#include <fstream>

namespace pwbench {

class WordlistWork {
public:
    explicit WordlistWork(std::vector<Byte> bytes);
    std::optional<CandidateView> next_candidate();

private:
    std::vector<Byte> bytes_;
    std::size_t position_{};
};

class WordlistStrategy {
public:
    using work_type = WordlistWork;

    explicit WordlistStrategy(const std::filesystem::path& path,
                              std::size_t block_bytes = 64 * 1024);
    std::optional<work_type> next_work();

private:
    void check_read() const;
    void complete_line(std::vector<Byte>& bytes);

    std::ifstream input_;
    std::size_t block_bytes_;
};

}

#endif
