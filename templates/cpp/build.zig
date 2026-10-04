const std = @import("std");

const tic80_reserved_memory = 96 * 1024;
const tic80_stack_size = 8 * 1024;

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{ .cpu_arch = .wasm32, .os_tag = .wasi });
    const exe = b.addExecutable(.{
        .name = "cart",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = .small, // binary with debug symbols is too large anyway
            .link_libc = true,
            .link_libcpp = true, // be careful, using some features might bloat the binary past the size limit
        }),
    });

    exe.root_module.addCSourceFile(.{
        .file = b.path("src/main.cpp"),
        .flags = &[_][]const u8{
            "-W",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused",
            "-Wconversion",
            "-Wsign-conversion",
            "-MP",
            "-fno-exceptions",
            "-std=c++20", // <- select your c++ standard
        },
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

    const run_cart = b.addSystemCommand(&[_][]const u8{ "tic80", "--skip", "--fs", ".", "--cmd" });
    run_cart.step.dependOn(b.getInstallStep());
    run_cart.addArtifactArg2(exe, .{ .prefix = "load cart.wasmp & import binary ", .suffix = " & save & run & quit" });

    const run_step = b.step("run", "Run the cartridge");
    run_step.dependOn(&run_cart.step);
}
