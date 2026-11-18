/* The filesystem's provider contract, without the studio: one scenario table
 * runs against the local provider on a temporary root and against a held fake
 * remote provider whose completions come after the call, plus the site
 * provider's wire answers over a held fake network, the cart cover and the one
 * outstanding operation. A call into studio.c would fail to link here.
 *
 * Target fs-test in cmake/tests.cmake; `ctest -R fs-test` runs it.
 */
#include "studio/fs.h"
#include "studio/fs_internal.h"
#include "studio/net.h"
#include "studio/paths.h"
#include "studio/system.h"
#include "cart.h"
#include "tools.h"
#include "ext/gif.h"
#include "core/core.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// The platform the filesystem reaches for; the test opens nothing.
static char OpenPath[TICNAME_MAX];

void tic_sys_open_path(const char* path)
{
    snprintf(OpenPath, sizeof OpenPath, "%s", path);
}

// The test's network: one held request at a time, answered by the test; no socket.
static net_get_callback NetCallback;
static void* NetCalldata;
static char NetUrl[TICNAME_MAX];

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    (void)net;
    assert(!NetCallback);

    NetCallback = callback;
    NetCalldata = calldata;
    snprintf(NetUrl, sizeof NetUrl, "%s", url);
}

// The shelf's requests go through the primitive: one held request, answered by
// the test; no socket.
typedef struct
{
    bool held;
    net_request_callback callback;
    void* calldata;
    net_method method;
    char url[TICNAME_MAX * 2];
    char authorization[128];
    u8 body[TICNAME_MAX];
    s32 bodySize;
} HeldRequest;

static HeldRequest Held;

void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    (void)net;
    assert(!Held.held);

    Held.held = true;
    Held.callback = callback;
    Held.calldata = calldata;
    Held.method = method;
    Held.authorization[0] = '\0';
    Held.bodySize = 0;

    snprintf(Held.url, sizeof Held.url, "%s", url);

    for(s32 i = 0; i < headersCount; i++)
        if(strcmp(headers[i].name, "Authorization") == 0)
            snprintf(Held.authorization, sizeof Held.authorization, "%s", headers[i].value);

    if(body && bodySize > 0 && bodySize <= (s32)sizeof Held.body)
    {
        memcpy(Held.body, body, bodySize);
        Held.bodySize = bodySize;
    }
}

static void answerRequestBytes(s32 status, const u8* body, s32 size)
{
    assert(Held.held);

    net_request_callback callback = Held.callback;
    void* calldata = Held.calldata;
    char url[TICNAME_MAX * 2];
    snprintf(url, sizeof url, "%s", Held.url);
    Held.held = false;

    net_request_data answer = {
        .type = net_request_done,
        .done = {.status = status, .size = size, .data = (u8*)body},
        .calldata = calldata,
        .url = url,
    };

    callback(&answer);
}

static void answerRequest(s32 status, const char* json)
{
    answerRequestBytes(status, (const u8*)json, (s32)strlen(json));
}

static void answerRequestError(s32 code)
{
    assert(Held.held);

    net_request_callback callback = Held.callback;
    void* calldata = Held.calldata;
    char url[TICNAME_MAX * 2];
    snprintf(url, sizeof url, "%s", Held.url);
    Held.held = false;

    net_request_data answer = {
        .type = net_request_error,
        .error = {.code = code},
        .calldata = calldata,
        .url = url,
    };

    callback(&answer);
}

// The account the shelf's remote inputs read: the token and the member's state.
typedef struct
{
    bool signedIn;
    char token[64];
    s32 unauthorizedCalls;
    char unauthorizedToken[64];
} TestAccount;

static bool accountSignedIn(void* data)
{
    return ((TestAccount*)data)->signedIn;
}

static const char* accountToken(void* data)
{
    return ((TestAccount*)data)->token;
}

static void accountUnauthorized(void* data, const char* token)
{
    TestAccount* account = data;
    account->unauthorizedCalls++;
    snprintf(account->unauthorizedToken, sizeof account->unauthorizedToken, "%s", token ? token : "");
}

// The name the filesystem remembers from a load: the fake's own.
typedef struct
{
    bool done;
    fs_result result;
    char message[TICNAME_MAX];
    char name[TICNAME_MAX];
    u8 bytes[TICNAME_MAX];
    s32 size;
    s32 count;
    fs_item items[8];
    char itemNames[8][TICNAME_MAX];
    char itemTitles[8][TICNAME_MAX];
    fs_item item;
    fs_usage usage;
    bool hasUsage;
} Answer;

static void copyMessage(Answer* answer, fs_result result)
{
    answer->done = true;
    answer->result = result;
    snprintf(answer->message, sizeof answer->message, "%s", result.message ? result.message : "");
}

static bool onItem(const fs_item* item, void* data)
{
    Answer* answer = data;

    if(answer->count < (s32)COUNT_OF(answer->items))
    {
        fs_item* copy = &answer->items[answer->count];
        *copy = *item;

        snprintf(answer->itemNames[answer->count], TICNAME_MAX, "%s", item->name ? item->name : "");
        copy->name = answer->itemNames[answer->count];

        if(item->title)
        {
            snprintf(answer->itemTitles[answer->count], TICNAME_MAX, "%s", item->title);
            copy->title = answer->itemTitles[answer->count];
        }
        else
            copy->title = NULL;

        answer->count++;
    }

    return true;
}

static void onListDone(const fs_usage* usage, fs_result result, void* data)
{
    Answer* answer = data;
    copyMessage(answer, result);

    if(usage)
    {
        answer->usage = *usage;
        answer->hasUsage = usage->has_usage;
    }
}

static void onLoad(const char* name, const u8* bytes, s32 size, fs_result result, void* data)
{
    Answer* answer = data;
    copyMessage(answer, result);

    if(result.code == FS_OK)
    {
        snprintf(answer->name, sizeof answer->name, "%s", name ? name : "");
        answer->size = MIN(size, (s32)sizeof answer->bytes);
        memcpy(answer->bytes, bytes, answer->size);
    }
}

static void onSave(const char* name, fs_result result, void* data)
{
    Answer* answer = data;
    copyMessage(answer, result);

    if(name)
        snprintf(answer->name, sizeof answer->name, "%s", name);
}

static void onDone(fs_result result, void* data)
{
    copyMessage(data, result);
}

static void onStat(const fs_item* item, fs_result result, void* data)
{
    Answer* answer = data;
    copyMessage(answer, result);

    if(result.code == FS_OK && item)
    {
        answer->item = *item;
        snprintf(answer->name, sizeof answer->name, "%s", item->name ? item->name : "");
        answer->item.name = answer->name;
        answer->item.title = NULL;
    }
}

// The cover the provider answers: a rendered cover needs a buffer of its own.
static u8 CoverBytes[96 * 1024];

static void onCover(const u8* bytes, s32 size, fs_result result, void* data)
{
    Answer* answer = data;
    copyMessage(answer, result);

    if(result.code == FS_OK)
    {
        assert(size <= (s32)sizeof CoverBytes);

        memcpy(CoverBytes, bytes, size);
        answer->size = size;
    }
}

static bool fakePump(void);

// The call the table makes and how it settles: local inline, remote after a pump.
typedef struct
{
    tic_fs* fs;
    bool remote;
} Contract;

static void settle(const Contract* contract)
{
    if(contract->remote)
        while(fakePump())
            ;
}

static void answerReady(const Contract* contract, const Answer* answer)
{
    if(contract->remote)
        assert(!answer->done);
    else
        assert(answer->done);

    settle(contract);

    assert(answer->done);
}

