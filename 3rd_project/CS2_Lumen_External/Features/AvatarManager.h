#pragma once

#include <d3d11.h>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <wincodec.h>
#include <winhttp.h>
#include <urlmon.h>
#include <condition_variable>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "windowscodecs.lib")

class AvatarManager {
private:
    struct AvatarData {
        ID3D11ShaderResourceView* texture = nullptr;
        bool loaded = false;
        bool failed = false;
    };

    ID3D11Device* device = nullptr;
    std::map<uint64_t, AvatarData> avatars;
    std::mutex mapMutex;

    std::queue<uint64_t> fetchQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> isRunning{ false };
    std::thread workerThread;

    AvatarManager() {}
    ~AvatarManager() {
        Stop();
    }

    // Decodes an image from file to RGBA and creates an SRV
    ID3D11ShaderResourceView* CreateTextureFromImage(const std::wstring& imagePath) {
        IWICImagingFactory* pFactory = nullptr;
        CoInitialize(NULL);
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_IWICImagingFactory,
            (LPVOID*)&pFactory
        );

        if (FAILED(hr)) return nullptr;

        IWICBitmapDecoder* pDecoder = nullptr;
        hr = pFactory->CreateDecoderFromFilename(
            imagePath.c_str(),
            NULL,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            &pDecoder
        );

        if (FAILED(hr)) { pFactory->Release(); return nullptr; }

        IWICBitmapFrameDecode* pFrame = nullptr;
        hr = pDecoder->GetFrame(0, &pFrame);
        if (FAILED(hr)) { pDecoder->Release(); pFactory->Release(); return nullptr; }

        IWICFormatConverter* pConverter = nullptr;
        hr = pFactory->CreateFormatConverter(&pConverter);
        if (FAILED(hr)) { pFrame->Release(); pDecoder->Release(); pFactory->Release(); return nullptr; }

