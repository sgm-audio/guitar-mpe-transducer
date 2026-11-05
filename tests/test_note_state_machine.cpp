#include <catch2/catch_test_macros.hpp>
#include <transducer/dsp/note_state_machine.h>

TEST_CASE("NoteStateMachine distinguishes smooth legato slide from re-plucked note", "[dsp][state_machine]") {
    using namespace transducer::dsp;
    NoteStateMachine nsm;

    REQUIRE_FALSE(nsm.is_active());

    // 1. Initial pluck at C4 (60.1)
    auto action1 = nsm.process(true, 60.1f, 0.8f, true);
    REQUIRE(action1.type == NoteActionType::kNoteOn);
    REQUIRE(action1.key == 60);
    REQUIRE(action1.velocity == 0.8f);
    REQUIRE(nsm.is_active());

    // 2. Smooth legato bend up 1.5 semitones without a new pluck transient
    auto action2 = nsm.process(false, 61.6f, 0.6f, true);
    REQUIRE(action2.type == NoteActionType::kBendModulate);
    REQUIRE(action2.key == 60);
    REQUIRE(action2.semitone_bend > 1.4f);
    REQUIRE(nsm.is_active());

    // 3. Heavy new pick attack at E4 (64.0) with transient
    auto action3 = nsm.process(true, 64.0f, 0.9f, true);
    // Should re-trigger: first note off, then note on
    REQUIRE(action3.type == NoteActionType::kRetrigger);
    REQUIRE(action3.key == 64);
    REQUIRE(action3.off_key == 60);
    REQUIRE(action3.velocity == 0.9f);
    REQUIRE(nsm.is_active());

    // 4. Decay below gate
    auto action4 = nsm.process(false, 64.0f, 0.0005f, false);
    REQUIRE(action4.type == NoteActionType::kNoteOff);
    REQUIRE(action4.key == 64);
    REQUIRE_FALSE(nsm.is_active());

    // 5. Subsequent idle frame returns kNone
    auto action5 = nsm.process(false, 0.0f, 0.0f, false);
    REQUIRE(action5.type == NoteActionType::kNone);
}

TEST_CASE("NoteStateMachine handles reset and pitch boundaries", "[dsp][state_machine]") {
    using namespace transducer::dsp;
    NoteStateMachine nsm;

    auto a1 = nsm.process(true, 130.0f, 0.7f, true);
    REQUIRE(a1.type == NoteActionType::kNoteOn);
    REQUIRE(a1.key == 127); // Clamped

    nsm.reset();
    REQUIRE_FALSE(nsm.is_active());

    auto a2 = nsm.process(false, 0.0f, 0.0f, false);
    REQUIRE(a2.type == NoteActionType::kNone);
}
