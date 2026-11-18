#include "plugin.h"
#include <transducer/plugin_state.h>
#include <transducer/clap/event_emitter.h>
#include <transducer/midi2/ump_builder.h>
#include <transducer/state/state_serializer.h>
#include <transducer/common/scoped_no_denormals.h>
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
    info->channel_count = transducer::PluginState::kMaxStrings;
    info->port_type = nullptr; // up to 6 discrete string channels (mono hosts may connect ch 0 only)
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

// Params extension
static uint32_t clap_params_count(const clap_plugin_t* plugin) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    return inst->state.param_manager.get_param_count();
}

static bool clap_params_get_info(const clap_plugin_t* plugin, uint32_t param_index, clap_param_info_t* param_info) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    return inst->state.param_manager.get_info(param_index, param_info);
}

static bool clap_params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* out_value) {
    if (!out_value) return false;
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    *out_value = inst->state.param_manager.get_value(param_id);
    return true;
}

static bool clap_params_value_to_text(
    const clap_plugin_t* plugin, clap_id param_id, double value, char* out_buffer, uint32_t out_buffer_capacity)
{
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    return inst->state.param_manager.value_to_text(param_id, value, out_buffer, out_buffer_capacity);
}

static bool clap_params_text_to_value(
    const clap_plugin_t* plugin, clap_id param_id, const char* param_value_text, double* out_value)
{
    (void)plugin;
    (void)param_id;
    if (!param_value_text || !out_value) return false;
    *out_value = std::atof(param_value_text);
    return true;
}

static void clap_params_flush(
    const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out)
{
    (void)out;
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    if (!in || !in->size) return;
    uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header_t* hdr = in->get(in, i);
        if (hdr && hdr->space_id == CLAP_CORE_EVENT_SPACE_ID && hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            inst->state.param_manager.set_value(ev->param_id, ev->value);
        }
    }
}

static const clap_plugin_params_t s_params_ext = {
    clap_params_count,
    clap_params_get_info,
    clap_params_get_value,
    clap_params_value_to_text,
    clap_params_text_to_value,
    clap_params_flush
};

// State extension
static bool clap_state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    return transducer::state::StateSerializer::serialize(inst->state.param_manager, stream);
}

static bool clap_state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    return transducer::state::StateSerializer::deserialize(inst->state.param_manager, stream);
}

