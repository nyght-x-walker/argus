# Argus – Code Style & Engineering Guidelines

This document defines the coding standards and engineering practices for **Argus**, the C++/OpenCV license plate recognition and flagging system. It is intended to ensure that the codebase is readable, maintainable, and suitable for academic review and future extension.

## 1. General Principles

- **Clarity over cleverness**: Prefer straightforward, well-named constructs to obscure tricks.
- **Single responsibility**: Each class and function should have one clear purpose.
- **Explicit is better than implicit**: Avoid hidden side effects; make data flow and ownership obvious.
- **Fail fast, fail loudly**: Validate inputs early and use clear error handling for invalid states.
- **Security-aware design**: Treat all external input (images, file paths, user data) as untrusted.

## 2. Language and Tooling

- **Language:** C++17 (or later, as supported by the course environment).
- **Primary libraries:**
  - **OpenCV** (`core`, `imgproc`, `imgcodecs`, optionally `objdetect`, `dnn`).
  - **Standard Library** for containers, strings, I/O, memory management.
  - Optional: lightweight JSON/CSV library for configuration and logging (if used, document it in `README.md`).
- **Build system:** **CMake** (preferred) or a clearly documented Makefile.
- **Target platform:** Linux (primary), with consideration for portability.

## 3. Project Structure

A suggested layout:

```text
argus/
  ├─ src/
  │   ├─ main.cpp
  │   ├─ image_source.cpp
  │   ├─ plate_detector.cpp
  │   ├─ plate_ocr.cpp
  │   ├─ plate_validator.cpp
  │   ├─ flag_store.cpp
  │   ├─ alert_service.cpp
  │   └─ logger.cpp
  ├─ include/
  │   ├─ image_source.h
  │   ├─ plate_detector.h
  │   ├─ plate_ocr.h
  │   ├─ plate_validator.h
  │   ├─ flag_store.h
  │   ├─ alert_service.h
  │   └─ logger.h
  ├─ apps/               # Optional: separate CLI/GUI entry points
  │   └─ argus_cli.cpp
  ├─ resources/
  │   ├─ plates_sample/
  │   ├─ test_images/
  │   └─ config/
  ├─ tests/
  ├─ cmake/              # Optional: custom CMake modules
  ├─ CMakeLists.txt
  ├─ README.md
  ├─ CODE_STYLE.md
  └─ (internal files not for submission, e.g. .claude/CLAUDE.md)
```

- `src/`: implementation files for core components.
- `include/`: public headers.
- `apps/`: optional separate applications (CLI, future GUI).
- `resources/`: sample images, configuration files, test data.
- `tests/`: unit/integration tests.

## 4. Naming Conventions

- **Files:** lowercase with underscores, e.g. `plate_detector.cpp`, `plate_detector.h`.
- **Classes / structs:** `PascalCase`, e.g. `PlateDetector`, `FlagStore`.
- **Functions / methods:** `camelCase`, e.g. `detectPlate`, `normalizePlateText`.
- **Variables:** `camelCase`, descriptive names (e.g. `plateText`, `confidenceScore`).
- **Constants:** `UPPER_SNAKE_CASE` or `constexpr` variables with clear names.
- **Namespaces:** use a top-level namespace `argus` to avoid name clashes.

Avoid:

- Single-letter names except for loop indices (`i`, `j`) in small scopes.
- Abbreviations that are not widely known.
- Names that do not reflect purpose (e.g. `data`, `tmp` for long-lived variables).

## 5. Code Organization

### 5.1. Headers

- Use `#pragma once` for header guards.
- Keep headers minimal: declarations only, no heavy implementation.
- Document class responsibilities and key methods with concise comments.

Example:

```cpp
#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace argus {

/// PlateDetector locates license plate regions in images.
class PlateDetector {
public:
    /// Detects plate regions and returns bounding boxes with confidence scores.
    std::vector<cv::Rect> detect(const cv::Mat& image, std::vector<double>& confidences);
};

} // namespace argus
```

### 5.2. Implementation

- Keep functions short and focused (ideally < 40–50 lines).
- Extract complex logic into helper functions with meaningful names.
- Avoid magic numbers; use named constants or configuration.

Example:

