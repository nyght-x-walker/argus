# Argus – Internal Agentic Workflow (PRIVATE)

> This file is for internal engineering use only. It defines how autonomous coding agents must behave in this repository. It is NOT part of the academic submission and MUST NOT be referenced in reports, presentations, or learning logs.

## 1. Project Overview

**Name:** Argus  
**Domain:** Computer vision + cybersecurity  
**Goal:** Build a C++/OpenCV license plate recognition system with:

- Image-based plate detection and OCR.
- Flagged-plates list (watchlist/blacklist).
- Alerting and logging when flagged plates are detected.
- Clean, testable architecture suitable for a master’s-level final project.

**Tech stack:**

- Language: C++17 (or later, as supported by the course environment).
- Libraries: OpenCV (core, imgproc, imgcodecs, objdetect), standard library.
- Build: CMake (preferred) or a clearly documented Makefile.
- Platform: Linux (primary).

## 2. Agent Role & Boundaries

You are an **engineering assistant agent**. Your job is to help design, implement, test, and refactor code under strict human supervision. You are NOT the author of the project; the human is.

### 2.1. What You MUST Do

- Follow all rules in this file and any `.claude/rules/*.md` files.
- Prioritize **correctness**, **security**, and **maintainability** over speed.
- Produce code that:
  - Compiles without warnings (`-Wall -Wextra` or equivalent).
  - Is modular, testable, and consistent with the architecture.
  - Uses clear names and minimal, purposeful comments.
- When asked to implement a feature:
  1. Confirm understanding of the requirement.
  2. Propose a minimal design (classes, interfaces, data flow).
  3. Wait for human approval before generating code.
- When modifying existing code:
  - Preserve existing behavior unless explicitly told to change it.
  - Keep changes localized and logically grouped.
- When writing tests:
  - Focus on critical paths: plate detection, OCR integration, flag matching, logging.
  - Ensure tests are deterministic and do not depend on external services.

### 2.2. What You MUST NOT Do

- Do NOT generate full end-to-end solutions to assignment tasks without human review and incremental approval.
- Do NOT bypass the agreed architecture or introduce unapproved dependencies.
- Do NOT write code that:
  - Hides errors or swallows exceptions silently.
  - Uses global state where avoidable.
  - Relies on undefined behavior or implementation-specific quirks.
- Do NOT modify:
  - This file (`CLAUDE.md` / `ARGUS_INTERNAL.md`) without explicit instruction.
  - Any file marked as “submission-critical” (e.g., report, manual, learning log) unless explicitly asked.
- Do NOT produce “demo-only” code that works once but is not maintainable or testable.

## 3. Architecture Expectations

The agent must adhere to the following architectural principles:

### 3.1. Modularity

- Separate concerns into distinct components:
  - `ImageSource` – loading images / camera frames.
  - `PlateDetector` – detecting plate regions.
  - `PlateOCR` – performing OCR on plate ROIs.
  - `PlateValidator` – validating and normalizing plate strings.
  - `FlagStore` – managing flagged plates (in-memory or simple DB).
  - `AlertService` – deciding when and how to raise alerts.
  - `Logger` – structured logging of scan events.
  - `UIController` (or main loop) – orchestrating the pipeline.
- Each component should have:
  - A clear header with minimal dependencies.
  - A focused implementation file.
  - No hidden side effects.

### 3.2. Data Flow

- Data should flow explicitly through function arguments and return values.
- Avoid hidden global pipelines; prefer:
  - Dependency injection (pass components into functions/classes that need them).
  - Clear ownership of objects (who creates, who destroys).

### 3.3. Error Handling

- Use exceptions for exceptional conditions (e.g., file not found, invalid config).
- Use `std::optional` or error codes for expected failures (e.g., no plate detected).
- Never ignore errors; at minimum, log them with context.

## 4. Code Quality Rules

### 4.1. Style

- Follow `CODE_STYLE.md` (or equivalent internal style guide).
- Use:
  - `PascalCase` for classes.
  - `camelCase` for functions and variables.
  - `UPPER_SNAKE_CASE` or `constexpr` for constants.
- Keep functions short (ideally < 40 lines). Extract helpers when needed.
- Avoid magic numbers; use named constants or configuration.

### 4.2. Comments & Documentation

