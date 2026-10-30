<div align="center">

# 🧩 Gizmo Common

### Shared building blocks for the OpenGizmo ESP32 projects

Gizmo Common is a PlatformIO library with the code every OpenGizmo device needs: Wi-Fi setup through a captive portal, a consistent web API, thread-safe serial logging, and text formatting that works both on the board and in native tests on your computer.

<p>
  <a href="https://github.com/open-gizmo/gizmo-common"><img alt="GitHub Repo" src="https://img.shields.io/badge/github-open--gizmo%2Fgizmo--common-111111?logo=github"></a>
  <a href="https://github.com/open-gizmo/gizmo-common/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/open-gizmo/gizmo-common?label=release"></a>
  <img alt="ESP32" src="https://img.shields.io/badge/esp32-ESP32-E7352C?logo=espressif&logoColor=white">
  <img alt="PlatformIO" src="https://img.shields.io/badge/platformio-PlatformIO-FF6B35?logo=platformio&logoColor=white">
  <img alt="Arduino" src="https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white">
  <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-2563EB">
</p>

<p>
  📡 Wi-Fi setup with captive portal • 🌐 Consistent web API • 🧾 Thread-safe logging • 🔢 Duration and number formatting • 🏷️ Versioned releases
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

Every OpenGizmo device ends up needing the same pieces: a way for the user to put it on their Wi-Fi without reflashing, a web API that behaves the same everywhere, logs you can trust while debugging, and numbers and times that read well on a tiny screen. Copying that code between projects means fixing the same bug twice, so it lives here instead.

The Wi-Fi setup covers the device side only: the connection, the setup hotspot, the captive portal and the web API. Each project ships its own setup page that talks to that API.

Each project pins an exact release of the library, so an update never changes a device by surprise: you bump the version when you are ready, build, and test. The library has its own native tests and CI, so the shared code is checked once, in one place.

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

- Wi-Fi setup without reflashing: an open setup hotspot with a captive portal, a network scan, and credentials that are saved only once they actually connect.
- Automatic reconnection, and a hotspot that turns itself off once the Wi-Fi is back.
- A web API with the same response format, CORS rules and body size limit on every gizmo.
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

Gizmo Common is built around these parts:

- 📡 **Wi-Fi setup module** (`gizmo/wifi_setup.h`) connects the device, runs the setup hotspot and serves the Wi-Fi API.
- 🌐 **Web API module** (`gizmo/http.h`) gives every endpoint the same responses, CORS checks and body handling.
- 🧾 **Logging module** (`gizmo/log.h`) wraps the serial port for the ESP32 firmware.
- 🔢 **Formatting module** (`gizmo/format.h`) turns durations and numbers into text, on the board and on a computer.
- 🏷️ **Release workflow** tests the library and publishes a `vX.Y.Z` tag that projects pin in their `lib_deps`.

### 🔄 Data Flow

1. A project lists `https://github.com/open-gizmo/gizmo-common.git#vX.Y.Z` in its `lib_deps`.
2. PlatformIO downloads that exact tag and builds the library together with the project.
3. The project includes the `<gizmo/...>` headers it needs and calls the `gizmo::` functions.
4. On a device without a saved network, the Wi-Fi setup opens the hotspot. The project's setup page uses the Wi-Fi API to scan, connect, and wait for the result.
5. When the library changes, a new release gets a new tag, and each project moves to it only when its pin is bumped.

<a id="quick-start"></a>
## 🚀 Quick Start

### 📋 Requirements

- A PlatformIO project for the ESP32 with the Arduino framework.
- VS Code with the PlatformIO IDE extension installed, or PlatformIO Core installed locally.

### 🔧 Initial Setup

