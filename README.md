# Tecflix — Phase A

Monorepo skeleton for the Tecflix video platform. Phase A establishes the C++20
service boundaries, React frontend, observability configuration, and architectural
decision records; business persistence and authentication are intentionally
placeholders.

## Build

Install CMake and vcpkg, then configure with the vcpkg toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

The C++ services require Boost.Beast and Boost.System. On Windows, install
vcpkg and set `VCPKG_ROOT` before opening or reloading the CMake project:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
Set-Location C:\dev\vcpkg
.\bootstrap-vcpkg.bat
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\dev\vcpkg", "User")
.\vcpkg.exe install --triplet x64-windows
```

Restart CLion after setting `VCPKG_ROOT`, then select the
`windows-debug` CMake preset. If vcpkg is installed elsewhere, replace
`C:\dev\vcpkg` in the commands and in `CMakePresets.json`.

If CLion uses a manually created CMake profile instead of the preset, add this
exact option under **CMake options**:

```text
-DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
-DVCPKG_TARGET_TRIPLET=x64-windows
```

Boost cannot be discovered until that toolchain has been used at configure
time. Changing the option requires deleting the CMake build directory or
selecting **Reset Cache and Reload Project**.

Run a service and check `http://localhost:8080/health` or `/ready`.

## Layout

- `services/common`: shared HTTP bootstrap and health responses.
- `services/*-api`: independently deployable C++20 service targets.
- `frontend`: React + TypeScript + Vite shell.
- `docs`: architecture, decisions, data structures, streaming, and operations.
- `observability`: OpenTelemetry, Prometheus, and Grafana placeholders.

The prototype files `PáginaInicio.cpp` and `PáginaInicio.h` were discarded as
part of Phase A; they are not part of the production skeleton.
