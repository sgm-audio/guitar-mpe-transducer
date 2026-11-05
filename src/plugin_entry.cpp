#include <clap/clap.h>

static bool plugin_init(const char *plugin_path) {
    (void)plugin_path;
    return true;
}

static void plugin_deinit(void) {}

static const void *plugin_get_factory(const char *factory_id) {
    (void)factory_id;
    return nullptr;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    plugin_init,
    plugin_deinit,
    plugin_get_factory,
};