static Answer listPath(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_list(contract->fs, path, onItem, onListDone, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer loadPath(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_load(contract->fs, path, onLoad, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer savePath(Contract* contract, const char* path, const void* data, s32 size)
{
    Answer answer = {0};
    tic_fs_save(contract->fs, path, data, size, onSave, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer deletePath(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_delete(contract->fs, path, onDone, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer statPath(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_stat(contract->fs, path, onStat, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer makeDir(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_mkdir(contract->fs, path, onDone, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer openFolder(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_openfolder(contract->fs, path, onDone, &answer);
    answerReady(contract, &answer);
    return answer;
}

static Answer coverPath(Contract* contract, const char* path)
{
    Answer answer = {0};
    tic_fs_cover(contract->fs, path, onCover, &answer);
    answerReady(contract, &answer);
    return answer;
}

static bool hasItem(const Answer* answer, const char* name)
{
    for(s32 i = 0; i < answer->count; i++)
        if(strcmp(answer->items[i].name, name) == 0)
            return true;

    return false;
}

// The held fake remote provider: an in-memory store whose answers wait for a pump.
enum
{
    FakeMaxEntries = 32
};

typedef struct
{
    char name[TICNAME_MAX];
    u8* data;
    s32 size;
    bool dir;
} FakeEntry;

typedef enum
{
    FakeList,
    FakeLoad,
    FakeSave,
    FakeDelete,
    FakeStat,
    FakeMkdir,
    FakeOpen,
    FakeCover,
} FakeKind;

typedef struct FakeOp
{
    struct FakeOp* next;
    FakeKind kind;
    char path[TICNAME_MAX];
    u8* bytes;
    s32 size;

    fs_list_item_cb onItem;
    fs_list_done_cb onListDone;
    fs_load_done_cb onLoad;
    fs_save_done_cb onSave;
    fs_done_cb onDone;
    fs_stat_done_cb onStat;
    fs_cover_done_cb onCover;
    void* data;
} FakeOp;

static struct
{
    FakeEntry entries[FakeMaxEntries];
    s32 count;

    FakeOp* head;
    FakeOp* tail;
} Fake;

static void fakeReset(void)
{
    for(s32 i = 0; i < Fake.count; i++)
        free(Fake.entries[i].data);

    memset(&Fake, 0, sizeof Fake);
}

static FakeOp* fakePush(FakeKind kind, const char* path, void* data)
{
    FakeOp* op = calloc(1, sizeof(FakeOp));
    assert(op);

    op->kind = kind;
    snprintf(op->path, sizeof op->path, "%s", path);
    op->data = data;

    if(Fake.tail)
        Fake.tail->next = op;
    else
        Fake.head = op;

    Fake.tail = op;

    return op;
}

// What the mount answers before its store: an asked-for status with its message.
static fs_result fakeSpecial(const char* path)
{
    if(strcmp(path, "denied") == 0)
        return (fs_result){FS_DENIED, "the mount refused it"};
    if(strcmp(path, "session") == 0)
        return (fs_result){FS_SESSION, "the session ended"};
    if(strcmp(path, "offline") == 0)
        return (fs_result){FS_OFFLINE, "the mount is offline"};
    if(strcmp(path, "quota") == 0)
        return (fs_result){FS_QUOTA, "the mount is full"};
    if(strcmp(path, "unsupported") == 0)
        return (fs_result){FS_UNSUPPORTED, "not on this mount"};

    return (fs_result){FS_OK, NULL};
}

static const char* baseName(const char* path)
{
    const char* ptr = path + strlen(path);

    while(ptr > path && ptr[-1] != '/')
        --ptr;

    return ptr;
}

static FakeEntry* fakeFind(const char* path)
{
    for(s32 i = 0; i < Fake.count; i++)
        if(strcmp(Fake.entries[i].name, path) == 0)
            return &Fake.entries[i];

    return NULL;
}

static bool fakeIsDir(const char* path)
{
    if(!*path)
        return true;

    FakeEntry* entry = fakeFind(path);
    return entry && entry->dir;
}

static char* fakeParent(const char* path, char* out, s32 size)
{
    const char* slash = strrchr(path, '/');

    if(slash)
        snprintf(out, size, "%.*s", (s32)(slash - path), path);
    else
        snprintf(out, size, "%s", "");

    return out;
}

static void fakeListDone(FakeOp* op, fs_result result)
{
    if(result.code == FS_OK)
    {
        char parent[TICNAME_MAX];

        for(s32 i = 0; i < Fake.count && result.code == FS_OK; i++)
        {
            FakeEntry* entry = &Fake.entries[i];
            fakeParent(entry->name, parent, sizeof parent);

            if(strcmp(parent, op->path) == 0)
            {
                fs_item item = {
                    .name = baseName(entry->name),
                    .title = NULL,
                    .dir = entry->dir,
                    .size = entry->dir ? -1 : entry->size,
                    .date = 0,
                };

                if(!op->onItem(&item, op->data))
                    break;
            }
        }
    }

    op->onListDone(NULL, result, op->data);
}

static void fakeServe(FakeOp* op)
{
    fs_result result = fakeSpecial(op->path);

    if(result.code != FS_OK && op->kind != FakeOpen)
    {
        switch(op->kind)
        {
            case FakeList: fakeListDone(op, result); break;
            case FakeLoad: op->onLoad(NULL, NULL, 0, result, op->data); break;
            case FakeSave: op->onSave(NULL, result, op->data); break;
            case FakeDelete:
            case FakeMkdir:
                op->onDone(result, op->data);
                break;
            case FakeStat: op->onStat(NULL, result, op->data); break;
            case FakeCover: op->onCover(NULL, 0, result, op->data); break;
            default: break;
        }

        return;
    }

    switch(op->kind)
    {
        case FakeList:
            if(!fakeIsDir(op->path))
                fakeListDone(op, (fs_result){FS_NOT_FOUND, "no such folder"});
            else
                fakeListDone(op, (fs_result){FS_OK, NULL});
            break;

        case FakeLoad:
        {
            FakeEntry* entry = fakeFind(op->path);

            if(entry && !entry->dir)
                op->onLoad(baseName(entry->name), entry->data, entry->size, (fs_result){FS_OK, NULL}, op->data);
            else
                op->onLoad(NULL, NULL, 0, (fs_result){FS_NOT_FOUND, "no such file"}, op->data);
            break;
        }

        case FakeSave:
        {
            char parent[TICNAME_MAX];
            fakeParent(op->path, parent, sizeof parent);

            if(!fakeIsDir(parent))
            {
                op->onSave(NULL, (fs_result){FS_NOT_FOUND, "no such folder"}, op->data);
                break;
            }

            FakeEntry* entry = fakeFind(op->path);

            if(!entry && Fake.count < FakeMaxEntries)
            {
                entry = &Fake.entries[Fake.count++];
                memset(entry, 0, sizeof *entry);
                snprintf(entry->name, sizeof entry->name, "%s", op->path);
            }

            if(!entry || entry->dir)
            {
                op->onSave(NULL, (fs_result){FS_DENIED, "not a file"}, op->data);
                break;
            }

            free(entry->data);
            entry->data = op->bytes;
            entry->size = op->size;
            op->bytes = NULL;

            op->onSave(baseName(op->path), (fs_result){FS_OK, NULL}, op->data);
            break;
        }

        case FakeDelete:
        {
            FakeEntry* entry = fakeFind(op->path);

            if(!entry)
                op->onDone((fs_result){FS_NOT_FOUND, "no such file"}, op->data);
            else
            {
                bool used = false;

                if(entry->dir && *entry->name)
                {
                    char parent[TICNAME_MAX];
                    char own[TICNAME_MAX];
                    snprintf(own, sizeof own, "%s", entry->name);

                    for(s32 i = 0; i < Fake.count; i++)
                    {
                        fakeParent(Fake.entries[i].name, parent, sizeof parent);

                        if(strcmp(parent, own) == 0)
                            used = true;
                    }
                }

                if(used)
                    op->onDone((fs_result){FS_IO, "the folder is not empty"}, op->data);
                else
                {
                    free(entry->data);
                    *entry = Fake.entries[--Fake.count];
                    op->onDone((fs_result){FS_OK, NULL}, op->data);
                }
            }
            break;
        }

        case FakeStat:
        {
            FakeEntry* entry = (*op->path) ? fakeFind(op->path) : NULL;
            bool root = !*op->path;

            if(!entry && !root)
                op->onStat(NULL, (fs_result){FS_NOT_FOUND, "no such file"}, op->data);
            else
            {
                char name[TICNAME_MAX];
                snprintf(name, sizeof name, "%s", root ? "" : baseName(op->path));

                fs_item item = {
                    .name = name,
                    .title = NULL,
                    .dir = root || entry->dir,
                    .size = root || entry->dir ? -1 : entry->size,
                    .date = 0,
                };

                op->onStat(&item, (fs_result){FS_OK, NULL}, op->data);
            }
            break;
        }

        case FakeMkdir:
        {
            char parent[TICNAME_MAX];
            fakeParent(op->path, parent, sizeof parent);

            if(fakeFind(op->path) || !fakeIsDir(parent))
                op->onDone((fs_result){FS_IO, "could not create the folder"}, op->data);
            else if(Fake.count >= FakeMaxEntries)
                op->onDone((fs_result){FS_IO, "the mount is full"}, op->data);
            else
            {
                FakeEntry* entry = &Fake.entries[Fake.count++];
                memset(entry, 0, sizeof *entry);
                snprintf(entry->name, sizeof entry->name, "%s", op->path);
                entry->dir = true;

                op->onDone((fs_result){FS_OK, NULL}, op->data);
            }
            break;
        }

        case FakeOpen:
            op->onDone((fs_result){FS_UNSUPPORTED, "not on this mount"}, op->data);
            break;

        case FakeCover:
            op->onCover(NULL, 0, (fs_result){FS_NOT_FOUND, "no cover"}, op->data);
            break;
    }
}

static bool fakePump(void)
{
    FakeOp* op = Fake.head;

    if(!op)
        return false;

    Fake.head = op->next;

    if(!Fake.head)
        Fake.tail = NULL;

    fakeServe(op);

    free(op->bytes);
    free(op);

    return true;
}

static void fakeList(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data)
{
    (void)fs;
    FakeOp* op = fakePush(FakeList, path, data);
    op->onItem = onItem;
    op->onListDone = onDone;
}

static void fakeLoad(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeLoad, path, data)->onLoad = onDone;
}

static void fakeSave(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data)
{
    (void)fs;
    FakeOp* op = fakePush(FakeSave, path, data);
    op->onSave = onDone;
    op->bytes = malloc(size ? size : 1);
    op->size = size;

    if(size)
        memcpy(op->bytes, bytes, size);
}

static void fakeDelete(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeDelete, path, data)->onDone = onDone;
}

static void fakeStat(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeStat, path, data)->onStat = onDone;
}

static void fakeMkdir(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeMkdir, path, data)->onDone = onDone;
}

static void fakeOpen(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeOpen, path, data)->onDone = onDone;
}

static void fakeCover(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeCover, path, data)->onCover = onDone;
}

static const fs_mount_ops FakeMount = {
    .list = fakeList,
    .load = fakeLoad,
    .save = fakeSave,
    .delete = fakeDelete,
    .stat = fakeStat,
    .mkdir = fakeMkdir,
    .openfolder = fakeOpen,
    .cover = fakeCover,
};

static char* makeRoot(const char* name)
{
    static char root[TICNAME_MAX];

    snprintf(root, sizeof root, "/tmp/%s-XXXXXX", name);
    assert(mkdtemp(root));

    return root;
}

// The same scenarios against either provider; only the mount-specific answers differ.
static void runContract(tic_fs* fs, bool remote)
{
    Contract contract = {fs, remote};
    static const u8 Payload[] = {0xde, 0xad, 0xbe, 0xef};

    {
        Answer list = listPath(&contract, "");
        assert(list.result.code == FS_OK);
        assert(!list.hasUsage || !list.usage.has_usage);
    }

    {
        // The working directory itself is a directory on either mount.
        Answer stat = statPath(&contract, "");
        assert(stat.result.code == FS_OK);
        assert(stat.item.dir);
    }

    {
        Answer save = savePath(&contract, "contract.tic", Payload, sizeof Payload);
        assert(save.result.code == FS_OK);
        assert(strcmp(save.name, "contract.tic") == 0);
    }

    {
        Answer stat = statPath(&contract, "contract.tic");
        assert(stat.result.code == FS_OK);
        assert(!stat.item.dir);
        assert(stat.item.size == (s64)sizeof Payload);
        assert(strcmp(stat.item.name, "contract.tic") == 0);
    }

    {
        Answer load = loadPath(&contract, "contract.tic");
        assert(load.result.code == FS_OK);
        assert(strcmp(load.name, "contract.tic") == 0);
        assert(load.size == (s32)sizeof Payload);
        assert(memcmp(load.bytes, Payload, sizeof Payload) == 0);
    }

    {
        Answer list = listPath(&contract, "");
        assert(list.result.code == FS_OK);
        // The listing is a snapshot by name, the display title is optional.
        assert(hasItem(&list, "contract.tic"));
        for(s32 i = 0; i < list.count; i++)
            if(strcmp(list.items[i].name, "contract.tic") == 0 && list.items[i].title)
                assert(strcmp(list.items[i].title, "contract.tic") == 0);
    }

    {
        Answer mkdir = makeDir(&contract, "sub");
        assert(mkdir.result.code == FS_OK);
    }

    {
        Answer stat = statPath(&contract, "sub");
        assert(stat.result.code == FS_OK);
        assert(stat.item.dir);
    }

    {
        Answer list = listPath(&contract, "sub");
        assert(list.result.code == FS_OK);
        assert(list.count == 0);
    }

    {
        // A folder with a file in it is not deleted, and that is an I/O refusal.
        Answer save = savePath(&contract, "sub/keep.tic", Payload, sizeof Payload);
        assert(save.result.code == FS_OK);

        Answer del = deletePath(&contract, "sub");
        assert(del.result.code == FS_IO);

        del = deletePath(&contract, "sub/keep.tic");
        assert(del.result.code == FS_OK);
    }

    {
        // A save into a folder that is not there fails without a new file.
        Answer save = savePath(&contract, "missing/contract.tic", Payload, sizeof Payload);
        assert(save.result.code != FS_OK);
    }

    {
        Answer stat = statPath(&contract, "missing/contract.tic");
        assert(stat.result.code == FS_NOT_FOUND);
    }

    {
        Answer del = deletePath(&contract, "contract.tic");
        assert(del.result.code == FS_OK);
    }

    {
        Answer load = loadPath(&contract, "contract.tic");
        assert(load.result.code == FS_NOT_FOUND);
    }

    {
        Answer stat = statPath(&contract, "contract.tic");
        assert(stat.result.code == FS_NOT_FOUND);
    }

    {
        Answer list = listPath(&contract, "no-such-folder");
        assert(list.result.code == FS_NOT_FOUND);
    }

    {
        Answer del = deletePath(&contract, "contract.tic");
        assert(del.result.code == FS_NOT_FOUND);
    }

    {
        Answer mkdir = makeDir(&contract, "sub");
        assert(mkdir.result.code == FS_IO);
    }

    {
        Answer folder = openFolder(&contract, "");
        assert(folder.result.code == (remote ? FS_UNSUPPORTED : FS_OK));
    }

    if(remote)
    {
        // Every status the table names, each with the mount's own message.
        struct
        {
            const char* path;
            fs_code code;
            const char* message;
        } Errors[] = {
            {"denied", FS_DENIED, "the mount refused it"},
            {"session", FS_SESSION, "the session ended"},
            {"offline", FS_OFFLINE, "the mount is offline"},
            {"unsupported", FS_UNSUPPORTED, "not on this mount"},
        };

        for(s32 i = 0; i < (s32)COUNT_OF(Errors); i++)
        {
            Answer answer = loadPath(&contract, Errors[i].path);

            assert(answer.result.code == Errors[i].code);
            assert(strcmp(answer.message, Errors[i].message) == 0);
        }

        {
            static const u8 Data[] = {1};
            Answer answer = savePath(&contract, "quota", Data, sizeof Data);

            assert(answer.result.code == FS_QUOTA);
            assert(strcmp(answer.message, "the mount is full") == 0);
        }

        Answer folder = openFolder(&contract, "unsupported");
        assert(folder.result.code == FS_UNSUPPORTED);

        Answer cover = coverPath(&contract, "contract.tic");
        assert(cover.result.code == FS_NOT_FOUND);
    }
    else
    {
        // The local mount answers the status it can produce: no read permission.
        static const u8 Data[] = {4};
        assert(savePath(&contract, "denied.tic", Data, sizeof Data).result.code == FS_OK);

        char path[TICNAME_MAX];
        snprintf(path, sizeof path, "%s", tic_fs_path(fs, "denied.tic"));
        assert(chmod(path, 0) == 0);

        Answer load = loadPath(&contract, "denied.tic");
        assert(load.result.code == FS_DENIED);

        assert(chmod(path, 0600) == 0);
        assert(deletePath(&contract, "denied.tic").result.code == FS_OK);
    }
}

static void testLocalContract(void)
{
    char* root = makeRoot("tic80-fs-local");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    OpenPath[0] = '\0';
    runContract(fs, false);

    assert(OpenPath[0]);
    assert(strstr(OpenPath, root) == OpenPath);

    tic_fs_deldir(fs, "sub");
    tic_fs_deldir(fs, "");

    puts("local contract ok");
}

// A host path the process owns is not a mount path: the unified calls stay inside.
static void testHostPathConfined(void)
{
    char* root = makeRoot("tic80-fs-confine");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    char outsideDir[] = "/tmp/tic80-fs-outside-XXXXXX";
    assert(mkdtemp(outsideDir));

    char outside[TICNAME_MAX];
    snprintf(outside, sizeof outside, "%s/outside.txt", outsideDir);

    static const u8 Contents[] = {0x5a, 0x5b, 0x5c};
    assert(fs_write(outside, Contents, sizeof Contents));

    Contract contract = {fs, false};
    static const u8 Payload[] = {1, 2, 3};

    Answer load = loadPath(&contract, outside);
    assert(load.result.code == FS_NOT_FOUND);

    Answer save = savePath(&contract, outside, Payload, sizeof Payload);
    assert(save.result.code != FS_OK);

    Answer del = deletePath(&contract, outside);
    assert(del.result.code == FS_NOT_FOUND);

    {
        s32 size = 0;
        void* data = fs_read(outside, &size);
        assert(data);
        assert(size == (s32)sizeof Contents);
        assert(memcmp(data, Contents, sizeof Contents) == 0);
        free(data);
    }

    assert(remove(outside) == 0);
    assert(rmdir(outsideDir) == 0);
    tic_fs_deldir(fs, "");

    puts("host paths confined ok");
}

// A nested load answers its name as asked, so a save writes the same file.
static void testLocalLoadName(void)
{
    char* root = makeRoot("tic80-fs-name");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    Contract contract = {fs, false};
    static const u8 First[] = {0x11, 0x22};
    static const u8 Second[] = {0x33, 0x44};

    assert(makeDir(&contract, "sub").result.code == FS_OK);
    assert(savePath(&contract, "sub/foo.tic", First, sizeof First).result.code == FS_OK);

    Answer load = loadPath(&contract, "sub/foo.tic");
    assert(load.result.code == FS_OK);
    assert(strcmp(load.name, "sub/foo.tic") == 0);
    assert(strcmp(tic_fs_path(fs, load.name), tic_fs_path(fs, "sub/foo.tic")) == 0);

    Answer save = savePath(&contract, load.name, Second, sizeof Second);
    assert(save.result.code == FS_OK);

    Answer again = loadPath(&contract, "sub/foo.tic");
    assert(again.result.code == FS_OK);
    assert(again.size == (s32)sizeof Second);
    assert(memcmp(again.bytes, Second, sizeof Second) == 0);
    assert(!fs_exists(tic_fs_path(fs, "foo.tic")));

    tic_fs_delfile(fs, "sub/foo.tic");
    tic_fs_deldir(fs, "sub");
    tic_fs_deldir(fs, "");

    puts("local load name ok");
}

// The local mount delivers its own order: folders first, then names without case.
static void testLocalListOrder(void)
{
    char* root = makeRoot("tic80-fs-order");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    Contract contract = {fs, false};
    static const u8 Data[] = {0};

    assert(savePath(&contract, "b.tic", Data, sizeof Data).result.code == FS_OK);
    assert(savePath(&contract, "A.tic", Data, sizeof Data).result.code == FS_OK);
    assert(makeDir(&contract, "sub").result.code == FS_OK);

    Answer list = listPath(&contract, "");
    assert(list.result.code == FS_OK);

    s32 sub = -1, upper = -1, lower = -1, firstFile = list.count;

    for(s32 i = 0; i < list.count; i++)
    {
        if(strcmp(list.items[i].name, "sub") == 0)
            sub = i;
        else if(strcmp(list.items[i].name, "A.tic") == 0)
            upper = i;
        else if(strcmp(list.items[i].name, "b.tic") == 0)
            lower = i;

        if(!list.items[i].dir && firstFile == list.count)
            firstFile = i;
    }

    assert(sub >= 0 && upper >= 0 && lower >= 0);
    assert(sub < firstFile);
    assert(upper < lower);

    for(s32 i = firstFile; i < list.count; i++)
        assert(!list.items[i].dir);

    // A local item has no site token.
    for(s32 i = 0; i < list.count; i++)
        assert(list.items[i].token == 0);

    tic_fs_delfile(fs, "A.tic");
    tic_fs_delfile(fs, "b.tic");
    tic_fs_deldir(fs, "sub");
    tic_fs_deldir(fs, "");

    puts("local list order ok");
}

static void testFakeRemoteContract(void)
{
    char* root = makeRoot("tic80-fs-remote");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    fakeReset();
    tic_fs_set_mount(fs, &FakeMount);

    runContract(fs, true);

    assert(!Fake.head);
    fakeReset();

    tic_fs_deldir(fs, "");
    puts("remote contract ok");
}

// A second operation while one waits, and a completion that starts the next one.
typedef struct
{
    Contract* contract;
    Answer load;
    Answer stat;
} Chain;

static void chainLoad(const char* name, const u8* bytes, s32 size, fs_result result, void* data)
{
    (void)name;
    (void)bytes;
    (void)size;

    Chain* chain = data;
    copyMessage(&chain->load, result);

    tic_fs_stat(chain->contract->fs, "contract.tic", onStat, &chain->stat);
}

static void testHeldOperations(void)
{
    char* root = makeRoot("tic80-fs-held");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    fakeReset();
    tic_fs_set_mount(fs, &FakeMount);

    Contract contract = {fs, true};
    static const u8 Payload[] = {0xaa, 0xbb};

    {
        Answer save = savePath(&contract, "contract.tic", Payload, sizeof Payload);
        assert(save.result.code == FS_OK);
    }

    {
        // Two outstanding operations keep their own answers in the other order.
        Answer load = {0};
        Answer save = {0};

        tic_fs_load(fs, "contract.tic", onLoad, &load);
        tic_fs_save(fs, "other.tic", Payload, sizeof Payload, onSave, &save);

        assert(!load.done);
        assert(!save.done);

        assert(fakePump());
        assert(load.done);
        assert(!save.done);

        assert(fakePump());
        assert(save.done);
        assert(strcmp(save.name, "other.tic") == 0);
    }

    {
        // The completion starts the next operation: inline local, held remote.
        Chain chain = {&contract};
        tic_fs_load(fs, "contract.tic", chainLoad, &chain);

        assert(!chain.load.done);
        assert(!chain.stat.done);

        while(fakePump())
            ;

        assert(chain.load.done);
        assert(chain.stat.done);
        assert(chain.stat.item.dir == false);
    }

    fakeReset();
    tic_fs_deldir(fs, "");
    puts("held operations ok");
}

// The site provider's wire shape: listing, lookup, cache, cover and status mapping.
static const char SiteListing[] =
    "{\"folders\":[{\"name\":\"games\"}],"
    "\"files\":[{\"name\":\"Demo\",\"hash\":\"abc\",\"filename\":\"demo.tic\",\"id\":7}]}";

static const char OtherListing[] = "{\"folders\":[],\"files\":[]}";

static bool netCallbackPending(void);

typedef struct
{
    char root[TICNAME_MAX];
    tic_fs* fs;
} Site;

static void siteSetup(Site* site)
{
    snprintf(site->root, sizeof site->root, "%s", makeRoot("tic80-fs-site"));
    site->fs = tic_fs_create(site->root, (tic_net*)site);
    assert(site->fs);

    assert(tic_fs_makedir(site->fs, TIC_LOCAL) == 0);
    assert(tic_fs_makedir(site->fs, TIC_LOCAL_VERSION) == 0);
    assert(tic_fs_makedir(site->fs, TIC_CACHE) == 0);

    tic_fs_changedir(site->fs, TIC_HOST);
    assert(!netCallbackPending());
}

static bool netCallbackPending(void)
{
    return NetCallback != NULL;
}

static void siteTeardown(Site* site)
{
    NetCallback = NULL;

    tic_fs_homedir(site->fs);
    tic_fs_delfile(site->fs, TIC_CACHE "abc.tic");
    tic_fs_delfile(site->fs, TIC_CACHE "abc.gif");
    tic_fs_deldir(site->fs, TIC_CACHE);
    tic_fs_deldir(site->fs, TIC_LOCAL_VERSION);
    tic_fs_deldir(site->fs, TIC_LOCAL);
    tic_fs_deldir(site->fs, "");
}

static void answerData(const void* data, s32 size)
{
    assert(NetCallback);

    net_get_callback callback = NetCallback;
    void* calldata = NetCalldata;
    NetCallback = NULL;

    net_get_data answer = {
        .type = net_get_done,
        .done = {.size = size, .data = (u8*)data},
        .calldata = calldata,
        .url = NetUrl,
    };

    callback(&answer);
}

static void answerJson(const char* json)
{
    answerData(json, (s32)strlen(json));
}

static void answerError(s32 code)
{
    assert(NetCallback);

    net_get_callback callback = NetCallback;
    void* calldata = NetCalldata;
    NetCallback = NULL;

    net_get_data answer = {
        .type = net_get_error,
        .error = {.code = code},
        .calldata = calldata,
        .url = NetUrl,
    };

    callback(&answer);
}

static void testSiteListing(void)
{
    Site site;
    siteSetup(&site);

    {
        Answer list = {0};

        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);
        assert(!list.done);

        answerJson(SiteListing);
        assert(list.done);
        assert(list.result.code == FS_OK);
        assert(list.count == 2);
        assert(strcmp(list.items[0].name, "games") == 0 && list.items[0].dir);
        assert(strcmp(list.items[1].name, "demo.tic") == 0 && !list.items[1].dir);
        assert(strcmp(list.items[1].title, "Demo") == 0);

        // The site's own id travels as an opaque token; a folder has none.
        assert(list.items[0].token == 0);
        assert(list.items[1].token == 7);

        assert(!list.hasUsage);
    }

    {
        // The load finds the hash in the listing it just read: only the cart is fetched.
        Answer load = {0};
        static const u8 Cart[] = {0x54, 0x49, 0x43};

        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        assert(strcmp(NetUrl, "/cart/abc/demo.tic") == 0);
        answerData(Cart, sizeof Cart);

        assert(load.done);
        assert(load.result.code == FS_OK);
        assert(strcmp(load.name, "demo.tic") == 0);
        assert(load.size == (s32)sizeof Cart);

        // The provider keeps the cart in the app's cache.
        s32 size = 0;
        void* cached = tic_fs_loadroot(site.fs, TIC_CACHE "abc.tic", &size);
        assert(cached);
        assert(size == (s32)sizeof Cart);
        assert(memcmp(cached, Cart, sizeof Cart) == 0);
        free(cached);

        // The first load consumed the listing; the second lists again and is
        // served from the cart's own cache, with no cart fetch.
        Answer again = {0};
        tic_fs_load(site.fs, "demo.tic", onLoad, &again);
        assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);
        answerJson(SiteListing);
        assert(again.done);
        assert(again.result.code == FS_OK);
        assert(!netCallbackPending());
    }

    {
        // A new listing replaces the cached one; a name it lacks is listed
        // for again before the refusal.
        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(OtherListing);
        assert(list.result.code == FS_OK);

        Answer load = {0};
        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);
        answerJson(OtherListing);
        assert(load.result.code == FS_NOT_FOUND);
    }

    siteTeardown(&site);
    puts("site listing ok");
}

static const char UpdatedSiteListing[] =
    "{\"folders\":[],\"files\":["
    "{\"name\":\"Demo\",\"hash\":\"def\",\"filename\":\"demo.tic\",\"id\":7},"
    "{\"name\":\"New\",\"hash\":\"ghi\",\"filename\":\"new.tic\",\"id\":8}]}";

typedef struct
{
    s32 items;
    bool done;
} LegacyListing;

static bool onLegacyItem(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    (void)name;
    (void)title;
    (void)hash;
    (void)id;
    (void)dir;

    ((LegacyListing*)data)->items++;
    return true;
}

static void onLegacyDone(void* data)
{
    ((LegacyListing*)data)->done = true;
}

// The provider owns its cache: a profile where the cache folder does not
// exist yet (boot makes .local, not .local/cache) still caches a fetched cart.
static void testSiteCacheFresh(void)
{
    char* root = makeRoot("tic80-fs-fresh");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    assert(tic_fs_makedir(fs, TIC_LOCAL) == 0);
    tic_fs_changedir(fs, TIC_HOST);

    Answer list = {0};
    tic_fs_list(fs, "", onItem, onListDone, &list);
    answerJson(SiteListing);
    assert(list.result.code == FS_OK);

    static const u8 Cart[] = {0x54, 0x49, 0x43};

    Answer load = {0};
    tic_fs_load(fs, "demo.tic", onLoad, &load);
    answerData(Cart, sizeof Cart);
    assert(load.result.code == FS_OK);

    s32 size = 0;
    void* cached = tic_fs_loadroot(fs, TIC_CACHE "abc.tic", &size);
    assert(cached);
    assert(size == (s32)sizeof Cart);
    free(cached);

    tic_fs_homedir(fs);
    tic_fs_delfile(fs, TIC_CACHE "abc.tic");
    tic_fs_deldir(fs, TIC_CACHE);
    tic_fs_deldir(fs, TIC_LOCAL);
    tic_fs_deldir(fs, "");

    puts("site cache fresh ok");
}

static void testSiteLegacyListing(void)
{
    Site site;
    siteSetup(&site);

    // The unified listing caches the old catalog.
    {
        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);
        assert(list.result.code == FS_OK);
    }

    // The legacy listing sees a newer catalog; its cached hashes must not survive.
    {
        LegacyListing legacy = {0};
        tic_fs_enum(site.fs, onLegacyItem, onLegacyDone, &legacy);
        assert(netCallbackPending());
        answerJson(UpdatedSiteListing);
        assert(legacy.done);
        assert(legacy.items == 2);
    }

    // The load lists again: the old hash is gone and the new one is fetched.
    {
        static const u8 Cart[] = {9, 8, 7};
        Answer load = {0};

        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);
        answerJson(UpdatedSiteListing);
        assert(strcmp(NetUrl, "/cart/def/demo.tic") == 0);
        answerData(Cart, sizeof Cart);
        assert(load.result.code == FS_OK);
    }

    // A cart the newer listing added is found from the listing the load read.
    {
        static const u8 Cart[] = {6, 5};
        Answer load = {0};

        tic_fs_load(site.fs, "new.tic", onLoad, &load);
        assert(strcmp(NetUrl, "/cart/ghi/new.tic") == 0);
        answerData(Cart, sizeof Cart);
        assert(load.result.code == FS_OK);
    }

    siteTeardown(&site);
    puts("site legacy listing ok");
}

static void testSiteLookupWithoutListing(void)
{
    Site site;
    siteSetup(&site);

    // A load with no listing behind it lists the folder, then fetches.
    Answer load = {0};
    static const u8 Cart[] = {0x01, 0x02};

    tic_fs_load(site.fs, "demo.tic", onLoad, &load);
    assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);

    answerJson(SiteListing);
    assert(strcmp(NetUrl, "/cart/abc/demo.tic") == 0);

    answerData(Cart, sizeof Cart);
    assert(load.result.code == FS_OK);
    assert(strcmp(load.name, "demo.tic") == 0);

    siteTeardown(&site);
    puts("site lookup ok");
}

