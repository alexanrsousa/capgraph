#include "mainwindow.h"
#include <windowsx.h>
#include <CommCtrl.h>

HINSTANCE MainWindow::hInstance = NULL;
const WCHAR MainWindow::szClassName[] = L"CapGraphMain";

enum {
    BID_SETAREA = 100,
    BID_STARTREC = 101,
    TID_CAPTURE = 103,
    SID_STATUSBAR = 104,
};

std::shared_ptr<MainWindow> MainWindow::Create(LPCWSTR szTitle) {
    return std::shared_ptr<MainWindow>(new MainWindow(szTitle));
}


MainWindow::MainWindow(LPCWSTR szTitle): hCurrentFont(NULL), bStarted(false)
{
    hWindow = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW, MainWindow::szClassName, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, MainWindow::hInstance, this);
    hbtnSetArea = CreateWindowW(L"BUTTON", L"Selecionar Regi\u00E3o...", BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWindow,
        (HMENU) BID_SETAREA, MainWindow::hInstance, nullptr);
    hbtnStartCapture = CreateWindowW(L"BUTTON", L"Iniciar", BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWindow,
        (HMENU) BID_STARTREC, MainWindow::hInstance, nullptr);
    hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, SBARS_SIZEGRIP | SBARS_TOOLTIPS | WS_VISIBLE | WS_CHILD, 
        0, 0, 0, 0, hWindow, (HMENU) SID_STATUSBAR, MainWindow::hInstance, nullptr);
    pAreaSelector = RectWindow::Create();

    RECT position;
    GetWindowRect(hWindow, &position);
    UpdateChildrenPos(&position);
    UpdateFont();
}

void MainWindow::SelectAreaClick()
{
    if (bStarted)
    {
        ToggleCaptureClick();
    }
    if (pAreaSelector) {
        pAreaSelector->StartSelecting();
    }
}

void MainWindow::ToggleCaptureClick()
{
    if (bStarted)
    {
        bStarted = false;
        KillTimer(hWindow, TID_CAPTURE);
        SetWindowTextW(hbtnStartCapture, L"Iniciar");
    }
    else if (pAreaSelector)
    {
        if (!pAreaSelector->HasSelectedArea())
        {
            MessageBoxW(hWindow, L"Por favor selecione uma regi\u00E3o para captura", NULL, MB_OK | MB_ICONERROR);
            return;
        }
        bStarted = true;
        SetTimer(hWindow, TID_CAPTURE, 100, NULL);
        SetWindowTextW(hbtnStartCapture, L"Parar");
    } 
    Redraw();  
}

void MainWindow::DoCapture()
{
    if (!pAreaSelector) {
        return;
    }
    const auto area = pAreaSelector->GetCaptureRect();
    const auto dpi = GetDpiForWindow(hWindow);
    HDC screen = GetDC(NULL);
    HDC winDc = GetDC(hWindow);
    BitBlt(winDc, ScaleToDPI(10, dpi), ScaleToDPI(45, dpi), area.right - area.left, area.bottom - area.top, screen, area.left, area.top, SRCCOPY);
    ReleaseDC(NULL, screen);
    ReleaseDC(hWindow, winDc);
}

void MainWindow::GetMinMaxInfo(LPMINMAXINFO minMaxInfo)
{
    const auto dpi = GetDpiForWindow(hWindow);
    LONG minWidth = ScaleToDPI(280, dpi);
    LONG minHeight = ScaleToDPI(300, dpi);
    minMaxInfo->ptMinTrackSize.x = (std::max)(minMaxInfo->ptMinTrackSize.x, minWidth);
    minMaxInfo->ptMinTrackSize.y = (std::max)(minMaxInfo->ptMinTrackSize.y, minHeight);
}

void MainWindow::UpdateChildrenPos(LPRECT clientArea)
{
    const auto dpi = GetDpiForWindow(hWindow);
    if (dpi)
    {
        SetWindowPos(
            hbtnSetArea, NULL,
            ScaleToDPI(10, dpi), ScaleToDPI(10, dpi),
            ScaleToDPI(120, dpi), ScaleToDPI(25, dpi),
            SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(
            hbtnStartCapture, NULL,
            ScaleToDPI(140, dpi), ScaleToDPI(10, dpi),
            ScaleToDPI(120, dpi), ScaleToDPI(25, dpi),
            SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(hStatusBar, WM_SIZE, 0, 0);
    }
}

void MainWindow::UpdateFont()
{
    const auto dpi = GetDpiForWindow(hWindow);
    NONCLIENTMETRICSW ncMetrics;
    ncMetrics.cbSize = sizeof(ncMetrics);
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncMetrics), &ncMetrics, 0, dpi))
    {
        HFONT font = CreateFontIndirectW(&ncMetrics.lfMessageFont);
        SendMessageW(hbtnSetArea, WM_SETFONT, (WPARAM) font, TRUE);
        SendMessageW(hbtnStartCapture, WM_SETFONT, (WPARAM) font, TRUE);
        if (hCurrentFont)
        {
            DeleteObject(hCurrentFont);
        }
        hCurrentFont = font;
    }
}

void MainWindow::DestroyCleanup()
{
    hWindow = 0;
    if (hCurrentFont)
    {
        DeleteObject(hCurrentFont);
        hCurrentFont = 0;
    }
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{    switch (message)
    {
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
                case BID_SETAREA:
                    SelectAreaClick();
                    return 0;
                case BID_STARTREC:
                    ToggleCaptureClick();
                    return 0;
            }
            break;
        }
        case WM_TIMER:
            if (wParam == TID_CAPTURE)
            {
                DoCapture();
            }
            return 0;
        case WM_WINDOWPOSCHANGED:
        {
            LPWINDOWPOS position = (LPWINDOWPOS) lParam;
            if ((position->flags & SWP_NOSIZE) == 0)
            {
                RECT windowCoords { position->x, position->y, position->x + position->cx, position->y + position->cy };
                UpdateChildrenPos(&windowCoords);
            }
            break;
        }
        case WM_GETMINMAXINFO:
        {
            GetMinMaxInfo((LPMINMAXINFO) lParam);
            return 0;
        }
        case WM_DESTROY:
        {
            DestroyCleanup();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWindow, message, wParam, lParam);
}

void MainWindow::Register(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    MainWindow::hInstance = hInstance;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_WINLOGO);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL; // MAKEINTRESOURCEW(IDC_GDICAPTURINGANIMAGE);
    wcex.lpszClassName = MainWindow::szClassName;
    wcex.hIconSm = LoadIcon(hInstance, IDI_WINLOGO);

    RegisterClassExW(&wcex);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        LPCREATESTRUCTW createStruct = (LPCREATESTRUCTW) lParam;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR) createStruct->lpCreateParams);
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    else 
    {
        MainWindow* window = (MainWindow*) GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (window && window->hWindow == hWnd)
        {
            return window->HandleMessage(message, wParam, lParam);
        }
        else if (message == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        else
        {
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
    }
}