#pragma once

#include <clap/clap.h>

extern const clap_plugin_descriptor_t s_guitar_transducer_descriptor;

const clap_plugin_t* create_guitar_transducer_plugin(const clap_host_t* host);
