#include <catch2/catch_test_macros.hpp>
#include <transducer/midi2/ump_builder.h>

TEST_CASE("UmpBuilder packs MIDI 2.0 Note On packet", "[midi2][ump]") {
    using namespace transducer::midi2;

    // Group 0, Channel 1, Note 60 (Middle C), Velocity 0.5 (maps to 0x8000 in top 16 bits)
    auto ump = UmpBuilder::makeNoteOn(0, 1, 60, 0.5f);

    uint32_t w0 = ump[0];
    uint32_t w1 = ump[1];

    // Message type 0x4, group 0x0 -> top nibble 0x4, second nibble 0x0
    REQUIRE(((w0 >> 28) & 0xF) == 0x4);
    REQUIRE(((w0 >> 24) & 0xF) == 0x0); // Group 0
    REQUIRE(((w0 >> 20) & 0xF) == 0x9); // Note On status
    REQUIRE(((w0 >> 16) & 0xF) == 0x1); // Channel 1
    REQUIRE(((w0 >> 8) & 0x7F) == 60);  // Note 60

    // Velocity in high 16 bits of w1
    uint16_t vel16 = static_cast<uint16_t>(w1 >> 16);
    REQUIRE(vel16 >= 0x7FFF);
    REQUIRE(vel16 <= 0x8001);
}

TEST_CASE("UmpBuilder packs MIDI 2.0 Per-Note Pitch Bend packet", "[midi2][ump]") {
    using namespace transducer::midi2;

    // Zero semitone offset -> 0x80000000 center
    auto center_ump = UmpBuilder::makePerNotePitchBend(0, 0, 64, 0.0f, 48.0f);
    REQUIRE(((center_ump[0] >> 20) & 0xF) == 0x6); // Status 0x6 = Per-Note Pitch Bend
    REQUIRE(((center_ump[0] >> 8) & 0x7F) == 64);
    REQUIRE(center_ump[1] == 0x80000000u);

    // Max positive bend (+48 semitones) -> 0xFFFFFFFF
    auto pos_ump = UmpBuilder::makePerNotePitchBend(0, 0, 64, 48.0f, 48.0f);
    REQUIRE(pos_ump[1] == 0xFFFFFFFFu);

    // Max negative bend (-48 semitones) -> 0x00000000
    auto neg_ump = UmpBuilder::makePerNotePitchBend(0, 0, 64, -48.0f, 48.0f);
    REQUIRE(neg_ump[1] == 0x00000000u);
}
