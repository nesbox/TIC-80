// yue_wrapper.h
#ifndef YUE_WRAPPER_H
#define YUE_WRAPPER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef YUE_STATIC_CONFIG
#define YUE_WRAPPER_API
#else
#ifdef _WIN32
    #ifdef YUE_WRAPPER_EXPORTS
        #define YUE_WRAPPER_API __declspec(dllexport)
    #else
        #define YUE_WRAPPER_API __declspec(dllimport)
    #endif
#else
    #define YUE_WRAPPER_API
#endif
#endif

typedef struct YueCompiler_t YueCompiler_t;
typedef struct YueConfig_t YueConfig_t;
typedef struct CompileInfo_t CompileInfo_t;
typedef struct GlobalVar_t GlobalVar_t;

// Access type enum
typedef enum {
    YUE_ACCESS_NONE,
    YUE_ACCESS_READ,
    YUE_ACCESS_CAPTURE,
    YUE_ACCESS_WRITE
} YueAccessType;

// Global variable structure
struct GlobalVar_t {
    const char* name;
    int line;
    int col;
    YueAccessType access_type;
};

// Configuration structure
struct YueConfig_t {
    bool lint_global_variable;
    bool implicit_return_root;
    bool reserve_line_number;
    bool use_space_over_tab;
    bool reserve_comment;
    bool exporting;
    bool profiling;
    int line_offset;
    const char* module;
};

// Compile info structure
struct CompileInfo_t {
    const char* codes;
    const char* error_msg;
    int error_line;
    int error_col;
    const char* error_display_message;
    GlobalVar_t* globals;
    int globals_count;
    double parse_time;
    double compile_time;
    bool used_var;
};

// Create default config
YUE_WRAPPER_API YueConfig_t* yue_config_create(void);

// Free config
YUE_WRAPPER_API void yue_config_free(YueConfig_t* config);

// Create compiler
YUE_WRAPPER_API YueCompiler_t* yue_compiler_create(void* lua_state, void (*lua_open)(void*), bool same_module);

// Destroy compiler
YUE_WRAPPER_API void yue_compiler_destroy(YueCompiler_t* compiler);

// Compile code
YUE_WRAPPER_API CompileInfo_t* yue_compile(YueCompiler_t* compiler, const char* codes, const YueConfig_t* config);

// Free compile info
YUE_WRAPPER_API void yue_compile_info_free(CompileInfo_t* info);

// Clear compiler state
YUE_WRAPPER_API void yue_compiler_clear(void* lua_state);

// Version and extension getters
YUE_WRAPPER_API const char* yue_get_version(void);
YUE_WRAPPER_API const char* yue_get_extension(void);

// Add before other declarations:
YUE_WRAPPER_API void* yue_get_parser_instance(void);
YUE_WRAPPER_API void yue_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // YUE_WRAPPER_H