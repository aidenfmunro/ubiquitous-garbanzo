#ifndef PWBENCH_TESTS_TEMP_DIRECTORY_HPP
#define PWBENCH_TESTS_TEMP_DIRECTORY_HPP

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

class TempDirectory {
public:
    TempDirectory() {
        auto pattern = (std::filesystem::temp_directory_path() / "pwbench-tests-XXXXXX").string();
        const auto directory = ::mkdtemp(pattern.data());
        if (directory == nullptr) {
            throw std::system_error(errno, std::generic_category(), "mkdtemp");
        }
        path_ = directory;
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    const std::filesystem::path& path() const {
        return path_;
    }

    std::filesystem::path write(std::string_view name, std::string_view data) const {
        const auto destination = path_ / name;
        std::ofstream output(destination, std::ios::binary);
        if (!data.empty()) {
            output.write(data.data(), static_cast<std::streamsize>(data.size()));
        }
        output.close();
        if (!output) {
            throw std::runtime_error("cannot write test file: " + destination.string());
        }
        return destination;
    }

private:
    std::filesystem::path path_;
};

#endif
