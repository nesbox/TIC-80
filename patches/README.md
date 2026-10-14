# Patches
## Gravity
### gravity-patch.patch
- `gravity_parser.c`, `gravity_vm.c` : Remove ENV and FILE optional libraries.
- `gravity_config.h` : Force define `bzero` if it doesn't exist.
- `gravity_opt_json.c`, `gravity_vm.c`, `gravity_json.c` : Rename `json_parse` to prevent a conflict.