static void testSiteLoadListsMiss(void)
{
    Site site;
    siteSetup(&site);

    // A listing fills the provider's hash cache with the old catalog.
    {
        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);
        assert(list.result.code == FS_OK);
    }

    // A name the cache lacks is listed for: the fresh catalog's hash is
    // fetched instead of refusing.
    {
        static const u8 Cart[] = {3, 2, 1};
        Answer load = {0};

        tic_fs_load(site.fs, "new.tic", onLoad, &load);
        assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);
        answerJson(UpdatedSiteListing);
        assert(strcmp(NetUrl, "/cart/ghi/new.tic") == 0);
        answerData(Cart, sizeof Cart);
        assert(load.result.code == FS_OK);
    }

    siteTeardown(&site);
    puts("site load miss ok");
}

static void testSiteStat(void)
{
    Site site;
    siteSetup(&site);

    Contract contract = {site.fs, false};

    {
        // The working directory itself answers without touching the network.
        Answer stat = statPath(&contract, "");
        assert(stat.result.code == FS_OK);
        assert(stat.item.dir);
        assert(!netCallbackPending());
    }

    {
        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);
        assert(list.result.code == FS_OK);
    }

    {
        Answer stat = statPath(&contract, "demo.tic");
        assert(stat.result.code == FS_OK);
        assert(!stat.item.dir);
        assert(!netCallbackPending());
    }

    {
        Answer stat = statPath(&contract, "games");
        assert(stat.result.code == FS_OK);
        assert(stat.item.dir);
    }

    {
        // A name the cache lacks is listed for before the refusal.
        Answer stat = {0};
        tic_fs_stat(site.fs, "nope", onStat, &stat);
        assert(!stat.done);
        assert(netCallbackPending());
        answerJson(SiteListing);
        assert(stat.done);
        assert(stat.result.code == FS_NOT_FOUND);
    }

    siteTeardown(&site);
    puts("site stat ok");
}

