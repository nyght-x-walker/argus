# Argus – openFrameworks App (Student Project)

License plate recognition and flagging system, single-window GUI built
with openFrameworks 0.12 and ofxImGui. Phase 2 covers image loading,
the center viewport and the basic docked panels.

## Dependencies

- openFrameworks 0.12 (`OF_ROOT` as set in `config.make`).
- ofxImGui cloned recursively into `<OF_ROOT>/addons/ofxImGui` and
  listed in `addons.make`.
- Compatibility patch for current OF headers (two added includes):
  - `addons/ofxImGui/src/imconfig.h`: `#include "ofConstants.h"`.
  - `addons/ofxImGui/src/BaseEngine.cpp`: `#include "ofUtils.h"`.
- A sample image at `resources/images/car_01.jpg`, also copied to
  `bin/data/resources/images/car_01.jpg` so the runtime data path
  resolves it.

## Build and Run

```bash
cd Argus
make
./bin/Argus
```

## Testing

- In-app self-check: `ofApp::runPhase2Tests()` runs once in `setup()`
  and reports `Phase 2 tests: OK` to the Console panel and stdout.
- Smoke script (no framework, deterministic, fast):
  `./tests/smoke_phase2.sh`.
- Manual UI pass: dock layout tiles on first run, the image is
  centered and aspect-fit, stub buttons log to Console, all bottom
  tabs open, `R` re-runs, panel toggles work.
- Formatting: `clang-format --dry-run --Werror src/ofApp.h
  src/ofApp.cpp` must pass against the root `.clang-format`.

## Known Limitations

- Detection, OCR, flag store and alerts are stubs for later phases.
- The sample image is a generated placeholder until real captures land.