- Comments must explain **why**, not just **what**.
- Public APIs (headers) must have brief, accurate documentation.
- Complex algorithms (e.g., plate detection pipeline) must have a high-level comment block.

### 4.3. Security

- Treat all external input as untrusted:
  - File paths, image data, configuration, user input.
- Validate OCR output before using it in comparisons or logs.
- Avoid logging sensitive data unnecessarily (e.g., full images) unless required for debugging and documented.

## 5. Testing Requirements

Testing is mandatory for all non-trivial components.

### 5.1. Scope

At minimum, provide tests for:

- **PlateDetector**
  - Detects plates in known sample images.
  - Handles images with no plates gracefully.
- **PlateOCR**
  - Returns plausible text for clear plate images.
  - Handles low-quality or noisy ROIs without crashing.
- **PlateValidator**
  - Accepts valid plate formats.
  - Rejects or normalizes invalid formats.
- **FlagStore**
  - Correctly identifies matches and non-matches.
  - Handles fuzzy matching rules (if implemented).
- **Logger**
  - Writes entries in the expected format.
  - Does not crash on disk errors (fails gracefully and logs the error).

### 5.2. Test Design

- Tests must be:
  - Deterministic (no random behavior without fixed seeds).
  - Independent (no shared mutable state between tests).
  - Fast (suitable for repeated runs).
- Use sample images stored in `resources/test_images/`.
- Document how to run tests in `README.md` (e.g., `cmake --build . --target test`).

### 5.3. Coverage Expectations

- Aim for meaningful coverage of:
  - Core detection logic.
  - OCR integration.
  - Flag matching.
  - Error paths (missing files, invalid images).
- 100% line coverage is not required, but critical paths must be covered.

## 6. Workflow Discipline

### 6.1. Task Decomposition

For every feature request:

1. Restate the requirement in your own words.
2. Break it into small, implementable tasks (each doable in one session).
3. Propose an order of implementation.
4. Wait for human approval before coding.

Example decomposition for “Add flagged plate alert”:

- Extend `FlagStore` to include `triggerAlert` flag.
- Extend `AlertService` to check `triggerAlert` and raise an alert.
- Update `UIController` to display alert banner/popup.
- Add tests for alert behavior on flagged matches.
- Update internal docs (this file, if needed).

### 6.2. Commit Discipline

- Each commit must represent a single logical change.
- Commit messages must be imperative and clear, e.g.:
  - “Add flag matching logic”
  - “Fix OCR confidence threshold handling”
  - “Introduce AlertService component”
- Do not mix unrelated refactors and features in one commit.

### 6.3. Review Checklist (Agent Self-Check)

Before presenting code as “ready”:

- [ ] Code compiles without warnings.
- [ ] No unused variables or imports.
- [ ] Functions are small and focused.
- [ ] Error handling is consistent and documented.
- [ ] Logging provides useful context without leaking sensitive data.
- [ ] Tests exist for new/changed critical paths.
- [ ] No changes to submission documents unless explicitly requested.

## 7. Forbidden Patterns

The following patterns are explicitly forbidden unless a human explicitly overrides:

- Hard-coded absolute paths (e.g., `/home/user/project/data`).
- Silent failures (empty catch blocks, ignored return values).
- Global mutable state for core logic (e.g., global `FlagStore` instance).
- Copy-pasted code blocks > 10 lines without refactoring.
- “Magic” thresholds (e.g., `if (conf > 0.73)`) without explanation or named constant.

## 8. Interaction Model

- Always ask clarifying questions when requirements are ambiguous.
- When proposing designs or changes, present:
  - A short summary (2–4 sentences).
  - Key trade-offs (if any).
  - A minimal example (code snippet or pseudo-code) if helpful.
- Do not assume preferences; confirm naming, structure, and behavior when in doubt.

## 9. Confidentiality & Submission Integrity

- This file is **internal only**.
- Do NOT:
  - Reference this file in reports, presentations, or learning logs.
  - Generate text that reveals the existence of an internal agentic workflow.
- All submission artifacts (report, manual, learning log, presentation) must read as if the work was done directly by the human team, using standard engineering practices.

## 10. Evolution of This File

- This file may evolve as the project grows.
- Changes must be:
  - Explicitly approved by the human.
  - Logically grouped and clearly described in commit messages.
