# Argus — License Plate Recognition & Flagging System

<p align="center">
  <img src="https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" />
  <img src="https://img.shields.io/badge/OpenCV-4.8-5C3EE8?style=for-the-badge&logo=opencv&logoColor=white" />
  <img src="https://img.shields.io/badge/Qt-6-41CD52?style=for-the-badge&logo=qt&logoColor=white" />
  <img src="https://img.shields.io/badge/CMake-3.16-064F8C?style=for-the-badge&logo=cmake&logoColor=white" />
  <img src="https://img.shields.io/badge/Mockup-Live-8be9fd?style=for-the-badge" />
</p>

<p align="center">
  <a href="https://nyght-x-walker.github.io/argus/"><b>▶ Live Mockup</b> — nyght-x-walker.github.io/argus</a> • Clean, testable desktop system for cybersecurity coursework
</p>

> **Not for production** — educational, offline-first, no face/person tracking. Mockup first to lock layout & flows before C++ backend.

---

## 1) At a Glance

| | |
|---|---|
| **Users** | Cybersecurity students · security operators (lab/demo) |
| **What it does** | Ingest **image / video frame** → detect plate → OCR → **normalize & validate** → check **flagged watchlist** (Blocked / Suspicious / Authorized) → **alert** if matched → **log** with decision & notes |
| **Inputs** | Drag & drop JPG/PNG/BMP/MP4 (50 MB, batch ≤10), camera frame |
| **Outputs** | Bounding box + plate text + confidence + per-char detail, flag badge, alert popup + sound, CSV/JSON export |

> **Try in mockup:** `Dashboard → drop traffic.mp4 → Scan → Process Frame → Save → Logs (red row + Recent Alerts)` or `Dashboard → Demo batch 10`.

---

## 2) Tech Stack

| Layer | Choice | Why |
|---|---|---|
| **Lang** | C++17 (gcc ≥9) | Course env, RAII, `std::optional`, smart pointers |
| **Vision** | OpenCV 4.8 `core/imgproc/imgcodecs/objdetect/videoio` | Plate ROI + `VideoCapture`, Linux-native |
| **OCR** | Tesseract 5.3 C++ API | Per-char confidence, EU/US/UK traineddata, offline |
| **UI** | Qt 6 Widgets | Single `QMainWindow` + `QStackedWidget` → 1:1 with HTML mockup `ui_mockup.html:1` |
| **Build** | CMake 3.16+ `target_*` | `argus_core` lib + `argus_app` + `argus_tests` |
| **Persist** | `resources/flagged.json` (atomic `tmp→rename→fsync`) + SQLite `argus.db` | Human-readable + queryable |
| **Mockup** | HTML/Tailwind + vanilla JS, GitHub Pages `master/index.html` | `https://nyght-x-walker.github.io/argus/` |

---

## 3) System Context

```mermaid
flowchart LR
  Operator([Operator]) -- drag/drop image or video --> UI
  UI -- triggers pipeline --> Argus
  Argus -- reads --> FlaggedStore[(flagged.json + SQLite)]
  Argus -- writes --> Logs[(argus.db + CSV/JSON)]
  Argus -- renders --> UI
  UI -- alert popup + sound + banner --> Operator

  subgraph UI[Qt / HTML Mockup]
    Dashboard
    Scan
    Flagged
    Logs
    Settings
  end

  subgraph ArgusCore[C++ Core]
    ImageSource --> PlateDetector --> PlateOCR --> PlateValidator --> FlagStore --> AlertService --> Logger
  end
```

---

## 4) High-Level Architecture

```mermaid
graph TD
  subgraph Window[UIController — QMainWindow]
    Nav[Left Nav / Bottom Tabs]
    Stack[QStackedWidget]
    Nav --- Stack
    Stack --- D[Dashboard]
    Stack --- S[Scan 3-panel]
    Stack --- F[Flagged]
    Stack --- L[Logs]
    Stack --- Set[Settings + Memory + Testing]
  end

  S --> Pipeline

  subgraph Pipeline[Pipeline — per scan / per batch / per video frame]
    IS[ImageSource<br/>cv::Mat + meta]
    PD[PlateDetector<br/>vector Rect + conf]
    OCR[PlateOCR<br/>OcrResult text + perChar]
    PV[PlateValidator<br/>normalize + regex]
    FS[FlagStore<br/>lookup norm → FlagEntry?]
    AS[AlertService<br/>cooldown check]
    LG[Logger<br/>ScanEvent → SQLite]
    IS --> PD --> OCR --> PV --> FS --> AS --> LG
  end

  Pipeline <--> UI
  FS <--> DB[(flagged.json<br/>+ SQLite)]
  LG --> DB
  UI --> Toast[Toast Stack + Bell]
```

**Project layout (target):**

