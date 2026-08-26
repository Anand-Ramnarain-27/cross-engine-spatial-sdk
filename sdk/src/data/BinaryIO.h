#pragma once

// Private helper shared by TileSerializer.cpp.

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

namespace spatial::data::detail
{
    // Fixed-width little-endian primitive I/O.
    class BinaryWriter
    {
    public:
        explicit BinaryWriter(std::ostream& out) noexcept : m_out(out) {}

        void writeU8(std::uint8_t v) { m_out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void writeU32(std::uint32_t v) { m_out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void writeI32(std::int32_t v) { m_out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void writeF32(float v) { m_out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }

        void writeBytes(const void* data, std::size_t size)
        {
            m_out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        }

        void writeString(const std::string& s)
        {
            writeU32(static_cast<std::uint32_t>(s.size()));
            writeBytes(s.data(), s.size());
        }

        [[nodiscard]] bool ok() const { return m_out.good(); }

    private:
        std::ostream& m_out;
    };

    class BinaryReader
    {
    public:
        // Sanity caps so a corrupted length/count field fails cleanly
        // instead of attempting a multi-gigabyte allocation.
        static constexpr std::uint32_t kMaxStringLength = 1u << 20;       // 1 MiB
        static constexpr std::uint32_t kMaxArrayCount = 20'000'000;

        explicit BinaryReader(std::istream& in) noexcept : m_in(in) {}

        [[nodiscard]] std::uint8_t readU8()
        {
            std::uint8_t v = 0;
            m_in.read(reinterpret_cast<char*>(&v), sizeof(v));
            return v;
        }

        [[nodiscard]] std::uint32_t readU32()
        {
            std::uint32_t v = 0;
            m_in.read(reinterpret_cast<char*>(&v), sizeof(v));
            return v;
        }

        [[nodiscard]] std::int32_t readI32()
        {
            std::int32_t v = 0;
            m_in.read(reinterpret_cast<char*>(&v), sizeof(v));
            return v;
        }

        [[nodiscard]] float readF32()
        {
            float v = 0.0f;
            m_in.read(reinterpret_cast<char*>(&v), sizeof(v));
            return v;
        }

        void readBytes(void* data, std::size_t size)
        {
            m_in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
        }

        [[nodiscard]] std::uint32_t readCount()
        {
            const std::uint32_t count = readU32();
            if (count > kMaxArrayCount)
            {
                m_corrupted = true;
                return 0;
            }
            return count;
        }

        [[nodiscard]] std::string readString()
        {
            const std::uint32_t length = readU32();
            if (length > kMaxStringLength)
            {
                m_corrupted = true;
                return {};
            }
            std::string s(length, '\0');
            if (length > 0)
            {
                readBytes(s.data(), length);
            }
            return s;
        }

        [[nodiscard]] bool ok() const { return m_in.good() && !m_corrupted; }

    private:
        std::istream& m_in;
        bool m_corrupted = false;
    };
}
