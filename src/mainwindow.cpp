#include "mainwindow.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <cmath>
#include <sstream>
#include <string>
#include <iomanip>

HINSTANCE MainWindow::hInstance = NULL;
const WCHAR MainWindow::szClassName[] = L"CapGraphMain";

constexpr int64_t FILE_TIME_TO_MILLISECONDS = 10000ll;
constexpr int64_t STILL_IMAGE_THRESHOLD = 3000ll * FILE_TIME_TO_MILLISECONDS;

enum {
    BID_SETAREA = 100,
    BID_STARTREC = 101,
    TID_CAPTURE = 103,
    SID_STATUSBAR = 104,
};

//--------------------------------------------------------------------------------------------
// Utility functions
//--------------------------------------------------------------------------------------------
double compareImages(std::vector<uint32_t>& img1, std::vector<uint32_t>& img2)
{
    if (img1.size() != img2.size())
    {
        return 0.0;
    }
    double meanSquareError = 0;
    for (size_t i = 0; i < img1.size(); i++)
    {
        uint8_t b1 = (img1[i] >> 16) & 0xFF;
        uint8_t g1 = (img1[i] >> 8) & 0xFF;
        uint8_t r1 = img1[i] & 0xFF;
        uint8_t b2 = (img2[i] >> 16) & 0xFF;
        uint8_t g2 = (img2[i] >> 8) & 0xFF;
        uint8_t r2 = img2[i] & 0xFF;
        meanSquareError += pow(r1 - r2, 2) + pow(g1 - g2, 2) + pow(b1 - b2, 2);
    }
    return meanSquareError / (3*img1.size());
}

COLORREF getAveragePixel(std::vector<uint32_t>& img1)
{
    double r = 0, g = 0, b = 0;
    double meanSquareError = 0;
    for (size_t i = 0; i < img1.size(); i++)
    {
        b += (img1[i] >> 16) & 0xFF;
        g += (img1[i] >> 8) & 0xFF;
        r += img1[i] & 0xFF;
    }
    b /= img1.size();
    g /= img1.size();
    r /= img1.size();
    return RGB(r,g,b);
}

const std::wstring formatCaptureItem(const CaptureItem& item)
{
    std::wstringstream ss;
    ss << std::setfill(L'0') << std::setw(2)
       << item.stTimestamp.wYear << L"-" << item.stTimestamp.wMonth << L"-"
       << item.stTimestamp.wDay << L" " << item.stTimestamp.wHour << L":"
       << item.stTimestamp.wMinute << L":" << item.stTimestamp.wSecond << " - "
       << std::hex << std::setw(6) << item.cAvgColor;
    return ss.str();
}


int64_t getFileTimeDiff(const FILETIME& ft1, const FILETIME& ft2)
{
    ULARGE_INTEGER uli1, uli2;
    uli1.LowPart = ft1.dwLowDateTime;
    uli1.HighPart = ft1.dwHighDateTime;
    uli2.LowPart = ft2.dwLowDateTime;
    uli2.HighPart = ft2.dwHighDateTime;
    return ((int64_t)uli1.QuadPart) - ((int64_t)uli2.QuadPart);
}

//--------------------------------------------------------------------------------------------
// MainWindow implementation
//--------------------------------------------------------------------------------------------

std::shared_ptr<MainWindow> MainWindow::Create(LPCWSTR szTitle) {
    return std::shared_ptr<MainWindow>(new MainWindow(szTitle));
}


MainWindow::MainWindow(LPCWSTR szTitle): hCurrentFont(NULL), csCapStatus(CaptureStatus::NotStarted)
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
    if (csCapStatus != CaptureStatus::NotStarted)
    {
        int msgResult = MessageBoxW(
            hWindow,
            L"A sele\u00E7\u00E3o de uma nova \u00E1rea interrompe a captura. Deseja continuar?", 
            L"Deseja interromper a captura?",
            MB_YESNO | MB_ICONWARNING);
        if (msgResult == IDNO)
        {
            return;
        }
        ToggleCaptureClick();
    }
    if (pAreaSelector) {
        pAreaSelector->StartSelecting();
    }
}

void MainWindow::ToggleCaptureClick()
{
    if (csCapStatus != CaptureStatus::NotStarted)
    {
        csCapStatus = CaptureStatus::NotStarted;
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
        csCapStatus = CaptureStatus::StillImage;
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
    const auto imWidth = (area.right - area.left);
    const auto imHeight = (area.bottom - area.top);
    // Draws image on window
    HDC screen = GetDC(NULL);
    HDC winDc = GetDC(hWindow);
    BitBlt(winDc, ScaleToDPI(10, dpi), ScaleToDPI(45, dpi), imWidth, imHeight, screen, area.left, area.top, SRCCOPY);
    // Reads image from screen
    HDC memDc = CreateCompatibleDC(winDc);
    HBITMAP hBitmap = CreateCompatibleBitmap(winDc, imWidth, imHeight);
    BITMAPINFOHEADER bihHeader = { 0 };
    std::vector<uint32_t> newImage(imWidth * imHeight);
    bihHeader.biSize = sizeof(BITMAPINFOHEADER);
    bihHeader.biWidth = imWidth;
    bihHeader.biHeight = imHeight;
    bihHeader.biPlanes = 1;
    bihHeader.biBitCount = 32;
    bihHeader.biCompression = BI_RGB;
    bihHeader.biSizeImage = 0;
    bihHeader.biXPelsPerMeter = 0;
    bihHeader.biYPelsPerMeter = 0;
    bihHeader.biClrUsed = 0;
    bihHeader.biClrImportant = 0;
    SelectObject(memDc, hBitmap);
    BitBlt(memDc, 0, 0, imWidth, imHeight, screen, area.left, area.top, SRCCOPY);
    GetDIBits(winDc, hBitmap, 0, imHeight, newImage.data(), (LPBITMAPINFO) &bihHeader, DIB_RGB_COLORS);
    // Compare if frames changed
    double diff = compareImages(vCaptureBuffer, newImage);
    bool imageChanged = diff > 0.01;
    if (csCapStatus == CaptureStatus::WaitingStillImage)
    {
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);
        if (imageChanged)
        {
            ftLastChangedImage = ftNow;
        }
        else if (getFileTimeDiff(ftNow, ftLastChangedImage) > STILL_IMAGE_THRESHOLD)
        {
            csCapStatus = CaptureStatus::StillImage;
            CaptureItem newItem;
            SYSTEMTIME stUtcTimestamp;
            FileTimeToSystemTime(&ftNow, &stUtcTimestamp);
            SystemTimeToTzSpecificLocalTime(NULL, &stUtcTimestamp, &newItem.stTimestamp);
            newItem.cAvgColor = getAveragePixel(newImage);
            vColorItems.push_back(newItem);
            auto statusText = formatCaptureItem(newItem);
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM) statusText.c_str());
        }
    }
    else if (csCapStatus == CaptureStatus::StillImage)
    {
        if (imageChanged)
        {
            FILETIME ftNow;
            GetSystemTimeAsFileTime(&ftNow);
            ftLastChangedImage = ftNow;
            csCapStatus = CaptureStatus::WaitingStillImage;
            std::wostringstream statusText;
            statusText << L"Esperando imagem... (" << diff << L")";
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM) statusText.str().c_str());
        }
    }
    // Stores the new image
    vCaptureBuffer = std::move(newImage);
    DeleteObject(hBitmap);
    ReleaseDC(NULL, screen);
    ReleaseDC(hWindow, winDc);
    ReleaseDC(hWindow, memDc);
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