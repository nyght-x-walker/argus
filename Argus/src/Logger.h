// Argus scan event persistence to JSON Lines.
// Append-only log with a bounded in-memory recent view.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace argus
{

/// ScanEvent captures one operator decision for later review.
struct ScanEvent
{
    std::string timestamp;
    std::string imagePath;
    std::string plateRaw;
    std::string plateNorm;
    float ocrConf = 0.0f;
    bool flagMatch = false;
    std::string flagType;
    std::string region;
    std::string decision;
    std::string operatorNotes;
    std::vector<float> perCharConf;
};

/// Logger appends scan events to JSONL and keeps recent entries.
/// Each file line holds one JSON object for simple offline review.
class Logger
{
public:
    /// Max recent events kept in memory to bound growth.
    static constexpr std::size_t MAX_RECENT_EVENTS = 100;

    /// Log file size triggering rotation to a numbered backup.
    static constexpr std::size_t ROTATE_BYTES = 5 * 1024 * 1024;

    /// Recent events newest last, trimmed to the bound above.
    std::vector<ScanEvent> recentEvents;

    /// Appends the event as one JSON object line at path.
    void log(const ScanEvent& event, const std::string& path = "resources/logs.jsonl");

    /// Writes recent events to CSV, returning the row count or -1 on error.
    int exportCsv(const std::string& path) const;

    /// Writes recent events to JSON array, true on success.
    bool exportJson(const std::string& path) const;

    /// Rotates the log file when it exceeds ROTATE_BYTES, true when rotated.
    bool rotate(const std::string& path = "resources/logs.jsonl");

    /// Drops log lines older than keepDays, returning kept count or -1.
    int purgeOlderThan(const std::string& path, int keepDays);
};

} // namespace argus
