﻿#include <Windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <iostream>
#include <string>
#include <unordered_map>
#include <Wtsapi32.h>

#include "3RVX.h"
#include "Controllers\Volume\CoreAudio.h"
#include "Controllers\Volume\VolumeController.h"
#include "HotkeyManager.h"
#include "Logger.h"
#include "Settings.h"
#include "Skin.h"
#include "VolumeOSD.h"

HANDLE mutex;
HINSTANCE hInst;
ULONG_PTR gdiplusToken;
HWND mainWnd;

CoreAudio *volCtrl;
VolumeOSD *vOsd;
std::unordered_map<int, int> hotkeys;

void init();
HWND CreateMainWnd(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


int APIENTRY
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
LPTSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;

#ifdef _DEBUG
    Logger::OpenConsole();
#endif

    QCLOG(L"  _____ ______     ____  _______ ");
    QCLOG(L" |___ /|  _ \\ \\   / /\\ \\/ /___ / ");
    QCLOG(L"   |_ \\| |_) \\ \\ / /  \\  /  |_ \\ ");
    QCLOG(L"  ___) |  _ < \\ V /   /  \\ ___) |");
    QCLOG(L" |____/|_| \\_\\ \\_/   /_/\\_\\____/ ");
    QCLOG(L"");

    QCLOG(L"Starting up...");

    mutex = CreateMutex(NULL, FALSE, L"Local\\3RVX");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) {
            ReleaseMutex(mutex);
        }

        HWND masterWnd = FindWindow(CLASS_3RVX, CLASS_3RVX);
        CLOG(L"A previous instance of the program is already running.\n"
            L"Requesting Settings dialog from window: %d", masterWnd);
        SendMessage(masterWnd, WM_3RVX_CONTROL, MSG_SETTINGS, NULL);

#ifdef _DEBUG
        CLOG(L"Press [enter] to terminate");
        std::cin.get();
#endif

        return EXIT_SUCCESS;
    }

    CLOG(L"App directory: %s", Settings::AppDir().c_str());

    using namespace Gdiplus;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    mainWnd = CreateMainWnd(hInstance);
    if (mainWnd == NULL) {
        CLOG(L"Could not create main window");
        return EXIT_FAILURE;
    }

    HRESULT hr = CoInitializeEx(NULL,
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (hr != S_OK) {
        CLOG(L"Failed to initialize the COM library.");
        return EXIT_FAILURE;
    }

    /* Tell the program to initialize */
    PostMessage(mainWnd, WM_3RVX_CONTROL, MSG_LOAD, NULL);

    /* Start the event loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

void init() {
    CLOG(L"Initializing...");

    volCtrl = new CoreAudio(mainWnd);
    volCtrl->Init();
    float currentVolume = volCtrl->Volume();

    Settings settings(L"Settings.xml");

    std::wstring skinName = settings.SkinName();
    std::wstring skinXML = Settings::AppDir() + L"\\" + SKINS_DIR L"\\"
        + skinName+ L"\\" SKIN_XML;
    Skin skin(skinXML);

    vOsd = new VolumeOSD(hInst);
    vOsd->LoadSkin(&skin);
    vOsd->MeterLevels(currentVolume);

    hotkeys = settings.Hotkeys();
    HotkeyManager *hkm = HotkeyManager::Instance(mainWnd);
    for (auto it = hotkeys.begin(); it != hotkeys.end(); ++it) {
        int combination = it->first;
        hkm->Register(combination);
    }

    WTSRegisterSessionNotification(mainWnd, NOTIFY_FOR_THIS_SESSION);
}

HWND CreateMainWnd(HINSTANCE hInstance) {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = NULL;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = NULL;
    wcex.cbWndExtra = NULL;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = CLASS_3RVX;
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        return NULL;
    }

    HWND hWnd = CreateWindowEx(
        NULL,
        CLASS_3RVX, CLASS_3RVX,
        NULL, NULL, NULL, //your boat, gently down the stream
        NULL, NULL, HWND_MESSAGE, NULL, hInstance, NULL);

    return hWnd;
}

LRESULT CALLBACK WndProc(
    HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {
    case MSG_VOL_CHNG: {
        float v = volCtrl->Volume();
        QCLOG(L"Volume level: %.0f", v * 100.0f);
        vOsd->MeterLevels(v);
        break;
    }

    case MSG_VOL_DEVCHNG: {
        CLOG(L"Device change detected.");
        volCtrl->ReattachDefaultDevice();
        break;
    }

    case WM_HOTKEY: {
        CLOG(L"Hotkey: %d", (int) wParam);
        int action = hotkeys[(int) wParam];
        if (action == 100) {
            float current = volCtrl->Volume();
            volCtrl->Volume(current + .1f);
        }
        break;
    }

    case WM_WTSSESSION_CHANGE:
        CLOG(L"Detected session change");
        break;

    case WM_CLOSE:
        CLOG(L"Shutting down");
        HotkeyManager::Instance()->Shutdown();
        vOsd->HideIcon();
        DestroyWindow(mainWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    }

    if (message == WM_3RVX_CONTROL) {
        switch (wParam) {
        case MSG_LOAD:
            init();
            break;

        case MSG_SETTINGS:
            CLOG(L"Launching settings editor");
            /* TODO: launch! */
            break;
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}