static void testSiteCover(void)
{
    Site site;
    siteSetup(&site);

    Contract contract = {site.fs, false};

    {
        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);
        assert(list.result.code == FS_OK);
    }

    {
        static const u8 Cover[] = {0x47, 0x49, 0x46};
        Answer cover = {0};

        tic_fs_cover(site.fs, "demo.tic", onCover, &cover);
        assert(strcmp(NetUrl, "/cart/abc/cover.gif") == 0);
        answerData(Cover, sizeof Cover);

        assert(cover.result.code == FS_OK);
        assert(cover.size == (s32)sizeof Cover);
        assert(memcmp(CoverBytes, Cover, sizeof Cover) == 0);

        s32 size = 0;
        void* cached = tic_fs_loadroot(site.fs, TIC_CACHE "abc.gif", &size);
        assert(cached);
        assert(size == (s32)sizeof Cover);
        free(cached);

        // The second cover is the cache's, with no request.
        Answer again = coverPath(&contract, "demo.tic");
        assert(again.result.code == FS_OK);
        assert(again.size == (s32)sizeof Cover);
        assert(!netCallbackPending());
    }

    siteTeardown(&site);
    puts("site cover ok");
}

static void testSiteWrites(void)
{
    Site site;
    siteSetup(&site);

    Contract contract = {site.fs, false};
    static const u8 Data[] = {1, 2, 3};

    Answer save = savePath(&contract, "x.tic", Data, sizeof Data);
    assert(save.result.code == FS_DENIED);

    Answer del = deletePath(&contract, "x.tic");
    assert(del.result.code == FS_DENIED);

    Answer mkdir = makeDir(&contract, "x");
    assert(mkdir.result.code == FS_UNSUPPORTED);

    Answer folder = openFolder(&contract, "");
    assert(folder.result.code == FS_UNSUPPORTED);

    siteTeardown(&site);
    puts("site writes ok");
}

