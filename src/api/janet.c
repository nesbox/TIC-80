#include "api.h"
#include "core/core.h"
#include <assert.h>

#if defined(TIC_BUILD_WITH_JANET)

#include "janet.h"

static const char *const JanetKeywords[] = {
    // special forms
    "def", "var", "fn", "do", "quote", "if", "splice", "while", "break", "set",
    "quasiquote", "unquote", "upscope",
    // literals
    "nil", "true", "false",
    // common macros
    // TODO: decide which ones should be hightlighted as keywords
    // "%=","*=","++","+=","--","-=","->","->>","-?>","-?>>","/=",
    // "and","as->","as-macro","as?->","assert","case","chr","comment","compif","comptime","compwhen","cond","coro","def-","default","defdyn","defer","defmacro","defmacro-","defn","defn-","delay","doc","each","eachk","eachp","edefer",
    // "ev/do-thread","ev/gather","ev/spawn","ev/spawn-thread","ev/with-deadline","ffi/defbind",
    // "fiber-fn","for","forever","forv","generate","if-let","if-not","if-with","import","juxt","label","let","loop","match","or","prompt","protect","repeat","seq","short-fn","tabseq","toggle","tracev","try","unless","use","var-","varfn","when","when-let","when-with","with","with-dyns","with-syms","with-vars",
};

static const char* DYN_TIC_MEM = ":tic_mem";

#define JANET_FN_S_tic(CNAME) \
    static const int32_t _cfun_##CNAME##_sourceline_ = __LINE__; \
    static Janet _cfun_##CNAME (int32_t argc, Janet *argv) { \
        tic_mem* tic = janet_unwrap_pointer(janet_dyn(DYN_TIC_MEM)); \
        assert(tic != NULL);

JANET_FN_S_tic(btn)
    janet_fixarity(argc, 1);
    return janet_wrap_boolean(tic_api_btn(tic, janet_getinteger(argv, 0)));
}

#define JANET_REG_S_ez(CNAME) {"CNAME", _cfun_##CNAME, NULL, __FILE__, _cfun_##CNAME##_sourceline_}

static const JanetRegExt cfuns[] = {
    JANET_REG_S_ez(btn),
    // # todo: 
    // JANET_REG_("circ", _circ),
    // JANET_REG_("circ", _circ),
    // JANET_REG_("circ", _circ),
    // JANET_REG_("circ", _circ),
    // JANET_REG_("circ", _circ),
    JANET_REG_END
};


static bool _evalJanet(tic_mem* tic, const char* code) {
    tic_core* core = (tic_core*)tic;
    JanetTable* env = core->currentVM;
    int err = janet_dostring(env, code, "main", NULL);
    if (err != 0) { /* let janet print error */ return false; }
    return true;
}

static bool initJanet(tic_mem *tic, const char *code) {
    tic_core *core = (tic_core *)tic;
    if (janet_init() != 0)
      return false;
    JanetTable *env = janet_core_env(NULL);
    janet_cfuns_ext(env, "tic80", cfuns);
    janet_setdyn(DYN_TIC_MEM, janet_wrap_pointer(tic));

    if (!_evalJanet(tic, code))
      return false;

    core->currentVM = env;
    return true;
}

static void closeJanet(tic_mem *tic) { janet_deinit(); }

static void callBoot(tic_mem* tic) {
    // TODO
}

static void callTick(tic_mem* tic) {
    // TODO
}

static void callScanline(tic_mem* tic, s32 row, void* data) {
    // TODO
}

static void callBorder(tic_mem* tic, s32 row, void* data) {
    // TODO
}

static void callMenu(tic_mem* tic, s32 index, void* data) {
    // TODO
}

static const tic_outline_item* getOutline(const char* code, s32* size) {
    // TODO
}

static void evalJanet(tic_mem* tic, const char* code) {
    _evalJanet(tic, code);
}

tic_script_config JanetSyntaxConfig =
{
    .id = 18,
    .name = "janet",
    .fileExtension = ".janet",
    .projectComment = "##",
    {
        .init = initJanet,
        .close = closeJanet,
        .boot = callBoot,
        .tick = callTick,

        .callback = {
            .scanline = callScanline,
            .border = callBorder,
            .menu = callMenu,
        },
    },

    .getOutline = getOutline,
    .eval = evalJanet,

    // .blockCommentStart = "(comment ",
    // .blockCommentEnd = ")",
    .blockCommentStart  = NULL,
    .blockCommentEnd    = NULL,
    .blockCommentStart2 = NULL,
    .blockCommentEnd2 = NULL,
    .blockStringStart = NULL,
    .blockStringEnd = NULL,
    .singleComment = "#",
    .blockEnd           = NULL,

    .keywords = JanetKeywords,
    .keywordsCount = COUNT_OF(JanetKeywords),
};

#endif /* defined(TIC_BUILD_WITH_JANET) */
