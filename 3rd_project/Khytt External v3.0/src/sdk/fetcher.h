#pragma once
#include <string>
#include <vector>

namespace Fetcher {
bool UpdateOffsets();
std::string GetRawData(const std::string &url);
bool DownloadMapArchive(const std::string &map_name);
} // namespace Fetcher
