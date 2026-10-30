<div align="center">

# 🧩 Gizmo Common

### Shared building blocks for the OpenGizmo ESP32 projects

Gizmo Common is a PlatformIO library with the code every OpenGizmo device needs: thread-safe serial logging and text formatting that works both on the board and in native tests on your computer.

<p>
  <a href="https://github.com/open-gizmo/gizmo-common"><img alt="GitHub Repo" src="https://img.shields.io/badge/github-open--gizmo%2Fgizmo--common-111111?logo=github"></a>
  <a href="https://github.com/open-gizmo/gizmo-common/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/open-gizmo/gizmo-common?label=release"></a>
  <img alt="ESP32" src="https://img.shields.io/badge/esp32-ESP32-E7352C?logo=espressif&logoColor=white">
  <img alt="PlatformIO" src="https://img.shields.io/badge/platformio-PlatformIO-FF6B35?logo=platformio&logoColor=white">
  <img alt="Arduino" src="https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white">
  <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-2563EB">
</p>

<p>
  🧾 Thread-safe logging • 🔢 Duration and number formatting • 🧪 Native unit tests • 🏷️ Versioned releases • 🔌 Drop-in for any gizmo
</p>

<p>
	<a href="#overview"><strong>Overview</strong></a> •
	<a href="#images"><strong>Images</strong></a> •
	<a href="#features"><strong>Features</strong></a> •
	<a href="#architecture"><strong>Architecture</strong></a> •
	<a href="#quick-start"><strong>Quick Start</strong></a> •
	<a href="#hardware"><strong>Hardware</strong></a> •
	<a href="#firmware-overview"><strong>Firmware Overview</strong></a> •
	<a href="#configuration"><strong>Configuration</strong></a> •
	<a href="#troubleshooting"><strong>Troubleshooting</strong></a> •
	<a href="#license"><strong>License</strong></a>
</p>

</div>

<a id="overview"></a>
## 📘 Overview

Every OpenGizmo device ends up needing the same small pieces: logs you can trust while debugging, and numbers and times that read well on a tiny screen. Copying that code between projects means fixing the same bug twice, so it lives here instead.

Each project pins an exact release of the library, so an update never changes a device by surprise: you bump the version when you are ready, build, and test. The library has its own native tests and CI, so the shared code is checked once, in one place.

