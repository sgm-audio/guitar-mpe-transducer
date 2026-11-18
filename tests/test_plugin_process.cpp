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
    int16_t key;
    double velocity;
};

struct OutCtx {
    std::vector<Captured>* sink;
    uint64_t block_start;
};

uint32_t in_size(const clap_input_events_t*) { return 0; }
const clap_event_header_t* in_get(const clap_input_events_t*, uint32_t) { return nullptr; }

bool out_push(const clap_output_events_t* l, const clap_event_header_t* h) {
    auto* c = static_cast<OutCtx*>(l->ctx);
    if (h->type == CLAP_EVENT_NOTE_ON || h->type == CLAP_EVENT_NOTE_OFF) {
        auto* n = reinterpret_cast<const clap_event_note_t*>(h);
        c->sink->push_back({c->block_start + h->time, h->type, n->key, n->velocity});
    }
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

// Runs the real CLAP process() over the signal in blocks and returns note events.
std::vector<Captured> run_plugin(const std::vector<float>& sig, uint32_t block) {
    REQUIRE(clap_entry.init(""));
    auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    clap_host_t host{};
    const clap_plugin_t* p = factory->create_plugin(factory, &host, desc->id);
    REQUIRE(p->init(p));
    REQUIRE(p->activate(p, kSr, 32, 512));
    REQUIRE(p->start_processing(p));

    std::vector<Captured> events;
    std::vector<float> in(sig), out(sig.size());
    OutCtx ctx{&events, 0};
    clap_input_events_t in_ev{nullptr, in_size, in_get};
    clap_output_events_t out_ev{&ctx, out_push};

    for (size_t pos = 0; pos < sig.size(); pos += block) {
        uint32_t n = static_cast<uint32_t>(std::min<size_t>(block, sig.size() - pos));
        float* in_ch = in.data() + pos;
        float* out_ch = out.data() + pos;
        clap_audio_buffer_t ib{}, ob{};
        ib.data32 = &in_ch; ib.channel_count = 1;
        ob.data32 = &out_ch; ob.channel_count = 1;
        clap_process_t pr{};
        pr.frames_count = n;
        pr.audio_inputs = &ib; pr.audio_inputs_count = 1;
        pr.audio_outputs = &ob; pr.audio_outputs_count = 1;
        pr.in_events = &in_ev; pr.out_events = &out_ev;
        ctx.block_start = pos;
        p->process(p, &pr);
    }
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
    clap_entry.deinit();
    return events;
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
