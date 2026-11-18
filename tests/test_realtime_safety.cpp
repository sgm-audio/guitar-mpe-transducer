#include <catch2/catch_test_macros.hpp>
#include <clap/clap.h>
#include <transducer/engine/transducer_engine.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
#include <vector>

extern const clap_plugin_entry_t clap_entry;

namespace {
thread_local bool g_trap_allocs = false;
thread_local size_t g_alloc_count = 0;
} // namespace

void* operator new(std::size_t sz) {
    if (g_trap_allocs) ++g_alloc_count;
    void* p = std::malloc(sz ? sz : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t sz) { return ::operator new(sz); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

TEST_CASE("Audio thread performs zero heap allocations over 10,000 blocks", "[rt][alloc]") {
    constexpr double kSr = 48000.0;
    constexpr uint32_t kBlock = 64;
    constexpr float kTwoPi = 6.28318530718f;

    REQUIRE(clap_entry.init(""));
    auto* factory = static_cast<const clap_plugin_factory_t*>(
        clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    const auto* desc = factory->get_plugin_descriptor(factory, 0);
    clap_host_t host{};
    const clap_plugin_t* plug = factory->create_plugin(factory, &host, desc->id);
    REQUIRE(plug->init(plug));
    REQUIRE(plug->activate(plug, kSr, kBlock, kBlock));
    REQUIRE(plug->start_processing(plug));

    std::vector<float> in(kBlock), out(kBlock);
    float* in_p = in.data();
    float* out_p = out.data();
    clap_audio_buffer_t ib{&in_p, nullptr, 1, 0, 0};
    clap_audio_buffer_t ob{&out_p, nullptr, 1, 0, 0};

    clap_event_note_t kb{};
    kb.header = {sizeof(kb), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0};
    kb.note_id = 1; kb.key = 60; kb.velocity = 0.8;

    struct InCtx { clap_event_note_t* ev; bool send; } in_ctx{&kb, false};
    clap_input_events_t in_ev{
        &in_ctx,
        [](const clap_input_events_t* l) -> uint32_t { return static_cast<InCtx*>(l->ctx)->send ? 1u : 0u; },
        [](const clap_input_events_t* l, uint32_t) -> const clap_event_header_t* {
            return &static_cast<InCtx*>(l->ctx)->ev->header;
        }
    };
    clap_output_events_t out_ev{
        nullptr,
        [](const clap_output_events_t*, const clap_event_header_t*) -> bool { return true; }
    };

    clap_process_t pr{};
    pr.frames_count = kBlock;
    pr.audio_inputs = &ib; pr.audio_inputs_count = 1;
    pr.audio_outputs = &ob; pr.audio_outputs_count = 1;
    pr.in_events = &in_ev; pr.out_events = &out_ev;

    g_alloc_count = 0;
    g_trap_allocs = true;
    for (int b = 0; b < 10000; ++b) {
        in_ctx.send = (b == 10 || b == 500);
        kb.header.type = (b == 500) ? CLAP_EVENT_NOTE_OFF : CLAP_EVENT_NOTE_ON;
        for (uint32_t i = 0; i < kBlock; ++i) {
            uint64_t idx = static_cast<uint64_t>(b) * kBlock + i;
            in[i] = ((b % 200) < 120)
                ? 0.4f * std::sin(kTwoPi * 220.0f * static_cast<float>(idx) / static_cast<float>(kSr))
                : 0.0f;
        }
        plug->process(plug, &pr);
    }
    g_trap_allocs = false;
    REQUIRE(g_alloc_count == 0);

    plug->stop_processing(plug);
    plug->deactivate(plug);
    plug->destroy(plug);
    clap_entry.deinit();

    // Also verify TransducerEngine with TargetAll voice routing active
    transducer::engine::TransducerEngine engine;
    engine.init(static_cast<float>(kSr));
    engine.set_voice_routing_mode(transducer::voice::RoutingMode::kTargetAll);
    engine.handle_keyboard_note_on(1, 60, 0.8f);
    engine.handle_keyboard_note_on(2, 64, 0.8f);
    transducer::engine::TransducerEngineOutput eng_out{};

    g_alloc_count = 0;
    g_trap_allocs = true;
    for (int b = 0; b < 10000; ++b) {
        engine.process_block(in.data(), kBlock, eng_out);
    }
    g_trap_allocs = false;
    REQUIRE(g_alloc_count == 0);
}
