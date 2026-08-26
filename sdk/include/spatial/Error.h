#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace spatial
{
    enum class ErrorCode
    {
        DatasetNotFound,
        InvalidDataset,
        UnsupportedVersion,
        TileLoadFailed,
        CorruptTile,
        OutOfMemory,
        GPUUploadFailed,
        InvalidState,
    };

    [[nodiscard]] constexpr std::string_view toString(ErrorCode code) noexcept
    {
        switch (code)
        {
            case ErrorCode::DatasetNotFound: return "DatasetNotFound";
            case ErrorCode::InvalidDataset: return "InvalidDataset";
            case ErrorCode::UnsupportedVersion: return "UnsupportedVersion";
            case ErrorCode::TileLoadFailed: return "TileLoadFailed";
            case ErrorCode::CorruptTile: return "CorruptTile";
            case ErrorCode::OutOfMemory: return "OutOfMemory";
            case ErrorCode::GPUUploadFailed: return "GPUUploadFailed";
            case ErrorCode::InvalidState: return "InvalidState";
        }
        return "Unknown";
    }

    struct Error
    {
        ErrorCode code;
        std::string message;
    };

    // Dependency-free stand-in for std::expected<T, Error> (C++23).
    template <typename T>
    class Expected
    {
    public:
        Expected(T value) : m_storage(std::move(value)) {} // NOLINT(*-explicit-constructor)
        Expected(Error error) : m_storage(std::move(error)) {} // NOLINT(*-explicit-constructor)

        [[nodiscard]] bool hasValue() const noexcept { return std::holds_alternative<T>(m_storage); }
        explicit operator bool() const noexcept { return hasValue(); }

        [[nodiscard]] const T& value() const& { return std::get<T>(m_storage); }
        [[nodiscard]] T& value() & { return std::get<T>(m_storage); }
        [[nodiscard]] T&& value() && { return std::get<T>(std::move(m_storage)); }

        [[nodiscard]] const T& operator*() const& { return value(); }
        [[nodiscard]] T& operator*() & { return value(); }

        [[nodiscard]] const Error& error() const& { return std::get<Error>(m_storage); }

    private:
        std::variant<T, Error> m_storage;
    };

    template <>
    class Expected<void>
    {
    public:
        Expected() = default; // NOLINT(*-explicit-constructor)
        Expected(Error error) : m_error(std::move(error)) {} // NOLINT(*-explicit-constructor)

        [[nodiscard]] bool hasValue() const noexcept { return !m_error.has_value(); }
        explicit operator bool() const noexcept { return hasValue(); }

        [[nodiscard]] const Error& error() const& { return *m_error; }

    private:
        std::optional<Error> m_error;
    };
}
