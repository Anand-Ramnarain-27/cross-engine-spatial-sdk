#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <unordered_map>

namespace spatial::data
{
    // Free-form key-value metadata attached to a Tile or dataset.
    class Metadata
    {
    public:
        void set(std::string key, std::string value) { m_entries[std::move(key)] = std::move(value); }

        [[nodiscard]] bool contains(const std::string& key) const { return m_entries.contains(key); }

        [[nodiscard]] std::optional<std::string> getString(const std::string& key) const
        {
            const auto it = m_entries.find(key);
            if (it == m_entries.end())
            {
                return std::nullopt;
            }
            return it->second;
        }

        [[nodiscard]] std::optional<double> getNumber(const std::string& key) const
        {
            const auto it = m_entries.find(key);
            if (it == m_entries.end())
            {
                return std::nullopt;
            }
            double result = 0.0;
            const auto begin = it->second.data();
            const auto end = it->second.data() + it->second.size();
            const auto [ptr, ec] = std::from_chars(begin, end, result);
            if (ec != std::errc{} || ptr != end)
            {
                return std::nullopt;
            }
            return result;
        }

        [[nodiscard]] const std::unordered_map<std::string, std::string>& entries() const noexcept { return m_entries; }

        [[nodiscard]] bool operator==(const Metadata&) const = default;

    private:
        std::unordered_map<std::string, std::string> m_entries;
    };
}
