#include <catch2/catch_test_macros.hpp>
#include <clap/clap.h>
#include <vector>

// Forward declare entry
extern const clap_plugin_entry_t clap_entry;

TEST_CASE("CLAP plugin descriptor and lifecycle initialization", "[clap][lifecycle]") {
    REQUIRE(clap_entry.init(""));

    const clap_plugin_factory_t* factory = static_cast<const clap_plugin_factory_t*>(
        clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    REQUIRE(factory != nullptr);

    uint32_t count = factory->get_plugin_count(factory);
    REQUIRE(count >= 1);

    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    REQUIRE(desc != nullptr);
    REQUIRE(std::string(desc->id) == "com.sgm-audio.guitar-mpe-transducer");

    clap_host_t dummy_host{};
    const clap_plugin_t* plug = factory->create_plugin(factory, &dummy_host, desc->id);
    REQUIRE(plug != nullptr);

    REQUIRE(plug->init(plug));
    REQUIRE(plug->activate(plug, 48000.0, 32, 512));

    // Audio ports extension
    const clap_plugin_audio_ports_t* audio_ports = static_cast<const clap_plugin_audio_ports_t*>(
        plug->get_extension(plug, CLAP_EXT_AUDIO_PORTS));
    REQUIRE(audio_ports != nullptr);
    REQUIRE(audio_ports->count(plug, true) == 1); // 1 input port

    // Note ports extension
    const clap_plugin_note_ports_t* note_ports = static_cast<const clap_plugin_note_ports_t*>(
        plug->get_extension(plug, CLAP_EXT_NOTE_PORTS));
    REQUIRE(note_ports != nullptr);
    REQUIRE(note_ports->count(plug, false) == 1); // 1 output note port

    clap_note_port_info_t out_port_info{};
    REQUIRE(note_ports->get(plug, 0, false, &out_port_info));
    REQUIRE((out_port_info.supported_dialects & CLAP_NOTE_DIALECT_CLAP) != 0);
    REQUIRE((out_port_info.supported_dialects & CLAP_NOTE_DIALECT_MIDI2) != 0);

    plug->deactivate(plug);
    plug->destroy(plug);
    clap_entry.deinit();
}
