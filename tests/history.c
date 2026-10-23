/* The undo history every studio editor commits through: a byte buffer and the
 * XOR deltas between its committed states. This test drives the API on a small
 * buffer and checks the bytes each step restores.
 *
 * Target history-test in cmake/tests.cmake; `ctest -R history` runs it.
 */
#include "ext/history.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 64

static u8 data[SIZE];

// history.c allocates through these: the target maps them here so a test can
// fail one allocation and count what stays alive.
static int alloc_calls;
static int alloc_fail = -1;
static size_t alloc_live;

void* history_test_malloc(size_t size)
{
    if(++alloc_calls == alloc_fail)
        return NULL;

    void* ptr = malloc(size);

    if(ptr)
        alloc_live++;

    return ptr;
}

void history_test_free(void* ptr)
{
    if(ptr)
    {
        alloc_live--;
        free(ptr);
    }
}

static void alloc_reset(void)
{
    alloc_calls = 0;
    alloc_fail = -1;
    alloc_live = 0;
}

static void reset(u8 value)
{
    memset(data, value, SIZE);
}

static void expect(u8 value)
{
    for(u32 i = 0; i < SIZE; i++)
        assert(data[i] == value);
}

// A commit with no changed byte records nothing and returns false.
static void test_no_change(void)
{
    reset(0);
    History* history = history_create(data, SIZE);

    assert(!history_add(history));

    // A change made and taken back before the commit is not a step either.
    data[7] = 0x5a;
    data[7] = 0;
    assert(!history_add(history));

    history_undo(history);
    expect(0);

    history_redo(history);
    expect(0);

    // An undo at the oldest step still restores the committed bytes.
    data[5] = 9;
    history_undo(history);
    expect(0);

    history_delete(history);
    assert(alloc_live == 0);
    puts("no change ok");
}

// The first and last bytes are the two edges of a delta, not a special case.
static void test_edges(void)
{
    reset(0);
    History* history = history_create(data, SIZE);

    data[0] = 0xaa;
    data[SIZE - 1] = 0x55;
    assert(history_add(history));

    history_undo(history);
    expect(0);

    history_redo(history);
    assert(data[0] == 0xaa && data[SIZE - 1] == 0x55);
    for(u32 i = 1; i < SIZE - 1; i++)
        assert(data[i] == 0);

    history_delete(history);
    assert(alloc_live == 0);
    puts("edges ok");
}

// Every commit is one undo step, in order, down to the oldest and back.
static void test_sequence(void)
{
    reset(0);
    History* history = history_create(data, SIZE);

    for(u32 step = 1; step <= 8; step++)
    {
        data[step * 7] = (u8)step;
        assert(history_add(history));
    }

    for(u32 step = 8; step >= 1; step--)
    {
        history_undo(history);

        for(u32 i = 1; i <= 8; i++)
            assert(data[i * 7] == (i < step ? (u8)i : 0));
    }

    // At the oldest step, one more undo is a no-op.
    history_undo(history);
    expect(0);

    for(u32 step = 1; step <= 8; step++)
    {
        history_redo(history);

        for(u32 i = 1; i <= 8; i++)
            assert(data[i * 7] == (i <= step ? (u8)i : 0));
    }

    // And one more redo at the newest is a no-op.
    history_redo(history);
    for(u32 i = 1; i <= 8; i++)
        assert(data[i * 7] == (u8)i);

    history_delete(history);
    assert(alloc_live == 0);
    puts("sequence ok");
}

// An edit after an undo drops the redo branch it replaced.
static void test_branch(void)
{
    reset(0);
    History* history = history_create(data, SIZE);

    data[0] = 1;
    assert(history_add(history));
    data[1] = 2;
    assert(history_add(history));
    data[2] = 3;
    assert(history_add(history));

    history_undo(history);
    history_undo(history);
    assert(data[0] == 1 && data[1] == 0 && data[2] == 0);

    data[3] = 4;
    assert(history_add(history));

    // The old steps 2 and 3 are gone: redo has nowhere to go.
    history_redo(history);
    assert(data[0] == 1 && data[1] == 0 && data[2] == 0 && data[3] == 4);

    history_undo(history);
    assert(data[0] == 1 && data[3] == 0);
    history_undo(history);
    expect(0);

    // A commit from the oldest step drops the whole redo branch too.
    data[4] = 5;
    assert(history_add(history));
    history_redo(history);
    assert(data[4] == 5);
    history_undo(history);
    expect(0);

    history_delete(history);
    assert(alloc_live == 0);
    puts("branch ok");
}

// Changes never committed are discarded by an undo, which lands on the last
// committed state; that is what the editors' insert mode relies on.
static void test_uncommitted(void)
{
    reset(0);
    History* history = history_create(data, SIZE);

    data[5] = 7;
    assert(history_add(history));

    data[6] = 8;
    history_undo(history);
    expect(0);

    // The uncommitted byte is gone with the step above it.
    assert(data[6] == 0);

    history_redo(history);
    assert(data[5] == 7 && data[6] == 0);

    // Redo at the newest step restores the committed bytes too.
    data[6] = 9;
    history_redo(history);
    assert(data[5] == 7 && data[6] == 0);

    history_delete(history);
    assert(alloc_live == 0);
    puts("uncommitted ok");
}

