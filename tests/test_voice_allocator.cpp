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

TEST_CASE("VoiceAllocator honours CLAP wildcards and releases every match", "[voice][allocator]") {
    using namespace transducer::voice;
    VoiceAllocator a;
    a.onKeyNoteOn(-1, 60, 0.5f, 0); // host without note ids
    a.onKeyNoteOn(-1, 64, 0.5f, 0);
    a.onKeyNoteOn(5, 67, 0.5f, 1);
    REQUIRE(a.active_voice_count() == 3);

    a.onKeyNoteOff(-1, 60, 0);          // id wildcard, exact key
    REQUIRE(a.active_voice_count() == 2);
    a.onKeyNoteOff(99, 64, 0);          // unknown id but voice has no id -> key match
    REQUIRE(a.active_voice_count() == 1);
    a.onKeyNoteOff(6, 67, 1);           // wrong id: must NOT release voice 5
    REQUIRE(a.active_voice_count() == 1);
    a.onKeyNoteOff(-1, -1, -1);         // full wildcard
    REQUIRE(a.active_voice_count() == 0);
}

TEST_CASE("VoiceAllocator steals the oldest voice when full and never allocates", "[voice][allocator]") {
    using namespace transducer::voice;
    VoiceAllocator a;
    a.set_routing_mode(RoutingMode::kTargetAll);
    for (int i = 0; i < 16; ++i) a.onKeyNoteOn(i, static_cast<int16_t>(40 + i), 0.5f);
    REQUIRE(a.active_voice_count() == 16);
    a.onKeyNoteOn(100, 90, 0.5f); // steals key 40
    REQUIRE(a.active_voice_count() == 16);
    auto t = a.get_modulation_targets();
    REQUIRE(t.size() == 16);
    bool has90 = false, has40 = false;
    for (const auto& v : t) { has90 |= v.key == 90; has40 |= v.key == 40; }
    REQUIRE(has90);
    REQUIRE_FALSE(has40);
}