```cpp
#include "plate_detector.h"

namespace argus {

// Minimum plate area as fraction of image area to reduce false positives.
constexpr double MIN_PLATE_AREA_FRACTION = 0.005;

std::vector<cv::Rect> PlateDetector::detect(const cv::Mat& image,
                                            std::vector<double>& confidences) {
    // Implementation...
}

} // namespace argus
```

### 5.3. Formatting

- Indentation: **4 spaces** (no tabs).
- Line length: aim for ≤ 100 characters; wrap longer lines where reasonable.
- Braces: use Allman or K&R consistently; prefer:

  ```cpp
  if (condition)
  {
      // ...
  }
  ```

- Blank lines:
  - One blank line between logical blocks inside a function.
  - Two blank lines between top-level functions / class definitions.

## 6. Memory Management & Modern C++

- Prefer **RAII** for all resources (files, sockets, OpenCV mats, etc.).
- Use **smart pointers** (`std::unique_ptr`, `std::shared_ptr`) instead of raw owning pointers.
- Avoid manual `new`/`delete`; rely on constructors, destructors, and standard containers.
- Pass large objects by `const &` when not modified, by reference or pointer when mutation is needed.
- Use `std::optional` for values that may be absent (e.g., “no plate detected”).

## 7. Error Handling

- Use **exceptions** for exceptional conditions:
  - File not found.
  - Invalid configuration.
  - Critical OpenCV failures.
- Use **return codes** or `std::optional` for expected failures:
  - No plate detected in an image.
  - OCR result too low confidence.
- Never ignore errors; at minimum, log them with context.

Example:

```cpp
if (!image.data)
{
    throw std::runtime_error("Failed to load image: " + imagePath);
}
```

## 8. Logging and Debugging

- Use a centralized `Logger` component for consistent log formatting.
- Log levels: `DEBUG`, `INFO`, `WARNING`, `ERROR`.
- Include timestamps and component names in log entries.
- Avoid logging sensitive data (e.g., full images) unless necessary and documented.

Example log line:

```text
[2026-09-02 14:30:12.456] [INFO] [PlateDetector] Detected 1 plate candidate(s) in image_001.png
```

## 9. Testing and Validation

- Write small tests for critical functions:
  - Plate detection on known images.
  - OCR output validation.
  - Plate format validation.
  - Flag matching logic.
  - Logging behavior.
- Use assertions (`assert`) for internal invariants in debug builds.
- Document how to run tests in `README.md`:

  ```bash
  cmake -B build -S .
  cmake --build build
  cd build
  ctest
  ```

### 9.1. Test Design

- Tests must be:
  - **Deterministic** (no random behavior without fixed seeds).
  - **Independent** (no shared mutable state between tests).
  - **Fast** (suitable for repeated runs).
- Use sample images stored in `resources/test_images/`.
- Name test files clearly, e.g. `test_plate_detector.cpp`, `test_flag_store.cpp`.

### 9.2. Coverage Expectations

- Aim for meaningful coverage of:
  - Core detection logic.
  - OCR integration.
  - Flag matching.
  - Error paths (missing files, invalid images).
- 100% line coverage is not required, but critical paths must be covered.

## 10. Security Considerations

- Validate all file paths and user inputs.
- Do not trust OCR output; sanitize before using in logs or comparisons.
- Implement fuzzy matching carefully to avoid false positives on flagged plates.
- Document limitations clearly in the report and manual.
- Avoid hard-coded secrets (API keys, passwords) in source code.

## 11. Documentation

- Every public class and function must have a brief comment describing its purpose.
- Complex algorithms (e.g., plate detection pipeline) must have a higher-level comment block explaining the approach.
- Keep `README.md` up to date with:
  - Build instructions.
  - Usage examples.
  - Dependencies.
  - Known limitations.

### 11.1. Header Documentation

At minimum, document:

- Class responsibility (1–2 sentences).
- Public methods: purpose, key parameters, return value.

Example:

```cpp
/// PlateValidator checks and normalizes license plate strings.
///
/// Supports a single plate format (e.g., EU-style: 1–2 letters, 1–4 digits, 1–3 letters).
/// Provides normalization (e.g., 0/O, 1/I) and validation against a regex.
class PlateValidator {
public:
    /// Returns true if plateText matches the configured format.
    bool isValid(const std::string& plateText) const;

    /// Normalizes plateText (uppercase, common character substitutions).
    std::string normalize(const std::string& plateText) const;
};
```

## 12. Commit Practices

