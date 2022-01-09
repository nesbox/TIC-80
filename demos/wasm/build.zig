const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    const mode = b.standardReleaseOptions();

    const lib = b.addSharedLibrary("cart", "src/main.zig", .unversioned);
    lib.setBuildMode(mode);
    lib.setTarget(.{ .cpu_arch = .wasm32, .os_tag = .freestanding });
    lib.import_memory = true;
    lib.initial_memory = 65536 * 4;
    lib.max_memory = 65536 * 4;
    lib.export_table = true;
    // all the memory below 96kb is reserved for TIC and memory mapped I/O
    // so our own usage must start above the 96kb mark
    lib.global_base = 96 * 1024;
    lib.stack_size = 8192;
    lib.export_symbol_names = &[_][]const u8{ "TIC", "OVR", "BDR", "SCR", "INIT" };
    lib.install();
}
