// Argus flagged plate watchlist backed by a JSON file.
// Exact-match lookup over normalized plate text.
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace argus
{

/// Watchlist severity driving alerts and display.
enum class FlagType
{
    Blocked,
    Suspicious,
    Authorized
};

/// Single watchlist record keyed by normalized plate text.
struct FlagEntry
{
    std::string plate;
    FlagType type = FlagType::Suspicious;
    std::string reason;
    std::string tags;
    std::string addedDate;
    bool triggerAlert = true;
};

/// Converts a stored type label, failing closed to Blocked.
FlagType flagTypeFromString(const std::string& text);

/// Converts a flag type to its stored label.
std::string flagTypeToString(FlagType type);

/// FlagStore loads, queries and persists the plate watchlist.
class FlagStore
{
public:
    /// Inserts or replaces the entry keyed by its plate text.
    void add(const FlagEntry& entry);

    /// Drops the entry keyed by plate text, if present.
    void remove(const std::string& plate);

    /// Returns the entry for normalized plate text, if flagged.
    std::optional<FlagEntry> lookup(const std::string& normalizedPlate) const;

    /// Returns all entries ordered by plate text.
    std::vector<FlagEntry> entries() const;

    /// Writes the store atomically through a temporary file.
    bool save(const std::string& path) const;

    /// Loads the store, keeping existing entries on failure.
    bool load(const std::string& path);

private:
    std::unordered_map<std::string, FlagEntry> records;
};

} // namespace argus