static const clap_plugin_state_t s_state_ext = {
    clap_state_save,
    clap_state_load
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
    transducer::common::ScopedFlushDenormals flush_denormals;

    auto* inst = static_cast<PluginInstance*>(plugin->plugin_data);
    auto& st = inst->state;
    st.thread_check.mark_audio_thread();

    if (!process || process->frames_count == 0) {
        return CLAP_PROCESS_CONTINUE;
    }

    const uint32_t in_ch = (process->audio_inputs_count > 0) ? process->audio_inputs[0].channel_count : 0;
    const uint32_t out_ch = (process->audio_outputs_count > 0) ? process->audio_outputs[0].channel_count : 0;
    const uint32_t num_strings = std::min<uint32_t>(in_ch, transducer::PluginState::kMaxStrings);

    const uint32_t frames = process->frames_count;
    const clap_input_events_t* in_ev = process->in_events;
    const clap_output_events_t* out_ev = process->out_events;

    auto apply_params = [&]() {
        const auto snap = st.param_manager.get_audio_snapshot();
        transducer::dsp::StringPipeline::Config cfg;
        cfg.transient_threshold = snap.transient_sensitivity;
        cfg.gate_threshold = snap.gate_threshold;
        cfg.chord_guard = (num_strings <= 1);
        for (uint32_t c = 0; c < num_strings; ++c) {
            st.strings[c].set_config(cfg);
        }
        st.pitch_bend_range = snap.bend_range;
        st.output_dialect = snap.output_dialect;
        st.voice_allocator.set_routing_mode(static_cast<transducer::voice::RoutingMode>(snap.routing_mode));
    };
    apply_params();

    auto push_checked = [&](bool ok) {
        if (!ok) ++st.dropped_events;
    };

    auto emit_core = [&](const transducer::dsp::CoreEvent& e) {
        using T = transducer::dsp::CoreEvent::Type;
        const int16_t ch = e.string_index;
        const uint8_t u8ch = static_cast<uint8_t>(ch & 0xF);
        const uint8_t u8key = static_cast<uint8_t>(std::clamp<int16_t>(e.key, 0, 127));

        if (st.output_dialect == 1) {
            switch (e.type) {
            case T::kNoteOn:
                push_checked(transducer::clap::EventEmitter::emitMidi2Ump(
                    out_ev, e.offset, 0, transducer::midi2::UmpBuilder::makeNoteOn(0, u8ch, u8key, e.value)));
                break;
            case T::kNoteOff:
                push_checked(transducer::clap::EventEmitter::emitMidi2Ump(
                    out_ev, e.offset, 0, transducer::midi2::UmpBuilder::makeNoteOff(0, u8ch, u8key, e.value)));
                break;
            case T::kBend:
                push_checked(transducer::clap::EventEmitter::emitMidi2Ump(
                    out_ev, e.offset, 0,
                    transducer::midi2::UmpBuilder::makePerNotePitchBend(0, u8ch, u8key, e.value, st.pitch_bend_range)));
                break;
            case T::kPressure:
                push_checked(transducer::clap::EventEmitter::emitMidi2Ump(
                    out_ev, e.offset, 0, transducer::midi2::UmpBuilder::makePolyPressure(0, u8ch, u8key, e.value)));
                break;
            case T::kBrightness:
                break; // no standard per-note UMP brightness without registered controller assignment
            }
            return;
        }

        switch (e.type) {
        case T::kNoteOn:
            push_checked(transducer::clap::EventEmitter::emitNoteOn(out_ev, e.offset, e.note_id, e.key, e.value, ch));
            break;
        case T::kNoteOff:
            push_checked(transducer::clap::EventEmitter::emitNoteOff(out_ev, e.offset, e.note_id, e.key, e.value, ch));
            break;
        case T::kBend:
            push_checked(transducer::clap::EventEmitter::emitTuningExpression(out_ev, e.offset, e.note_id, e.key, e.value, ch));
            break;
        case T::kPressure:
            push_checked(transducer::clap::EventEmitter::emitPressureExpression(out_ev, e.offset, e.note_id, e.key, e.value, ch));
            break;
        case T::kBrightness:
            push_checked(transducer::clap::EventEmitter::emitBrightnessExpression(out_ev, e.offset, e.note_id, e.key, e.value, ch));
            break;
        }
    };

    // Input events are time-ordered; handle at their timestamp so output events stay time-ordered.
    const uint32_t num_events = (in_ev && in_ev->size) ? in_ev->size(in_ev) : 0;
    uint32_t next_event = 0;
    auto handle_event = [&](const clap_event_header_t* hdr) {
        if (!hdr || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
        switch (hdr->type) {
        case CLAP_EVENT_NOTE_ON: {
            const auto* on = reinterpret_cast<const clap_event_note_t*>(hdr);
            st.voice_allocator.onKeyNoteOn(
                on->note_id, on->key, static_cast<float>(on->velocity), on->channel);
            if (out_ev && out_ev->try_push) push_checked(out_ev->try_push(out_ev, hdr));
            break;
        }
        case CLAP_EVENT_NOTE_OFF: {
            const auto* off = reinterpret_cast<const clap_event_note_t*>(hdr);
            st.voice_allocator.onKeyNoteOff(off->note_id, off->key, off->channel);
            if (out_ev && out_ev->try_push) push_checked(out_ev->try_push(out_ev, hdr));
            break;
        }
        case CLAP_EVENT_PARAM_VALUE: {
            const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            st.param_manager.set_value(pv->param_id, pv->value);
            apply_params();
            break;
        }
        default:
            break;
        }
    };

    for (uint32_t s = 0; s < frames; ++s) {
        while (next_event < num_events) {
            const clap_event_header_t* hdr = in_ev->get(in_ev, next_event);
            if (hdr && hdr->time > s) break;
            handle_event(hdr);
            ++next_event;
        }

        for (uint32_t c = 0; c < out_ch; ++c) {
            const float* src = (c < in_ch) ? process->audio_inputs[0].data32[c] : nullptr;
            if (process->audio_outputs[0].data32[c]) {
                process->audio_outputs[0].data32[c][s] = src ? src[s] : 0.0f;
            }
        }

        for (uint32_t c = 0; c < num_strings; ++c) {
            const float* src = process->audio_inputs[0].data32[c];
            st.strings[c].process_sample(src ? src[s] : 0.0f, s, emit_core);
        }
    }

    while (next_event < num_events) {
        handle_event(in_ev->get(in_ev, next_event++));
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
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &s_params_ext;
    }
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
        return &s_state_ext;
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
