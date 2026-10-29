# 🧩 gizmo-common

Shared helpers for the [OpenGizmo](https://github.com/open-gizmo) ESP32 projects, packaged as a PlatformIO library.

## 📦 Modules

- 🧾 **`gizmo/log.h`**: serial logging with one shared, mutex-guarded buffer (safe from any FreeRTOS task) and compile-time checks of `printf` format strings. Arduino targets only.
- 🔢 **`gizmo/format.h`**: text formatting that works on the board and in native tests:
  - `formatDuration()`: seconds as `MM:SS`, never cut off (`DURATION_TEXT_SIZE` fits any value).
  - `formatNumber()`: thousands separators in US (`1,234.56`) or EU (`1.234,56`) style, written into your buffer and cut off safely.
  - `getSignificantDecimals()`: decimals that keep small values readable (`0.08123` instead of `0.08`).

## 🚀 Usage

Pin an exact version in your project's `platformio.ini`:

```ini
lib_deps =
	https://github.com/open-gizmo/gizmo-common.git#v1.0.0
```

Then:

```cpp
#include <gizmo/format.h>
#include <gizmo/log.h>

void setup() {
	gizmo::initLog(true); // Call first, before other tasks can log
	gizmo::printLogfln("Booted, %u tasks", 3);
}
```

See [examples/basic](examples/basic/main.cpp) for more.

While changing the library itself, a project can point at a local copy instead (don't commit this line):

```ini
lib_deps =
	symlink://../gizmo-common
```

## 🧪 Development

```bash
pio test -e native   # Unit tests on your computer
pio run -e esp32dev  # Compile check for the ESP32
```

Both run in CI on every push.

## 🏷️ Versioning

Releases are git tags following [semantic versioning](https://semver.org): a breaking change bumps the major version, so projects only change behavior when you update their pin.

To release:

1. Bump `version` in `library.json` and push it.
2. Run the **Release 📦** workflow by hand (Actions > Release > Run workflow, on the branch to release). It runs the tests and the ESP32 build, then creates the `v<version>` tag and a GitHub Release with generated notes.

Pushing a `v*` tag yourself works too: the workflow then checks that it matches `library.json` before releasing.

## 📄 License

MIT, see [LICENSE](LICENSE).
