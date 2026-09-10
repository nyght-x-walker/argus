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
};

/// Logger appends scan events to JSONL and keeps recent entries.
/// Each file line holds one JSON object for simple offline review.
class Logger
{
public:
    /// Max recent events kept in memory to bound growth.
    static constexpr std::size_t MAX_RECENT_EVENTS = 100;

    /// Recent events newest last, trimmed to the bound above.
    std::vector<ScanEvent> recentEvents;

    /// Appends the event as one JSON object line at path.
    void log(const ScanEvent& event, const std::string& path = "resources/logs.jsonl");
};

} // namespace argus
