#ifndef PWBENCH_STRATEGIES_WORDLIST_HPP
#define PWBENCH_STRATEGIES_WORDLIST_HPP

#include "pwbench/types.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace pwbench {

class WordlistWork {
public:
    explicit WordlistWork(std::vector<Byte> bytes);
    std::optional<CandidateView> next_candidate() {
        if (position_ == bytes_.size()) {
            return std::nullopt;
        }
        const auto* begin = bytes_.data() + position_;
        const auto* newline =
            static_cast<const Byte*>(std::memchr(begin, '\n', bytes_.size() - position_));
        if (newline == nullptr) {
            const auto size = bytes_.size() - position_;
            position_ = bytes_.size();
            return CandidateView{begin, size};
        }
        auto size = static_cast<std::size_t>(newline - begin);
        if (size != 0 && newline[-1] == '\r') {
            --size;
        }
        position_ = static_cast<std::size_t>(newline - bytes_.data()) + 1;
        return CandidateView{begin, size};
    }

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
