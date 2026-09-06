# Argus – openFrameworks App (Student Project)

License plate recognition and flagging system, single-window GUI built
with openFrameworks 0.12, ofxImGui, OpenCV and Tesseract 5. Image
loading, the center viewport and the basic docked panels are in,
plus the PlateDetector contour heuristic with viewport bounding
box overlays and Tesseract reading of the best candidate.

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
- Tesseract 5 with English data (`brew install tesseract` on macOS;
  system `tesseract` and `lept` pkg-config modules on Linux).
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

- In-app self-checks: `runStartupChecks()`, `runDetectorChecks()`
  and `runOcrChecks()` run once in `setup()` and report to the
  Console panel and stdout.
- Smoke script (no framework, deterministic, fast):
  `./tests/smoke.sh` expects all three verdict lines.
- Manual UI pass: dock layout tiles on first run, the image is
  centered and aspect-fit, the Run button draws candidate boxes with
  confidence labels plus the recognized text on the best box, the
  inspector shows plate text with per-character chips, `R` re-runs.
- Formatting: `clang-format --dry-run --Werror src/ofApp.h
  src/ofApp.cpp src/PlateDetector.h src/PlateDetector.cpp
  src/PlateOCR.h src/PlateOCR.cpp` must pass against the root
  `.clang-format`.

## Known Limitations

- Detection is a contour heuristic stub; boxes often miss the plate
  until OCR-backed validation lands.
- Small or angled plates read poorly; ROI upscaling is future work.
- Confidence scores mix aspect estimates with Tesseract values.
