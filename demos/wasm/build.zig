const std = @import("std");

const tic80_reserved_memory = 96 * 1024;
const tic80_stack_size = 8 * 1024;

pub fn build(b: *std.Build) !void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.resolveTargetQuery(.{ .cpu_arch = .wasm32, .os_tag = .freestanding });
    const exe = b.addExecutable(.{
        .name = "cart",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    exe.rdynamic = true;
    exe.entry = .disabled;
    exe.import_memory = true;
    // TIC-80 reserves the first 96 KiB of linear memory, so reserve that space
    // inside the stack region and leave 8 KiB of actual stack above it.
    exe.stack_size = tic80_reserved_memory + tic80_stack_size;
    exe.initial_memory = 65536 * 4;
    exe.max_memory = 65536 * 4;
    exe.export_table = true;

    b.installArtifact(exe);
}
