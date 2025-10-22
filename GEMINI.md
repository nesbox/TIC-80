# Project Overview: TIC-80 TINY COMPUTER

TIC-80 is a free and open-source fantasy computer designed for creating, playing, and sharing small games. It provides a complete development environment with built-in tools for code, sprites, maps, sound effects, and music editing, all accessible via a command line. The project emphasizes retro-style game development by imposing technical limitations such as a 240x136 pixel display and a 16-color palette.

**Key Features:**
*   **Multi-language Support:** Develop games using Lua, Moonscript, Javascript, Ruby, Wren, Fennel, Squirrel, Janet, and Python.
*   **Integrated Editors:** Built-in tools for code, sprites, world maps, sound effects, and music.
*   **Cartridge System:** Games are packaged into easily distributable cartridge files.
*   **Cross-Platform:** Works on various operating systems, allowing cartridges to be played on any device.
*   **Pro Version:** Offers additional features like saving/loading cartridges in text format and more memory banks.

# Building and Running

The project uses CMake as its primary build system. Detailed instructions for various platforms (Windows, Linux, Mac, FreeBSD) are available in the `README.md`.

**General Build Steps:**

1.  **Clone the repository:**
    ```bash
    git clone --recursive https://github.com/nesbox/TIC-80
    cd TIC-80/build
    ```
2.  **Configure with CMake:**
    Run `cmake` with appropriate options for your platform and desired features. To ensure all tools (including `xplode`) are built, use the `-DBUILD_TOOLS=ON` flag. A common configuration to build with all supported scripts is:
    ```bash
    cmake -DBUILD_WITH_ALL=On -DBUILD_TOOLS=ON ..
    ```
    For specific platforms and advanced options (e.g., `BUILD_SDLGPU=On`, `BUILD_STATIC=On`), refer to the "Build instructions" section in the `README.md`.

3.  **Build the project:**
    ```bash
    cmake --build . --parallel
    # or for Make-based systems:
    # make -j$(nproc) # or make -j4 for Mac/FreeBSD
    ```

**Running the Application:**

After a successful build, the executable `tic80` will typically be found in the `build/bin` directory.

*   **If installed system-wide:**
    ```bash
    tic80
    ```
*   **From the build directory:**
    ```bash
    ./tic80
    ```

# Development Conventions

*   **Code of Conduct:** The project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md), promoting an open and welcoming environment.
*   **Security Policy:** Only the latest releases receive security updates. Vulnerabilities should be reported via a [GitHub issue](https://github.com/nesbox/TIC-80/issues/new) or directly to `grigoruk@gmail.com`. Refer to `SECURITY.md` for more details.
*   **Code Formatting:** The project uses `.clang-format` for consistent code style, based on LLVM coding standards. Key formatting rules include:
    *   4-space indentation.
    *   Allman style for braces (`BreakBeforeBraces: Allman`).
    *   `AllowShortIfStatementsOnASingleLine: true`.
    *   `PointerAlignment: Left`.
    *   `NamespaceIndentation: None`.
    *   `EmptyLineBeforeAccessModifier: LogicalBlock`.
