# Argus — License Plate Recognition & Flagging System
## Architecture & Workflow

> Clean, testable desktop system for cybersecurity coursework. C++17 + OpenCV, single-window Qt UI, modular pipeline with watchlist alerting.

### 1. Overview

**Argus** ingests vehicle images or video frames, detects license plates, runs OCR, normalizes and validates the plate string, checks it against a flagged-plates watchlist (blocked / suspicious / authorized), raises an alert if matched, and logs every scan with decision and operator notes. The UI mockup is deployed at `https://nyght-x-walker.github.io/argus/` (`index.html` copy of `ui_mockup.html`).

**Primary users:** cybersecurity students, security operators in lab/demo settings. **Not for production** — educational scope, offline by default, no face/person tracking.

### 2. Goals / Non-Goals

**In scope:**
- Image + video frame plate detection & OCR with confidence.
- Flagged-plates CRUD (plate, type, reason, tags, `triggerAlert`, `Active/Inactive`).
- Alerting (popup + sound) with cooldown, structured logging, CSV/JSON export.
- Memory-aware processing (cache limits, batch, spill-to-disk hint).
- Deterministic, testable components with CMake + CTest.

**Out of scope:** real-time RTSP, multi-camera, cloud sync, face recognition, access-control enforcement (UI placeholder only).

### 3. Tech Stack

| Layer | Choice | Why |
|---|---|---|
| Language | C++17 (gcc ≥9) | Course environment, modern RAII/smart pointers, `std::optional` |
| Vision | OpenCV 4.8 `core, imgproc, imgcodecs, objdetect, videoio` | Plate ROI, `VideoCapture` for video, widely available on Linux |
| OCR | Tesseract 5.3 (C++ API) | Per-char confidence, EU/US/UK traineddata, offline |
| UI | Qt 6 Widgets (`QMainWindow`, `QStackedWidget`, `QGraphicsView`, `QTableView`) | Single window, matches HTML mockup tabs, signal/slot |
| Build | CMake 3.16+, target-based | `argus_core` lib + `argus_app` + `argus_tests` |
| Persist | `resources/flagged.json` (atomic write) + SQLite `argus.db` (logs, flagged mirror) | Human-readable + queryable; fsync on every FlagStore mutation |
| Deploy (mockup) | GitHub Pages `master`/`index.html` | `https://nyght-x-walker.github.io/argus/` |

### 4. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ UIController (QMainWindow + QStackedWidget)                      │
│  Dashboard | Scan (3-panel) | Flagged | Logs | Settings | Mem    │
└──────────────────────────┬──────────────────────────────────────┘
                           │ signals/slots, dependency injection
┌──────────────────────────▼──────────────────────────────────────┐
│ Pipeline (orchestrated per scan / per batch / per video frame) │
│ ImageSource → PlateDetector → PlateOCR → PlateValidator → FlagStore → AlertService → Logger │
└─────────────────────────────────────────────────────────────────┘
         │                │           │               │           │
    cv::Mat          vector<Rect> string+conf  normalized string optional<FlagEntry>  LogEntry → SQLite/CSV
```

**Project layout (target):**

```
argus/
  include/{image_source.h, plate_detector.h, plate_ocr.h, plate_validator.h, flag_store.h, alert_service.h, logger.h}
  src/{*.cpp, main.cpp}
  resources/{flagged.json, test_images/, haarcascade_russian_plate_number.xml}
  tests/{test_flag_store.cpp, test_plate_validator.cpp, test_logger.cpp}
  ui_mockup.html, index.html  # deployed mockup
  CMakeLists.txt, ARCHITECTURE.md, README.md
```

All headers use `#pragma once`, `namespace argus`, minimal includes. Implementation files <40 lines per function.

### 5. Components

**ImageSource** — loads `cv::Mat` from file, drag-drop, or `VideoCapture` frame. Returns `std::optional<cv::Mat>` + metadata `{path, w, h, bytes, fps, frameIdx, timestamp}`. Validates MIME + size (50 MB) + canonical path (`filesystem::canonical`, reject `..`).

