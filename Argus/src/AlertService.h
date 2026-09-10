// Argus alert gating for flagged plate matches.
// Cooldown filtering over watchlist severity.
#pragma once

#include "FlagStore.h"

namespace argus
{

/// AlertService decides whether a watchlist hit raises an alert.
/// Cooldown suppresses repeat banners for the same plate burst.
class AlertService
{
public:
    /// Cooldown window in seconds between alerts.
    float cooldownSeconds = 300.0f;

    /// True for alert-worthy hits outside the cooldown window.
    bool shouldAlert(const FlagEntry& entry, double secondsSinceLastAlert) const;
};

} // namespace argus
