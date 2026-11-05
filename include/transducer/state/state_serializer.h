#pragma once

#include <transducer/params/parameter_manager.h>
#include <clap/clap.h>
#include <cstdint>
#include <cstring>
#include <array>

namespace transducer::state {

#pragma pack(push, 1)
struct StateHeader {
    uint32_t magic{0x47545231}; // 'GTR1'
    uint16_t version{1};
    uint16_t num_params{params::kNumParameters};
    uint32_t checksum{0};
};

struct ParamRecord {
    uint32_t id{0};
    double value{0.0};
};
#pragma pack(pop)

class StateSerializer {
public:
    static constexpr uint32_t kMagic = 0x47545231;
    static constexpr uint16_t kVersion = 1;

    static uint32_t calculate_checksum(const uint8_t* data, size_t size) noexcept {
        uint32_t sum = 0x811c9dc5; // FNV-1a 32-bit offset basis
        for (size_t i = 0; i < size; ++i) {
            sum ^= data[i];
            sum *= 0x01000193; // FNV prime
        }
        return sum;
    }

    static bool serialize_to_buffer(const params::ParameterManager& params,
                                    void* out_buffer,
                                    size_t max_size,
                                    size_t& out_size) noexcept {
        constexpr size_t required_size = sizeof(StateHeader) + params::kNumParameters * sizeof(ParamRecord);
        if (max_size < required_size || !out_buffer) return false;

        StateHeader header;
        header.magic = kMagic;
        header.version = kVersion;
        header.num_params = params::kNumParameters;

        std::array<ParamRecord, params::kNumParameters> records{};
        for (uint32_t i = 0; i < params::kNumParameters; ++i) {
            records[i].id = i;
            records[i].value = params.get_value(i);
        }

        header.checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(records.data()), sizeof(records));

        auto* out_ptr = static_cast<uint8_t*>(out_buffer);
        std::memcpy(out_ptr, &header, sizeof(StateHeader));
        std::memcpy(out_ptr + sizeof(StateHeader), records.data(), sizeof(records));

        out_size = required_size;
        return true;
    }

    static bool deserialize_from_buffer(params::ParameterManager& params,
                                        const void* in_buffer,
                                        size_t size) noexcept {
        constexpr size_t required_size = sizeof(StateHeader) + params::kNumParameters * sizeof(ParamRecord);
        if (size < required_size || !in_buffer) return false;

        StateHeader header{};
        std::memcpy(&header, in_buffer, sizeof(StateHeader));

        if (header.magic != kMagic || header.version != kVersion || header.num_params != params::kNumParameters) {
            return false;
        }

        const auto* records = reinterpret_cast<const ParamRecord*>(static_cast<const uint8_t*>(in_buffer) + sizeof(StateHeader));
        uint32_t computed_checksum = calculate_checksum(reinterpret_cast<const uint8_t*>(records), sizeof(ParamRecord) * params::kNumParameters);
        if (computed_checksum != header.checksum) {
            return false;
        }

        for (uint32_t i = 0; i < params::kNumParameters; ++i) {
            params.set_value(records[i].id, records[i].value);
        }
        return true;
    }

    static bool serialize(const params::ParameterManager& params, const clap_ostream_t* stream) noexcept {
        if (!stream || !stream->write) return false;
        std::array<uint8_t, sizeof(StateHeader) + params::kNumParameters * sizeof(ParamRecord)> buffer{};
        size_t written = 0;
        if (!serialize_to_buffer(params, buffer.data(), buffer.size(), written)) return false;

        int64_t bytes = stream->write(stream, buffer.data(), written);
        return bytes == static_cast<int64_t>(written);
    }

    static bool deserialize(params::ParameterManager& params, const clap_istream_t* stream) noexcept {
        if (!stream || !stream->read) return false;
        constexpr size_t required_size = sizeof(StateHeader) + params::kNumParameters * sizeof(ParamRecord);
        std::array<uint8_t, required_size> buffer{};

        size_t total_read = 0;
        while (total_read < required_size) {
            int64_t bytes = stream->read(stream, buffer.data() + total_read, required_size - total_read);
            if (bytes <= 0) break;
            total_read += static_cast<size_t>(bytes);
        }

        if (total_read < required_size) return false;
        return deserialize_from_buffer(params, buffer.data(), total_read);
    }
};

} // namespace transducer::state