**PlateDetector** — `std::vector<cv::Rect> detect(const cv::Mat&, std::vector<double>& confs)`. Pipeline comment block: `grayscale → bilateral → Canny → findContours → aspect 2–5, area 0.5%–15% of image → NMS`. Constants `constexpr double MIN_PLATE_AREA_FRACTION=0.005`. Returns empty on no plate (expected, not exception).

**PlateOCR** — `struct OcrResult{ std::string text; double meanConf; std::vector<CharConf> perChar; }` via `TessBaseAPI`. Config `oem LSTM, psm SINGLE_LINE, eu/us/uk lang` from Settings. Rejects if `meanConf < threshold`.

**PlateValidator** — `bool isValid(normalized)` + `std::string normalize(raw)` (upper, trim, `O→0, I→1, 5→S, '-' removal`) + `Region {EU,US,UK} → regex` (`EU: ^[A-Z]{1,3}[0-9]{1,4}[A-Z]{0,2}$`). Used in badge `✓ EU valid` and Flagged modal live preview.

**FlagStore** — in-memory `unordered_map<string, FlagEntry>` keyed by normalized plate. `FlagEntry{plate,type{Blocked|Suspicious|Authorized},reason,tags, addedDate, lastMatched, status, triggerAlert}`. API `std::optional<FlagEntry> lookup(norm)`, `add/update/remove`, `save()` atomic `write tmp → rename → fsync`, `load()` on start. Handles fuzzy (`levenshtein ≤1` optional, only if conf<85).

**AlertService** — `bool shouldAlert(optional<FlagEntry>, Settings)` checks `triggerAlert && status==Active && cooldown(now - lastAlert > X min)`. Side effect: popup + sound `QSystemTrayIcon::showMessage`, log; not in core pipeline.

**Logger** — append-only `logs` table `{ts, imagePath, thumbPath, plate, rawPlate, conf, perCharJson, flagMatch, flagType, region, decision{Allow|Block|Review}, operator, notes, memUsed}`. `exportCsv(filter)` streams filtered rows; `rotate()` daily; graceful `disk full → stderr + toast`.

**UIController** — owns `QStackedWidget` views (mockup maps 1:1). Left nav `QListView`, Scan 3-panel (`QGraphicsView` with `QGraphicsRectItem` bbox green/red/yellow), right `QFormLayout` decision + notes. Settings uses `QSettings` + local file `config.json`.

### 6. Data Flow & Sequences

**Flow A — Normal image:**
`drop image_01.jpg → ImageSource::load() → Detector → {rect, 92%} → OCR → "AB123CD" 92% → Validator normalize → FlagStore::lookup → null → Alert no-op → Logger::log(Normal, Allow) → Dashboard + Logs row (green), toast "Scan saved"`

**Flow C — Flagged video frame:**
`video traffic.mp4 @00:01.2 → VideoCapture → mat → Detector → OCR "AB123CD" 92% → Validator → lookup → Blocked {reason:Stolen} → Banner `⚠ FLAGGED PLATE` + AlertService popup (10s, stacks, bell +1) + Logger Flagged/Review → Logs red row + Recent Alerts panel`

**Batch (performance):**
`drop 10 images → QtConcurrent::mapped 2 threads → progress 3/10 30% bar (scanLeft batchProgress) → each mat 4.2 MB, decoded 11.8 MB, bump mem 0.14 GB/image → on >85% auto-clear 40% img cache → Cancel → bump -0.15 GB`

**Error:**
`corrupt.jpg → ImageSource throws → toast "libjpeg: bogus marker" + dropZone border-danger 2.6s`
`OCR fail → conf "—", perChar "OCR failed", validator ✗ invalid, forced Review radio, bbox hidden`

### 7. Data Models

