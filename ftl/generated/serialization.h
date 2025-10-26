#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace ftl {
namespace serialization {

/**
 * @brief Helper class for parsing fields from a byte buffer
 */
class Parser {
public:
    Parser(const uint8_t* data, uint8_t length)
        : data_(data), length_(length), offset_(0) {}

    template<typename T>
    T read(size_t offset) const {
        T value;
        std::memcpy(&value, data_ + offset, sizeof(T));
        return value;
    }

    template<typename T, size_t N>
    std::span<const T> read_array(size_t offset) const {
        return std::span<const T>(
            reinterpret_cast<const T*>(data_ + offset), N);
    }

    std::string_view read_string(size_t& dynamic_offset) const {
        uint8_t len = data_[dynamic_offset++];
        std::string_view str(
            reinterpret_cast<const char*>(data_ + dynamic_offset), len);
        dynamic_offset += len;
        return str;
    }

private:
    const uint8_t* data_;
    uint8_t length_;
    size_t offset_;
};

/**
 * @brief Helper class for serializing fields into a byte buffer
 */
class Serializer {
public:
    Serializer(uint8_t* data, uint8_t max_length)
        : data_(data), max_length_(max_length), offset_(0) {}

    template<typename T>
    bool write(size_t offset, T value) {
        if (offset + sizeof(T) > max_length_) return false;
        std::memcpy(data_ + offset, &value, sizeof(T));
        return true;
    }

    template<typename T, size_t N>
    bool write_array(size_t offset, std::span<const T, N> value) {
        if (offset + sizeof(T) * N > max_length_) return false;
        std::memcpy(data_ + offset, value.data(), sizeof(T) * N);
        return true;
    }

    bool write_string(size_t& dynamic_offset, std::string_view value) {
        if (dynamic_offset + 1 + value.size() > max_length_) return false;
        data_[dynamic_offset++] = static_cast<uint8_t>(value.size());
        std::memcpy(data_ + dynamic_offset, value.data(), value.size());
        dynamic_offset += value.size();
        return true;
    }

    size_t dynamic_offset() const { return offset_; }
    void set_dynamic_offset(size_t offset) { offset_ = offset; }

private:
    uint8_t* data_;
    uint8_t max_length_;
    size_t offset_;
};

} // namespace serialization
} // namespace ftl