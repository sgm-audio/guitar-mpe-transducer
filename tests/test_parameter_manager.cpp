#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/params/parameter_manager.h>

TEST_CASE("ParameterManager handles lock-free reads and parameter value updates", "[params]") {
    using namespace transducer::params;
    ParameterManager pm;

    REQUIRE(pm.get_param_count() >= 4);

    // Initial default bend range is 48.0 semitones
    REQUIRE_THAT(pm.get_value(kParamBendRange), Catch::Matchers::WithinAbs(48.0, 1e-4));

    // Update bend range from UI / automation to 24.0
    pm.set_value(kParamBendRange, 24.0);
    REQUIRE_THAT(pm.get_value(kParamBendRange), Catch::Matchers::WithinAbs(24.0, 1e-4));

    // Audio thread reads snapshot lock-free
    auto snapshot = pm.get_audio_snapshot();
    REQUIRE_THAT(snapshot.bend_range, Catch::Matchers::WithinAbs(24.0f, 1e-4f));

    // Test text conversion
    char text[64]{};
    REQUIRE(pm.value_to_text(kParamBendRange, 48.0, text, sizeof(text)));
    REQUIRE(std::string(text) == "48.0 st");
}
