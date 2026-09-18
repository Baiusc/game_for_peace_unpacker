#pragma once
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>

#pragma comment(lib, "wininet.lib")

class Downloader {
public:
    static std::string DownloadString(const std::string& url) {
        std::string result;
        HINTERNET hInternet = InternetOpenA("CS2-External", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (hInternet) {
            HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
            if (hUrl) {
                char buffer[4096];
                DWORD bytesRead;
                while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    result.append(buffer, bytesRead);
                }
                InternetCloseHandle(hUrl);
            }
            InternetCloseHandle(hInternet);
        }
        return result;
    }

    static bool DownloadToFile(const std::string& url, const std::string& filePath) {
        bool success = false;
        HINTERNET hInternet = InternetOpenA("CS2-External", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (hInternet) {
            HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
            if (hUrl) {
                HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    char buffer[4096];
                    DWORD bytesRead;
                    DWORD bytesWritten;
                    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                        WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL);
                    }
                    CloseHandle(hFile);
                    success = true;
                }
                InternetCloseHandle(hUrl);
            }
            InternetCloseHandle(hInternet);
        }
        return success;
    }
};
