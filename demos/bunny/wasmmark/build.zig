const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    const mode = b.standardReleaseOptions();

    const lib = b.addSharedLibrary("cart", "src/main.zig", .unversioned);
    lib.setBuildMode(mode);
    lib.setTarget(.{ .cpu_arch = .wasm32, .os_tag = .freestanding });
    lib.import_memory = true;
    lib.initial_memory = 64 * 1024 * 4;
    lib.max_memory = 64 * 1024 * 4;
    // 0x14e04
    // + 1024 is because we keep adding stuff, we really shouldn't do that
    lib.export_table = true;
    // lib.global_base = 85508 + 1024;
    lib.global_base = 96 * 1024;
    lib.stack_size = 8192;
    lib.export_symbol_names = &[_][]const u8{ "TIC", "OVR", "BDR", "SCR" };
    lib.install();
}
