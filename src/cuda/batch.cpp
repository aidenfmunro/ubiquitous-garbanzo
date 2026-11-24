#include "pwbench/cuda/batch.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pwbench {

void CudaBatch::clear() {
    size_ = 0;
    offsets_.resize(1);
}

void CudaBatch::grow(std::size_t additional) {
    const auto maximum = bytes_.max_size();
    if (additional > maximum - size_ ||
        additional > std::numeric_limits<std::uint64_t>::max() - size_) {
        throw std::length_error("CUDA candidate batch is too large");
    }
    const auto growth = std::min(std::max(bytes_.size(), std::size_t{64}), maximum - bytes_.size());
    bytes_.resize(std::max(size_ + additional, bytes_.size() + growth));
}

CandidateView CudaBatch::candidate(std::size_t index) const {
    if (index >= count()) {
        throw std::out_of_range("CUDA algorithm returned an invalid candidate index");
    }
    const auto begin = static_cast<std::size_t>(offsets_[index]);
    const auto size = static_cast<std::size_t>(offsets_[index + 1] - offsets_[index]);
    return {size == 0 ? nullptr : bytes_.data() + begin, size};
}

}
