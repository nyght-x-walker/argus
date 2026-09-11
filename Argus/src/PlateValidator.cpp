// Argus plate text normalization and format validation.
// Single-region EU generic checks with confusion repair.
#include "PlateValidator.h"

#include <cctype>
#include <iterator>

namespace
{

// EU generic shape: 1-3 letters, 1-4 digits, 0-3 tail letters.
// Three tail letters admit current UK-style LLNNLLL plates.
constexpr int MAX_LEAD_LETTERS = 3;
constexpr int MIN_LEAD_LETTERS = 1;
constexpr int MAX_DIGITS = 4;
constexpr int MIN_DIGITS = 1;
constexpr int MAX_TAIL_LETTERS = 3;

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

// One directed glyph confusion, letters to digits or back.
struct GlyphSwap
{
    char from;
    char to;
};

// Letters misread inside digit slots, first valid repair wins.
constexpr GlyphSwap LETTER_TO_DIGIT[] = {
    {'B', '8'}, {'G', '6'}, {'L', '1'}, {'S', '5'}, {'Z', '2'}, {'A', '4'}, {'E', '6'},
};

// Digits misread inside letter slots, tried after the digit swaps.
constexpr GlyphSwap DIGIT_TO_LETTER[] = {
    {'0', 'O'}, {'1', 'I'}, {'5', 'S'}, {'2', 'Z'}, {'8', 'B'}, {'6', 'G'}, {'4', 'A'},
};

// Tries one directed swap per position, first validating swap wins.
bool trySwaps(const std::string& text, const GlyphSwap* swaps, std::size_t swapCount,
              const argus::PlateValidator* validator, std::string& fixed)
{
    for (std::size_t pos = 0; pos < text.size(); ++pos)
    {
        for (std::size_t swap = 0; swap < swapCount; ++swap)
        {
            if (text[pos] != swaps[swap].from)
            {
                continue;
            }
            std::string attempt = text;
            attempt[pos] = swaps[swap].to;
            argus::Region region = argus::Region::Unknown;
            if (validator->isValid(attempt, region))
            {
                fixed = attempt;
                return true;
            }
        }
    }
    return false;
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

std::string PlateValidator::repair(const std::string& normalized) const
{
    Region region = Region::Unknown;
    if (normalized.empty() || isValid(normalized, region))
    {
        return normalized;
    }
    // Digit slots first, unrepairable reads stay honest.
    std::string fixed;
    if (trySwaps(normalized, LETTER_TO_DIGIT, std::size(LETTER_TO_DIGIT), this, fixed))
    {
        return fixed;
    }
    // Letter slots only when letters exist, digit strings stay invalid.
    bool hasLetter = false;
    for (char glyph : normalized)
    {
        if (std::isupper(static_cast<unsigned char>(glyph)) != 0)
        {
            hasLetter = true;
            break;
        }
    }
    if (hasLetter && trySwaps(normalized, DIGIT_TO_LETTER, std::size(DIGIT_TO_LETTER), this, fixed))
    {
        return fixed;
    }
    return normalized;
}

ValidationResult PlateValidator::validate(const std::string& raw) const
{
    ValidationResult result;
    result.normalized = repair(normalize(raw));
    result.valid = isValid(result.normalized, result.region);
    return result;
}

} // namespace argus