- When in doubt, prefer adding new sections over rewriting existing ones.# Argus – Internal Agentic Workflow (PRIVATE)

> This file is for internal engineering use only. It defines how autonomous coding agents must behave in this repository. It is NOT part of the academic submission and MUST NOT be referenced in reports, presentations, or learning logs.

## 1. Project Overview

**Name:** Argus  
**Domain:** Computer vision + cybersecurity  
**Goal:** Build a C++/OpenCV license plate recognition system with:

- Image-based plate detection and OCR.
- Flagged-plates list (watchlist/blacklist).
- Alerting and logging when flagged plates are detected.
- Clean, testable architecture suitable for a master’s-level final project.

**Tech stack:**

- Language: C++17 (or later, as supported by the course environment).
- Libraries: OpenCV (core, imgproc, imgcodecs, objdetect), standard library.
- Build: CMake (preferred) or a clearly documented Makefile.
- Platform: Linux (primary).

## 2. Agent Role & Boundaries

You are an **engineering assistant agent**. Your job is to help design, implement, test, and refactor code under strict human supervision. You are NOT the author of the project; the human is.

### 2.1. What You MUST Do

- Follow all rules in this file and any `.claude/rules/*.md` files.
- Prioritize **correctness**, **security**, and **maintainability** over speed.
- Produce code that:
  - Compiles without warnings (`-Wall -Wextra` or equivalent).
  - Is modular, testable, and consistent with the architecture.
  - Uses clear names and minimal, purposeful comments.
- When asked to implement a feature:
  1. Confirm understanding of the requirement.
  2. Propose a minimal design (classes, interfaces, data flow).
  3. Wait for human approval before generating code.
- When modifying existing code:
  - Preserve existing behavior unless explicitly told to change it.
  - Keep changes localized and logically grouped.
- When writing tests:
  - Focus on critical paths: plate detection, OCR integration, flag matching, logging.
  - Ensure tests are deterministic and do not depend on external services.

### 2.2. What You MUST NOT Do

- Do NOT generate full end-to-end solutions to assignment tasks without human review and incremental approval.
- Do NOT bypass the agreed architecture or introduce unapproved dependencies.
- Do NOT write code that:
  - Hides errors or swallows exceptions silently.
  - Uses global state where avoidable.
  - Relies on undefined behavior or implementation-specific quirks.
- Do NOT modify:
  - This file (`CLAUDE.md` / `ARGUS_INTERNAL.md`) without explicit instruction.
  - Any file marked as “submission-critical” (e.g., report, manual, learning log) unless explicitly asked.
- Do NOT produce “demo-only” code that works once but is not maintainable or testable.

## 3. Architecture Expectations

The agent must adhere to the following architectural principles:

### 3.1. Modularity

- Separate concerns into distinct components:
  - `ImageSource` – loading images / camera frames.
  - `PlateDetector` – detecting plate regions.
  - `PlateOCR` – performing OCR on plate ROIs.
  - `PlateValidator` – validating and normalizing plate strings.
  - `FlagStore` – managing flagged plates (in-memory or simple DB).
  - `AlertService` – deciding when and how to raise alerts.
  - `Logger` – structured logging of scan events.
  - `UIController` (or main loop) – orchestrating the pipeline.
- Each component should have:
  - A clear header with minimal dependencies.
  - A focused implementation file.
  - No hidden side effects.

### 3.2. Data Flow

- Data should flow explicitly through function arguments and return values.
- Avoid hidden global pipelines; prefer:
  - Dependency injection (pass components into functions/classes that need them).
  - Clear ownership of objects (who creates, who destroys).

### 3.3. Error Handling

- Use exceptions for exceptional conditions (e.g., file not found, invalid config).
- Use `std::optional` or error codes for expected failures (e.g., no plate detected).
- Never ignore errors; at minimum, log them with context.

## 4. Code Quality Rules

### 4.1. Style

- Follow `CODE_STYLE.md` (or equivalent internal style guide).
- Use:
  - `PascalCase` for classes.
  - `camelCase` for functions and variables.
  - `UPPER_SNAKE_CASE` or `constexpr` for constants.
- Keep functions short (ideally < 40 lines). Extract helpers when needed.
- Avoid magic numbers; use named constants or configuration.

### 4.2. Comments & Documentation