```text
argus/
  include/  image_source.h  plate_detector.h  plate_ocr.h  plate_validator.h  flag_store.h  alert_service.h  logger.h
  src/      image_source.cpp ...  main.cpp
  resources/ flagged.json  test_images/{clear_01.jpg, angled_02.jpg, night_03.jpg, no_plate_04.jpg, noisy_06.jpg, video_clip_05.mp4}
  tests/    test_plate_detector.cpp  test_plate_validator.cpp  test_flag_store.cpp
  ui_mockup.html  index.html  ARCHITECTURE.md  CMakeLists.txt
```

> Conventions: `include/*.h` `#pragma once`, `namespace argus`, minimal includes; `src/*.cpp` functions **<40 lines** `CODE_STYLE.md:114`.

---

## 5) Components

| Component | API | Key Details |
|---|---|---|
| **ImageSource** | `std::optional<Mat> load(path/videoFrame)` + `{w,h,bytes,fps,frameIdx,ts}` | MIME + 50 MB guard, `filesystem::canonical` reject `..`, header sniff |
| **PlateDetector** | `vector<Rect> detect(Mat, vector<double>& confs)` | `gray → bilateral → Canny → findContours → aspect 2–5, area 0.5%–15% → NMS`. `constexpr MIN_PLATE_AREA_FRACTION=0.005`. Empty = expected |
| **PlateOCR** | `OcrResult{string text; double meanConf; vector<CharConf> perChar;}` | `TessBaseAPI`, `oem LSTM`, `psm SINGLE_LINE`, lang `eu/us/uk` from Settings |
| **PlateValidator** | `string normalize(raw)` / `bool isValid(norm)` / `Region` | `upper, trim, O→0, I→1, 5→S, '-' strip` + `EU ^[A-Z]{1,3}[0-9]{1,4}[A-Z]{0,2}$`. Live preview in badge + Flagged modal |
| **FlagStore** | `optional<FlagEntry> lookup(norm)` / `add/update/remove` / `save()` | `unordered_map<norm,FlagEntry>` + atomic `tmp→rename→fsync`. Fuzzy `lev≤1` only if `conf<85` |
| **AlertService** | `bool shouldAlert(entry, Settings)` | `triggerAlert && status==Active && cooldown(now-lastAlert>5min)` → `QSystemTrayIcon::showMessage` |
| **Logger** | `log(ScanEvent)` / `exportCsv(filter)` / `rotate()` | `logs{ts,imagePath,thumbPath,plateRaw,plateNorm,conf,perCharJson,flagMatch,flagType,region,decision,op,notes,mem}`. Graceful `ENOSPC → toast` |
| **UIController** | `QStackedWidget` + signal/slot | Maps 1:1 to mockup views; `QGraphicsView+RectItem` bbox green/red/yellow; `QSettings+config.json` |

```mermaid
classDiagram
  class FlagEntry {
    +string plate
    +FlagType type
    +string reason, tags
    +string addedDate, lastMatched
    +FlagStatus status
    +bool triggerAlert
  }
  class ScanEvent {
    +string imagePath, plateRaw, plateNorm, region
    +double conf
    +vector~CharConf~ perChar
    +bool flagMatch
    +optional~FlagEntry~ flag
    +string decision, op, notes
    +time_point ts
    +size_t memBytes
  }
  class OcrResult {
    +string text
    +double meanConf
    +vector~CharConf~ perChar
  }
  FlagEntry <-- ScanEvent
  OcrResult --> ScanEvent
```

---

## 6) Data Flow & Sequences

### Flow A — Normal image

```mermaid
sequenceDiagram
  participant U as Operator
  participant IS as ImageSource
  participant PD as PlateDetector
  participant OCR as PlateOCR
  participant PV as PlateValidator
  participant FS as FlagStore
  participant LG as Logger
  participant UI as UI (Dashboard/Logs)
  U->>IS: drop image_01.jpg
  IS->>PD: Mat (1920×1080)
  PD-->>OCR: Rect + 92%
  OCR-->>PV: "AB123CD" 92% + perChar
  PV-->>FS: norm "AB123CD" (EU valid)
  FS-->>LG: lookup → null
  LG-->>UI: log Normal/Allow + green row + toast "Scan saved"
```

### Flow C — Flagged video frame

```mermaid
sequenceDiagram
  participant U as Operator
  participant VC as VideoCapture
  participant PD as PlateDetector
  participant OCR as PlateOCR
  participant PV as PlateValidator
  participant FS as FlagStore
  participant AS as AlertService
  participant LG as Logger
  U->>VC: traffic.mp4 @00:01.2
  VC->>PD: Mat frame
  PD-->>OCR: Rect + 92%
  OCR-->>PV: "AB123CD" 92%
  PV-->>FS: norm → Found BLOCKED {Stolen}
  FS->>AS: shouldAlert? triggerAlert+Active+cooldown
  AS-->>U: ⚠ Banner + popup (10s, stacks) + bell+1 + sound
  AS->>LG: Flagged/Review → Logs red row + Recent Alerts
```

### Batch & Video (performance)

