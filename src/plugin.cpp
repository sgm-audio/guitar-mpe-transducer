#include "plugin.h"
#include <transducer/plugin_state.h>
#include <transducer/clap/event_emitter.h>
#include <transducer/midi2/ump_builder.h>
#include <cstring>
#include <cmath>
#include <algorithm>

static const char* s_features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_UTILITY,
    nullptr
};

const clap_plugin_descriptor_t s_guitar_transducer_descriptor = {
    CLAP_VERSION,
    "com.sgm-audio.guitar-mpe-transducer",
    "Guitar MPE/CLAP Transducer",
    "SGM Audio",
    "https://github.com/sgm-audio/guitar-mpe-transducer",
    "",
    "",
    "0.1.0",
    "Real-time guitar to CLAP note expression and MIDI 2.0 UMP transducer",
    s_features
};

struct PluginInstance {
    clap_plugin_t plugin;
    const clap_host_t* host{nullptr};
    transducer::PluginState state{};
};

// Audio ports extension
static uint32_t clap_audio_ports_count(const clap_plugin_t* plugin, bool is_input) {
    (void)plugin;
    (void)is_input;
    return 1;
}

static bool clap_audio_ports_get(
    const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_audio_port_info_t* info)
{
    (void)plugin;
    if (index != 0 || !info) return false;

    info->id = 0;
    if (is_input) {
        std::strncpy(info->name, "Guitar In", sizeof(info->name) - 1);
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    } else {
        std::strncpy(info->name, "Guitar Thru", sizeof(info->name) - 1);
        info->flags = 0;
    }
    info->channel_count = 1;
    info->port_type = CLAP_PORT_MONO;
    info->in_place_pair = 0;
    return true;
}

static const clap_plugin_audio_ports_t s_audio_ports_ext = {
    clap_audio_ports_count,
    clap_audio_ports_get
};

// Note ports extension
static uint32_t clap_note_ports_count(const clap_plugin_t* plugin, bool is_input) {
    (void)plugin;
    (void)is_input;
    return 1;
}

static bool clap_note_ports_get(
    const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_note_port_info_t* info)
{
    (void)plugin;
    if (index != 0 || !info) return false;

    info->id = 0;
    if (is_input) {
        std::strncpy(info->name, "Keyboard In", sizeof(info->name) - 1);
    } else {
        std::strncpy(info->name, "Transduced Notes Out", sizeof(info->name) - 1);
    }
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI2;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    return true;
}

static const clap_plugin_note_ports_t s_note_ports_ext = {
    clap_note_ports_count,
    clap_note_ports_get
};

// Plugin lifecycle
static bool plugin_init(const clap_plugin_t* plugin) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    inst->state.thread_check.set_host(inst->host);
    return true;
}

static void plugin_destroy(const clap_plugin_t* plugin) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    delete inst;
}

static bool plugin_activate(
    const clap_plugin_t* plugin, double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count)
{
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    inst->state.sample_rate = sample_rate;
    inst->state.min_frames = min_frames_count;
    inst->state.max_frames = max_frames_count;
    inst->state.reset();
    return true;
}

static void plugin_deactivate(const clap_plugin_t* plugin) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    inst->state.reset();
}

static bool plugin_start_processing(const clap_plugin_t* plugin) {
    (void)plugin;
    return true;
}

static void plugin_stop_processing(const clap_plugin_t* plugin) {
    (void)plugin;
}

static void plugin_reset(const clap_plugin_t* plugin) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    inst->state.reset();
}

