#![cfg(feature = "buddy-alloc")]

use std::alloc::{GlobalAlloc, Layout};
use std::cell::RefCell;

use buddy_alloc::{BuddyAllocParam, FastAllocParam, NonThreadsafeAlloc};

extern "C" {
    static __heap_base: u8;
}

const FAST_HEAP_SIZE: usize = 24 * 1024;
const LEAF_SIZE: usize = 16;

// This allocator implementation will, on first use, prepare a NonThreadsafeAlloc using all the remaining memory.
// This is done at runtime as a workaround for the fact that __heap_base can't be accessed at compile time.
struct TicAlloc(RefCell<Option<NonThreadsafeAlloc>>);

unsafe impl GlobalAlloc for TicAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let is_none = self.0.borrow().is_none();
        if is_none {
            let mut inner = self.0.borrow_mut();
            *inner = {
                let fast_heap_ptr = std::ptr::addr_of!(__heap_base);
                let heap_ptr = fast_heap_ptr.add(FAST_HEAP_SIZE);
                let heap_size = 0x40000 - (heap_ptr as usize);

                let fast_param = FastAllocParam::new(fast_heap_ptr, FAST_HEAP_SIZE);
                let buddy_param = BuddyAllocParam::new(heap_ptr, heap_size, LEAF_SIZE);
                Some(NonThreadsafeAlloc::new(fast_param, buddy_param))
            };
        }
        self.0.borrow().as_ref().unwrap().alloc(layout)
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.0.borrow().as_ref().unwrap().dealloc(ptr, layout)
    }
}

unsafe impl Sync for TicAlloc {}

#[global_allocator]
static ALLOC: TicAlloc = TicAlloc(RefCell::new(None));
