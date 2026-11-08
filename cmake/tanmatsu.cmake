################################
# Tanmatsu (ESP32-P4) source lists
################################
#
# Shared between the ESP-IDF component in src/system/tanmatsu and any other
# build that wants the same set of files. ESP-IDF drives its own CMake, so this
# file only fills in variables; it never creates targets.
#
# Set TIC80_ROOT to the repository root before including.

if(NOT TIC80_ROOT)
    message(FATAL_ERROR "TIC80_ROOT must be set before including tanmatsu.cmake")
endif()

set(TIC80_SRC ${TIC80_ROOT}/src)
set(TIC80_VENDOR ${TIC80_ROOT}/vendor)
set(TIC80_BACKEND ${TIC80_SRC}/system/tanmatsu)

# TIC-80 core
set(TIC80_TANMATSU_CORE_SRCS
    ${TIC80_SRC}/fftdata.c
    ${TIC80_SRC}/vqtdata.c
    ${TIC80_SRC}/core/core.c
    ${TIC80_SRC}/core/draw.c
    ${TIC80_SRC}/core/io.c
    ${TIC80_SRC}/core/sound.c
    ${TIC80_SRC}/tic.c
    ${TIC80_SRC}/cart.c
    ${TIC80_SRC}/tools.c
    ${TIC80_SRC}/zip.c
    ${TIC80_SRC}/tilesheet.c
    ${TIC80_SRC}/script.c
    ${TIC80_SRC}/ext/fft.c
    ${TIC80_SRC}/ext/vqt.c
    ${TIC80_SRC}/ext/vqt_kernel.c
    ${TIC80_SRC}/ext/kiss_fft.c
    ${TIC80_SRC}/ext/kiss_fftr.c
    ${TIC80_SRC}/ext/png.c
    ${TIC80_SRC}/ext/gif.c
)

# Studio, with the editors and surf. The port supplies its own net.c, the way
# the 3DS and Switch ports do, so surf reaches tic80.com as well as the local
# filesystem.
set(TIC80_TANMATSU_STUDIO_SRCS
    ${TIC80_SRC}/studio/screens/run.c
    ${TIC80_SRC}/studio/screens/menu.c
    ${TIC80_SRC}/studio/screens/mainmenu.c
    ${TIC80_SRC}/studio/screens/start.c
    ${TIC80_SRC}/studio/screens/console.c
    ${TIC80_SRC}/studio/screens/surf.c
    ${TIC80_BACKEND}/net.c
    ${TIC80_SRC}/studio/clipboard.c
    ${TIC80_SRC}/studio/editors/code/text.c
    ${TIC80_SRC}/studio/editors/code/find.c
    ${TIC80_SRC}/studio/editors/code/vi.c
    ${TIC80_SRC}/studio/editors/code/minimap.c
    ${TIC80_SRC}/studio/editors/code/code.c
    ${TIC80_SRC}/studio/editors/sprite/sprite.c
    ${TIC80_SRC}/studio/editors/map/map.c
    ${TIC80_SRC}/studio/editors/map/world.c
    ${TIC80_SRC}/studio/editors/sfx/sfx.c
    ${TIC80_SRC}/studio/editors/music/music.c
    ${TIC80_SRC}/studio/studio.c
    ${TIC80_SRC}/studio/modes.c
    ${TIC80_SRC}/studio/boot.c
    ${TIC80_SRC}/studio/anim.c
    ${TIC80_SRC}/studio/embed.c
    ${TIC80_SRC}/studio/sound.c
    ${TIC80_SRC}/studio/ui.c
    ${TIC80_SRC}/studio/keyboard.c
    ${TIC80_SRC}/studio/mouse.c
    ${TIC80_SRC}/studio/menubar.c
    ${TIC80_SRC}/studio/studiomenu.c
    ${TIC80_SRC}/studio/export.c
    ${TIC80_SRC}/studio/rom.c
    ${TIC80_SRC}/studio/config.c
    ${TIC80_SRC}/studio/fs.c
    ${TIC80_SRC}/ext/history.c
    ${TIC80_SRC}/ext/md5.c
    ${TIC80_SRC}/ext/json.c
)