        hr = pConverter->Initialize(
            pFrame,                     // Input source to convert
            GUID_WICPixelFormat32bppRGBA, // Destination pixel format
            WICBitmapDitherTypeNone,    // Specified dither pattern
            NULL,                       // Specify a particular palette
            0.0f,                       // Alpha threshold
            WICBitmapPaletteTypeCustom  // Palette translation type
        );
        if (FAILED(hr)) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release(); return nullptr; }

        UINT width, height;
        pConverter->GetSize(&width, &height);

        std::vector<BYTE> buffer(width * height * 4);
        hr = pConverter->CopyPixels(NULL, width * 4, (UINT)buffer.size(), buffer.data());

        ID3D11ShaderResourceView* textureView = nullptr;
        if (SUCCEEDED(hr)) {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA subResource = {};
            subResource.pSysMem = buffer.data();
            subResource.SysMemPitch = width * 4;
            subResource.SysMemSlicePitch = 0;

            ID3D11Texture2D* pTexture = nullptr;
            hr = device->CreateTexture2D(&desc, &subResource, &pTexture);
            if (SUCCEEDED(hr) && pTexture) {
                device->CreateShaderResourceView(pTexture, NULL, &textureView);
                pTexture->Release();
            }
        }

        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();

        return textureView;
    }

    std::string FetchXML(const std::wstring& url) {
        std::string result;
        HINTERNET hSession = WinHttpOpen(L"CS2 External/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            URL_COMPONENTS urlComp = { 0 };
            urlComp.dwStructSize = sizeof(urlComp);
            
            wchar_t hostName[256];
            wchar_t urlPath[1024];
            
            urlComp.lpszHostName = hostName;
            urlComp.dwHostNameLength = 256;
            urlComp.lpszUrlPath = urlPath;
            urlComp.dwUrlPathLength = 1024;
            
            if (WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
                HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
                if (hConnect) {
                    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
                    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
                    if (hRequest) {
                        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                            if (WinHttpReceiveResponse(hRequest, NULL)) {
                                DWORD dwSize = 0;
                                DWORD dwDownloaded = 0;
                                do {
                                    dwSize = 0;
                                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                                        break;
                                    if (dwSize == 0)
                                        break;
                                    
                                    char* pszOutBuffer = new char[dwSize + 1];
                                    ZeroMemory(pszOutBuffer, dwSize + 1);
                                    
                                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                                        result.append(pszOutBuffer, dwDownloaded);
                                    }
                                    delete[] pszOutBuffer;
                                } while (dwSize > 0);
                            }
                        }
                        WinHttpCloseHandle(hRequest);
                    }
                    WinHttpCloseHandle(hConnect);
                }
            }
            WinHttpCloseHandle(hSession);
        }
        return result;
    }

    std::wstring ExtractAvatarUrl(const std::string& xml) {
        std::string tag = "<avatarFull><![CDATA[";
        size_t start = xml.find(tag);
        if (start != std::string::npos) {
            start += tag.length();
            size_t end = xml.find("]]>", start);
            if (end != std::string::npos) {
                std::string urlStr = xml.substr(start, end - start);
                return std::wstring(urlStr.begin(), urlStr.end());
            }
        }
        return L"";
    }

    void WorkerLoop() {
        while (isRunning) {
            uint64_t steamID = 0;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this]() { return !fetchQueue.empty() || !isRunning; });
                
                if (!isRunning) break;
                
                steamID = fetchQueue.front();
                fetchQueue.pop();
            }

            if (steamID == 0) continue;

            // Fetch XML
            std::wstring profileUrl = L"https://steamcommunity.com/profiles/" + std::to_wstring(steamID) + L"/?xml=1";
            std::string xml = FetchXML(profileUrl);
            
            std::wstring avatarUrl = ExtractAvatarUrl(xml);
            
            ID3D11ShaderResourceView* srv = nullptr;
            if (!avatarUrl.empty()) {
                // Download image to temp file
                wchar_t tempPath[MAX_PATH];
                GetTempPathW(MAX_PATH, tempPath);
                std::wstring tempFile = std::wstring(tempPath) + L"avatar_" + std::to_wstring(steamID) + L".jpg";
                
                HRESULT hr = URLDownloadToFileW(NULL, avatarUrl.c_str(), tempFile.c_str(), 0, NULL);
                if (SUCCEEDED(hr)) {
                    srv = CreateTextureFromImage(tempFile);
                    DeleteFileW(tempFile.c_str());
                }
            }
            
            std::lock_guard<std::mutex> lock(mapMutex);
            if (srv) {
                avatars[steamID].texture = srv;
                avatars[steamID].loaded = true;
                avatars[steamID].failed = false;
            } else {
                avatars[steamID].loaded = false;
                avatars[steamID].failed = true;
            }
        }
    }

public:
    static AvatarManager& Get() {
        static AvatarManager instance;
        return instance;
    }

    void Initialize(ID3D11Device* pDevice) {
        device = pDevice;
        if (!isRunning) {
            isRunning = true;
            workerThread = std::thread(&AvatarManager::WorkerLoop, this);
        }
    }

    void Stop() {
        isRunning = false;
        cv.notify_all();
        if (workerThread.joinable()) {
            workerThread.join();
        }

        std::lock_guard<std::mutex> lock(mapMutex);
        for (auto& pair : avatars) {
            if (pair.second.texture) {
                pair.second.texture->Release();
                pair.second.texture = nullptr;
            }
        }
        avatars.clear();
    }

    ID3D11ShaderResourceView* GetAvatar(uint64_t steamID) {
        if (!device || steamID == 0) return nullptr;

        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = avatars.find(steamID);
        if (it != avatars.end()) {
            return it->second.texture;
        }

        avatars[steamID].loaded = false;
        avatars[steamID].failed = false;
        avatars[steamID].texture = nullptr;

        {
            std::lock_guard<std::mutex> qLock(queueMutex);
            fetchQueue.push(steamID);
        }
        cv.notify_one();

        return nullptr;
    }
};
