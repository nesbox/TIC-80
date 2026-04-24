## ADDED Requirements

### Requirement: LuaJIT Runtime Integration
The build system SHALL integrate LuaJIT as the default scripting runtime when `BUILD_WITH_LUA` is enabled.

#### Scenario: Building with Lua
- **WHEN** TIC-80 is configured with `BUILD_WITH_LUA=ON`
- **THEN** The system SHALL compile and link against LuaJIT.

### Requirement: Scripting Runtime Exclusivity
When Lua is enabled, all other scripting runtimes (e.g., JS, Fennel, Moonscript, Python) SHALL be automatically disabled by the build system.

#### Scenario: Verifying other runtimes
- **WHEN** TIC-80 is built with `BUILD_WITH_LUA=ON`
- **THEN** No other language runtimes (Fennel, JS, etc.) shall be included in the final binary.

### Requirement: API Compatibility
The LuaJIT runtime SHALL expose the complete TIC-80 API to scripts, identical in functionality to the standard Lua runtime.

#### Scenario: Verifying API calls
- **WHEN** A standard TIC-80 script calls `pix(x, y, color)`
- **THEN** The API call SHALL function as expected with the LuaJIT runtime.
