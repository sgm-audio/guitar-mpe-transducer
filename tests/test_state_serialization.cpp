#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <transducer/state/state_serializer.h>
#include <transducer/params/parameter_manager.h>
#include <vector>
#include <cstring>

TEST_CASE("StateSerializer saves and restores parameter state faithfully", "[state][serialization]") {
    using namespace transducer::state;
    using namespace transducer::params;

    ParameterManager source_params;
    source_params.set_value(kParamBendRange, 24.0);
    source_params.set_value(kParamTransientSensitivity, 0.035);
    source_params.set_value(kParamGateThreshold, 0.008);
    source_params.set_value(kParamRoutingMode, 2.0); // kTargetAll

    std::vector<uint8_t> buffer(512, 0);
    size_t written_size = 0;

    bool saved = StateSerializer::serialize_to_buffer(source_params, buffer.data(), buffer.size(), written_size);
    REQUIRE(saved);
    REQUIRE(written_size > 0);

    ParameterManager dest_params;
    // Ensure destination defaults are different before restore
    REQUIRE(dest_params.get_value(kParamBendRange) != Catch::Approx(24.0));

    bool loaded = StateSerializer::deserialize_from_buffer(dest_params, buffer.data(), written_size);
    REQUIRE(loaded);

    REQUIRE(dest_params.get_value(kParamBendRange) == Catch::Approx(24.0));
    REQUIRE(dest_params.get_value(kParamTransientSensitivity) == Catch::Approx(0.035));
    REQUIRE(dest_params.get_value(kParamGateThreshold) == Catch::Approx(0.008));
    REQUIRE(dest_params.get_value(kParamRoutingMode) == Catch::Approx(2.0));
}

TEST_CASE("StateSerializer rejects corrupt or truncated data", "[state][serialization]") {
    using namespace transducer::state;
    using namespace transducer::params;

    ParameterManager params;
    std::vector<uint8_t> buffer(512, 0);
    size_t written_size = 0;
    REQUIRE(StateSerializer::serialize_to_buffer(params, buffer.data(), buffer.size(), written_size));

    // Corrupt magic header
    buffer[0] ^= 0xFF;
    ParameterManager dest;
    REQUIRE_FALSE(StateSerializer::deserialize_from_buffer(dest, buffer.data(), written_size));

    // Truncated buffer
    REQUIRE_FALSE(StateSerializer::deserialize_from_buffer(dest, buffer.data(), 4));
}

TEST_CASE("StateSerializer works through CLAP stream interface", "[state][clap]") {
    using namespace transducer::state;
    using namespace transducer::params;

    ParameterManager source;
    source.set_value(kParamBendRange, 12.0);

    std::vector<uint8_t> stream_data;
    clap_ostream_t ostream{};
    ostream.ctx = &stream_data;
    ostream.write = [](const clap_ostream_t* s, const void* buffer, uint64_t size) -> int64_t {
        auto* vec = static_cast<std::vector<uint8_t>*>(s->ctx);
        const auto* ptr = static_cast<const uint8_t*>(buffer);
        vec->insert(vec->end(), ptr, ptr + size);
        return static_cast<int64_t>(size);
    };

    REQUIRE(StateSerializer::serialize(source, &ostream));

    struct StreamContext {
        const uint8_t* data;
        size_t size;
        size_t read_offset;
    };
    StreamContext in_ctx{stream_data.data(), stream_data.size(), 0};

    clap_istream_t istream{};
    istream.ctx = &in_ctx;
    istream.read = [](const clap_istream_t* s, void* buffer, uint64_t size) -> int64_t {
        auto* ctx = static_cast<StreamContext*>(s->ctx);
        size_t remaining = ctx->size - ctx->read_offset;
        size_t to_read = std::min(static_cast<size_t>(size), remaining);
        if (to_read == 0) return 0;
        std::memcpy(buffer, ctx->data + ctx->read_offset, to_read);
        ctx->read_offset += to_read;
        return static_cast<int64_t>(to_read);
    };

    ParameterManager dest;
    REQUIRE(StateSerializer::deserialize(dest, &istream));
    REQUIRE(dest.get_value(kParamBendRange) == Catch::Approx(12.0));
}

#include <transducer/presets/factory_presets.h>
#include <set>
#include <string>

TEST_CASE("Factory presets round-trip through StateSerializer within parameter bounds", "[state][presets]") {
    using namespace transducer::state;
    using namespace transducer::params;
    using namespace transducer::presets;

    REQUIRE(kNumFactoryPresets == 25);

    std::set<std::string> seen_names;
    std::set<std::string> seen_filenames;

    for (const auto& preset : kFactoryPresets) {
        REQUIRE(seen_names.insert(preset.name).second);
        REQUIRE(seen_filenames.insert(preset.filename).second);

        ParameterManager src;
        src.set_value(kParamBendRange, preset.bend_range);
        src.set_value(kParamTransientSensitivity, preset.transient_sensitivity);
        src.set_value(kParamGateThreshold, preset.gate_threshold);
        src.set_value(kParamRoutingMode, preset.routing_mode);
        src.set_value(kParamOutputDialect, preset.output_dialect);

        std::array<uint8_t, 512> buf{};
        size_t written = 0;
        REQUIRE(StateSerializer::serialize_to_buffer(src, buf.data(), buf.size(), written));

        ParameterManager dst;
        REQUIRE(StateSerializer::deserialize_from_buffer(dst, buf.data(), written));
        REQUIRE(dst.get_value(kParamBendRange) == Catch::Approx(preset.bend_range));
        REQUIRE(dst.get_value(kParamTransientSensitivity) == Catch::Approx(preset.transient_sensitivity));
        REQUIRE(dst.get_value(kParamGateThreshold) == Catch::Approx(preset.gate_threshold));
        REQUIRE(dst.get_value(kParamRoutingMode) == Catch::Approx(static_cast<double>(preset.routing_mode)));
        REQUIRE(dst.get_value(kParamOutputDialect) == Catch::Approx(static_cast<double>(preset.output_dialect)));
    }
}
