# Change: Publish the WCH bridge project

- Date: 2026-10-19
- Type: Project packaging
- Related metric: `docs/spec.md`
- Commit: pending

## 1. Previous state

The bridge source, build outputs, raw evidence and WCH SDK examples were mixed
inside a local experiment directory.

## 2. Approach

Publish only the target bridge source, selected vendor support files, tests,
tools, wiring, and release image. Keep machine-local presets, build cache, raw
captures and the full EVT archive out of Git.

## 3. Changes

- Added the template project entry and verification docs.
- Removed machine-specific tool locations from the shared CMake preset.
- Added vendor-code notice and local build ignores.
- Added environment-configurable toolchain discovery to the verification helper.

## 4. Validation

- Existing 2026-10-18 15-second real-stream and host-test results are recorded
  in `docs/verification.md`.
- A clean out-of-tree firmware build passed on 2026-10-19; no board was reflashed.

## 5. Limitations

Long-duration stability and complete recovery after FPGA loss remain open.