static clap_process_status plugin_process(const clap_plugin_t* plugin, const clap_process_t* process) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    inst->state.thread_check.mark_audio_thread();

    if (!process || process->frames_count == 0) {
        return CLAP_PROCESS_CONTINUE;
    }

    const float* in_audio = nullptr;
    if (process->audio_inputs_count > 0 && process->audio_inputs[0].channel_count > 0) {
        in_audio = process->audio_inputs[0].data32[0];
    }

    float* out_audio = nullptr;
    if (process->audio_outputs_count > 0 && process->audio_outputs[0].channel_count > 0) {
        out_audio = process->audio_outputs[0].data32[0];
    }

    const uint32_t frames = process->frames_count;
    const clap_output_events_t* out_ev = process->out_events;

    for (uint32_t s = 0; s < frames; ++s) {
        float sample = in_audio ? in_audio[s] : 0.0f;
        if (out_audio) {
            out_audio[s] = sample; // Pass-thru guitar monitor
        }

        // 1. Run transient detector
        auto trans_res = inst->state.transient_detector.process(sample);

        // 2. Run YIN pitch tracker
        auto pitch_opt = inst->state.pitch_tracker.processSample(
            sample, static_cast<float>(inst->state.sample_rate));

        if (pitch_opt.has_value()) {
            const auto& pitch = *pitch_opt;
            float continuous_midi = inst->state.pitch_continuity.process(
                pitch.midi_note, pitch.confidence, pitch.is_valid);

            // Note Off / String decay check
            float rel_vel = inst->state.release_velocity.process(trans_res.envelope);
            if (trans_res.envelope < 0.002f && inst->state.active_note_id != -1) {
                transducer::clap::EventEmitter::emitNoteOff(
                    out_ev, s, inst->state.active_note_id, inst->state.active_key, rel_vel);

                auto ump_off = transducer::midi2::UmpBuilder::makeNoteOff(
                    0, 0, static_cast<uint8_t>(inst->state.active_key), rel_vel);
                transducer::clap::EventEmitter::emitMidi2Ump(out_ev, s, 0, ump_off);

                inst->state.active_note_id = -1;
                inst->state.active_key = -1;
            }

            // Note On Pluck Trigger
            if (trans_res.onset_detected && pitch.is_valid) {
                if (inst->state.active_note_id != -1) {
                    transducer::clap::EventEmitter::emitNoteOff(
                        out_ev, s, inst->state.active_note_id, inst->state.active_key);
                }

                inst->state.active_key = static_cast<int16_t>(std::round(continuous_midi));
                inst->state.active_note_id = ++inst->state.next_note_id;

                transducer::clap::EventEmitter::emitNoteOn(
                    out_ev, s, inst->state.active_note_id, inst->state.active_key, trans_res.velocity);

                auto ump_on = transducer::midi2::UmpBuilder::makeNoteOn(
                    0, 0, static_cast<uint8_t>(inst->state.active_key), trans_res.velocity);
                transducer::clap::EventEmitter::emitMidi2Ump(out_ev, s, 0, ump_on);
            }

            // Continuous Tuning & Pressure Modulation
            if (inst->state.active_note_id != -1 && pitch.is_valid) {
                float semitone_bend = continuous_midi - static_cast<float>(inst->state.active_key);

                // CLAP Note Expression Tuning (microtonal bend)
                transducer::clap::EventEmitter::emitTuningExpression(
                    out_ev, s, inst->state.active_note_id, inst->state.active_key, semitone_bend);

                // CLAP Note Expression Pressure (string energy)
                double norm_press = std::clamp(static_cast<double>(trans_res.envelope) * 6.0, 0.0, 1.0);
                transducer::clap::EventEmitter::emitPressureExpression(
                    out_ev, s, inst->state.active_note_id, inst->state.active_key, norm_press);

                // MIDI 2.0 UMP Type 4 Per-Note Pitch Bend
                auto ump_bend = transducer::midi2::UmpBuilder::makePerNotePitchBend(
                    0, 0, static_cast<uint8_t>(inst->state.active_key), semitone_bend, inst->state.pitch_bend_range);
                transducer::clap::EventEmitter::emitMidi2Ump(out_ev, s, 0, ump_bend);
            }
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* plugin, const char* id) {
    (void)plugin;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &s_audio_ports_ext;
    }
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
        return &s_note_ports_ext;
    }
    return nullptr;
}

static void plugin_on_main_thread(const clap_plugin_t* plugin) {
    (void)plugin;
}

const clap_plugin_t* create_guitar_transducer_plugin(const clap_host_t* host) {
    auto* inst = new PluginInstance();
    inst->host = host;
    inst->plugin.desc = &s_guitar_transducer_descriptor;
    inst->plugin.plugin_data = inst;
    inst->plugin.init = plugin_init;
    inst->plugin.destroy = plugin_destroy;
    inst->plugin.activate = plugin_activate;
    inst->plugin.deactivate = plugin_deactivate;
    inst->plugin.start_processing = plugin_start_processing;
    inst->plugin.stop_processing = plugin_stop_processing;
    inst->plugin.reset = plugin_reset;
    inst->plugin.process = plugin_process;
    inst->plugin.get_extension = plugin_get_extension;
    inst->plugin.on_main_thread = plugin_on_main_thread;
    return &inst->plugin;
}