static void testSiteErrors(void)
{
    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerError(401);
        assert(list.result.code == FS_SESSION);
        assert(strcmp(list.message, "the session ended") == 0);

        siteTeardown(&site);
    }

    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerError(404);
        assert(list.result.code == FS_NOT_FOUND);

        siteTeardown(&site);
    }

    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerError(-1);
        assert(list.result.code == FS_OFFLINE);

        siteTeardown(&site);
    }

    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);

        Answer load = {0};
        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        answerError(404);
        assert(load.result.code == FS_NOT_FOUND);

        siteTeardown(&site);
    }

    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);

        Answer load = {0};
        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        answerError(413);
        assert(load.result.code == FS_QUOTA);

        siteTeardown(&site);
    }

    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);

        Answer load = {0};
        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        answerError(500);
        assert(load.result.code == FS_IO);

        siteTeardown(&site);
    }

    {
        Site site;
        siteSetup(&site);

        Answer list = {0};
        tic_fs_list(site.fs, "", onItem, onListDone, &list);
        answerJson(SiteListing);

        Answer load = {0};
        tic_fs_load(site.fs, "demo.tic", onLoad, &load);
        answerError(403);
        assert(load.result.code == FS_DENIED);

        siteTeardown(&site);
    }

    puts("site errors ok");
}

