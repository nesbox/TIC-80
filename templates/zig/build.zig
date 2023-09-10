const std = @import("std");

pub fn build(b: *std.Build) !void {
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addSharedLibrary(.{
        .name = "cart",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = .{ .cpu_arch = .wasm32, .os_tag = .wasi },
        .optimize = optimize,
    });

    lib.import_memory = true;
    lib.stack_size = 8192;
    lib.initial_memory = 65536 * 4;
    lib.max_memory = 65536 * 4;

    lib.export_table = true;

    // all the memory below 96kb is reserved for TIC and memory mapped I/O
    // so our own usage must start above the 96kb mark
    lib.global_base = 96 * 1024;

    lib.export_symbol_names = &[_][]const u8{ "TIC", "OVR", "BDR", "BOOT" };

    b.installArtifact(lib);
}
