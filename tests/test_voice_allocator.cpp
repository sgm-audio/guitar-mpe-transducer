#include <catch2/catch_test_macros.hpp>
#include <transducer/voice/voice_allocator.h>

TEST_CASE("VoiceAllocator tracks keyboard voices and selects targets", "[voice][allocator]") {
    using namespace transducer::voice;
    VoiceAllocator alloc;

    REQUIRE(alloc.active_voice_count() == 0);

    // Keyboard plays C Major triad: C4 (60), E4 (64), G4 (67)
    alloc.onKeyNoteOn(10, 60, 0.8f);
    alloc.onKeyNoteOn(11, 64, 0.7f);
    alloc.onKeyNoteOn(12, 67, 0.9f);

    REQUIRE(alloc.active_voice_count() == 3);

    // Target Highest mode
    alloc.set_routing_mode(RoutingMode::kTargetHighest);
    auto target_high = alloc.get_modulation_targets();
    REQUIRE(target_high.size() == 1);
    REQUIRE(target_high[0].key == 67); // G4 is highest
    REQUIRE(target_high[0].note_id == 12);

    // Target All mode
    alloc.set_routing_mode(RoutingMode::kTargetAll);
    auto target_all = alloc.get_modulation_targets();
    REQUIRE(target_all.size() == 3);

    // Release G4
    alloc.onKeyNoteOff(12, 67);
    REQUIRE(alloc.active_voice_count() == 2);

    // Target Highest should now be E4 (64)
    alloc.set_routing_mode(RoutingMode::kTargetHighest);
    auto next_high = alloc.get_modulation_targets();
    REQUIRE(next_high.size() == 1);
    REQUIRE(next_high[0].key == 64);
}