// The local cover is the cart's screen, answered as the GIF the site's cover is.
static u8 coverPixel(s32 pos)
{
    return (u8)((pos / 3) % TIC_PALETTE_SIZE);
}

static void saveCart(tic_fs* fs, const char* name)
{
    static tic_cartridge cart;
    memset(&cart, 0, sizeof cart);

    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
    {
        cart.bank0.palette.vbank0.colors[i] = (tic_rgb){i * 16, 255 - i * 16, i * 8};
    }

    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        tic_tool_poke4(cart.bank0.screen.data, i, coverPixel(i));

    u8* buffer = malloc(TIC_CART_SAVE_SIZE);
    assert(buffer);

    s32 size = tic_cart_save(&cart, buffer);
    assert(size > 0);

    Contract contract = {fs, false};
    Answer save = savePath(&contract, name, buffer, size);
    assert(save.result.code == FS_OK);

    free(buffer);
}

static void testLocalCover(void)
{
    char* root = makeRoot("tic80-fs-cover");
    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    Contract contract = {fs, false};

    saveCart(fs, "cover.tic");

    Answer cover = coverPath(&contract, "cover.tic");
    assert(cover.result.code == FS_OK);
    assert(cover.size > 0);

    {
        gif_image* image = gif_read_data(CoverBytes, cover.size);
        assert(image);
        assert(image->width == TIC80_WIDTH);
        assert(image->height == TIC80_HEIGHT);
        assert(image->colors == TIC_PALETTE_SIZE);

        for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
            assert(image->buffer[i] == coverPixel(i));

        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        {
            assert(image->palette[i].r == (u8)(i * 16));
            assert(image->palette[i].g == (u8)(255 - i * 16));
            assert(image->palette[i].b == (u8)(i * 8));
        }

        gif_close(image);
    }

    // A cart whose screen is empty has no cover.
    {
        static tic_cartridge empty;
        memset(&empty, 0, sizeof empty);

        // A palette gives the cart something to save; the screen stays empty.
        for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
            empty.bank0.palette.vbank0.colors[i] = (tic_rgb){i * 16, 255 - i * 16, i * 8};

        u8* buffer = malloc(TIC_CART_SAVE_SIZE);
        assert(buffer);

        s32 size = tic_cart_save(&empty, buffer);
        assert(size > 0);

        Answer save = savePath(&contract, "empty.tic", buffer, size);
        assert(save.result.code == FS_OK);

        free(buffer);
    }

    {
        Answer cover = coverPath(&contract, "empty.tic");
        assert(cover.result.code == FS_NOT_FOUND);
    }

    {
        Answer cover = coverPath(&contract, "missing.tic");
        assert(cover.result.code == FS_NOT_FOUND);
    }

    tic_fs_delfile(fs, "cover.tic");
    tic_fs_delfile(fs, "empty.tic");
    tic_fs_deldir(fs, "");

    puts("local cover ok");
}

