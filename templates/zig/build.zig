const std = @import("std");

pub fn build(b: *std.Build) !void {
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "cart",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = .{ .cpu_arch = .wasm32, .os_tag = .wasi },
        .optimize = optimize,
    });

    exe.entry = .disabled;
    exe.import_memory = true;
    exe.stack_size = 8192;
    exe.initial_memory = 65536 * 4;
    exe.max_memory = 65536 * 4;

    exe.export_table = true;

    // all the memory below 96kb is reserved for TIC and memory mapped I/O
    // so our own usage must start above the 96kb mark
    exe.global_base = 96 * 1024;

    exe.export_symbol_names = &[_][]const u8{ "TIC", "OVR", "BDR", "BOOT" };

    b.installArtifact(exe);
}
