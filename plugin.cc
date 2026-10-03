#include "gcc-plugin.h"
#include "plugin-version.h"
#include <coretypes.h>
#include <tree.h>
#include <c-family/c-pragma.h>
#include <cpplib.h>
#include <input.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
// Required for all GCC plugins
int plugin_is_GPL_compatible = 1;

static FILE *macro_log_file = NULL;
static void (*original_define_callback)(cpp_reader *, location_t, cpp_hashnode *) = NULL;

// Helper to strip directory paths and get the filename (e.g., "src/main.c" -> "main.c")
const char *get_base_filename(const char *path) {
    if (!path) return "unknown";
    const char *slash = strrchr(path, '/');
#if defined(_WIN32) || defined(__CYGWIN__)
    const char *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
#endif
    return slash ? slash + 1 : path;
}

// Callback invoked by libcpp every time a #define is encountered
static void macro_define_callback(cpp_reader *pfile, location_t loc, cpp_hashnode *node) {
    if (macro_log_file && node) {
        // Extract the macro string name and print it on its own line
        const char *macro_name = (const char *)NODE_NAME(node);
        fprintf(macro_log_file, "%s\n", macro_name);
    }

    // Chain to the original GCC define callback if one was present
    if (original_define_callback) {
        original_define_callback(pfile, loc, node);
    }
}

// Event callback executed when pragmas are initialized (parse_in & main_input_filename are set)
void log_pragma(void *gcc_data, void *user_data) {
    if (!parse_in) return;

    // 1. Create the output folder if it doesn't exist
    const char *output_dir = "macro_logs";
#if defined(_WIN32)
    mkdir(output_dir);
#else
    mkdir(output_dir, 0755);
#endif

    // 2. Build the output filepath: "macro_logs/<source_file>.macros.txt"
    const char *base_name = get_base_filename(main_input_filename);
    char log_filepath[512];
    snprintf(log_filepath, sizeof(log_filepath), "%s/%s.macros.txt", output_dir, base_name);

    // 3. Open the file for writing
    macro_log_file = fopen(log_filepath, "w");
    if (!macro_log_file) {
        fprintf(stderr, "[Macro Plugin Error] Could not open file: %s\n", log_filepath);
        return;
    }

    // 4. Hook into libcpp callbacks
    cpp_callbacks *cb = cpp_get_callbacks(parse_in);
    original_define_callback = cb->define;
    cb->define = macro_define_callback;
}

// Cleanup callback to close the file handle at the end of the translation unit
void cleanup_plugin(void *gcc_data, void *user_data) {
    if (macro_log_file) {
        fclose(macro_log_file);
        macro_log_file = NULL;
    }
}

// Plugin entry point
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
    if (!plugin_default_version_check(version, &gcc_version)) {
        fprintf(stderr, "[Macro Plugin Error] Incompatible GCC version.\n");
        return 1;
    }

    // Register callback when pragmas are setup (preprocessor ready)
    register_callback(
        plugin_info->base_name,
        PLUGIN_PRAGMAS,
        log_pragma,
        NULL
    );

    // Register cleanup callback when compilation finishes
    register_callback(
        plugin_info->base_name,
        PLUGIN_FINISH,
        cleanup_plugin,
        NULL
    );

    return 0;
}