#include "yue_wrapper.h"
#include "yuescript/yue_compiler.h"
#include <cstring>
#include <new>

extern "C" {

YueConfig_t* yue_config_create(void) {
    YueConfig_t* config = new (std::nothrow) YueConfig_t();
    if (config) {
        config->lint_global_variable = false;
        config->implicit_return_root = true;
        config->reserve_line_number = true;
        config->use_space_over_tab = false;
        config->reserve_comment = false;
        config->exporting = false;
        config->profiling = false;
        config->line_offset = 0;
        config->module = nullptr;
    }
    return config;
}

void yue_config_free(YueConfig_t* config) {
    delete config;
}

YueCompiler_t* yue_compiler_create(void* lua_state, void (*lua_open)(void*), bool same_module) {
    auto cpp_lua_open = lua_open ? 
        std::function<void(void*)>([lua_open](void* l) { lua_open(l); }) :
        std::function<void(void*)>();
    
    auto* compiler = new (std::nothrow) yue::YueCompiler(lua_state, cpp_lua_open, same_module);
    return reinterpret_cast<YueCompiler_t*>(compiler);
}

void yue_compiler_destroy(YueCompiler_t* compiler) {
    delete reinterpret_cast<yue::YueCompiler*>(compiler);
}

CompileInfo_t* yue_compile(YueCompiler_t* compiler, const char* codes, const YueConfig_t* config) {
    yue::YueConfig cpp_config;
    if (config) {
        cpp_config.lintGlobalVariable = config->lint_global_variable;
        cpp_config.implicitReturnRoot = config->implicit_return_root;
        cpp_config.reserveLineNumber = config->reserve_line_number;
        cpp_config.useSpaceOverTab = config->use_space_over_tab;
        cpp_config.reserveComment = config->reserve_comment;
        cpp_config.exporting = config->exporting;
        cpp_config.profiling = config->profiling;
        cpp_config.lineOffset = config->line_offset;
        if (config->module) {
            cpp_config.module = config->module;
        }
    }

    auto cpp_compiler = reinterpret_cast<yue::YueCompiler*>(compiler);
    auto result = cpp_compiler->compile(codes, cpp_config);

    auto* info = new (std::nothrow) CompileInfo_t();
    if (!info) return nullptr;

    // Copy the data
    info->codes = strdup(result.codes.c_str());
    if (result.error) {
        info->error_msg = strdup(result.error->msg.c_str());
        info->error_line = result.error->line;
        info->error_col = result.error->col;
        info->error_display_message = strdup(result.error->displayMessage.c_str());
    } else {
        info->error_msg = nullptr;
        info->error_line = 0;
        info->error_col = 0;
        info->error_display_message = nullptr;
    }

    info->parse_time = result.parseTime;
    info->compile_time = result.compileTime;
    info->used_var = result.usedVar;

    if (result.globals) {
        info->globals_count = result.globals->size();
        info->globals = new GlobalVar_t[info->globals_count];
        for (size_t i = 0; i < result.globals->size(); ++i) {
            info->globals[i].name = strdup((*result.globals)[i].name.c_str());
            info->globals[i].line = (*result.globals)[i].line;
            info->globals[i].col = (*result.globals)[i].col;
            info->globals[i].access_type = static_cast<YueAccessType>(
                static_cast<int>((*result.globals)[i].accessType));
        }
    } else {
        info->globals = nullptr;
        info->globals_count = 0;
    }

    return info;
}

void yue_compile_info_free(CompileInfo_t* info) {
    if (!info) return;
    
    free((void*)info->codes);
    free((void*)info->error_msg);
    free((void*)info->error_display_message);
    
    if (info->globals) {
        for (int i = 0; i < info->globals_count; ++i) {
            free((void*)info->globals[i].name);
        }
        delete[] info->globals;
    }
    
    delete info;
}

void yue_compiler_clear(void* lua_state) {
    yue::YueCompiler::clear(lua_state);
}

const char* yue_get_version(void) {
    static std::string version_str(yue::version);
    return version_str.c_str();
}

const char* yue_get_extension(void) {
    static std::string extension_str(yue::extension);
    return extension_str.c_str();
}

}