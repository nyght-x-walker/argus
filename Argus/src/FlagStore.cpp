// Argus flagged plate watchlist backed by a JSON file.
// Exact, near-miss and prefix lookup over normalized plate text.
#include "FlagStore.h"

#include "ofJson.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{

// Leading characters shared before a prefix hit counts.
constexpr std::size_t MIN_PREFIX_SEARCH = 3;

} // namespace

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

// Edit distance between plates, computed over two rolling rows.
std::size_t levenshteinDistance(const std::string& left, const std::string& right)
{
    if (left.empty())
    {
        return right.size();
    }
    if (right.empty())
    {
        return left.size();
    }
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t col = 0; col <= right.size(); ++col)
    {
        previous[col] = col;
    }
    for (std::size_t row = 1; row <= left.size(); ++row)
    {
        current[0] = row;
        for (std::size_t col = 1; col <= right.size(); ++col)
        {
            std::size_t change = left[row - 1] == right[col - 1] ? 0 : 1;
            current[col] =
                std::min({previous[col] + 1, current[col - 1] + 1, previous[col - 1] + change});
        }
        previous.swap(current);
    }
    return previous[right.size()];
}

// True when two plates differ by at most one substitution or gap.
bool withinOneEdit(const std::string& left, const std::string& right)
{
    if (left.empty() || right.empty())
    {
        return false;
    }
    std::size_t longer = std::max(left.size(), right.size());
    std::size_t shorter = std::min(left.size(), right.size());
    if (longer - shorter > 1)
    {
        return false;
    }
    return levenshteinDistance(left, right) <= 1;
}

std::optional<FlagEntry> FlagStore::lookupFuzzy(const std::string& normalizedPlate) const
{
    if (normalizedPlate.empty())
    {
        return std::nullopt;
    }
    // Sorted entries keep the winner deterministic on ties.
    for (const auto& entry : entries())
    {
        if (withinOneEdit(normalizedPlate, entry.plate))
        {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<FlagEntry> FlagStore::lookupPrefix(const std::string& normalizedPlate) const
{
    if (normalizedPlate.size() < MIN_PREFIX_SEARCH)
    {
        return std::nullopt;
    }
    // Sorted entries keep the winner deterministic on ties.
    for (const auto& entry : entries())
    {
        std::size_t shared = 0;
        while (shared < normalizedPlate.size() && shared < entry.plate.size() &&
               normalizedPlate[shared] == entry.plate[shared])
        {
            ++shared;
        }
        if (shared >= MIN_PREFIX_SEARCH)
        {
            return entry;
        }
    }
    return std::nullopt;
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
