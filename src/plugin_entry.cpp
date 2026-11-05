#include "plugin.h"
#include <clap/clap.h>
#include <cstring>

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t *factory) {
    (void)factory;
    return 1;
}

static const clap_plugin_descriptor_t *factory_get_plugin_descriptor(
    const clap_plugin_factory_t *factory, uint32_t index)
{
    (void)factory;
    if (index == 0) {
        return &s_guitar_transducer_descriptor;
    }
    return nullptr;
}

static const clap_plugin_t *factory_create_plugin(
    const clap_plugin_factory_t *factory, const clap_host_t *host, const char *plugin_id)
{
    (void)factory;
    if (std::strcmp(plugin_id, s_guitar_transducer_descriptor.id) == 0) {
        return create_guitar_transducer_plugin(host);
    }
    return nullptr;
}

static const clap_plugin_factory_t s_plugin_factory = {
    factory_get_plugin_count,
    factory_get_plugin_descriptor,
    factory_create_plugin,
};

static bool plugin_init(const char *plugin_path) {
    (void)plugin_path;
    return true;
}

static void plugin_deinit(void) {}

static const void *plugin_get_factory(const char *factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &s_plugin_factory;
    }
    return nullptr;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    plugin_init,
    plugin_deinit,
    plugin_get_factory,
};
