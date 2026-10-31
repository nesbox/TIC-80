/* The game runner links without the studio: this file builds it with the core
 * over a filesystem on a temporary root, and a fake host that records the end,
 * the exit, the error and the trace.
 *
 * Target run-test in cmake/tests.cmake; `ctest -R run-test` runs it.
 */
#include "studio/screens/run.h"
#include "studio/fs.h"
#include "studio/paths.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static tic_mem* Tic;
static tic_fs* Fs;

static bool Active;

static s32 EndedCalls;
static s32 ExitedCalls;
static s32 ErrorCalls;
static s32 TraceCalls;

static char ErrorText[TICNAME_MAX];
static char TraceText[TICNAME_MAX];
static u8 TraceColor;

static void fakeEnded(void* host)
{
    (void)host;
    EndedCalls++;
}

static void fakeExited(void* host)
{
    (void)host;
    ExitedCalls++;
}

static void fakeError(void* host, const char* info)
{
    (void)host;
    ErrorCalls++;
    snprintf(ErrorText, sizeof ErrorText, "%s", info);
}

static void fakeTrace(void* host, const char* text, u8 color)
{
    (void)host;
    TraceCalls++;
    snprintf(TraceText, sizeof TraceText, "%s", text);
    TraceColor = color;
}

static bool fakeIsActive(void* host)
{
    (void)host;
    return Active;
}

// The platform the runner asks: the test is all of it.
u64 tic_sys_counter_get(void)
{
    return 0;
}

u64 tic_sys_freq_get(void)
{
    return 1;
}

void tic_sys_preseed(void)
{
}

void tic_sys_open_path(const char* path)
{
    (void)path;
}

// A machine with nothing to run: the runner's tick and the persistent memory
// are what this test is about, not a language.
static s32 MachineTicks;

static void noopTick(tic_mem* tic)
{
    (void)tic;
    MachineTicks++;
}

static void quietMachine(void)
{
    tic_core* core = (tic_core*)Tic;
    core->state.tick = noopTick;
    core->state.initialized = true;
}

static void reset(void)
{
    Active = true;
    EndedCalls = ExitedCalls = ErrorCalls = TraceCalls = 0;
    MachineTicks = 0;
    ErrorText[0] = TraceText[0] = '\0';
    TraceColor = 0;
}

// An editors build reports the end and the console lines; a player build
// reports the exit and nothing else.
static Run* makeRun(bool ended, bool exited)
{
    Run* run = calloc(1, sizeof(Run));
    assert(run);

    run_init(run, &(RunDeps){
                      .host = NULL,
                      .tic = Tic,
                      .fs = Fs,
                      .ended = ended ? fakeEnded : NULL,
                      .exited = exited ? fakeExited : NULL,
                      .error = ended ? fakeError : NULL,
                      .trace = ended ? fakeTrace : NULL,
                      .isActive = fakeIsActive,
                  });

    return run;
}

static void testPersistent(void)
{
    reset();

    Run* run = makeRun(false, false);
    assert(strlen(run->saveid) > 0);

    // A block written under the save id comes back in the machine when the
    // run is made.
    tic_persistent saved;
    memset(&saved, 0, sizeof saved);
    saved.data[0] = 0x5A;
    saved.data[1] = 0xA5;

    assert(tic_fs_saveroot(Fs, run->saveid, &saved, sizeof saved, true));

    run_free(run);
    run = makeRun(false, false);

    assert(Tic->ram->persistent.data[0] == 0x5A);
    assert(Tic->ram->persistent.data[1] == 0xA5);
    assert(memcmp(&run->pmem, &saved, sizeof saved) == 0);

    // A block the machine writes during a tick lands in the file.
    Tic->ram->persistent.data[2] = 0x3C;
    run->tick(run);

    s32 size = 0;
    void* data = tic_fs_loadroot(Fs, run->saveid, &size);
    assert(data);
    assert(size == (s32)sizeof(tic_persistent));
    assert(((tic_persistent*)data)->data[2] == 0x3C);
    free(data);

    assert(tic_fs_delfile(Fs, run->saveid) == 0);

    run_free(run);
    puts("persistent ok");
}

static void testEmptyCart(void)
{
    reset();

    // Nothing to run: the machine itself reports the empty code, and the run
    // reports it before anything else.
    Run* run = makeRun(true, false);
    run->tick(run);

    assert(EndedCalls == 1);
    assert(ErrorCalls == 1);
    assert(strcmp(ErrorText, "the code is empty") == 0);

    run_free(run);
    puts("empty cart ok");
}

static void testReinit(void)
{
    reset();

    Run* run = makeRun(false, false);
    run->exit = true;

    // The studio enters RUN through run_init every time: the exit clears and
    // the block under the save id comes back.
    tic_persistent saved;
    memset(&saved, 0, sizeof saved);
    saved.data[3] = 0x77;
    assert(tic_fs_saveroot(Fs, run->saveid, &saved, sizeof saved, true));

    RunDeps deps = run->deps;
    run_init(run, &deps);

    assert(!run->exit);
    assert(Tic->ram->persistent.data[3] == 0x77);
    assert(memcmp(&run->pmem, &saved, sizeof saved) == 0);

    assert(tic_fs_delfile(Fs, run->saveid) == 0);

    run_free(run);
    puts("reinit ok");
}

static void testExit(void)
{
    reset();

    // The cart asks to exit: an editors build lands in the console, a player
    // build leaves the studio.
    Run* run = makeRun(true, false);
    run->exit = true;
    run->tick(run);

    assert(EndedCalls == 1);

    run_free(run);

    reset();

    Run* player = makeRun(false, true);
    player->exit = true;
    player->tick(player);

    assert(ExitedCalls == 1);

    run_free(player);
    puts("exit ok");
}

static void testGate(void)
{
    reset();

    // Another mode owns the frame: the machine does not tick and the exit is
    // not handled. The mode comes back: the tick runs and the exit lands.
    Run* run = makeRun(true, false);
    Active = false;
    run->exit = true;
    run->tick(run);

    assert(MachineTicks == 0);
    assert(EndedCalls == 0);

    Active = true;
    run->tick(run);

    assert(MachineTicks == 1);
    assert(EndedCalls == 1);

    run_free(run);
    puts("gate ok");
}

static void testReport(void)
{
    reset();

    Run* run = makeRun(true, false);

    // The machine's error goes to the console through the host, after the
    // mode is moved there.
    run->tickData.error(run->tickData.data, "boom");

    assert(EndedCalls == 1);
    assert(ErrorCalls == 1);
    assert(strcmp(ErrorText, "boom") == 0);

    run->tickData.trace(run->tickData.data, "hello", 7);

    assert(TraceCalls == 1);
    assert(strcmp(TraceText, "hello") == 0);
    assert(TraceColor == 7);

    run_free(run);
    puts("report ok");
}

int main(void)
{
    // A root of this run's own: two runs never share one.
    char root[] = "/tmp/tic80-run-XXXXXX";
    assert(mkdtemp(root));

    Fs = tic_fs_create(root, NULL);
    assert(Fs);

    tic_fs_makedir(Fs, TIC_LOCAL);

    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    // Before the machine is quieted: nothing to run is the machine's own
    // error path, and the run reports it.
    testEmptyCart();

    quietMachine();

    testPersistent();
    testReinit();
    testExit();
    testGate();
    testReport();

    tic_core_close(Tic);

    assert(tic_fs_deldir(Fs, TIC_LOCAL) == 0);
    assert(tic_fs_deldir(Fs, "") == 0);

    puts("run ok");
    return 0;
}
