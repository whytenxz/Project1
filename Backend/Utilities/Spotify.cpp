#include "Spotify.h"
#include <windows.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <psapi.h>
#include <algorithm>

#pragma comment(lib, "ole32.lib")

namespace Spotify {
    static std::string g_CurrentTrack = "No active media playing";
    static bool g_IsRunning = false;
    static bool g_IsPlaying = false;
    static std::mutex g_TrackMutex;
    static std::atomic<bool> g_Initialized(false);
    static std::atomic<bool> g_ThreadRunning(true);
    static std::thread g_WorkerThread;

    struct MediaWindowInfo {
        std::string title;
        DWORD processId = 0;
        bool found = false;
    };

    static std::string GetProcessNameFromId(DWORD pid) {
        std::string name = "";
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            char path[MAX_PATH];
            DWORD size = sizeof(path);
            if (QueryFullProcessImageNameA(hProcess, 0, path, &size)) {
                std::string exeName = path;
                size_t lastSlash = exeName.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    name = exeName.substr(lastSlash + 1);
                } else {
                    name = exeName;
                }
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            }
            CloseHandle(hProcess);
        }
        return name;
    }

    static bool IsProcessPlayingAudio(const std::string& targetProcessName) {
        if (targetProcessName.empty())
            return false;

        HRESULT hr;
        IMMDeviceEnumerator* pEnumerator = NULL;
        IMMDevice* pDevice = NULL;
        IAudioSessionManager2* pSessionManager = NULL;
        IAudioSessionEnumerator* pSessionEnumerator = NULL;

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (FAILED(hr)) return false;

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        pEnumerator->Release();
        if (FAILED(hr)) return false;

        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
        pDevice->Release();
        if (FAILED(hr)) return false;

        hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
        pSessionManager->Release();
        if (FAILED(hr)) return false;

        int sessionCount = 0;
        pSessionEnumerator->GetCount(&sessionCount);
        
        bool isPlaying = false;
        for (int i = 0; i < sessionCount; i++) {
            IAudioSessionControl* pSessionControl = NULL;
            IAudioSessionControl2* pSessionControl2 = NULL;
            hr = pSessionEnumerator->GetSession(i, &pSessionControl);
            if (FAILED(hr)) continue;

            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
            pSessionControl->Release();
            if (FAILED(hr)) continue;

            DWORD processId = 0;
            pSessionControl2->GetProcessId(&processId);
            
            AudioSessionState state;
            pSessionControl2->GetState(&state);

            if (processId != 0 && state == AudioSessionStateActive) {
                std::string sessionProcName = GetProcessNameFromId(processId);
                if (sessionProcName == targetProcessName) {
                    isPlaying = true;
                    pSessionControl2->Release();
                    break;
                }
            }
            pSessionControl2->Release();
        }
        pSessionEnumerator->Release();
        return isPlaying;
    }

    static BOOL CALLBACK EnumMediaWindowsProc(HWND hwnd, LPARAM lParam) {
        if (!IsWindowVisible(hwnd))
            return TRUE;

        char title[512]{};
        if (GetWindowTextA(hwnd, title, sizeof(title)) == 0)
            return TRUE;

        std::string sTitle(title);
        if (sTitle.empty())
            return TRUE;

        // Check if it's Spotify desktop
        char className[256]{};
        if (GetClassNameA(hwnd, className, sizeof(className)) > 0) {
            if (strcmp(className, "SpotifyMainWindow") == 0) {
                if (sTitle != "Spotify") {
                    MediaWindowInfo* info = reinterpret_cast<MediaWindowInfo*>(lParam);
                    info->title = sTitle;
                    GetWindowThreadProcessId(hwnd, &info->processId);
                    info->found = true;
                    return FALSE; // Stop enumerating
                }
            }
        }

        // Check for YT Music (browser or desktop app)
        size_t pos = sTitle.find(" - YouTube Music");
        if (pos != std::string::npos) {
            MediaWindowInfo* info = reinterpret_cast<MediaWindowInfo*>(lParam);
            info->title = sTitle.substr(0, pos);
            GetWindowThreadProcessId(hwnd, &info->processId);
            info->found = true;
            return FALSE;
        }

        // Check for YouTube (browser)
        pos = sTitle.find(" - YouTube");
        if (pos != std::string::npos) {
            MediaWindowInfo* info = reinterpret_cast<MediaWindowInfo*>(lParam);
            info->title = sTitle.substr(0, pos);
            GetWindowThreadProcessId(hwnd, &info->processId);
            info->found = true;
            return FALSE;
        }

        // Check for SoundCloud
        pos = sTitle.find(" - SoundCloud");
        if (pos != std::string::npos) {
            MediaWindowInfo* info = reinterpret_cast<MediaWindowInfo*>(lParam);
            info->title = sTitle.substr(0, pos);
            GetWindowThreadProcessId(hwnd, &info->processId);
            info->found = true;
            return FALSE;
        }

        // Check for Apple Music
        pos = sTitle.find(" - Apple Music");
        if (pos != std::string::npos) {
            MediaWindowInfo* info = reinterpret_cast<MediaWindowInfo*>(lParam);
            info->title = sTitle.substr(0, pos);
            GetWindowThreadProcessId(hwnd, &info->processId);
            info->found = true;
            return FALSE;
        }

        return TRUE;
    }

    static void WorkerThreadProc() {
        CoInitialize(NULL);

        while (g_ThreadRunning) {
            MediaWindowInfo info{};
            info.found = false;

            EnumWindows(EnumMediaWindowsProc, reinterpret_cast<LPARAM>(&info));

            bool isPlaying = false;
            if (info.found && info.processId != 0) {
                std::string processName = GetProcessNameFromId(info.processId);
                isPlaying = IsProcessPlayingAudio(processName);
            }

            {
                std::lock_guard<std::mutex> lock(g_TrackMutex);
                if (info.found) {
                    g_IsRunning = true;
                    g_CurrentTrack = info.title;
                    g_IsPlaying = isPlaying;
                } else {
                    g_IsRunning = false;
                    g_CurrentTrack = "No active media playing";
                    g_IsPlaying = false;
                }
            }

            // Sleep for 1000ms to eliminate CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        CoUninitialize();
    }

    bool IsRunning() {
        std::lock_guard<std::mutex> lock(g_TrackMutex);
        return g_IsRunning;
    }

    bool IsPlaying() {
        std::lock_guard<std::mutex> lock(g_TrackMutex);
        return g_IsPlaying;
    }

    std::string GetCurrentTrack() {
        std::lock_guard<std::mutex> lock(g_TrackMutex);
        return g_CurrentTrack;
    }

    void Update() {
        if (!g_Initialized.exchange(true)) {
            g_WorkerThread = std::thread(WorkerThreadProc);
            g_WorkerThread.detach(); // Detach to let it run independently
        }
    }

    void PlayPause() {
        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
        input[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
    }

    void NextTrack() {
        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_MEDIA_NEXT_TRACK;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = VK_MEDIA_NEXT_TRACK;
        input[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
    }

    void PrevTrack() {
        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_MEDIA_PREV_TRACK;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = VK_MEDIA_PREV_TRACK;
        input[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
    }
}
