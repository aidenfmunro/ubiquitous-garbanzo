#include "pwbench/cuda/runtime.hpp"

#include "pwbench/cuda/batch.hpp"

#include <algorithm>
#include <string>

namespace pwbench {

void check_cuda(cudaError_t error, std::string_view operation) {
    if (error != cudaSuccess) {
        cudaGetLastError();
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(error));
    }
}

CudaContext::CudaContext(int device) : device_(device) {
    check_cuda(cudaGetDevice(&previous_device_), "cudaGetDevice");
    check_cuda(cudaSetDevice(device_), "cudaSetDevice");
    try {
        check_cuda(cudaGetDeviceProperties(&properties_, device_), "cudaGetDeviceProperties");
        check_cuda(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking),
                   "cudaStreamCreateWithFlags");
    } catch (...) {
        cudaSetDevice(previous_device_);
        throw;
    }
}

CudaContext::~CudaContext() {
    drain();
    cudaStreamDestroy(stream_);
    if (device_ != previous_device_) {
        cudaSetDevice(previous_device_);
    }
}

void CudaContext::synchronize() const {
    check_cuda(cudaStreamSynchronize(stream_), "cudaStreamSynchronize");
}

void CudaContext::drain() const noexcept {
    cudaStreamSynchronize(stream_);
}

CudaWorkspace::CudaWorkspace(int device) : context_(device), match_(1) {}

CudaDeviceBatch CudaWorkspace::upload(const CudaBatch& batch) {
    bytes_.reserve(std::max(batch.bytes().size(), std::size_t{1}));
    offsets_.reserve(batch.offsets().size());
    const auto stream = context_.stream();
    if (!batch.bytes().empty()) {
        check_cuda(cudaMemcpyAsync(bytes_.data(), batch.bytes().data(), batch.bytes().size(),
                                   cudaMemcpyHostToDevice, stream),
                   "cudaMemcpyAsync(candidate bytes)");
    }
    check_cuda(cudaMemcpyAsync(offsets_.data(), batch.offsets().data(),
                               batch.offsets().size_bytes(), cudaMemcpyHostToDevice, stream),
               "cudaMemcpyAsync(candidate offsets)");
    check_cuda(cudaMemsetAsync(match_.data(), 0xff, sizeof(CudaMatch), stream),
               "cudaMemsetAsync(match)");
    return {bytes_.data(), offsets_.data(), batch.count()};
}

unsigned long long CudaWorkspace::complete() {
    check_cuda(cudaMemcpyAsync(&host_match_, match_.data(), sizeof(CudaMatch),
                               cudaMemcpyDeviceToHost, context_.stream()),
               "cudaMemcpyAsync(match)");
    context_.synchronize();
    return host_match_.index;
}

}
