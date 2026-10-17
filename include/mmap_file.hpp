#pragma once

#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace itch {

// RAII wrapper for POSIX read-only memory-mapped files
class MemoryMappedFile {
private:
    const uint8_t* mapped_data_{nullptr};
    size_t file_size_{0};
    int fd_{-1};

public:
    explicit MemoryMappedFile(const char* filepath) {
        fd_ = ::open(filepath, O_RDONLY);
        if (fd_ == -1) {
            throw std::runtime_error(std::string("Failed to open file: ") + filepath);
        }

        struct stat sb{};
        if (::fstat(fd_, &sb) == -1) {
            ::close(fd_);
            throw std::runtime_error("Failed to query file size via fstat.");
        }
        file_size_ = static_cast<size_t>(sb.st_size);

        if (file_size_ == 0) {
            ::close(fd_);
            throw std::runtime_error("File is empty.");
        }

        // Map file into virtual address space (zero-copy, read-only)
        void* ptr = ::mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd_);
            throw std::runtime_error("mmap() failed.");
        }

        // Hint kernel to read ahead sequentially for maximum throughput
        ::madvise(ptr, file_size_, MADV_SEQUENTIAL);

        mapped_data_ = static_cast<const uint8_t*>(ptr);
    }

    ~MemoryMappedFile() noexcept {
        if (mapped_data_ != nullptr && mapped_data_ != MAP_FAILED) {
            ::munmap(const_cast<uint8_t*>(mapped_data_), file_size_);
        }
        if (fd_ != -1) {
            ::close(fd_);
        }
    }

    // Disable copy semantics to prevent double-unmapping
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

    [[nodiscard]] const uint8_t* data() const noexcept { return mapped_data_; }
    [[nodiscard]] size_t size() const noexcept { return file_size_; }
};

} // namespace itch