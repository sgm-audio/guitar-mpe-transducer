#include <catch2/catch_test_macros.hpp>
#include <clap/clap.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

extern const clap_plugin_entry_t clap_entry;

namespace {
constexpr double kSr = 48000.0;
constexpr float kTwoPi = 6.28318530718f;

struct Captured {
    uint64_t abs_sample;
    uint16_t type;
    int16_t channel;
    int32_t note_id;
    int16_t key;
    double velocity;
};

union AnyEvent {
    clap_event_header_t h;
    clap_event_note_t n;
    clap_event_param_value_t p;
};

struct HostEvent { uint64_t abs_sample; AnyEvent ev; };

struct InCtx {
    std::vector<AnyEvent> block;
};

struct OutCtx {
    std::vector<Captured>* sink;
    uint64_t block_start;
};

uint32_t in_size(const clap_input_events_t* l) { return static_cast<uint32_t>(static_cast<InCtx*>(l->ctx)->block.size()); }
const clap_event_header_t* in_get(const clap_input_events_t* l, uint32_t i) {
    return &static_cast<InCtx*>(l->ctx)->block[i].h;
}

HostEvent key_on(uint64_t t, int16_t key) {
    HostEvent e{t, {}};
    e.ev.n = {};
    e.ev.n.header = {sizeof(clap_event_note_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0};
    e.ev.n.note_id = 7; e.ev.n.key = key; e.ev.n.velocity = 0.8;
    return e;
}

HostEvent param(uint64_t t, clap_id id, double v) {
    HostEvent e{t, {}};
    e.ev.p = {};
    e.ev.p.header = {sizeof(clap_event_param_value_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    e.ev.p.param_id = id; e.ev.p.value = v;
    return e;
}

bool out_push(const clap_output_events_t* l, const clap_event_header_t* h) {
    auto* c = static_cast<OutCtx*>(l->ctx);
    int16_t ch = 0, key = -1;
    int32_t id = -1;
    double vel = 0;
    if (h->type == CLAP_EVENT_NOTE_ON || h->type == CLAP_EVENT_NOTE_OFF) {
        auto* n = reinterpret_cast<const clap_event_note_t*>(h);
        ch = n->channel; id = n->note_id; key = n->key; vel = n->velocity;
    }
    c->sink->push_back({c->block_start + h->time, h->type, ch, id, key, vel});
    return true;
}

void add_pluck(std::vector<float>& buf, size_t start, float f0, float amp, float tau) {
    for (size_t i = start; i < buf.size(); ++i) {
        float t = static_cast<float>(i - start) / static_cast<float>(kSr);
        float att = std::min(t / 0.001f, 1.0f);
        float ph = kTwoPi * f0 * t;
        buf[i] += amp * att * std::exp(-t / tau) *
                  (std::sin(ph) + 0.4f * std::sin(2 * ph) + 0.2f * std::sin(3 * ph)) / 1.6f;
    }
}

std::vector<Captured> run_plugin_channels(const std::vector<std::vector<float>>& chans,
                                          uint32_t block,
                                          const std::vector<HostEvent>& host_events = {}) {
    const size_t len = chans[0].size();
    const uint32_t nch = static_cast<uint32_t>(chans.size());
    REQUIRE(clap_entry.init(""));
    auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    clap_host_t host{};
    const clap_plugin_t* p = factory->create_plugin(factory, &host, desc->id);
    REQUIRE(p->init(p));
    REQUIRE(p->activate(p, kSr, 32, 512));
    REQUIRE(p->start_processing(p));

    std::vector<Captured> events;
    std::vector<std::vector<float>> out(nch, std::vector<float>(len, 0.0f));
    std::vector<float*> in_ptrs(nch), out_ptrs(nch);
    OutCtx ctx{&events, 0};
    InCtx in_ctx;
    clap_input_events_t in_ev{&in_ctx, in_size, in_get};
    clap_output_events_t out_ev{&ctx, out_push};

    for (size_t pos = 0; pos < len; pos += block) {
        uint32_t n = static_cast<uint32_t>(std::min<size_t>(block, len - pos));
        for (uint32_t c = 0; c < nch; ++c) {
            in_ptrs[c] = const_cast<float*>(chans[c].data()) + pos;
            out_ptrs[c] = out[c].data() + pos;
        }
        clap_audio_buffer_t ib{}, ob{};
        ib.data32 = in_ptrs.data(); ib.channel_count = nch;
        ob.data32 = out_ptrs.data(); ob.channel_count = nch;
        clap_process_t pr{};
        pr.frames_count = n;
        pr.audio_inputs = &ib; pr.audio_inputs_count = 1;
        pr.audio_outputs = &ob; pr.audio_outputs_count = 1;
        pr.in_events = &in_ev; pr.out_events = &out_ev;
        ctx.block_start = pos;
        in_ctx.block.clear();
        for (const auto& he : host_events) {
            if (he.abs_sample >= pos && he.abs_sample < pos + n) {
                AnyEvent e = he.ev;
                e.h.time = static_cast<uint32_t>(he.abs_sample - pos);
                in_ctx.block.push_back(e);
            }
        }
        p->process(p, &pr);
    }
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
    clap_entry.deinit();
    return events;
}

std::vector<Captured> run_plugin(const std::vector<float>& sig, uint32_t block,
                                 const std::vector<HostEvent>& host_events = {}) {
    return run_plugin_channels({sig}, block, host_events);
}

std::vector<Captured> only(const std::vector<Captured>& e, uint16_t type) {
    std::vector<Captured> r;
    for (auto& x : e) if (x.type == type) r.push_back(x);
    return r;
}
} // namespace

TEST_CASE("plugin_process emits exactly one correct Note On per pluck at any alignment", "[clap][process][onset]") {
    for (uint32_t block : {32u, 64u, 100u, 512u}) {
        for (size_t offset : {1000u, 1037u, 1063u}) { // not aligned to the 64-sample YIN hop
            std::vector<float> sig(24000, 0.0f);
            add_pluck(sig, offset, 220.0f, 0.4f, 0.3f); // A3 = MIDI 57
            auto ev = run_plugin(sig, block);
            auto ons = only(ev, CLAP_EVENT_NOTE_ON);
            INFO("block=" << block << " offset=" << offset);
            REQUIRE(ons.size() == 1);
            REQUIRE(ons[0].key == 57);
            REQUIRE(ons[0].velocity > 0.7);
            REQUIRE(ons[0].abs_sample - offset < static_cast<uint64_t>(0.060 * kSr));
        }
    }
}

TEST_CASE("plugin_process uses the new note's pitch when plucking over a ringing note", "[clap][process][onset]") {
    std::vector<float> sig(60000, 0.0f);
    add_pluck(sig, 1000, 110.0f, 0.3f, 0.5f);   // A2 = 45, still ringing
    add_pluck(sig, 20000, 329.63f, 0.5f, 0.5f); // E4 = 64
    auto ev = run_plugin(sig, 64);
    auto ons = only(ev, CLAP_EVENT_NOTE_ON);
    auto offs = only(ev, CLAP_EVENT_NOTE_OFF);
    REQUIRE(ons.size() == 2);
    REQUIRE(ons[0].key == 45);
    REQUIRE(ons[1].key == 64);
    REQUIRE(ons[1].abs_sample >= 20000);
    // the first note is released no later than the second note starts
    REQUIRE_FALSE(offs.empty());
    REQUIRE(offs.front().key == 45);
    REQUIRE(offs.front().abs_sample <= ons[1].abs_sample);
}

TEST_CASE("plugin_process velocity follows pluck strength", "[clap][process][velocity]") {
    double v_soft = 0, v_loud = 0;
    for (float amp : {0.03f, 0.6f}) {
        std::vector<float> sig(24000, 0.0f);
        add_pluck(sig, 1037, 220.0f, amp, 0.3f);
        auto ons = only(run_plugin(sig, 64), CLAP_EVENT_NOTE_ON);
        REQUIRE(ons.size() == 1);
        (amp < 0.1f ? v_soft : v_loud) = ons[0].velocity;
    }
    REQUIRE(v_loud - v_soft > 0.4);
}

TEST_CASE("plugin_process emits no note for silence", "[clap][process]") {
    std::vector<float> sig(24000, 0.0f);
    REQUIRE(run_plugin(sig, 64).empty());
}

TEST_CASE("plugin_process applies PARAM_VALUE events (transient sensitivity)", "[clap][process][params]") {
    std::vector<float> sig(24000, 0.0f);
    add_pluck(sig, 1037, 220.0f, 0.04f, 0.3f); // quiet: peak ~0.04

    REQUIRE(only(run_plugin(sig, 64), CLAP_EVENT_NOTE_ON).size() == 1);

    // Max sensitivity floor 0.05 is above the pluck: no note.
    auto muted = run_plugin(sig, 64, {param(0, 1 /*kParamTransientSensitivity*/, 0.05)});
    REQUIRE(only(muted, CLAP_EVENT_NOTE_ON).empty());
}

TEST_CASE("plugin_process emits output events in non-decreasing time order", "[clap][process][order]") {
    std::vector<float> sig(24000, 0.0f);
    add_pluck(sig, 1037, 220.0f, 0.4f, 0.3f);
    // Keyboard events land in the middle of the generated note activity.
    auto ev = run_plugin(sig, 512, {key_on(1500, 60), key_on(3000, 62), key_on(4001, 64)});
    REQUIRE(only(ev, CLAP_EVENT_NOTE_ON).size() >= 4); // 3 forwarded + 1 generated
    for (size_t i = 1; i < ev.size(); ++i) {
        INFO("event " << i);
        REQUIRE(ev[i].abs_sample >= ev[i - 1].abs_sample);
    }
}

TEST_CASE("plugin_process tracks 3 simultaneous strings on discrete hexaphonic channels", "[clap][process][poly]") {
    std::vector<std::vector<float>> chans(6, std::vector<float>(24000, 0.0f));
    add_pluck(chans[0], 1000, 82.41f, 0.4f, 0.4f);  // E2 = 40
    add_pluck(chans[2], 1200, 146.83f, 0.4f, 0.4f); // D3 = 50
    add_pluck(chans[4], 1400, 246.94f, 0.4f, 0.4f); // B3 = 59

    auto ons = only(run_plugin_channels(chans, 64), CLAP_EVENT_NOTE_ON);
    REQUIRE(ons.size() == 3);
    bool seen0 = false, seen2 = false, seen4 = false;
    for (const auto& e : ons) {
        if (e.channel == 0 && e.key == 40) seen0 = true;
        if (e.channel == 2 && e.key == 50) seen2 = true;
        if (e.channel == 4 && e.key == 59) seen4 = true;
    }
    REQUIRE(seen0);
    REQUIRE(seen2);
    REQUIRE(seen4);
}

TEST_CASE("plugin_process mono chord guard suppresses strummed chord", "[clap][process][chord]") {
    std::vector<float> sig(24000, 0.0f);
    add_pluck(sig, 1000, 110.0f, 0.35f, 0.4f);
    add_pluck(sig, 2000, 164.81f, 0.35f, 0.4f); // 20 ms later -> strum
    add_pluck(sig, 3000, 220.0f, 0.35f, 0.4f);

    auto ons = only(run_plugin(sig, 64), CLAP_EVENT_NOTE_ON);
    // First pluck may start before the second arrives, then must be cut off and the chord muted.
    REQUIRE(ons.size() <= 1);
}

TEST_CASE("plugin_process emits either CLAP or MIDI2 UMP, never both", "[clap][process][dialect]") {
    std::vector<float> sig(24000, 0.0f);
    add_pluck(sig, 1037, 220.0f, 0.4f, 0.3f);

    auto ev_clap = run_plugin(sig, 64); // default dialect 0
    REQUIRE(only(ev_clap, CLAP_EVENT_NOTE_ON).size() == 1);
    REQUIRE(only(ev_clap, CLAP_EVENT_MIDI2).empty());
    REQUIRE_FALSE(only(ev_clap, CLAP_EVENT_NOTE_EXPRESSION).empty());

    auto ev_ump = run_plugin(sig, 64, {param(0, 4 /*kParamOutputDialect*/, 1.0)});
    REQUIRE(only(ev_ump, CLAP_EVENT_NOTE_ON).empty());
    REQUIRE(only(ev_ump, CLAP_EVENT_NOTE_EXPRESSION).empty());
    REQUIRE_FALSE(only(ev_ump, CLAP_EVENT_MIDI2).empty());
}

TEST_CASE("plugin_process throttles expression events to <=250Hz", "[clap][process][throttle]") {
    std::vector<float> sig(48000, 0.0f); // 1 second
    add_pluck(sig, 1000, 220.0f, 0.4f, 0.8f);
    auto expr = only(run_plugin(sig, 64), CLAP_EVENT_NOTE_EXPRESSION);
    // 3 expression types * <= 250 Hz = <= 750/s (without throttle it was ~2250/s)
    REQUIRE(expr.size() < 750);
}

TEST_CASE("plugin_process emits Note Off promptly on sudden string choke", "[clap][process][release]") {
    std::vector<float> sig(36000, 0.0f);
    add_pluck(sig, 1000, 220.0f, 0.5f, 2.0f);
    // Sudden choke at sample 15000
    for (size_t i = 15000; i < sig.size(); ++i) sig[i] = 0.0f;

    auto ev = run_plugin(sig, 64);
    auto offs = only(ev, CLAP_EVENT_NOTE_OFF);
    REQUIRE(offs.size() == 1);
    REQUIRE(offs[0].abs_sample >= 15000);
    REQUIRE(offs[0].abs_sample < 15000 + static_cast<uint64_t>(0.025 * kSr));
    REQUIRE(offs[0].velocity > 0.6);
}
