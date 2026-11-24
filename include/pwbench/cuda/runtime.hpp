#ifndef PWBENCH_CUDA_RUNTIME_HPP
#define PWBENCH_CUDA_RUNTIME_HPP

#include "pwbench/cuda/types.hpp"

#include <cuda_runtime_api.h>

#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pwbench {

void check_cuda(cudaError_t error, std::string_view operation);

class CudaContext {
public:
    explicit CudaContext(int device);
    ~CudaContext();
    CudaContext(const CudaContext&) = delete;
    CudaContext& operator=(const CudaContext&) = delete;

    cudaStream_t stream() const noexcept {
        return stream_;
    }
    const cudaDeviceProp& properties() const noexcept {
        return properties_;
    }
    void synchronize() const;
    void drain() const noexcept;

private:
    int previous_device_{};
    int device_{};
    cudaStream_t stream_{};
    cudaDeviceProp properties_{};
};

template <class T>
class CudaBuffer {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    CudaBuffer() = default;
    explicit CudaBuffer(std::size_t count) {
        reserve(count);
    }
    ~CudaBuffer() {
        release();
    }
    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;
    CudaBuffer(CudaBuffer&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)), capacity_(std::exchange(other.capacity_, 0)) {
    }
    CudaBuffer& operator=(CudaBuffer&& other) noexcept {
        if (this != &other) {
            release();
            data_ = std::exchange(other.data_, nullptr);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }

    void reserve(std::size_t count) {
        if (count <= capacity_) {
            return;
        }
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::length_error("CUDA buffer size overflow");
        }
        void* allocation{};
        check_cuda(cudaMalloc(&allocation, count * sizeof(T)), "cudaMalloc");
        release();
        data_ = static_cast<T*>(allocation);
        capacity_ = count;
    }
    T* data() noexcept {
        return data_;
    }
    const T* data() const noexcept {
        return data_;
    }

private:
    void release() noexcept {
        if (data_ != nullptr) {
            cudaFree(data_);
        }
    }

    T* data_{};
    std::size_t capacity_{};
};

class CudaBatch;

class CudaWorkspace {
public:
    explicit CudaWorkspace(int device);
    const CudaContext& context() const noexcept {
        return context_;
    }
    CudaMatch* match() noexcept {
        return match_.data();
    }
    CudaDeviceBatch upload(const CudaBatch& batch);
    unsigned long long complete();

private:
    CudaMatch host_match_;
    CudaContext context_;
    CudaBuffer<Byte> bytes_;
    CudaBuffer<std::uint64_t> offsets_;
    CudaBuffer<CudaMatch> match_;
};

}

#endif
