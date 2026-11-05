#pragma once

#include <clap/clap.h>
#include <array>
#include <cstdint>

namespace transducer::clap {

class EventEmitter {
public:
    static inline bool emitNoteOn(
        const clap_output_events_t* out,
        uint32_t sample_offset,
        int32_t note_id,
        int16_t key,
        double velocity,
        int16_t channel = 0,
        uint16_t port_index = 0)
    {
        if (!out || !out->try_push) return false;

        clap_event_note_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.time = sample_offset;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = CLAP_EVENT_NOTE_ON;
        ev.header.flags = 0;
        ev.note_id = note_id;
        ev.port_index = port_index;
        ev.channel = channel;
        ev.key = key;
        ev.velocity = velocity;

        return out->try_push(out, &ev.header);
    }

    static inline bool emitNoteOff(
        const clap_output_events_t* out,
        uint32_t sample_offset,
        int32_t note_id,
        int16_t key,
        int16_t channel = 0,
        uint16_t port_index = 0)
    {
        if (!out || !out->try_push) return false;

        clap_event_note_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.time = sample_offset;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = CLAP_EVENT_NOTE_OFF;
        ev.header.flags = 0;
        ev.note_id = note_id;
        ev.port_index = port_index;
        ev.channel = channel;
        ev.key = key;
        ev.velocity = 0.0;

        return out->try_push(out, &ev.header);
    }

    static inline bool emitTuningExpression(
        const clap_output_events_t* out,
        uint32_t sample_offset,
        int32_t note_id,
        int16_t key,
        double semitone_offset,
        int16_t channel = 0,
        uint16_t port_index = 0)
    {
        if (!out || !out->try_push) return false;

        clap_event_note_expression_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.time = sample_offset;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = CLAP_EVENT_NOTE_EXPRESSION;
        ev.header.flags = 0;
        ev.expression_id = CLAP_NOTE_EXPRESSION_TUNING;
        ev.note_id = note_id;
        ev.port_index = port_index;
        ev.channel = channel;
        ev.key = key;
        ev.value = semitone_offset;

        return out->try_push(out, &ev.header);
    }

    static inline bool emitPressureExpression(
        const clap_output_events_t* out,
        uint32_t sample_offset,
        int32_t note_id,
        int16_t key,
        double pressure,
        int16_t channel = 0,
        uint16_t port_index = 0)
    {
        if (!out || !out->try_push) return false;

        clap_event_note_expression_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.time = sample_offset;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = CLAP_EVENT_NOTE_EXPRESSION;
        ev.header.flags = 0;
        ev.expression_id = CLAP_NOTE_EXPRESSION_PRESSURE;
        ev.note_id = note_id;
        ev.port_index = port_index;
        ev.channel = channel;
        ev.key = key;
        ev.value = pressure;

        return out->try_push(out, &ev.header);
    }

    static inline bool emitMidi2Ump(
        const clap_output_events_t* out,
        uint32_t sample_offset,
        uint16_t port_index,
        const std::array<uint32_t, 2>& words)
    {
        if (!out || !out->try_push) return false;

        clap_event_midi2_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.time = sample_offset;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = CLAP_EVENT_MIDI2;
        ev.header.flags = 0;
        ev.port_index = port_index;
        ev.data[0] = words[0];
        ev.data[1] = words[1];

        return out->try_push(out, &ev.header);
    }
};

} // namespace transducer::clap
