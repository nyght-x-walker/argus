// Argus plate text normalization and format validation.
// Single-region EU generic checks with confusion repair.
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

/// PlateValidator cleans OCR text and checks one EU generic format.
class PlateValidator
{
public:
    /// Uppercases, strips noise and repairs O/I confusions.
    std::string normalize(const std::string& raw) const;

    /// Checks the EU generic shape, reporting the region on success.
    bool isValid(const std::string& normalized, Region& region) const;

    /// Normalizes then validates in one call.
    ValidationResult validate(const std::string& raw) const;
};

} // namespace argus
