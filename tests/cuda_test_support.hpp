#ifndef PWBENCH_TESTS_CUDA_TEST_SUPPORT_HPP
#define PWBENCH_TESTS_CUDA_TEST_SUPPORT_HPP

#include <cuda_runtime_api.h>
#include <gtest/gtest.h>

namespace fixtures {

class CudaTest : public ::testing::Test {
protected:
    void SetUp() override {
        int count{};
        const auto error = cudaGetDeviceCount(&count);
        if (error == cudaErrorNoDevice || error == cudaErrorInsufficientDriver ||
            (error == cudaSuccess && count == 0)) {
            GTEST_SKIP() << "CUDA device is unavailable";
        }
        ASSERT_EQ(error, cudaSuccess) << cudaGetErrorString(error);
        ASSERT_EQ(cudaSetDevice(0), cudaSuccess);
    }
};

}

#endif
