// Argus scan event persistence to JSON Lines.
// One JSON object per line with graceful file error handling.
#include "Logger.h"

#include "ofJson.h"
#include "ofMain.h"

#include <cstdio>
#include <ctime>
#include <fstream>

namespace argus
{

// Escapes one CSV cell with quotes when it holds separators.
std::string csvCell(const std::string& text)
{
    bool quoted = text.find_first_of(",\"\n") != std::string::npos;
    if (!quoted)
    {
        return text;
    }
    std::string escaped;
    for (char glyph : text)
    {
        if (glyph == '"')
        {
            escaped.push_back('"');
        }
        escaped.push_back(glyph);
    }
    return "\"" + escaped + "\"";
}

// Builds the JSON object for one event including per-char scores.
ofJson eventJson(const ScanEvent& event)
{
    ofJson stored;
    stored["timestamp"] = event.timestamp;
    stored["imagePath"] = event.imagePath;
    stored["plateRaw"] = event.plateRaw;
    stored["plateNorm"] = event.plateNorm;
    stored["ocrConf"] = event.ocrConf;
    stored["flagMatch"] = event.flagMatch;
    stored["flagType"] = event.flagType;
    stored["region"] = event.region;
    stored["decision"] = event.decision;
    stored["operatorNotes"] = event.operatorNotes;
    stored["perCharConf"] = event.perCharConf;
    return stored;
}

void Logger::log(const ScanEvent& event, const std::string& path)
{
    ofJson stored = eventJson(event);

    // Resolve through the data folder and ensure the parent exists.
    std::string absolutePath = ofToDataPath(path, false);
    std::string parentDir = ofFilePath::getEnclosingDirectory(absolutePath, false);
    if (!parentDir.empty())
    {
        ofDirectory::createDirectory(parentDir, true, true);
    }

    // Append one line, reporting failures without throwing.
    std::ofstream out(absolutePath, std::ios::app);
    if (!out.is_open())
    {
        ofLogError("Logger") << "Failed to open log file: " << path;
        return;
    }
    out << stored.dump() << "\n";
    if (out.fail())
    {
        ofLogError("Logger") << "Failed to write log file: " << path;
        return;
    }
    out.close();

    // Keep a bounded in-memory view for the Logs tab.
    recentEvents.push_back(event);
    while (recentEvents.size() > MAX_RECENT_EVENTS)
    {
        recentEvents.erase(recentEvents.begin());
    }
}

int Logger::exportCsv(const std::string& path) const
{
    std::string absolutePath = ofToDataPath(path, false);
    std::ofstream out(absolutePath);
    if (!out.is_open())
    {
        ofLogError("Logger") << "Failed to open CSV export: " << path;
        return -1;
    }
    out << "timestamp,imagePath,plateRaw,plateNorm,ocrConf,flagMatch,flagType,region,"
           "decision,operatorNotes\n";
    int rows = 0;
    for (const auto& event : recentEvents)
    {
        out << csvCell(event.timestamp) << "," << csvCell(event.imagePath) << ","
            << csvCell(event.plateRaw) << "," << csvCell(event.plateNorm) << "," << event.ocrConf
            << "," << (event.flagMatch ? "YES" : "NO") << "," << csvCell(event.flagType) << ","
            << csvCell(event.region) << "," << csvCell(event.decision) << ","
            << csvCell(event.operatorNotes) << "\n";
        ++rows;
    }
    return out.fail() ? -1 : rows;
}

bool Logger::exportJson(const std::string& path) const
{
    ofJson stored = ofJson::array();
    for (const auto& event : recentEvents)
    {
        stored.push_back(eventJson(event));
    }
    return ofSavePrettyJson(ofToDataPath(path, false), stored);
}

bool Logger::rotate(const std::string& path)
{
    std::string absolutePath = ofToDataPath(path, false);
    ofFile file(absolutePath);
    if (!file.exists() || file.getSize() < ROTATE_BYTES)
    {
        return false;
    }
    return std::rename(absolutePath.c_str(), (absolutePath + ".1").c_str()) == 0;
}

int Logger::purgeOlderThan(const std::string& path, int keepDays)
{
    std::string absolutePath = ofToDataPath(path, false);
    std::ifstream input(absolutePath);
    if (!input.is_open())
    {
        return -1;
    }
    std::time_t cutoffTime = std::time(nullptr) - keepDays * 86400;
    char cutoffBuf[20] = {0};
    std::strftime(cutoffBuf, sizeof(cutoffBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&cutoffTime));
    std::string cutoff(cutoffBuf);
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line.find("\"timestamp\"") == std::string::npos)
        {
            continue;
        }
        try
        {
            ofJson parsed = ofJson::parse(line);
            if (parsed.value("timestamp", "") >= cutoff)
            {
                kept.push_back(line);
            }
        }
        catch (const std::exception&)
        {
            continue;
        }
    }
    input.close();
    std::ofstream out(absolutePath, std::ios::trunc);
    if (!out.is_open())
    {
        return -1;
    }
    for (const auto& keptLine : kept)
    {
        out << keptLine << "\n";
    }
    return out.fail() ? -1 : static_cast<int>(kept.size());
}

} // namespace argus
