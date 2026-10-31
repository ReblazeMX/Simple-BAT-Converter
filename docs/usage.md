# Simple BAT Converter — Usage Guide

## Quick start

```bat
sbc -c hello.bat
hello.exe
```

Double-clicking `hello.exe` from Explorer runs the embedded script. For console builds, the window stays open at the end by default (`Press any key to continue . . .`).

---

## Passing arguments

`script.bat`:

```bat
@echo off
echo Arg1=%~1
echo Arg2=%~2
echo All=%*
```

```bat
sbc -c script.bat -o script.exe
script.exe one two three
```

---

## Output control

```bat
sbc -c app.bat -o dist\app.exe -f
sbc -c app.bat -n MyApp -d build
sbc -c app.bat --dry-run --verbose
```

The output `.exe` defaults to the current working directory, named after the input file.

---

## Custom icon

Embed a `.ico` file as the generated executable's icon with `--icon` (or `-i`):

```bat
sbc -c app.bat --icon app.ico -o app.exe
```

The icon works for both console and `--no-console` builds and is stored as a native Windows icon resource.

---

## Keeping and cleaning temporary files

By default the generated EXE deletes its extracted script after running. To keep it (for debugging), compile with `--keep-temp`; the launcher then prints the location of the retained files. Leftovers can be removed at any time with:

```bat
sbc --clean
```

---

## Console vs no-console

### Console (default)

```bat
sbc -c tool.bat --console -o tool.exe
```

A normal console window is shown. Suitable for scripts that print output or wait for input.

### No console window

Useful for scripts launched from shortcuts that should not flash a console:

```bat
sbc -c silent.bat --no-console -o silent.exe
```

The launcher runs `cmd.exe` with `CREATE_NO_WINDOW`, so no console window appears at all and `echo` output is not visible. Interactive scripts (e.g. `pause` or `set /p`) should use the default console mode instead.

---

## Pause after script

Controls whether the generated **console** EXE waits for a keypress when finished.

| Option | Behavior |
|--------|----------|
| *(default)* | Pause only when double-clicked from Explorer |
| `--pause` | Always pause after the script finishes |
| `--no-pause` | Never pause (good for scripts used in pipelines) |

Examples:

```bat
sbc -c demo.bat -f
sbc -c demo.bat -f --pause
sbc -c demo.bat -f --no-pause
```

When pause is active, the launcher shows:

```text
Press any key to continue . . .
```

Running the EXE from an existing Command Prompt does **not** pause in auto mode, so interactive CLI use stays normal.

---

## Inspecting an input

```bat
sbc --info myscript.cmd
```

Shows path, size, default output name, and selected subsystem.

---

## Help, version, and license

```bat
sbc -h
sbc --help
sbc --version
sbc --license
```

---

## Full example session

```bat
sbc -c tests\basic.bat -o out\basic.exe -f --verbose
out\basic.exe
sbc -c tests\arguments.bat -o out\args.exe -f --no-pause
out\args.exe hello world
```
