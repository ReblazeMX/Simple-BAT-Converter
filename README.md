# Simple BAT Converter

A lightweight Windows command-line utility for packaging BAT and CMD scripts into standalone EXE files.

**Product:** Simple BAT Converter (SBC)  
**Command:** `sbc`  
**Author:** ReblazeMX  
**License:** MIT  
**Version:** 1.0.0

---

## Description

Simple BAT Converter turns a Windows Batch script (`.bat` / `.cmd`) into a real native PE executable. The generated EXE embeds your script and a small launcher that runs it through `cmd.exe` at runtime. The original script file is not required after conversion.

```text
sbc -c example.bat
```

produces:

```text
example.exe
```

---

## Features

- Convert `.bat` and `.cmd` to standalone `.exe`
- Real Windows PE output (not a renamed batch file)
- Embedded script payload — no external `.bat` needed at runtime
- Argument forwarding (`%1`, `%2`, `%*`)
- Console or GUI (`--no-console`) subsystem
- Auto-pause when double-clicked from Explorer (window stays open)
- `--pause` / `--no-pause` to control end-of-run behavior
- Unicode-aware paths (UTF-16 Windows APIs)
- Force overwrite, quiet/verbose modes, dry-run, info
- Zero third-party dependencies (C++17 + Windows API)
- CMake build for MSVC and MinGW-w64

---

## Requirements

- Windows 7 or later (x64 or x86)
- To **build** SBC:
  - CMake 3.16+
  - A C++17 compiler: Visual Studio 2019+ (MSVC) or MinGW-w64
- To **run** generated EXEs: only Windows + `cmd.exe` (always present)

No compiler is required on machines that only *use* `sbc.exe` or run generated executables.

---

## Installation

### Pre-built binary

Copy `sbc.exe` somewhere on your `PATH`, or invoke it by full path.

### Building from source

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The tool is written to:

```text
build\bin\sbc.exe
```

**Visual Studio (MSVC) example:**

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**MinGW-w64 example:**

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Usage

```text
sbc [command] [options] [arguments]
```

### Compile

```bat
sbc -c example.bat
sbc -c example.bat -o example.exe
sbc --compile script.cmd --output myapp.exe
sbc -c script.bat -d build
sbc -c script.bat -f
sbc -c script.bat --verbose
sbc -c script.bat --no-console
sbc -c script.bat --pause
sbc -c script.bat --no-pause
```

### Other commands

```bat
sbc -h
sbc --help
sbc -v
sbc --version
sbc --license
sbc --info script.bat
sbc -c script.bat --dry-run
sbc --clean
```

### Pause behavior (generated EXE)

By default, a **console** EXE pauses at the end when it was **double-clicked** from Explorer, so the window does not close immediately:

```text
Press any key to continue . . .
```

It does **not** pause when launched from an existing Command Prompt or PowerShell session.

| Option | Effect on generated EXE |
|--------|-------------------------|
| *(default)* | Pause only when double-clicked (auto) |
| `--pause` | Always pause after the script finishes |
| `--no-pause` | Never pause after the script finishes |

`--no-console` builds have no console window; pause options do not apply to them in a visible way.

---

## CLI reference

| Option | Description |
|--------|-------------|
| `-c, --compile <file>` | Compile a `.bat` / `.cmd` file |
| `-o, --output <file>` | Output `.exe` path |
| `-n, --name <name>` | Output base name |
| `-d, --output-dir <dir>` | Output directory |
| `-f, --force` | Overwrite existing output |
| `-q, --quiet` | Suppress non-essential output |
| `-v, --verbose` | Verbose diagnostics |
| `--version` | Show version |
| `-h, --help` | Show help |
| `--license` | Show MIT license |
| `--no-console` | GUI subsystem (no console window) |
| `--console` | Console subsystem (default) |
| `--pause` | Always pause after the embedded script finishes |
| `--no-pause` | Never pause after the embedded script finishes |
| `--keep-temp` | Reserved |
| `--clean` | Clean temporary build files |
| `--dry-run` | Show actions without writing the EXE |
| `--info` | Show input file information |

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid arguments |
| 3 | Input file not found |
| 4 | Unsupported file type |
| 5 | Output error |
| 6 | Build error |
| 7 | Runtime / launcher error |

---

## Architecture overview

1. **Build time (this project)**  
   CMake builds two tiny launcher stubs (`console` and `no-console`), then embeds their PE bytes into `sbc.exe` as byte arrays.

2. **Conversion time (`sbc -c`)**  
   - Reads and validates the input script  
   - Selects the appropriate stub  
   - Appends: `[script bytes][original name][trailer with magic SBC1]`  
   - Writes the combined image as the output EXE  

3. **Runtime (generated EXE)**  
   - Locates its own path (`GetModuleFileNameW`)  
   - Reads the trailer from EOF (magic `SBC1`)  
   - Extracts the script to a randomized folder under `%TEMP%`  
   - Runs `cmd.exe /D /S /C` with the script and forwarded arguments  
   - Deletes temporary files  
   - Optionally pauses (auto / always / never — see `--pause` / `--no-pause`)  
   - Returns the script’s exit code  

Batch language features (`set`, `if`, `for`, `goto`, `call`, environment variables, etc.) are handled by Windows `cmd.exe`, not by a custom interpreter.

---

## Limitations

- Windows only (PE + `cmd.exe`)
- Generated EXEs are not obfuscated or encrypted; the script can be recovered from the binary
- Very large scripts increase EXE size one-to-one with the payload
- `--no-console` hides the console; scripts that need interactive console I/O should use the default console mode
- Pause-on-exit applies to console builds; use `--pause` / `--no-pause` to override the default auto behavior
- Antivirus products may flag generic “script packed into EXE” patterns; this is a known class of false positives for packagers

---

## Troubleshooting

**`Error: Unsupported input file type`**  
Only `.bat` and `.cmd` are accepted.

**`Error: Output file already exists`**  
Pass `-f` / `--force` to overwrite.

**`Error: Failed to build executable`**  
Check write permissions and that the output path is valid.

**Window flashes and closes when double-clicking the EXE**  
Rebuild with a current `sbc` (auto-pause is the default for console EXEs). To force a pause always:

```bat
sbc -c script.bat -f --pause
```

To disable pause (e.g. for automation):

```bat
sbc -c script.bat -f --no-pause
```

**Generated EXE exits immediately with code 7**  
The payload may be corrupted, or temp directory creation failed. Recompile with `sbc -c ... -f`.

**Paths with spaces**  
Quote them:

```bat
sbc -c "C:\My Scripts\example script.bat" -o "C:\Out\app.exe"
```

---

## Project structure

```text
SimpleBatConverter/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
├── cmake/
│   └── embed_stub.cmake
├── include/
│   ├── cli.hpp
│   ├── compiler.hpp
│   ├── exit_codes.hpp
│   ├── file_utils.hpp
│   ├── payload.hpp
│   └── version.hpp
├── src/
│   ├── main.cpp
│   ├── cli.cpp
│   ├── compiler.cpp
│   ├── file_utils.cpp
│   └── version.cpp
├── resources/
│   ├── sbc.rc
│   └── launcher/
│       └── launcher_main.cpp
├── tests/
│   ├── basic.bat
│   ├── arguments.bat
│   └── environment.bat
└── docs/
    └── usage.md
```

---

## License

MIT License — see [LICENSE](LICENSE).

```text
Copyright (c) 2026 ReblazeMX
```

---

## Author

**ReblazeMX**
