#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace cs2esp {

// RAII-обёртка над /proc/<pid>/mem внешнего процесса.
// Запись не предусмотрена — только чтение (open O_RDONLY + pread64,
// фолбэк на process_vm_readv при частичном чтении через границу маппинга).
class ProcessMemory {
public:
    ~ProcessMemory();

    ProcessMemory() = default;
    ProcessMemory(const ProcessMemory&) = delete;
    ProcessMemory& operator=(const ProcessMemory&) = delete;

    // Найти процесс по имени (по /proc/*/comm или командной строке)
    // и открыть /proc/<pid>/mem.
    bool open(const std::string& process_name, std::string& error);
    void close();

    bool attached() const { return pid_ > 0; }
    int  pid() const { return pid_; }

    // База модуля по /proc/<pid>/maps (первая маппировка файла с offset 0).
    // 0, если модуль еще не загружен.
    std::uint64_t module_base(const std::string& module_name) const;

    // Чтение size байт. Ировер fail с текстом ошибки в error; readonly-чтение
    // без логов — read_quiet (для циклов, чтобы не спамить).
    bool read(std::uint64_t address, void* buffer, std::size_t size,
              std::string& error) const;
    bool read_quiet(std::uint64_t address, void* buffer, std::size_t size) const;

    template <typename T>
    bool read(std::uint64_t address, T& out, std::string& error) const {
        return read(address, &out, sizeof(T), error);
    }

    template <typename T>
    T read_or(std::uint64_t address, T default_value) const {
        T value{};
        if (!read_quiet(address, &value, sizeof(T))) return default_value;
        return value;
    }

    // жив ли процесс (по kill(pid, 0) + чтению /proc/<pid>/maps)
    bool process_alive() const;

private:
    int pid_ = 0;
    int fd_ = -1;
};

} // namespace cs2esp
