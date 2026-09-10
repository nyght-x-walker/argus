// Argus alert gating for flagged plate matches.
// Single rule over trigger flag, severity and cooldown.
#include "AlertService.h"

namespace argus
{

bool AlertService::shouldAlert(const FlagEntry& entry, double secondsSinceLastAlert) const
{
    // Authorized entries never raise alerts by policy.
    if (!entry.triggerAlert)
    {
        return false;
    }
    if (entry.type != FlagType::Blocked && entry.type != FlagType::Suspicious)
    {
        return false;
    }
    // Cooldown gate keeps repeat matches quiet.
    return secondsSinceLastAlert >= cooldownSeconds;
}

} // namespace argus
