# Agent instructions

For AI agents (Claude Code, Codex, Copilot and others) working on this repository: the shared library of the OpenGizmo gadgets (the firmware library in `src/`, the `gizmo-embed` tool and the Nuxt web layer in `web/`). The README explains how each part works and how the gadgets use it.

## With every change

1. **Write the changelog.** When the gadgets that use the library would notice the change (a feature, a fix, a new or renamed setting, a change in the web layer that users see), add a line under `## Unreleased` in [CHANGELOG.md](CHANGELOG.md), in the same commit. Write it for the people who update a gadget to the new version: what changed, and what a gadget has to do to adopt it (a new key in `app.config.ts`, a renamed text, new wiring). Internal changes (refactoring, tests, CI) need no line unless they change behavior.
2. **Keep the docs in sync.** Update the README when the way a gadget uses the library changes, and keep the conventions it describes (the order of the pages, the device page) true for every gadget.
3. **Check it before calling it done.** Run `npm test` (the tool and the web layer), `pio test -e native`, and the ESP32 build of the examples, like the CI does.

## Releasing

Follow "Releasing a new version" in the README. The version's notes must be moved from `## Unreleased` to a `## vX.Y.Z - YYYY-MM-DD` heading in CHANGELOG.md: they become the GitHub release notes, and the release workflow stops before tagging when they are missing or when notes are left under Unreleased. After a release, the gadgets move to the new tag in both `webapp/package.json` and `microcontroller/platformio.ini`, so the firmware and the web layer name the same version.

Work on the `feature` branch; changes reach `develop` and then `main` one step at a time, only for a release.
