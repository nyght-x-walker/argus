# Argus – openFrameworks App (Student Project)

License plate recognition and flagging system, single-window GUI built
with openFrameworks 0.12, ofxImGui and ofxOpenCv. Phase 2 covers
image loading, the center viewport and the basic docked panels.
Phase 3 adds the PlateDetector contour heuristic with viewport
bounding box overlays.

## Dependencies

- openFrameworks 0.12 (`OF_ROOT` as set in `config.make`).
- ofxImGui cloned recursively into `<OF_ROOT>/addons/ofxImGui` and
  listed in `addons.make`.
- Compatibility patch for current OF headers (two added includes):
  - `addons/ofxImGui/src/imconfig.h`: `#include "ofConstants.h"`.
  - `addons/ofxImGui/src/BaseEngine.cpp`: `#include "ofUtils.h"`.
- System OpenCV resolved through pkg-config in `config.make`
  (Linux opencv5, macOS brew opencv4). The code uses the OpenCV C++
  API directly: the bundled ofxOpenCv wrapper still targets the
  removed C API (`IplImage`) and cannot compile against OpenCV 4 or
  later, so it is intentionally not listed in `addons.make`.
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

- In-app self-checks: `runStartupChecks()` and `runDetectorChecks()`
  run once in `setup()` and report to the Console panel and stdout.
- Smoke script (no framework, deterministic, fast):
  `./tests/smoke.sh` expects both verdict lines.
- Manual UI pass: dock layout tiles on first run, the image is
  centered and aspect-fit, the Run button draws candidate boxes with
  confidence labels, stub buttons log to Console, `R` re-runs.
- Formatting: `clang-format --dry-run --Werror src/ofApp.h
  src/ofApp.cpp src/PlateDetector.h src/PlateDetector.cpp` must pass
  against the root `.clang-format`.

## Known Limitations

- Detection is a contour heuristic stub; OCR, flag store and alerts
  land in later phases.
- Confidence scores are aspect-based estimates, not learned values.