// ---- the shelf mount ----

typedef struct
{
    char root[TICNAME_MAX];
    tic_fs* fs;
    TestAccount account;
} Shelf;

static void shelfSetup(Shelf* shelf, bool signedIn)
{
    snprintf(shelf->root, sizeof shelf->root, "%s", makeRoot("tic80-fs-shelf"));
    shelf->fs = tic_fs_create(shelf->root, NULL);
    assert(shelf->fs);

    shelf->account = (TestAccount){.signedIn = signedIn};
    snprintf(shelf->account.token, sizeof shelf->account.token, "tok123");

    tic_fs_set_remote(shelf->fs, &(tic_fs_remote){
                                     .signed_in = accountSignedIn,
                                     .token = accountToken,
                                     .unauthorized = accountUnauthorized,
                                     .data = &shelf->account,
                                 });

    Held.held = false;
}

static void shelfTeardown(Shelf* shelf)
{
    Held.held = false;
    tic_fs_deldir(shelf->fs, "");
}

// The root lists the shelf only while signed in, and the shelf name answers a
// cd with the sign-in hint when signed out.
static void testShelfMount(void)
{
    Shelf shelf;
    shelfSetup(&shelf, false);

    {
        Answer list = {0};
        tic_fs_list(shelf.fs, "", onItem, onListDone, &list);
        assert(list.result.code == FS_OK);

        for(s32 i = 0; i < list.count; i++)
            assert(strcmp(list.items[i].name, TIC_SHELF_DIR) != 0);

        Answer stat = {0};
        tic_fs_stat(shelf.fs, TIC_SHELF_DIR, onStat, &stat);
        assert(stat.result.code == FS_DENIED);
        assert(strstr(stat.message, "login"));
    }

    shelf.account.signedIn = true;

    {
        Answer list = {0};
        tic_fs_list(shelf.fs, "", onItem, onListDone, &list);
        assert(list.result.code == FS_OK);

        s32 found = -1;
        for(s32 i = 0; i < list.count; i++)
            if(strcmp(list.items[i].name, TIC_SHELF_DIR) == 0)
                found = i;

        assert(found >= 0);
        assert(list.items[found].dir);
        assert(strcmp(list.items[found].title, "SHELF") == 0);

        // A leading slash still names the mount, for `cd /shelf`.
        Answer stat = {0};
        tic_fs_stat(shelf.fs, "/" TIC_SHELF_DIR, onStat, &stat);
        assert(stat.result.code == FS_OK);
        assert(stat.item.dir);
        assert(strcmp(stat.item.name, TIC_SHELF_DIR) == 0);

        tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);
        char dir[TICNAME_MAX];
        tic_fs_dir(shelf.fs, dir);
        assert(strcmp(dir, TIC_SHELF_DIR) == 0);

        tic_fs_dirback(shelf.fs);
        tic_fs_dir(shelf.fs, dir);
        assert(strcmp(dir, "") == 0);
    }

    shelfTeardown(&shelf);
    puts("shelf mount ok");
}

static void testShelfList(void)
{
    Shelf shelf;
    shelfSetup(&shelf, true);
    tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);

    {
        Answer list = {0};
        tic_fs_list(shelf.fs, "", onItem, onListDone, &list);

        assert(Held.held);
        assert(Held.method == net_method_get);
        assert(strcmp(Held.url, "/api/v1/client/shelf") == 0);
        assert(strcmp(Held.authorization, "Bearer tok123") == 0);

        answerRequest(200,
                      "{\"carts\":[{\"key\":\"my-game\",\"title\":\"My Game\",\"size\":123,\"updated\":1794940745061}],"
                      "\"count\":1,\"bytes\":123,\"max_count\":100,\"max_bytes\":33554432}");

        assert(list.result.code == FS_OK);
        assert(list.count == 1);
        assert(strcmp(list.items[0].name, "my-game.tic") == 0);
        assert(list.items[0].title == NULL); // the shelf shows keys
        assert(!list.items[0].dir);
        assert(list.items[0].size == 123);
        assert(list.items[0].date == 1794940745061LL);
        assert(list.hasUsage);
        assert(list.usage.count == 1 && list.usage.max_count == 100);
        assert(list.usage.bytes == 123 && list.usage.max_bytes == 33554432);
    }

    {
        // A 401 ends the session the request was sent with.
        Answer list = {0};
        tic_fs_list(shelf.fs, "", onItem, onListDone, &list);
        answerRequest(401, "{\"error\":\"token revoked\"}");

        assert(list.result.code == FS_SESSION);
        assert(shelf.account.unauthorizedCalls == 1);
        assert(strcmp(shelf.account.unauthorizedToken, "tok123") == 0);
    }

    {
        // A transport error says the shelf needs a network.
        Answer list = {0};
        tic_fs_list(shelf.fs, "", onItem, onListDone, &list);
        answerRequestError(-1);

        assert(list.result.code == FS_OFFLINE);
        assert(strstr(list.message, "network"));
    }

    shelfTeardown(&shelf);
    puts("shelf list ok");
}

