// Argus flagged plate watchlist backed by a JSON file.
// Exact-match lookup over normalized plate text.
#include "FlagStore.h"

#include "ofJson.h"

#include <algorithm>
#include <cstdio>

namespace argus
{

FlagType flagTypeFromString(const std::string& text)
{
    if (text == "Blocked")
    {
        return FlagType::Blocked;
    }
    if (text == "Authorized")
    {
        return FlagType::Authorized;
    }
    if (text == "Suspicious")
    {
        return FlagType::Suspicious;
    }
    // Unknown labels fail closed to the most restrictive type.
    return FlagType::Blocked;
}

std::string flagTypeToString(FlagType type)
{
    switch (type)
    {
    case FlagType::Blocked:
        return "Blocked";
    case FlagType::Authorized:
        return "Authorized";
    default:
        return "Suspicious";
    }
}

void FlagStore::add(const FlagEntry& entry)
{
    records[entry.plate] = entry;
}

void FlagStore::remove(const std::string& plate)
{
    records.erase(plate);
}

std::optional<FlagEntry> FlagStore::lookup(const std::string& normalizedPlate) const
{
    auto found = records.find(normalizedPlate);
    if (found == records.end())
    {
        return std::nullopt;
    }
    return found->second;
}

std::vector<FlagEntry> FlagStore::entries() const
{
    std::vector<FlagEntry> ordered;
    for (const auto& slot : records)
    {
        ordered.push_back(slot.second);
    }
    std::sort(ordered.begin(), ordered.end(), [](const FlagEntry& left, const FlagEntry& right)
              { return left.plate < right.plate; });
    return ordered;
}

bool FlagStore::save(const std::string& path) const
{
    ofJson stored = ofJson::array();
    for (const auto& entry : entries())
    {
        stored.push_back({
            {"plate", entry.plate},
            {"type", flagTypeToString(entry.type)},
            {"reason", entry.reason},
            {"tags", entry.tags},
            {"addedDate", entry.addedDate},
            {"triggerAlert", entry.triggerAlert},
        });
    }
    std::string staging = path + ".tmp";
    if (!ofSavePrettyJson(staging, stored))
    {
        return false;
    }
    return std::rename(staging.c_str(), path.c_str()) == 0;
}

bool FlagStore::load(const std::string& path)
{
    ofJson stored;
    try
    {
        stored = ofLoadJson(path);
    }
    catch (const std::exception&)
    {
        return false;
    }
    if (!stored.is_array())
    {
        return false;
    }
    std::unordered_map<std::string, FlagEntry> parsed;
    for (const auto& item : stored)
    {
        FlagEntry entry;
        entry.plate = item.value("plate", "");
        entry.type = flagTypeFromString(item.value("type", ""));
        entry.reason = item.value("reason", "");
        entry.tags = item.value("tags", "");
        entry.addedDate = item.value("addedDate", "");
        entry.triggerAlert = item.value("triggerAlert", false);
        if (!entry.plate.empty())
        {
            parsed[entry.plate] = entry;
        }
    }
    records = parsed;
    return true;
}

} // namespace argus
