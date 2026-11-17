# Changelog

What changed in each release. New notes go under **Unreleased** while the work is done; a release moves them under its version, and the release workflow publishes that section as the GitHub release notes (it refuses to release a version without them).

## Unreleased

- The Wi-Fi connection and the factory reset hold are now explicit state machines (`wifi_connection.h`, `factory_reset_hold.h`), built natively and covered by tests: boot, retries, wrong credentials, changing network while connected, router reboots, the hotspot shutdown and `millis()` rollover. The device behaves as before, apart from the two fixes below.
- `getWifiStatus()` and `/api/check-connection` say `Disconnected` once the Wi-Fi drops. They kept saying `Connected` until a retry failed.
- A setup page that polls `/api/check-connection` right after `/api/connect` can't read the previous status anymore: the request now counts as `Connecting` from the moment it arrives.
- `WifiStatus` moved to `gizmo/wifi_connection.h`, and `FACTORY_RESET_HOLD_MS` to `gizmo/factory_reset_hold.h` (now a `uint32_t`). `wifi_setup.h` and `device.h` include them, so gadgets have nothing to change.

## v2.5.2 - 2026-11-14

- Device page: the hostname is the mDNS name the device answers on (for example `gizmo-ticker.local`), like the setup page shows. It was the Wi-Fi's default name (`esp32s3-XXXXXX.local`), which doesn't open the device.
- The Wi-Fi also takes the mDNS name, so the router lists the device by it instead of `esp32s3-XXXXXX`.
- New `gizmo::getHostname()`, the configured mDNS name without `.local`.

## v2.5.1 - 2026-11-13

- Device card: the first group is now **Info**, so it doesn't repeat the title of the Device page that holds the card.

## v2.5.0 - 2026-11-13

- Web layer: an optional dark theme logo, `logoDark` in the app config, shown in the sidebar, the setup page and the error page. Without it, the same logo shows in both themes.
- README: the device page and the order of the pages of every OpenGizmo app (the gadget's pages, then Device, then WiFi).

## Earlier versions

See the [GitHub releases](https://github.com/open-gizmo/gizmo-common/releases) up to v2.4.1.