static void testShelfLoad(void)
{
    Shelf shelf;
    shelfSetup(&shelf, true);
    tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);

    {
        static const u8 Cart[] = {0x54, 0x49, 0x43};
        Answer load = {0};

        tic_fs_load(shelf.fs, "my-game.tic", onLoad, &load);

        assert(Held.held);
        assert(Held.method == net_method_get);
        assert(strcmp(Held.url, "/api/v1/client/shelf/my-game/cart.tic") == 0);

        answerRequestBytes(200, Cart, sizeof Cart);
        assert(load.result.code == FS_OK);
        assert(strcmp(load.name, "my-game") == 0); // the key is the identity
        assert(load.size == (s32)sizeof Cart);
        assert(memcmp(load.bytes, Cart, sizeof Cart) == 0);
    }

    {
        Answer load = {0};
        tic_fs_load(shelf.fs, "my-game", onLoad, &load);
        assert(Held.held);
        assert(strcmp(Held.url, "/api/v1/client/shelf/my-game/cart.tic") == 0);

        answerRequest(404, "{\"error\":\"no such cart\"}");
        assert(load.result.code == FS_NOT_FOUND);
        assert(strcmp(load.message, "no such cart") == 0);
    }

    shelfTeardown(&shelf);
    puts("shelf load ok");
}

static void testShelfSave(void)
{
    Shelf shelf;
    shelfSetup(&shelf, true);
    tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);

    static const u8 Cart[] = {0x54, 0x49, 0x43, 0x00};

    {
        Answer save = {0};
        tic_fs_save(shelf.fs, "My Game.tic", Cart, sizeof Cart, onSave, &save);

        assert(Held.held);
        assert(Held.method == net_method_put);
        assert(strcmp(Held.url, "/api/v1/client/shelf/My-Game") == 0);
        assert(Held.bodySize == (s32)sizeof Cart);
        assert(memcmp(Held.body, Cart, sizeof Cart) == 0);

        answerRequest(201,
                      "{\"key\":\"My-Game\",\"count\":1,\"bytes\":4,"
                      "\"max_count\":100,\"max_bytes\":33554432}");

        assert(save.result.code == FS_OK);
        assert(strcmp(save.name, "My-Game") == 0);
        assert(strstr(save.message, "created on the shelf"));
        assert(strstr(save.message, "1/100 carts"));
        assert(strstr(save.message, "4/33554432 bytes"));
    }

    {
        // The same key replaces; the server says so and answers its spelling.
        Answer save = {0};
        tic_fs_save(shelf.fs, "My Game", Cart, sizeof Cart, onSave, &save);
        assert(Held.held);
        answerRequest(200, "{\"key\":\"my-game\",\"count\":1,\"bytes\":4,\"max_count\":100,\"max_bytes\":33554432}");
        assert(save.result.code == FS_OK);
        assert(strcmp(save.name, "my-game") == 0);
        assert(strstr(save.message, "replaced on the shelf"));
    }

    {
        // A quota refusal passes the server's message through.
        Answer save = {0};
        tic_fs_save(shelf.fs, "big.tic", Cart, sizeof Cart, onSave, &save);
        answerRequest(413, "{\"error\":\"the shelf is full\"}");
        assert(save.result.code == FS_QUOTA);
        assert(strcmp(save.message, "the shelf is full") == 0);
    }

    shelfTeardown(&shelf);
    puts("shelf save ok");
}

static void testShelfDelete(void)
{
    Shelf shelf;
    shelfSetup(&shelf, true);
    tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);

    {
        Answer del = {0};
        tic_fs_delete(shelf.fs, "my-game.tic", onDone, &del);

        assert(Held.held);
        assert(Held.method == net_method_delete);
        assert(strcmp(Held.url, "/api/v1/client/shelf/my-game") == 0);

        answerRequest(200, "{\"count\":0,\"bytes\":0,\"max_count\":100,\"max_bytes\":33554432}");
        assert(del.result.code == FS_OK);
        assert(strstr(del.message, "0/100 carts"));
    }

    {
        Answer del = {0};
        tic_fs_delete(shelf.fs, "missing", onDone, &del);
        answerRequest(404, "{\"error\":\"no such cart\"}");
        assert(del.result.code == FS_NOT_FOUND);
    }

    shelfTeardown(&shelf);
    puts("shelf delete ok");
}

// The shelf has no cheap stat, no folders and no covers; the server answers
// whether a key exists.
static void testShelfStatAndShape(void)
{
    Shelf shelf;
    shelfSetup(&shelf, true);
    tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);

    {
        Answer stat = {0};
        tic_fs_stat(shelf.fs, "", onStat, &stat);
        assert(stat.result.code == FS_OK);
        assert(stat.item.dir);
    }

    {
        Answer stat = {0};
        tic_fs_stat(shelf.fs, "my-game.tic", onStat, &stat);
        assert(stat.result.code == FS_NOT_FOUND);
    }

    {
        Answer mkdir = {0};
        tic_fs_mkdir(shelf.fs, "folder", onDone, &mkdir);
        assert(mkdir.result.code == FS_UNSUPPORTED);

        Answer folder = {0};
        tic_fs_openfolder(shelf.fs, "", onDone, &folder);
        assert(folder.result.code == FS_UNSUPPORTED);

        Answer cover = {0};
        tic_fs_cover(shelf.fs, "my-game.tic", onCover, &cover);
        assert(cover.result.code == FS_NOT_FOUND);
    }

    shelfTeardown(&shelf);
    puts("shelf shape ok");
}

// Signed out, every shelf operation answers the sign-in hint without a request.
static void testShelfSignedOut(void)
{
    Shelf shelf;
    shelfSetup(&shelf, false);
    tic_fs_changedir(shelf.fs, TIC_SHELF_DIR);

    {
        Answer list = {0};
        tic_fs_list(shelf.fs, "", onItem, onListDone, &list);
        assert(list.result.code == FS_DENIED);
        assert(strstr(list.message, "login"));
        assert(!Held.held);

        Answer load = {0};
        tic_fs_load(shelf.fs, "my-game.tic", onLoad, &load);
        assert(load.result.code == FS_DENIED);
        assert(!Held.held);

        Answer save = {0};
        tic_fs_save(shelf.fs, "my-game.tic", (const u8*)"x", 1, onSave, &save);
        assert(save.result.code == FS_DENIED);
        assert(!Held.held);

        Answer del = {0};
        tic_fs_delete(shelf.fs, "my-game.tic", onDone, &del);
        assert(del.result.code == FS_DENIED);
        assert(!Held.held);
    }

    shelfTeardown(&shelf);
    puts("shelf signed out ok");
}

int main(void)
{
    testLocalContract();
    testHostPathConfined();
    testLocalLoadName();
    testLocalListOrder();
    testFakeRemoteContract();
    testHeldOperations();
    testSiteListing();
    testSiteCacheFresh();
    testSiteLegacyListing();
    testSiteLookupWithoutListing();
    testSiteLoadListsMiss();
    testSiteStat();
    testSiteCover();
    testSiteWrites();
    testSiteErrors();
    testLocalCover();
    testShelfMount();
    testShelfList();
    testShelfLoad();
    testShelfSave();
    testShelfDelete();
    testShelfStatAndShape();
    testShelfSignedOut();

    puts("fs ok");
    return 0;
}
