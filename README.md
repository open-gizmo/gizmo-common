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
- Web apps built into the firmware: the `gizmo-embed` tool gzips a static site into C++, and one call serves it with the right cache headers and a 404 page.
- A shared Nuxt layer for those web apps: the Wi-Fi pages, the dashboard layout, a device card with restart and forget network, and mock device routes for the dev server.
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
- 🗂️ **Embedded assets module** (`gizmo/embedded_assets.h`) serves a web app built into the firmware, and the **`gizmo-embed` tool** generates it from the app's build.
- 🧱 **Web app layer** (`web/`) is a Nuxt layer with the pages, components and mock routes every OpenGizmo web app shares.
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
	#include <gizmo/embedded_assets.h> // Web app built into the firmware
	#include <gizmo/device.h>          // Device details, restart and forget network
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
- 🗂️ `embedded_assets.*` finds and serves the files of an embedded web app.
- 🧱 `web/` is not firmware: it is the Nuxt layer of the web apps, and talks to the Wi-Fi and device APIs of the modules here.
- 📟 `device.*` serves the device details, and restarts the device or forgets the network when the web app asks.
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

### 📟 Device API

`gizmo::setupDeviceApi(server, { "Project Name", "v1.2.3" })` registers the device routes, and `gizmo::manageDevice()` in the loop performs the restarts they ask for:

| Endpoint | Response `data` |
| --- | --- |
| `GET /api/device` | `{ name, version, uptimeSeconds, freeHeap, ssid, accessPointActive, ip, hostname, signalQuality }` (the last three once connected, `signalQuality` from 0 to 100) |
| `POST /api/restart` | `null`, then the device restarts about a second later |
| `POST /api/forget-wifi` | `null`, then the device forgets the saved network and restarts into the setup hotspot |

`gizmo::scheduleRestart()` restarts the device the same way from a project's own routes.

### 🗂️ Embedded web apps

A project's web app (for example a Nuxt site built with `nuxt generate`) is built into the firmware in two steps:

1. **Generate the C++ from the site build.** Add the library to the web app as a dev dependency, pinned to the same tag as `lib_deps`, and run its `gizmo-embed` tool after the build:

	```json
	"scripts": {
		"build:embedded": "nuxt generate && gizmo-embed .output/public ../microcontroller/src/webui"
	},
	"devDependencies": {
		"gizmo-common": "github:open-gizmo/gizmo-common#vX.Y.Z"
	}
	```

	It writes `generated_assets.h` and `generated_assets.cpp` (keep them out of git, and generate them in CI).

2. **Serve the files in the server's not-found handler.**

	```cpp
	#include "webui/generated_assets.h"

	server.onNotFound([](AsyncWebServerRequest *request) {
		if (gizmo::handleEmbeddedAssetRequest(request, EMBEDDED_ASSETS, EMBEDDED_ASSET_COUNT)) {
			return;
		}
		request->send(404);
	});
	```

- Pages are served at their folder (`settings/index.html` at `/settings`), with or without a trailing slash.
- A missing page gets the site's `/404.html` with a 404 status; a missing API call, file or script gets a plain 404.
- Every file is sent gzipped. Files under `/_nuxt/` have a content hash in their name and are cached for a year, while pages and the build manifest are checked again on every load.

### 🧱 Shared web app layer

The web apps of the OpenGizmo projects share their Wi-Fi setup, error page, dashboard layout and device settings, so they live in a Nuxt layer in [web/](web/) instead of being copied. It gives an app:

- The pages `/wifi` and `/ap-setup` (the setup page of the hotspot), the network picker, the error page and the `dashboard` layout.
- `<GizmoDeviceCard />` for the settings page: name, firmware, uptime, free memory, saved network, address and signal from `GET /api/device`, with **Restart** and **Forget network** behind a confirmation.
- `fetchEsp()` (the device address comes from `runtimeConfig.public.espBaseUrl`), the Wi-Fi and device calls, and `handleBackendErrors()`, which turns an `errorId` into a translated message. The Wi-Fi state lives in the layer's own `useGizmoStore()`, never in the store of an app.
- `<OptionPickerDialog />`, and English and Italian strings under the `gizmo` key, merged with the strings of the app.
- Mock routes for the dev server (`/api/networks`, `/api/connect`, `/api/check-connection`, `/api/device`, `/api/restart`, `/api/forget-wifi`) that answer like the firmware, in `web/server/utils/gizmo-mock.js`.

To use it, add the library to the web app (like for `gizmo-embed`, pinned to the same tag as `lib_deps`), and the packages the layer needs from the app:

```json
"devDependencies": {
	"gizmo-common": "github:open-gizmo/gizmo-common#vX.Y.Z"
}
```

The layer needs `nuxt`, `@brumaombra/ui-vintage`, `@nuxtjs/i18n`, `@hugeicons/vue`, `@hugeicons/core-free-icons` and `tailwindcss` in the app (they are its peer dependencies, so the app keeps the versions). Then:

1. **Extend the layer** in `nuxt.config.ts`. Nuxt finds a layer by its folder, so the path has to be written out (`gizmo-common/web` alone is not found).

	```ts
	export default defineNuxtConfig({
		extends: ['./node_modules/gizmo-common/web']
	});
	```

2. **Give the layer the name, logo and pages of the app** in `app/app.config.ts`. The layout, the setup page and the error page read them, and the Wi-Fi page is added with the word `'wifi'`:

	```ts
	export default defineAppConfig({
		gizmo: {
			name: 'Holiday Strip',
			logo: '/logo.svg',
			homePath: '/',
			nav: [
				{ id: 'home', labelKey: 'nav.home.label', descriptionKey: 'nav.home.description', icon: Home01Icon, to: '/' },
				'wifi'
			]
		}
	});
	```

	The labels are keys of the app's own locale files.

3. **Let Tailwind scan the layer.** Tailwind 4 only reads the sources of the app, so add one line to its `main.css`, after `@import "tailwindcss"`:

	```css
	@import "gizmo-common/web/app/assets/css/gizmo.css";
	```

4. **Use the card** in the settings page with `<GizmoDeviceCard />`.
5. **Name the mock device** (dev server only). The mock routes come with the layer, and an app keeps its own routes for its own endpoints. A Nitro plugin gives the mock the name of the app, and its own mock state can build on `getGizmoMockState()`:

	```js
	import { configureGizmoMock } from 'gizmo-common/web/server/utils/gizmo-mock.js';

	export default defineNitroPlugin(() => {
		configureGizmoMock({ name: useAppConfig().gizmo.name });
	});
	```

To try the layer before a release, link the folder instead of the tag: `rm -rf node_modules/gizmo-common && ln -s /path/to/gizmo-common node_modules/gizmo-common`.

### 🧪 Tests

The formatting and embedded assets modules, the `gizmo-embed` tool and the web layer are tested on your computer, no board needed. The Wi-Fi setup and web API need a board, so CI compiles them through the examples:

```bash
pio test -e native   # Unit tests
npm test             # Tests of the gizmo-embed tool and of the web layer (run npm ci first)
pio run -e esp32dev -e esp32dev_wifi_setup  # Compile check of the library and its examples for the ESP32
```

The tests live in [test/](test/). Both commands run in CI on every push.

<a id="configuration"></a>
## ⚙️ Configuration

### Versioning

Releases are git tags following [semantic versioning](https://semver.org): a breaking change bumps the major version, so projects only change behavior when their pin is updated.

### Releasing a new version

1. Bump `version` in [library.json](library.json) and [package.json](package.json) on `feature`.
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