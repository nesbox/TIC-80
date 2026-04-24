## ADDED Requirements

### Requirement: LuaJIT Build Option
The build system SHALL provide a CMake option `BUILD_WITH_LUAJIT` that allows the user to select LuaJIT as the Lua runtime.

#### Scenario: Enabling LuaJIT
- **WHEN** CMake is run with `-DBUILD_WITH_LUAJIT=ON`
- **THEN** The build system SHALL configure the project to use LuaJIT and disable all other scripting runtimes (JS, Fennel, Moonscript, Yue, etc.).

### Requirement: Cross-Platform Desktop Support
The LuaJIT runtime SHALL be supported on Windows, Linux, and macOS platforms.

#### Scenario: Building on Windows
- **WHEN** Building on Windows with LuaJIT enabled
- **THEN** The system SHALL successfully compile LuaJIT using the appropriate toolchain (MSVC or MinGW).

### Requirement: TIC-80 API Compatibility
The LuaJIT runtime SHALL expose the complete TIC-80 API to scripts, identical to the standard Lua runtime.

#### Scenario: Calling TIC-80 API
- **WHEN** A script calls `cls(1)` in LuaJIT mode
- **THEN** The screen SHALL be cleared with color 1.

### Requirement: Script Execution (Lua 5.1 / LuaJIT)
The LuaJIT runtime SHALL execute scripts using the LuaJIT engine, supporting Lua 5.1 syntax and LuaJIT extensions (e.g., FFI).

#### Scenario: Running a LuaJIT script
- **WHEN** A script is executed in LuaJIT mode
- **THEN** The script SHALL execute using the LuaJIT engine.

### Requirement: Exclusion of Other Runtimes
When LuaJIT is enabled, all other script runtimes SHALL be disabled to provide a focused performance testing environment.

#### Scenario: Checking other runtimes
- **WHEN** TIC-80 is built with LuaJIT enabled
- **THEN** Other languages like Fennel, Moonscript, and JS SHALL NOT be available in the console or for script execution.
