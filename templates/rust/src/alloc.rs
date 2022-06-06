use buddy_alloc::{BuddyAllocParam, FastAllocParam, NonThreadsafeAlloc};

const FAST_HEAP_SIZE: usize = 32 * 1024;
const HEAP_SIZE: usize = 128 * 1024;
const LEAF_SIZE: usize = 16;

const FAST_HEAP_PTR: *const u8 = 0x18000 as *const u8;
const HEAP_PTR: *const u8 = 0x20000 as *const u8;

#[global_allocator]
static ALLOC: NonThreadsafeAlloc = {
    let fast_param = FastAllocParam::new(FAST_HEAP_PTR, FAST_HEAP_SIZE);
    let buddy_param = BuddyAllocParam::new(HEAP_PTR, HEAP_SIZE, LEAF_SIZE);
    NonThreadsafeAlloc::new(fast_param, buddy_param)
};
