#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/clap/event_emitter.h>
#include <vector>

// Mock CLAP event list collector
struct MockClapOutputEvents {
    clap_output_events_t list{};
    std::vector<std::vector<uint8_t>> stored_events;

    MockClapOutputEvents() {
        list.ctx = this;
        list.try_push = [](const clap_output_events_t* out, const clap_event_header_t* hdr) -> bool {
            auto* self = static_cast<MockClapOutputEvents*>(out->ctx);
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(hdr);
            self->stored_events.emplace_back(ptr, ptr + hdr->size);
            return true;
        };
    }
};

TEST_CASE("EventEmitter emits Note On and Note Off", "[clap][events]") {
    using namespace transducer::clap;
    MockClapOutputEvents mock;

    EventEmitter::emitNoteOn(&mock.list, 0, 101, 64, 0.85);
    REQUIRE(mock.stored_events.size() == 1);

    auto* on = reinterpret_cast<const clap_event_note_t*>(mock.stored_events[0].data());
    REQUIRE(on->header.type == CLAP_EVENT_NOTE_ON);
    REQUIRE(on->note_id == 101);
    REQUIRE(on->key == 64);
    REQUIRE_THAT(on->velocity, Catch::Matchers::WithinAbs(0.85, 1e-4));

    EventEmitter::emitNoteOff(&mock.list, 100, 101, 64);
    REQUIRE(mock.stored_events.size() == 2);

    auto* off = reinterpret_cast<const clap_event_note_t*>(mock.stored_events[1].data());
    REQUIRE(off->header.type == CLAP_EVENT_NOTE_OFF);
    REQUIRE(off->header.time == 100);
    REQUIRE(off->note_id == 101);
    REQUIRE(off->key == 64);
}

TEST_CASE("EventEmitter emits Note Expression Tuning and Pressure", "[clap][events]") {
    using namespace transducer::clap;
    MockClapOutputEvents mock;

    EventEmitter::emitTuningExpression(&mock.list, 32, 101, 64, 1.25);
    REQUIRE(mock.stored_events.size() == 1);
    auto* tuning = reinterpret_cast<const clap_event_note_expression_t*>(mock.stored_events[0].data());
    REQUIRE(tuning->header.type == CLAP_EVENT_NOTE_EXPRESSION);
    REQUIRE(tuning->expression_id == CLAP_NOTE_EXPRESSION_TUNING);
    REQUIRE(tuning->note_id == 101);
    REQUIRE_THAT(tuning->value, Catch::Matchers::WithinAbs(1.25, 1e-4));

    EventEmitter::emitPressureExpression(&mock.list, 32, 101, 64, 0.72);
    REQUIRE(mock.stored_events.size() == 2);
    auto* press = reinterpret_cast<const clap_event_note_expression_t*>(mock.stored_events[1].data());
    REQUIRE(press->expression_id == CLAP_NOTE_EXPRESSION_PRESSURE);
    REQUIRE_THAT(press->value, Catch::Matchers::WithinAbs(0.72, 1e-4));
}
