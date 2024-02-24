#include "mainwindow.h"
#include <CommCtrl.h>
#include <windows.h>

// Global variables
HINSTANCE hInst = 0;
RECT rcCaptureArea = {0, 0, 0, 0};
WCHAR szRectSelWindowClass[] = L"CapGraphRectSelector";
WCHAR szTitle[] = L"CapGraph";

void InitControls() {
    INITCOMMONCONTROLSEX init;
    init.dwSize = sizeof(INITCOMMONCONTROLSEX);
    init.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&init);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    InitControls();
    hInst = hInstance;

    MainWindow::Register(hInstance);
    RectWindow::Register(hInstance);

    auto mainWindow = MainWindow::Create(szTitle);

    if (!mainWindow->GetHandle()) {
        return FALSE;
    }

    mainWindow->Show(nCmdShow);
    mainWindow->Update();

    MSG msg;

    // Main message loop:
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}