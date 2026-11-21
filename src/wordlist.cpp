#include "pwbench/strategies/wordlist.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace pwbench {

WordlistWork::WordlistWork(std::vector<Byte> bytes) : bytes_(std::move(bytes)) {}

std::optional<CandidateView> WordlistWork::next_candidate() {
    if (position_ == bytes_.size()) {
        return std::nullopt;
    }
    const auto begin = position_;
    while (position_ < bytes_.size() && bytes_[position_] != '\n') {
        ++position_;
    }
    auto size = position_ - begin;
    if (position_ < bytes_.size()) {
        if (size != 0 && bytes_[position_ - 1] == '\r') {
            --size;
        }
        ++position_;
    }
    return CandidateView{bytes_.data() + begin, size};
}

WordlistStrategy::WordlistStrategy(const std::filesystem::path& path, std::size_t block_bytes)
    : block_bytes_(block_bytes) {
    if (block_bytes == 0 ||
        block_bytes > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::invalid_argument("wordlist block size is out of range");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::runtime_error("wordlist path is a directory: " + path.string());
    }
    input_.open(path, std::ios::binary);
    if (!input_) {
        throw std::runtime_error("cannot open wordlist: " + path.string());
    }
}

std::optional<WordlistStrategy::work_type> WordlistStrategy::next_work() {
    if (input_.eof()) {
        return std::nullopt;
    }
    std::vector<Byte> bytes(block_bytes_);
    input_.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const auto count = input_.gcount();
    check_read();
    bytes.resize(static_cast<std::size_t>(count));
    if (bytes.empty()) {
        return std::nullopt;
    }
    if (!input_.eof() && bytes.back() != '\n') {
        complete_line(bytes);
    }
    return work_type(std::move(bytes));
}

void WordlistStrategy::check_read() const {
    if (input_.bad() || (input_.fail() && !input_.eof())) {
        throw std::runtime_error("failed to read wordlist");
    }
}

void WordlistStrategy::complete_line(std::vector<Byte>& bytes) {
    std::string tail;
    std::getline(input_, tail);
    check_read();
    bytes.insert(bytes.end(), tail.begin(), tail.end());
    if (!input_.eof()) {
        bytes.push_back('\n');
    }
}

}
