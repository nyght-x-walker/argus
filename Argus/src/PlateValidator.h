// Argus plate text normalization and format validation.
// EU generic checks with US/UK region refinement and confusion repair.
#pragma once

#include <string>

namespace argus
{

/// Plate issuing region detected by format matching.
enum class Region
{
    EU,
    US,
    UK,
    Unknown
};

/// Normalized text with its region verdict.
struct ValidationResult
{
    std::string normalized;
    Region region = Region::Unknown;
    bool valid = false;
};

/// PlateValidator cleans OCR text and checks EU, US and UK formats.
/// The EU generic shape decides validity; strict US/UK shapes refine the region.
class PlateValidator
{
public:
    /// Uppercases, strips noise and repairs O/I confusions.
    std::string normalize(const std::string& raw) const;

    /// Tries single confusion swaps until a known shape accepts.
    std::string repair(const std::string& normalized) const;

    /// Checks known shapes, reporting the most specific region on success.
    bool isValid(const std::string& normalized, Region& region) const;

    /// Normalizes, repairs one confusion, then validates in one call.
    ValidationResult validate(const std::string& raw) const;
};

} // namespace argus