# Lua. liolib/loslib/linit are left out on purpose: luaapi.c registers its own
# library list and neither io nor os is in it, and both pull in newlib calls
# (system, popen, tmpfile) that do not link on ESP-IDF.
set(TIC80_TANMATSU_LUA_SRCS
    ${TIC80_VENDOR}/lua/lapi.c
    ${TIC80_VENDOR}/lua/lcode.c
    ${TIC80_VENDOR}/lua/lctype.c
    ${TIC80_VENDOR}/lua/ldebug.c
    ${TIC80_VENDOR}/lua/ldo.c
    ${TIC80_VENDOR}/lua/ldump.c
    ${TIC80_VENDOR}/lua/lfunc.c
    ${TIC80_VENDOR}/lua/lgc.c
    ${TIC80_VENDOR}/lua/llex.c
    ${TIC80_VENDOR}/lua/lmem.c
    ${TIC80_VENDOR}/lua/lobject.c
    ${TIC80_VENDOR}/lua/lopcodes.c
    ${TIC80_VENDOR}/lua/lparser.c
    ${TIC80_VENDOR}/lua/lstate.c
    ${TIC80_VENDOR}/lua/lstring.c
    ${TIC80_VENDOR}/lua/ltable.c
    ${TIC80_VENDOR}/lua/ltm.c
    ${TIC80_VENDOR}/lua/lundump.c
    ${TIC80_VENDOR}/lua/lvm.c
    ${TIC80_VENDOR}/lua/lzio.c
    ${TIC80_VENDOR}/lua/lauxlib.c
    ${TIC80_VENDOR}/lua/lbaselib.c
    ${TIC80_VENDOR}/lua/lcorolib.c
    ${TIC80_VENDOR}/lua/ldblib.c
    ${TIC80_VENDOR}/lua/lmathlib.c
    ${TIC80_VENDOR}/lua/lstrlib.c
    ${TIC80_VENDOR}/lua/ltablib.c
    ${TIC80_VENDOR}/lua/lutf8lib.c
    ${TIC80_VENDOR}/lua/loadlib.c
    ${TIC80_VENDOR}/lua/lbitlib.c
    ${TIC80_SRC}/api/luaapi.c
    ${TIC80_SRC}/api/parse_note.c
    ${TIC80_SRC}/api/lua.c
)

set(TIC80_TANMATSU_VENDOR_SRCS
    ${TIC80_VENDOR}/blip-buf/blip_buf.c
    ${TIC80_VENDOR}/blip-buf/wave_writer.c
    ${TIC80_VENDOR}/argparse/argparse.c
    ${TIC80_VENDOR}/zip/src/zip.c
    ${TIC80_VENDOR}/giflib/dgif_lib.c
    ${TIC80_VENDOR}/giflib/egif_lib.c
    ${TIC80_VENDOR}/giflib/gif_err.c
    ${TIC80_VENDOR}/giflib/gif_font.c
    ${TIC80_VENDOR}/giflib/gif_hash.c
    ${TIC80_VENDOR}/giflib/gifalloc.c
    ${TIC80_VENDOR}/giflib/openbsd-reallocarray.c
    ${TIC80_VENDOR}/zlib/adler32.c
    ${TIC80_VENDOR}/zlib/compress.c
    ${TIC80_VENDOR}/zlib/crc32.c
    ${TIC80_VENDOR}/zlib/deflate.c
    ${TIC80_VENDOR}/zlib/inflate.c
    ${TIC80_VENDOR}/zlib/infback.c
    ${TIC80_VENDOR}/zlib/inftrees.c
    ${TIC80_VENDOR}/zlib/inffast.c
    ${TIC80_VENDOR}/zlib/trees.c
    ${TIC80_VENDOR}/zlib/uncompr.c
    ${TIC80_VENDOR}/zlib/zutil.c
    ${TIC80_VENDOR}/libpng/png.c
    ${TIC80_VENDOR}/libpng/pngerror.c
    ${TIC80_VENDOR}/libpng/pngget.c
    ${TIC80_VENDOR}/libpng/pngmem.c
    ${TIC80_VENDOR}/libpng/pngpread.c
    ${TIC80_VENDOR}/libpng/pngread.c
    ${TIC80_VENDOR}/libpng/pngrio.c
    ${TIC80_VENDOR}/libpng/pngrtran.c
    ${TIC80_VENDOR}/libpng/pngrutil.c
    ${TIC80_VENDOR}/libpng/pngset.c
    ${TIC80_VENDOR}/libpng/pngtrans.c
    ${TIC80_VENDOR}/libpng/pngwio.c
    ${TIC80_VENDOR}/libpng/pngwrite.c
    ${TIC80_VENDOR}/libpng/pngwtran.c
    ${TIC80_VENDOR}/libpng/pngwutil.c
)

set(TIC80_TANMATSU_BACKEND_SRCS
    ${TIC80_BACKEND}/tic80_tanmatsu.c
    ${TIC80_BACKEND}/display.c
    ${TIC80_BACKEND}/audio.c
    ${TIC80_BACKEND}/keymap.c
    ${TIC80_BACKEND}/usb_hid.c
    ${TIC80_BACKEND}/storage.c
)