Gizmo Common is used by [Gizmo Ticker](https://github.com/open-gizmo/gizmo-ticker) and [Gizmo Reolink Switch](https://github.com/open-gizmo/gizmo-reolink-switch).

<a id="images"></a>
## 🖼️ Images

### Example output

The [basic example](examples/basic/main.cpp) logs a formatted uptime and price once per second:

```text
gizmo-common basic example
Uptime 00:00, price 0.08123
Uptime 00:01, price 0.08123
Uptime 00:02, price 0.08123
```

<a id="features"></a>
## ✨ Features

- Serial logging with one shared, mutex-guarded buffer, safe to call from any FreeRTOS task.
- Compile-time checks of `printf`-style format strings against their arguments.
- `MM:SS` duration formatting that never cuts off long durations.
- Number formatting with US (`1,234.56`) or EU (`1.234,56`) separators, written into your buffer and cut off safely.
- Significant decimals for small values (`0.08123` instead of `0.08`).
- Formatting that also builds natively, so projects can test their own code against it on a computer.
- Native unit tests and an ESP32 compile check in CI on every push.
- Versioned GitHub releases, created by a one-click workflow.

<a id="architecture"></a>
## 🏗️ Architecture

Gizmo Common is built around three parts:

- 🧾 **Logging module** (`gizmo/log.h`) wraps the serial port for the ESP32 firmware.
- 🔢 **Formatting module** (`gizmo/format.h`) turns durations and numbers into text, on the board and on a computer.
- 🏷️ **Release workflow** tests the library and publishes a `vX.Y.Z` tag that projects pin in their `lib_deps`.

### 🔄 Data Flow

1. A project lists `https://github.com/open-gizmo/gizmo-common.git#vX.Y.Z` in its `lib_deps`.
2. PlatformIO downloads that exact tag and builds the library together with the project.
3. The project includes `<gizmo/log.h>` and `<gizmo/format.h>` and calls the `gizmo::` functions.
4. When the library changes, a new release gets a new tag, and each project moves to it only when its pin is bumped.

<a id="quick-start"></a>
## 🚀 Quick Start

### 📋 Requirements

- A PlatformIO project for the ESP32 with the Arduino framework.
- VS Code with the PlatformIO IDE extension installed, or PlatformIO Core installed locally.

### 🔧 Initial Setup

1. **Add the library to your project's `platformio.ini`, pinned to an exact release.**

	```ini
	lib_deps =
		https://github.com/open-gizmo/gizmo-common.git#v1.0.0
	```

	Check the [GitHub Releases](https://github.com/open-gizmo/gizmo-common/releases/latest) page for the latest version.

2. **Start logging first thing in `setup()`.**

	```cpp
	#include <gizmo/log.h>

	void setup() {
		gizmo::initLog(true); // Call first, before other tasks can log
		gizmo::printLogfln("Booted, %u tasks", 3);
	}
	```

3. **Format durations and numbers.**

	```cpp
	#include <gizmo/format.h>

	char duration[gizmo::DURATION_TEXT_SIZE];
	gizmo::formatDuration(125, duration, sizeof(duration)); // "02:05"

	char price[24];
	gizmo::formatNumber(0.08123, gizmo::getSignificantDecimals(0.08123), gizmo::NumberFormat::US, price, sizeof(price)); // "0.08123"
	```

<a id="hardware"></a>
## 🔌 Hardware

### Supported targets

| Target | Logging | Formatting |
| --- | --- | --- |
| ESP32 with the Arduino framework, core 3 (`espressif32@8.0.0`) | ✅ | ✅ |
| ESP32 with the Arduino framework, core 2 (`espressif32@6.13.0`) | ✅ | ✅ |
| Native tests on your computer (`platform = native`) | ➖ Skipped | ✅ |

The logging module needs the Arduino `Serial` port and FreeRTOS, so native builds leave it out automatically.

<a id="firmware-overview"></a>
## 🔍 Firmware Overview

### ⚙️ General

Everything lives in the `gizmo::` namespace, so it can't clash with project code. The headers are under [src/gizmo/](src/gizmo/), and [examples/basic](examples/basic/main.cpp) shows both modules at work.

### 🧩 Firmware Layers

The library is split into focused modules under `src/gizmo/`:

- 🧾 `log.*` prints logs through one shared buffer guarded by a mutex.
- 🔢 `format.*` formats durations and numbers.

### 🧾 Logging

- `initLog(isEnabled, baudRate)` starts the serial port and creates the mutex. Call it first in `setup()`: logs before it are dropped.
- `printLog()` and `printLogln()` print any value Arduino's `Serial` can print.
- `printLogf()` and `printLogfln()` take a `printf` format. The compiler checks it against the arguments when `-Wformat` is enabled (add `build_src_flags = -Wformat` to your environment).
- Lines up to 255 characters are printed in full, longer ones are cut off.

### 🔢 Formatting

- `formatDuration(seconds, buffer, size)` writes `MM:SS`. A `DURATION_TEXT_SIZE` buffer fits any value.
- `formatNumber(value, decimals, format, buffer, size)` adds thousands separators. `NaN` and infinity are shown as zero.
- `getSignificantDecimals(value, significantDigits, minDecimals, maxDecimals)` picks the decimals: 2 from 1 up, more below so small values keep 4 significant digits (with the defaults).

### 🧪 Tests

The formatting module is tested on your computer, no board needed:

```bash
pio test -e native   # Unit tests
pio run -e esp32dev  # Compile check of the whole library for the ESP32
```

The tests live in [test/](test/). Both commands run in CI on every push.

<a id="configuration"></a>
## ⚙️ Configuration

### Versioning

Releases are git tags following [semantic versioning](https://semver.org): a breaking change bumps the major version, so projects only change behavior when their pin is updated.

### Releasing a new version

1. Bump `version` in [library.json](library.json) on `feature`.
2. Merge `feature` into `develop`, then `develop` into `main`.
3. Run the **Release 📦** workflow by hand (Actions > Release > Run workflow, on `main`). It runs the tests and the ESP32 build, then creates the `vX.Y.Z` tag and a GitHub Release with generated notes.

Pushing a `v*` tag yourself works too: the workflow then checks that it matches `library.json` before releasing.

### Local development

While changing the library, a project can use a local copy instead of a release (don't commit this line):

```ini
lib_deps =
	symlink://../gizmo-common
```

<a id="troubleshooting"></a>
## 🧰 Troubleshooting

- Logs don't appear: call `gizmo::initLog(true)` first in `setup()`, and check the monitor speed matches the baud rate (115200 by default).
- `gizmo/log.h` functions are missing in native tests: logging is only built for Arduino targets. Test the code around it, or keep the logging calls out of the modules you test natively.
- Format warnings after adding the library: the compiler found a `printLogf()` argument that doesn't match its format. Fix the argument type or the format.
- A project still uses old library code: it is pinned to an older tag. Bump the `#vX.Y.Z` in its `lib_deps`, or delete its `.pio/libdeps` folder after changing the pin.
- The release workflow fails at "Validate tag version": the pushed tag doesn't match `version` in `library.json`.
- The release workflow fails at "Check the version is new": that version is already released. Bump `version` in `library.json` first.

<a id="license"></a>
## 📄 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

<p align="center">
	<img src="https://raw.githubusercontent.com/open-gizmo/.github/main/profile/images/open-gizmo-logo-with-text.min.png" alt="OpenGizmo" width="200">
	<br/>
	<strong>Real projects. Real hardware. Real fun.</strong>
	<br/>
	<sub>A small corner of the internet for ESP32 builds that are useful, remixable, and actually fun to use.</sub>
</p>