```mermaid
flowchart LR
  B[Drop 10 images] --> Q[QtConcurrent::mapped<br/>2 threads]
  Q --> P[Progress 3/10 30%<br/>scanLeft batchProgress]
  P --> M[Each mat 4.2 MB<br/>decoded 11.8 MB<br/>+0.14 GB/image]
  M --> C{>85%?}
  C -->|yes| A[Auto-clear 40% img cache<br/>bump -0.15 GB]
  C -->|no| N[Cancel?]
  N --> E[Done]
```

**Error paths:** `corrupt.jpg → ImageSource throw → toast "libjpeg: bogus marker" + dropZone border-danger 2.6s` · `OCR fail → conf "—" / perChar "OCR failed" / validator ✗ invalid / forced Review + bbox hidden`.

---

## 7) Error Handling & Security

| Concern | Rule |
|---|---|
| **Exceptions** | file not found, invalid config, SQLite failure |
| **Optional** | no plate, low conf (`conf=="—"`), empty ROI |
| **Input** | `filesystem::canonical` reject `..`, MIME sniff, OCR `sanitize` + cap 128, no log of full images |
| **State** | No global mutable; DI `PlateDetector(cfg)`, `FlagStore(path)` |
| **Privacy** | `Settings.blur` → `cv::GaussianBlur` ROI only for `thumbs/` |

---

## 8) Memory Budget (live in mockup)

```mermaid
pie title Memory Breakdown (1.42 / 4.0 GB = 34%)
  "Image buffers" : 420
  "Video cache" : 180
  "OCR engine" : 285
  "Logs & thumbs" : 124
  "Qt / overhead" : 210
  "FlagStore" : 12
  "Headroom" : 2600
```

*Shown in 4 places:* header chip `MEM 1.4GB 34%` bar `29:1` → click scrolls to `memoryPanel` `96:1`; sidebar widget `42:1` `1.4/4GB peak 2.1GB`; Dashboard gauge `34% OK • headroom 2.6GB` + bars + 24-bar sparkline (5-min history); Scan bottom `Mem 342 MB` + right `MEMORY (THIS VIEW)` card. **Policy:** `Settings → Memory Management` limits `Max 3.20 GB / Image cache 512 MB / 24 frames`, `Auto-clear >85%` frees 40% img +70% vid, live drift `±0.02 GB/3.2s`, bumps `+0.14 GB/image, +0.38 GB/video`.

---

## 9) Build, Test, Run

```bash
cmake -B build -S . && cmake --build build -j   # -Wall -Wextra -Werror
./build/argus_app                                # Qt window
ctest --test-dir build --output-on-failure
```

| Target | What |
|---|---|
| `argus_core` | lib: detector + OCR + validator + store + logger |
| `argus_app` | `src/main.cpp` + Qt window |
| `argus_tests` | Catch2: `test_plate_detector`, `test_plate_validator`, `test_flag_store` |
| `resources/test_images/` | `clear_01.jpg` `angled_02.jpg` `night_03.jpg` `no_plate_04.jpg` `noisy_06.jpg` `video_clip_05.mp4` — deterministic, offline |

**Coverage target:** 92% critical paths (see Settings → Testing & Build accordion).

---

## 10) Testing Strategy

```mermaid
mindmap
  root((Tests))
    Detector
      clear → ≥1 rect
      no-plate → empty no throw
    OCR
      clear ROI → conf>70
      noisy → "—" no crash
    Validator
      AB123CD → accept
      ab-123/cd → AB123CD
      !! → reject
    FlagStore
      exact + fuzzy lev1
      case-insensitive
      trigger default
    Logger
      append + export filtered
      ENOSPC toast
```

---

## 11) Workflow

```mermaid
flowchart TD
  A[1 scaffold<br/>argus_core + ImageSource + Detector stub + ctest hello] --> B[2 OCR + Validator<br/>Tess wrapper + regex + normalize]
  B --> C[3 FlagStore + Alert<br/>JSON+SQLite + cooldown]
  C --> D[4 Logger + UI<br/>Dashboard→Scan wiring]
  D --> E[5 CRUD + Logs + Settings<br/>QSettings + memState]
  E --> F[Polish<br/>video queue + spill-to-disk + per-char sparkline]

  G[Commit] --> H[One logical change<br/>imperative msg<br/>no mix refactor]
  H --> I[PR checklist<br/>-Wall -Wextra, no unused,<40 lines,<br/>error handling, no hard paths,<br/>tests for new paths]
```

*Docs:* `ARCHITECTURE.md` (this file) + `README.md` (build/run) + live mockup `https://nyght-x-walker.github.io/argus/`.

---

## 12) Roadmap

| Version | Focus |
|---|---|
| **v0.2** | `VideoCapture` double-buffered queue, spill-to-disk when cache > limit, per-char sparkline in Logs |
| **v0.3** | Roles Admin/Operator/Viewer (now placeholder), retention cron, blur pipeline benchmark |

---

<p align="center"><sub>Mockup first to lock layout/flows before C++ backend — this file is source of truth for boundaries & state.</sub></p>