// A buffer of one byte and one of none still behave.
static void test_small(void)
{
    u8 one = 0;
    History* history = history_create(&one, sizeof one);
    assert(history);

    one = 1;
    assert(history_add(history));
    history_undo(history);
    assert(one == 0);
    history_redo(history);
    assert(one == 1);
    history_delete(history);

    history = history_create(NULL, 0);
    assert(history);
    assert(!history_add(history));
    history_undo(history);
    history_redo(history);
    history_delete(history);

    assert(alloc_live == 0);
    puts("small ok");
}

// A commit with nothing to record keeps the redo branch it was called on.
static void test_noop_branch(void)
{
    reset(0);
    History* history = history_create(data, SIZE);

    data[0] = 1;
    assert(history_add(history));
    data[1] = 2;
    assert(history_add(history));

    history_undo(history);
    assert(!history_add(history));
    history_redo(history);
    assert(data[0] == 1 && data[1] == 2);

    // And a no-op commit adds no step of its own.
    history_undo(history);
    assert(!history_add(history));
    history_undo(history);
    expect(0);

    history_delete(history);
    assert(alloc_live == 0);
    puts("noop branch ok");
}

// A failed allocation is never a crash: create cleans up, and a step that
// cannot be stored is not recorded, leaving the history consistent.
static void test_alloc_fail(void)
{
    reset(0);

    for(int call = 1; call <= 3; call++)
    {
        alloc_reset();
        alloc_fail = call;
        assert(history_create(data, SIZE) == NULL);
        assert(alloc_live == 0);
    }

    alloc_reset();
    History* history = history_create(data, SIZE);
    assert(history);

    data[0] = 1;
    alloc_fail = alloc_calls + 1;
    assert(!history_add(history));
    assert(alloc_live == 3);

    data[1] = 2;
    alloc_fail = alloc_calls + 2;
    assert(!history_add(history));
    assert(alloc_live == 3);

    // The next commit records both changes as one step.
    alloc_fail = -1;
    assert(history_add(history));
    history_undo(history);
    expect(0);

    history_delete(history);
    assert(alloc_live == 0);
    puts("alloc fail ok");
}

// Every entry point takes NULL, so an editor whose history failed to allocate
// still runs.
static void test_null(void)
{
    assert(!history_add(NULL));
    history_undo(NULL);
    history_redo(NULL);
    history_delete(NULL);

    puts("null ok");
}

static u32 rng_state = 0x2545f491;

static u32 rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

// Random edits, undos and redos against full snapshots: the history has to
// hand back the same bytes the snapshots say it owes, whatever the order.
static void test_random(void)
{
    enum
    {
        OPS = 200000,
        STEPS = OPS + 1
    };

    // Every op that commits is a new step, and one that commits after an undo
    // drops the branch it replaced: OPS is the most snapshots the model holds.
    u8(*snapshots)[SIZE] = malloc(sizeof(*snapshots) * STEPS);
    assert(snapshots);

    for(u32 i = 0; i < SIZE; i++)
        data[i] = (u8)rng();

    u32 count = 1, current = 0;
    memcpy(snapshots[0], data, SIZE);

    History* history = history_create(data, SIZE);

    for(u32 op = 0; op < OPS; op++)
    {
        switch(rng() % 10)
        {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            {
                // Change a random span, edges included as often as not.
                u32 start = rng() % SIZE;
                u32 len = 1 + rng() % (SIZE - start);
                for(u32 i = 0; i < len; i++)
                    data[start + i] = (u8)rng();

                bool changed = memcmp(data, snapshots[current], SIZE) != 0;
                assert(history_add(history) == changed);

                if(changed)
                {
                    count = current + 1;
                    memcpy(snapshots[count], data, SIZE);
                    current = count++;
                }
                break;
            }

            case 5:
            case 6:
            case 7:
                // Uncommitted edits are dropped: undo lands on a commit.
                data[rng() % SIZE] ^= (u8)rng();
                if(current > 0)
                    current--;
                history_undo(history);
                assert(memcmp(data, snapshots[current], SIZE) == 0);
                break;

            default:
                data[rng() % SIZE] ^= (u8)rng();
                if(current + 1 < count)
                    current++;
                history_redo(history);
                assert(memcmp(data, snapshots[current], SIZE) == 0);
                break;
        }
    }

    free(snapshots);
    history_delete(history);
    assert(alloc_live == 0);
    puts("random ok");
}

int main(void)
{
    test_no_change();
    test_edges();
    test_sequence();
    test_branch();
    test_noop_branch();
    test_uncommitted();
    test_small();
    test_alloc_fail();
    test_null();
    test_random();

    puts("history checks passed");
    return 0;
}