- Write clear, imperative commit messages, e.g.:
  - “Add flag matching logic”
  - “Fix OCR confidence threshold handling”
  - “Introduce AlertService component”
- Group related changes together; avoid mixing unrelated refactorings and features in one commit.
- Reference issues or tasks in commit messages where applicable.

## 13. Review Checklist

Before merging or finalizing code:

- [ ] Code compiles without warnings (with `-Wall -Wextra` or equivalent).
- [ ] No unused variables or imports.
- [ ] Naming is consistent and descriptive.
- [ ] Functions are small and focused.
- [ ] Error handling is consistent and documented.
- [ ] Logging provides useful context without leaking sensitive data.
- [ ] Tests exist for new/changed critical paths.
- [ ] Documentation (headers, README) is complete and accurate.

## 14. CMake Guidelines

- Use modern CMake (target-based):
  - `add_library`, `add_executable`.
  - `target_include_directories`, `target_link_libraries`.
- Do not use global `include_directories` or `link_libraries`.
- Organize targets logically:
  - `argus_core` library for core components.
  - `argus_cli` executable for the main application.
  - `argus_tests` executable for tests (if using CTest).

Minimal example (`CMakeLists.txt`):

```cmake
cmake_minimum_required(VERSION 3.16)
project(argus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_library(argus_core
    src/image_source.cpp
    src/plate_detector.cpp
    src/plate_ocr.cpp
    src/plate_validator.cpp
    src/flag_store.cpp
    src/alert_service.cpp
    src/logger.cpp
)

target_include_directories(argus_core PUBLIC include)
target_link_libraries(argus_core PRIVATE ${OpenCV_LIBS})

add_executable(argus_cli src/main.cpp)
target_link_libraries(argus_cli PRIVATE argus_core)

# Optional: tests
add_executable(argus_tests tests/test_plate_detector.cpp tests/test_flag_store.cpp)
target_link_libraries(argus_tests PRIVATE argus_core)

enable_testing()
add_test(NAME ArgusTests COMMAND argus_tests)
```

---

If you want, next I can give you the **first architecture + implementation prompt** that uses this style and the internal agentic workflow to set up the full Argus codebase (CMake, classes, initial skeleton, tests plan).

Citations:
[1] skills/cpp/SKILL.md at main - C++ Development Guidelines - GitHub https://github.com/Mindrally/skills/blob/main/cpp/SKILL.md
[2] C++ Style Guide - GitHub https://github.com/endless-sky/endless-sky/wiki/C---Style-Guide
[3] Best Practices & Guidelines for C++ in 2026 https://hackingcpp.com/cpp/guidelines
[4] Modern C++ Best Practices: Write Better Code in 2026 Guide https://sudoflare.com/programming/best-practices/
[5] Modern CMake for C++ Projects http://www.database-doctor.com/posts/modern-cmake
[6] How to Configure a C++ OpenCV Project with CMake: Easiest ... https://www.w3tutorials.net/blog/configuring-an-c-opencv-project-with-cmake/
[7] Setting up a Professional C++ CMake Project Structure https://www.studyplan.dev/google-benchmark/cmake-build-systems
[8] Modern C++ Clean Code the Definitive Practical Guide - Scribd https://www.scribd.com/document/962122084/Modern-C-Clean-Code-the-Definitive-Practical-Guide
[9] CMake Mastery Part 16: Structuring Projects https://www.wasilzafar.com/pages/series/cmake-mastery/cmake-mastery-part16-structuring-projects.html
[10] Integrating OpenCV with Android using CMake - Krybot Blog https://blog.krybot.com/t/integrating-opencv-with-android-using-cmake/27637
[11] Technical CMake Tutorial: Library & Executable Projects https://www.studocu.vn/vn/document/truong-dai-hoc-fpt/tu-tuong-ho-chi-minh/technical-cmake-tutorial-library-executable-projects/148104483
[12] C++ Style Guide | PDF | Letter Case - Scribd https://www.scribd.com/document/909934147/C-Style-Guide
[13] Document 8 | PDF | C++ | Object Oriented Programming https://www.scribd.com/document/930475219/Document-8-1
[14] C++ Demos | opencv/opencv_zoo | DeepWiki https://deepwiki.com/opencv/opencv_zoo/7.2-c++-demos
[15] Mir C++ style guide https://canonical-mir.readthedocs-hosted.com/2.26/contributing/reference/cppguide/

