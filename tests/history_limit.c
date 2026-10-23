/* The history keeps its stored deltas under a byte budget and drops the oldest
 * steps first. This test builds against a tiny budget — HISTORY_MAX_BYTES comes
 * from its target — so eviction takes a few commits.
 *
 * Target history-limit-test in cmake/tests.cmake; `ctest -R history` runs it.
 */
#include "ext/history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum
{
    SIZE = 32,
    STEP = 10,
};

static u8 data[SIZE];

static void reset(void)
{
    memset(data, 0, SIZE);
}

// Three ten-byte steps: the third takes the total past 25 bytes and drops the
// initial state and the first step.
static void test_evict(void)
{
    reset();
    History* history = history_create(data, SIZE);

    memset(data, 1, STEP);
    assert(history_add(history));
    memset(data, 2, STEP);
    assert(history_add(history));
    memset(data, 3, STEP);
    assert(history_add(history));

    history_undo(history);
    for(int i = 0; i < STEP; i++)
        assert(data[i] == 2);

    // The first step is gone, so the second is where undo stops.
    history_undo(history);
    for(int i = 0; i < STEP; i++)
        assert(data[i] == 2);

    history_redo(history);
    for(int i = 0; i < STEP; i++)
        assert(data[i] == 3);

    history_redo(history);
    for(int i = 0; i < STEP; i++)
        assert(data[i] == 3);

    history_delete(history);
    puts("evict ok");
}

// A step larger than the whole budget is still the current one: it is kept,
// and the state before it is what the history forgets.
static void test_oversized(void)
{
    reset();
    History* history = history_create(data, SIZE);

    for(int i = 0; i < SIZE; i++)
        data[i] = (u8)(i + 1);
    assert(history_add(history));

    history_undo(history);
    for(int i = 0; i < SIZE; i++)
        assert(data[i] == (u8)(i + 1));

    history_delete(history);
    puts("oversized ok");
}

// Dropping the redo branch gives its bytes back before the budget is checked.
static void test_branch(void)
{
    reset();
    History* history = history_create(data, SIZE);

    memset(data, 1, STEP);
    assert(history_add(history));
    memset(data, 2, STEP);
    assert(history_add(history));

    history_undo(history);

    // A ten-byte delta over [6, 16), replacing the ten-byte step above it:
    // twenty bytes in all, still under the budget.
    memset(data + 6, 3, 10);
    assert(history_add(history));

    history_undo(history);
    for(int i = 0; i < STEP; i++)
        assert(data[i] == 1);
    for(int i = STEP; i < 16; i++)
        assert(data[i] == 0);

    history_redo(history);
    for(int i = 0; i < 6; i++)
        assert(data[i] == 1);
    for(int i = 6; i < 16; i++)
        assert(data[i] == 3);

    // A fourth ten-byte step passes the budget: the initial state and the
    // first step go, and undo stops at the second.
    memset(data, 4, STEP);
    assert(history_add(history));

    history_undo(history);
    for(int i = 0; i < 6; i++)
        assert(data[i] == 1);
    for(int i = 6; i < 16; i++)
        assert(data[i] == 3);

    history_undo(history);
    for(int i = 0; i < 6; i++)
        assert(data[i] == 1);

    history_delete(history);
    puts("branch ok");
}

int main(void)
{
    test_evict();
    test_oversized();
    test_branch();

    puts("history limit checks passed");
    return 0;
}
