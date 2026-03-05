#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Neuron
{

/// Cross-platform file system utilities.
///
/// Wraps std::filesystem with project-specific conventions
/// (home directory, asset paths).
class FileSystem
{
public:
    /// Set the root directory for asset resolution.
    static void setHomeDirectory(const std::filesystem::path& dir);

    /// Get the currently configured home directory.
    [[nodiscard]] static const std::filesystem::path& homeDirectory();

    /// Read an entire binary file into a byte vector.
    /// Path is resolved relative to homeDirectory() if not absolute.
    [[nodiscard]] static std::optional<std::vector<uint8_t>>
    readBinaryFile(const std::filesystem::path& relativePath);

    /// Read an entire text file into a string.
    [[nodiscard]] static std::optional<std::string>
    readTextFile(const std::filesystem::path& relativePath);

    /// Write bytes to a file (creates or overwrites).
    static bool writeBinaryFile(const std::filesystem::path& relativePath,
                                std::span<const uint8_t> data);

    /// Check whether a file exists (resolved via homeDirectory).
    [[nodiscard]] static bool exists(const std::filesystem::path& relativePath);

private:
    inline static std::filesystem::path m_homeDir;
};

} // namespace Neuron