1. **Add the library to your project's `platformio.ini`, pinned to an exact release.**

	```ini
	lib_deps =
		https://github.com/open-gizmo/gizmo-common.git#vX.Y.Z
	```

	Replace `vX.Y.Z` with the latest version from the [GitHub Releases](https://github.com/open-gizmo/gizmo-common/releases/latest) page. The libraries it needs come along automatically, pinned by the library.

2. **Include the headers of the modules you need.**

	```cpp
	#include <gizmo/wifi_setup.h> // Wi-Fi setup and captive portal
	#include <gizmo/http.h>       // Web API helpers
	#include <gizmo/log.h>        // Logging
	#include <gizmo/format.h>     // Duration and number formatting
	```

3. **Start from the examples.** [examples/basic](examples/basic/main.cpp) shows logging and formatting, and [examples/wifi_setup](examples/wifi_setup/main.cpp) shows a complete Wi-Fi setup. Each function is documented in its header under [src/gizmo/](src/gizmo/).

<a id="hardware"></a>
## 🔌 Hardware

### Supported targets

| Target | Wi-Fi setup and web API | Logging | Formatting |
| --- | --- | --- | --- |
| ESP32 with the Arduino framework, core 3 (`espressif32@8.0.0`) | ✅ | ✅ | ✅ |
| Native tests on your computer (`platform = native`) | ➖ Skipped | ➖ Skipped | ✅ |

The Wi-Fi setup, the web API and logging need the ESP32 (Wi-Fi, `Serial`, FreeRTOS), so native builds leave them out automatically, and the web server isn't even downloaded. A project only pays in flash for the modules it calls. The library is built and tested on Arduino core 3.

<a id="firmware-overview"></a>
## 🔍 Firmware Overview

### ⚙️ General

Everything lives in the `gizmo::` namespace, so it can't clash with project code. The headers are under [src/gizmo/](src/gizmo/), each function is documented there, and the [examples](examples/) show the modules at work.

### 🧩 Firmware Layers

The library is split into focused modules under `src/gizmo/`:

- 📡 `wifi_setup.*` runs the Wi-Fi connection, the setup hotspot, the captive portal and the Wi-Fi API.
- 🌐 `http.*` sends API responses, checks CORS and reads request bodies.
- 🧾 `log.*` prints logs through one shared buffer guarded by a mutex.
- 🔢 `format.*` formats durations and numbers.

### 📡 Wi-Fi setup

- Never blocks the main loop: it connects, retries, and opens an open setup hotspot whenever there's no working network.
- New credentials are tried first and saved only once they connect. If they fail, the device goes back to the saved network.
- The hotspot turns itself off once the setup page has seen the connection succeed, or shortly after the Wi-Fi comes back.
- Only clients connected through the hotspot are redirected to the setup page. Clients on the normal network are never redirected.
- The saved network lives in its own NVS namespace, apart from the project's settings.

The Wi-Fi API the setup page talks to:

| Endpoint | Request | Response `data` |
| --- | --- | --- |
| `GET /api/networks` | Starts a scan (the first call may return an empty list) | `{ networks: [{ ssid, signal, quality, channel, secured }] }` |
| `POST /api/connect` | `{ ssid, password }` (empty password for open networks, up to 32 and 64 characters) | `{ connectionStatus }` |
| `GET /api/check-connection` | Poll after `connect` until the status is not `2` | `{ connectionStatus, ssid, ip, hostname }` (the last three once connected) |

`connectionStatus` is `0` (not connected), `1` (connected) or `2` (connecting).

### 🌐 Web API

Every gizmo answers its API the same way, so any UI can talk to any device:

- Every response has the shape `{ errorId, errorMessage, data }`, with `errorId` and `errorMessage` set to `null` on success.
- Only same-origin requests are accepted, so other websites can't call the device from a visitor's browser.
- Request bodies are limited to 2 KB.

### 🧪 Tests

The formatting module is tested on your computer, no board needed. The Wi-Fi setup and web API need a board, so CI compiles them through the examples:

```bash
pio test -e native   # Unit tests
pio run -e esp32dev -e esp32dev_wifi_setup  # Compile check of the library and its examples for the ESP32
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

- The setup hotspot doesn't appear: it only opens when there's no saved network or the saved one fails to connect (about 12 seconds after boot). Check the serial logs.
- The phone doesn't open the setup page by itself: open any `http://` address in the browser while connected to the hotspot, and you'll be redirected.
- `<hostname>.local` addresses don't work: mDNS isn't supported by every router and Android version. Use the IP address shown by `/api/check-connection` instead.
- `invalid_connection_field` when connecting: the network name is longer than 32 characters or the password longer than 64.
- Logs don't appear: start logging first thing in `setup()` (earlier logs are dropped), and check the monitor speed matches the baud rate (115200 by default).
- Wi-Fi setup, web API or logging functions are missing in native tests: they are only built for the ESP32. Keep them out of the modules you test natively.
- Format warnings after adding the library: the compiler found a log argument that doesn't match its `printf` format. Fix the argument type or the format.
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