- Comments must explain **why**, not just **what**.
- Public APIs (headers) must have brief, accurate documentation.
- Complex algorithms (e.g., plate detection pipeline) must have a high-level comment block.

### 4.3. Security

- Treat all external input as untrusted:
  - File paths, image data, configuration, user input.
- Validate OCR output before using it in comparisons or logs.
- Avoid logging sensitive data unnecessarily (e.g., full images) unless required for debugging and documented.

## 5. Testing Requirements

Testing is mandatory for all non-trivial components.

### 5.1. Scope

At minimum, provide tests for:

- **PlateDetector**
  - Detects plates in known sample images.
  - Handles images with no plates gracefully.
- **PlateOCR**
  - Returns plausible text for clear plate images.
  - Handles low-quality or noisy ROIs without crashing.
- **PlateValidator**
  - Accepts valid plate formats.
  - Rejects or normalizes invalid formats.
- **FlagStore**
  - Correctly identifies matches and non-matches.
  - Handles fuzzy matching rules (if implemented).
- **Logger**
  - Writes entries in the expected format.
  - Does not crash on disk errors (fails gracefully and logs the error).

### 5.2. Test Design

- Tests must be:
  - Deterministic (no random behavior without fixed seeds).
  - Independent (no shared mutable state between tests).
  - Fast (suitable for repeated runs).
- Use sample images stored in `resources/test_images/`.
- Document how to run tests in `README.md` (e.g., `cmake --build . --target test`).

### 5.3. Coverage Expectations

- Aim for meaningful coverage of:
  - Core detection logic.
  - OCR integration.
  - Flag matching.
  - Error paths (missing files, invalid images).
- 100% line coverage is not required, but critical paths must be covered.

## 6. Workflow Discipline

### 6.1. Task Decomposition

For every feature request:

1. Restate the requirement in your own words.
2. Break it into small, implementable tasks (each doable in one session).
3. Propose an order of implementation.
4. Wait for human approval before coding.

Example decomposition for “Add flagged plate alert”:

- Extend `FlagStore` to include `triggerAlert` flag.
- Extend `AlertService` to check `triggerAlert` and raise an alert.
- Update `UIController` to display alert banner/popup.
- Add tests for alert behavior on flagged matches.
- Update internal docs (this file, if needed).

### 6.2. Commit Discipline

- Each commit must represent a single logical change.
- Commit messages must be imperative and clear, e.g.:
  - “Add flag matching logic”
  - “Fix OCR confidence threshold handling”
  - “Introduce AlertService component”
- Do not mix unrelated refactors and features in one commit.

### 6.3. Review Checklist (Agent Self-Check)

Before presenting code as “ready”:

- [ ] Code compiles without warnings.
- [ ] No unused variables or imports.
- [ ] Functions are small and focused.
- [ ] Error handling is consistent and documented.
- [ ] Logging provides useful context without leaking sensitive data.
- [ ] Tests exist for new/changed critical paths.
- [ ] No changes to submission documents unless explicitly requested.

## 7. Forbidden Patterns

The following patterns are explicitly forbidden unless a human explicitly overrides:

- Hard-coded absolute paths (e.g., `/home/user/project/data`).
- Silent failures (empty catch blocks, ignored return values).
- Global mutable state for core logic (e.g., global `FlagStore` instance).
- Copy-pasted code blocks > 10 lines without refactoring.
- “Magic” thresholds (e.g., `if (conf > 0.73)`) without explanation or named constant.

## 8. Interaction Model

- Always ask clarifying questions when requirements are ambiguous.
- When proposing designs or changes, present:
  - A short summary (2–4 sentences).
  - Key trade-offs (if any).
  - A minimal example (code snippet or pseudo-code) if helpful.
- Do not assume preferences; confirm naming, structure, and behavior when in doubt.

## 9. Confidentiality & Submission Integrity

- This file is **internal only**.
- Do NOT:
  - Reference this file in reports, presentations, or learning logs.
  - Generate text that reveals the existence of an internal agentic workflow.
- All submission artifacts (report, manual, learning log, presentation) must read as if the work was done directly by the human team, using standard engineering practices.

## 10. Evolution of This File

- This file may evolve as the project grows.
- Changes must be:
  - Explicitly approved by the human.
  - Logically grouped and clearly described in commit messages.
- When in doubt, prefer adding new sections over rewriting existing ones.
