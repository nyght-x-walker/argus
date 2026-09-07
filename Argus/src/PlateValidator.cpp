// Argus plate text normalization and format validation.
// Single-region EU generic checks with confusion repair.
#include "PlateValidator.h"

#include <cctype>

namespace
{

// EU generic shape: 1-3 letters, 1-4 digits, 0-2 tail letters.
constexpr int MAX_LEAD_LETTERS = 3;
constexpr int MIN_LEAD_LETTERS = 1;
constexpr int MAX_DIGITS = 4;
constexpr int MIN_DIGITS = 1;
constexpr int MAX_TAIL_LETTERS = 2;

// Consumes up to maxCount letters or digits from pos onward.
std::size_t skipGroup(const std::string& text, std::size_t pos, int maxCount, bool letters)
{
    std::size_t start = pos;
    while (pos < text.size() && static_cast<int>(pos - start) < maxCount)
    {
        unsigned char code = static_cast<unsigned char>(text[pos]);
        bool wanted = letters ? std::isupper(code) != 0 : std::isdigit(code) != 0;
        if (!wanted)
        {
            break;
        }
        ++pos;
    }
    return pos;
}

} // namespace

namespace argus
{

std::string PlateValidator::normalize(const std::string& raw) const
{
    std::string clean;
    for (char glyph : raw)
    {
        unsigned char code = static_cast<unsigned char>(glyph);
        if (std::isalnum(code) == 0)
        {
            continue;
        }
        char upper = static_cast<char>(std::toupper(code));
        if (upper == 'O')
        {
            upper = '0';
        }
        else if (upper == 'I')
        {
            upper = '1';
        }
        clean.push_back(upper);
    }
    return clean;
}

bool PlateValidator::isValid(const std::string& normalized, Region& region) const
{
    region = Region::Unknown;
    std::size_t pos = 0;
    pos = skipGroup(normalized, pos, MAX_LEAD_LETTERS, true);
    std::size_t leadCount = pos;
    pos = skipGroup(normalized, pos, MAX_DIGITS, false);
    std::size_t digitCount = pos - leadCount;
    pos = skipGroup(normalized, pos, MAX_TAIL_LETTERS, true);
    if (pos != normalized.size() || leadCount < MIN_LEAD_LETTERS || digitCount < MIN_DIGITS)
    {
        return false;
    }
    region = Region::EU;
    return true;
}

ValidationResult PlateValidator::validate(const std::string& raw) const
{
    ValidationResult result;
    result.normalized = normalize(raw);
    result.valid = isValid(result.normalized, result.region);
    return result;
}

} // namespace argus