```cpp
enum class FlagType{ Blocked, Suspicious, Authorized };
enum class FlagStatus{ Active, Inactive };
struct FlagEntry{
  std::string plate, reason, tags, addedDate, lastMatched;
  FlagType type; FlagStatus status; bool triggerAlert;
};
struct ScanEvent{
  std::string imagePath, plateRaw, plateNorm, region;
  double conf; std::vector<CharConf> perChar; bool flagMatch;
  std::optional<FlagEntry> flag; std::string decision, op, notes;
  std::chrono::system_clock::time_point ts; size_t memBytes;
};
```

### 8. Error Handling & Security

* Exceptions: file not found, invalid config, SQLite failure. `optional`/codes: no plate, low conf.
* Validate every external input (path canonicalized, image header sniffed, OCR string sanitized before log/compare, length capped 128).
* No global mutable state; DI in constructors `PlateDetector(detectorConfig)`, `FlagStore(storagePath)`.
* Do not log full images; thumbs blurred if `Settings.blur` (`GaussianBlur` ROI).

### 9. Memory Budget (as in mockup Memory panel)

`headerMem 1.4/4GB 34%, sidebar 1.4/4GB, Dashboard gauge 34% + breakdown: Image 420 MB, Video 180 MB, OCR 285 MB, FlagStore 12 MB, Logs 124 MB, Qt 210 MB, peak 2.14 GB, limit 3.2 GB`. Live drift `±0.02 GB /3.2s`, bump on load `+0.14 GB/image`, `+0.38 GB/video`. Auto-clear >85% frees 40% img + 70% vid.

### 10. Build, Test, Run

```bash
cmake -B build -S . && cmake --build build -j
./build/argus_app                 # Qt window
ctest --test-dir build --output-on-failure
# test_images/: clear_01.jpg, angled_02.jpg, night_03.jpg, no_plate_04.jpg, video_clip_05.mp4, noisy_06.jpg
```

Targets: `argus_core` (lib), `argus_app` (main), `argus_tests` (Catch2). `-Wall -Wextra -Werror`.

### 11. Testing Strategy

* Detector: known sample → ≥1 rect, no-plate → empty without throw.
* OCR: clear ROI → plausible text, conf>70; noisy ROI → no crash, returns `—`.
* Validator: `accept "AB123CD"`, `normalize "ab-123/cd" → "AB123CD"`, reject `"!!"`.
* FlagStore: exact + fuzzy toggle, case-insensitive, `triggerAlert` default ON for Blocked/Suspicious.
* Logger: append + export filtered + disk-full fallback.
* Coverage: 92% critical paths (mockup Settings Testing & Build accordion).

### 12. Workflow

**Task decomposition (example):**
1. `argus_core` scaffold + `ImageSource` + `PlateDetector` stub + `ctest` hello.
2. `PlateOCR` Tesseract wrapper + `PlateValidator` regex + normalize.
3. `FlagStore` JSON+SQLite + `AlertService` cooldown.
4. `Logger` + `UIController` Dashboard→Scan wiring.
5. Flagged CRUD + Logs detail + Settings persistence (`QSettings` + `memState`).

**Commit discipline:** one logical change, imperative message (`Add flag matching logic`), no mix of refactor+feature; branch `feat/...`, PR with checklist.

**Review checklist:** compiles `-Wall -Wextra`, no unused, <40-line funcs, error handling documented, no hard-coded paths, tests for new critical paths.

**Docs:** `ARCHITECTURE.md` (this file), `README.md` (build/run), mockup `https://nyght-x-walker.github.io/argus/`. Internal agentic workflow (`ARGUS_INTERNAL.md`) not part of submission.

### 13. Roadmap

* v0.2: `VideoCapture` double-buffered queue, `spill to disk` when cache > limit, per-char confidence sparkline in Logs.
* v0.3: Admin/Operator/Viewer roles (placeholder now), retention cron, blur pipeline benchmark.

---

*Mockup implemented first to lock layout/flows before C++ backend; take this file as source of truth for component boundaries and state.*
