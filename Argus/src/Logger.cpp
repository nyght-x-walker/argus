// Argus scan event persistence to JSON Lines.
// One JSON object per line with graceful file error handling.
#include "Logger.h"

#include "ofJson.h"
#include "ofMain.h"

#include <fstream>

namespace argus
{

void Logger::log(const ScanEvent& event, const std::string& path)
{
    // Build the JSON object with one key per event field.
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

} // namespace argus