set(TIC80_TANMATSU_SRCS
    ${TIC80_TANMATSU_CORE_SRCS}
    ${TIC80_TANMATSU_STUDIO_SRCS}
    ${TIC80_TANMATSU_LUA_SRCS}
    ${TIC80_TANMATSU_VENDOR_SRCS}
    ${TIC80_TANMATSU_BACKEND_SRCS}
)

set(TIC80_TANMATSU_INCLUDE_DIRS
    ${TIC80_ROOT}/include
    ${TIC80_SRC}
    ${TIC80_SRC}/studio
    ${TIC80_BACKEND}
    ${TIC80_VENDOR}/lua
    ${TIC80_VENDOR}/blip-buf
    ${TIC80_VENDOR}/argparse
    ${TIC80_VENDOR}/zip/src
    ${TIC80_VENDOR}/giflib
    ${TIC80_VENDOR}/zlib
    ${TIC80_VENDOR}/libpng
    ${TIC80_VENDOR}/jsmn
)

set(TIC80_TANMATSU_DEFINITIONS
    TIC_RUNTIME_STATIC
    TIC_BUILD_WITH_LUA
    BUILD_EDITORS
    BUILD_SURF
    BUILD_DEPRECATED
    LUA_COMPAT_5_2
    # 32-bit ints and single precision floats for Lua, as the 3DS port does.
    # This core is rv32imafc with the ilp32f ABI, so it has hardware single
    # precision but emulates doubles in software, and Lua numbers are doubles by
    # default. Carts doing per-pixel maths pay for that on every operation.
    LUA_32BITS
    PNG_ARM_NEON_OPT=0
    __TANMATSU__=1
)

# TIC-80's own build generates version.h and runtime_versions.h. ESP-IDF drives
# its own CMake and never runs the top level lists file, so the two headers are
# generated here instead, into the directory the caller asks for.
function(tic80_tanmatsu_generate_headers OUTPUT_DIR)
    include(${TIC80_ROOT}/cmake/version.cmake)

    configure_file(${TIC80_ROOT}/version.h.in ${OUTPUT_DIR}/version.h @ONLY)

    # Lua is the only runtime this port builds; the rest stay "unknown" so the
    # console's VERSION output does not claim languages that are not there.
    set(TIC_RT_LUA "unknown")
    set(TIC_RT_RUBY "unknown")
    set(TIC_RT_JS "unknown")
    set(TIC_RT_MOON "unknown")
    set(TIC_RT_YUE "unknown")
    set(TIC_RT_FENNEL "unknown")
    set(TIC_RT_SCHEME "unknown")
    set(TIC_RT_SQUIRREL "unknown")
    set(TIC_RT_WREN "unknown")
    set(TIC_RT_WASM "unknown")
    set(TIC_RT_JANET "unknown")
    set(TIC_RT_PYTHON "unknown")
    set(TIC_RT_MINISCRIPT "unknown")
    set(TIC_RT_FORTH "unknown")
    set(TIC_RT_GRAVITY "unknown")

    if(EXISTS "${TIC80_ROOT}/vendor/lua/lua.h")
        file(STRINGS "${TIC80_ROOT}/vendor/lua/lua.h" LUA_MAJOR_LINE REGEX "^#define LUA_VERSION_MAJOR[ \t]+\"[^\"]+\"")
        file(STRINGS "${TIC80_ROOT}/vendor/lua/lua.h" LUA_MINOR_LINE REGEX "^#define LUA_VERSION_MINOR[ \t]+\"[^\"]+\"")
        file(STRINGS "${TIC80_ROOT}/vendor/lua/lua.h" LUA_RELEASE_LINE REGEX "^#define LUA_VERSION_RELEASE[ \t]+\"[^\"]+\"")

        if(LUA_MAJOR_LINE AND LUA_MINOR_LINE AND LUA_RELEASE_LINE)
            string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" LUA_MAJOR "${LUA_MAJOR_LINE}")
            string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" LUA_MINOR "${LUA_MINOR_LINE}")
            string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" LUA_RELEASE "${LUA_RELEASE_LINE}")
            set(TIC_RT_LUA "Lua ${LUA_MAJOR}.${LUA_MINOR}.${LUA_RELEASE}")
        endif()
    endif()

    configure_file(${TIC80_ROOT}/cmake/runtime_versions.h.in ${OUTPUT_DIR}/runtime_versions.h @ONLY)

    # libpng ships its configuration as a prebuilt header that the build is
    # expected to drop in place.
    configure_file(${TIC80_ROOT}/vendor/libpng/scripts/pnglibconf.h.prebuilt ${OUTPUT_DIR}/pnglibconf.h COPYONLY)
endfunction()
