#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cs2esp {

template <typename T>
struct ReadResult {
    T value{};
    bool ok = false;

    constexpr bool success() const noexcept { return ok; }
    constexpr explicit operator bool() const noexcept { return ok; }
    constexpr operator T() const noexcept { return value; }
    constexpr const T& operator*() const noexcept { return value; }
    constexpr const T* operator->() const noexcept { return &value; }
    constexpr T value_or(T def) const noexcept { return ok ? value : def; }
};

// External process memory reader utilizing direct process_vm_readv syscalls
// without maintaining open file descriptors in /proc/self/fd/.
class ProcessMemory {
public:
    ~ProcessMemory();

    ProcessMemory() = default;
    ProcessMemory(const ProcessMemory&) = delete;
    ProcessMemory& operator=(const ProcessMemory&) = delete;

    bool open(const std::string& process_name, std::string& error);
    void close();

    bool attached() const { return pid_.load() > 0; }
    int  pid() const { return pid_.load(); }

    std::uint64_t module_base(const std::string& module_name) const;

    bool read(std::uint64_t address, void* buffer, std::size_t size,
              std::string& error) const;
    bool read_quiet(std::uint64_t address, void* buffer, std::size_t size) const;

    template <typename T>
    bool read(std::uint64_t address, T& out, std::string& error) const {
        return read(address, &out, sizeof(T), error);
    }

    template <typename T>
    std::optional<T> read(std::uint64_t address) const {
        T value{};
        if (!read_quiet(address, &value, sizeof(T))) return std::nullopt;
        return value;
    }

    template <typename T>
    ReadResult<T> read_or(std::uint64_t address, T default_value) const {
        ReadResult<T> res{};
        if (read_quiet(address, &res.value, sizeof(T))) {
            res.ok = true;
        } else {
            res.value = default_value;
            res.ok = false;
        }
        return res;
    }

    bool process_alive() const;

private:
    std::atomic<int> pid_{0};
};

} // namespace cs2esp
