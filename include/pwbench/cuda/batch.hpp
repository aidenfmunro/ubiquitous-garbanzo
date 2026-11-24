#ifndef PWBENCH_CUDA_BATCH_HPP
#define PWBENCH_CUDA_BATCH_HPP

#include "pwbench/types.hpp"

#include <cstring>
#include <span>
#include <stdexcept>

namespace pwbench {

class CudaBatch {
public:
    void clear();
    void append(CandidateView candidate) {
        if (candidate.size != 0 && candidate.data == nullptr) {
            throw std::invalid_argument("nonempty candidate has null data");
        }
        if (candidate.size > bytes_.size() - size_) {
            grow(candidate.size);
        }
        offsets_.push_back(size_ + candidate.size);
        if (candidate.size != 0) {
            std::memcpy(bytes_.data() + size_, candidate.data, candidate.size);
        }
        size_ += candidate.size;
    }
    CandidateView candidate(std::size_t index) const;

    std::size_t count() const noexcept {
        return offsets_.size() - 1;
    }
    std::span<const Byte> bytes() const noexcept {
        return {bytes_.data(), size_};
    }
    std::span<const std::uint64_t> offsets() const noexcept {
        return offsets_;
    }

private:
    void grow(std::size_t additional);

    std::vector<Byte> bytes_;
    std::vector<std::uint64_t> offsets_{0};
    std::size_t size_{};
};

}

#endif
