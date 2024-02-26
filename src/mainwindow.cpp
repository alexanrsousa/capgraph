#include "mainwindow.h"
#include "resources.h"
#include <CommCtrl.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <windowsx.h>

HINSTANCE MainWindow::hInstance = NULL;
const WCHAR MainWindow::szClassName[] = L"CapGraphMain";

const uint8_t utf8BOM[] = {0xEF, 0xBB, 0xBF};

constexpr int64_t FILE_TIME_TO_MILLISECONDS = 10000ll;

enum {
    BID_SETAREA = 100,
    BID_STARTREC = 101,
    TID_CAPTURE = 103,
    SID_STATUSBAR = 104,
    LID_DATALIST = 105,
    BID_SAVEDATA = 106,
    BID_CLEARDATA = 107,
    BID_SETINTERVAL = 108,
    TID_MAINTOOLBAR = 109,
};

//--------------------------------------------------------------------------------------------
// Utility functions
//--------------------------------------------------------------------------------------------
static double compareImages(std::vector<uint32_t>& img1, std::vector<uint32_t>& img2) {
    if (img1.size() != img2.size()) {
        return 0.0;
    }
    double meanSquareError = 0;
    for (size_t i = 0; i < img1.size(); i++) {
        uint8_t b1 = (img1[i] >> 16) & 0xFF;
        uint8_t g1 = (img1[i] >> 8) & 0xFF;
        uint8_t r1 = img1[i] & 0xFF;
        uint8_t b2 = (img2[i] >> 16) & 0xFF;
        uint8_t g2 = (img2[i] >> 8) & 0xFF;
        uint8_t r2 = img2[i] & 0xFF;
        meanSquareError += pow(r1 - r2, 2) + pow(g1 - g2, 2) + pow(b1 - b2, 2);
    }
    return meanSquareError / (3 * img1.size());
}

static COLORREF getAveragePixel(std::vector<uint32_t>& img1) {
    double r = 0, g = 0, b = 0;
    double meanSquareError = 0;
    for (size_t i = 0; i < img1.size(); i++) {
        b += (img1[i] >> 16) & 0xFF;
        g += (img1[i] >> 8) & 0xFF;
        r += img1[i] & 0xFF;
    }
    b /= img1.size();
    g /= img1.size();
    r /= img1.size();
    return RGB(r, g, b);
}

static wchar_t getListDelimiter() {
    WCHAR delimiter[4];
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SLIST, delimiter, 4);
    return delimiter[0];
}

static std::wstring formatCaptureItemTimestamp(const CaptureItem& item) {
    std::wstringstream ss;
    ss << std::setfill(L'0') << std::setw(4) << item.stTimestamp.wYear << L"-" << std::setw(2) << item.stTimestamp.wMonth << L"-"
       << std::setw(2) << item.stTimestamp.wDay << L" " << std::setw(2) << item.stTimestamp.wHour << L":" << std::setw(2)
       << item.stTimestamp.wMinute << L":" << std::setw(2) << item.stTimestamp.wSecond;
    return ss.str();
}

static std::wstring formatCaptureItemColor(const CaptureItem& item) {
    std::wstringstream ss;
    ss << "#" << std::setfill(L'0') << std::hex << std::uppercase << std::setw(6) << item.cAvgColor;
    return ss.str();
}

static std::wstring formatCaptureItem(const CaptureItem& item) {
    std::wstringstream ss;
    ss << std::setfill(L'0') << std::setw(4) << item.stTimestamp.wYear << L"-" << std::setw(2) << item.stTimestamp.wMonth << L"-"
       << std::setw(2) << item.stTimestamp.wDay << L" " << std::setw(2) << item.stTimestamp.wHour << L":" << std::setw(2)
       << item.stTimestamp.wMinute << L":" << std::setw(2) << item.stTimestamp.wSecond << L" - " << std::hex << std::setw(6)
       << std::uppercase << item.cAvgColor;
    return ss.str();
}

std::string getUtf8(const std::wstring& wstr) {
    if (wstr.empty())
        return std::string();
    int sz = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 0, 0, 0, 0);
    std::string res(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &res[0], sz, 0, 0);
    return res;
}

static void writeCaptureLine(std::ofstream& file, const CaptureItem& item, std::string delimiter) {
    file << '"' << getUtf8(formatCaptureItemTimestamp(item)) << '"' << delimiter << '"' << getUtf8(formatCaptureItemColor(item)) << '"'
         << delimiter << (unsigned int)GetRValue(item.cAvgColor) << delimiter << (unsigned int)GetGValue(item.cAvgColor) << delimiter
         << (unsigned int)GetBValue(item.cAvgColor) << std::endl;
}

static int64_t getFileTimeDiff(const FILETIME& ft1, const FILETIME& ft2) {
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

MainWindow::MainWindow(LPCWSTR szTitle)
    : hCurrentFont(NULL)
    , csCapStatus(CaptureStatus::NotStarted)
    , iStillImageDuration(3000) {
    // Creates the main window
    hWindow = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW, MainWindow::szClassName, szTitle, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, MainWindow::hInstance, this);
    // Creates the child controls
    hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, SBARS_SIZEGRIP | SBARS_TOOLTIPS | WS_VISIBLE | WS_CHILD, 0, 0, 0, 0,
                                 hWindow, (HMENU)SID_STATUSBAR, MainWindow::hInstance, nullptr);
    hlvDataList = CreateWindowExW(0, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL, 0, 0, 0, 0,
                                  hWindow, (HMENU)LID_DATALIST, MainWindow::hInstance, nullptr);
    htbToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_NODIVIDER, 0, 0, 0, 0,
                                 hWindow, (HMENU)TID_MAINTOOLBAR, MainWindow::hInstance, nullptr);
    pAreaSelector = RectWindow::Create();
    pAreaSelector->OnSetCaptureRect = [this](const RECT& rect) {
        UNREFERENCED_PARAMETER(rect);
        SendMessageW(htbToolbar, TB_ENABLEBUTTON, BID_STARTREC, TRUE);
    };
    // Sets up the toolbar
    SetupToolbar();
    SetupToolbarImages();
    // Sets the list view columns
    SetupColumns();
    // Sets up the position of child controls
    RECT position;
    GetClientRect(hWindow, &position);
    UpdateChildrenPos(&position);
    // Sets the font for the child controls
    UpdateFont();
}

void MainWindow::SelectAreaClick() {
    if (csCapStatus != CaptureStatus::NotStarted) {
        int msgResult = MessageBoxW(hWindow, L"A sele\u00E7\u00E3o de uma nova \u00E1rea interrompe a captura. Deseja continuar?",
                                    L"Deseja interromper a captura?", MB_YESNO | MB_ICONWARNING);
        if (msgResult == IDNO) {
            return;
        }
        ToggleCaptureClick();
    }
    if (pAreaSelector) {
        SendMessageW(htbToolbar, TB_ENABLEBUTTON, BID_STARTREC, FALSE);
        pAreaSelector->StartSelecting();
    }
}

void MainWindow::ToggleCaptureClick() {
    TBBUTTONINFOW tbi;
    tbi.cbSize = sizeof(TBBUTTONINFOW);
    tbi.dwMask = TBIF_TEXT | TBIF_IMAGE;
    if (csCapStatus != CaptureStatus::NotStarted) {
        csCapStatus = CaptureStatus::NotStarted;
        KillTimer(hWindow, TID_CAPTURE);
        tbi.iImage = MAKELONG(1, 0);
        tbi.pszText = L"Iniciar Captura";
        SendMessageW(htbToolbar, TB_SETBUTTONINFOW, BID_STARTREC, (LPARAM)&tbi);
    } else if (pAreaSelector) {
        if (!pAreaSelector->HasSelectedArea()) {
            MessageBoxW(hWindow, L"Por favor selecione uma regi\u00E3o para captura", NULL, MB_OK | MB_ICONERROR);
            return;
        }
        csCapStatus = CaptureStatus::StillImage;
        SetTimer(hWindow, TID_CAPTURE, 100, NULL);
        tbi.iImage = MAKELONG(2, 0);
        tbi.pszText = L"Parar Captura";
        SendMessageW(htbToolbar, TB_SETBUTTONINFOW, BID_STARTREC, (LPARAM)&tbi);
    }
    Redraw();
}
void MainWindow::ClearDataClick() {
    auto result = MessageBoxW(hWindow, L"Tem certeza que deseja limpar os dados?", L"Limpar Dados", MB_YESNO | MB_ICONWARNING);
    if (result == IDNO) {
        return;
    }
    SendMessageW(hlvDataList, LVM_DELETEALLITEMS, 0, 0);
    vColorItems.clear();
    SendMessageW(htbToolbar, TB_ENABLEBUTTON, BID_CLEARDATA, FALSE);
    SendMessageW(htbToolbar, TB_ENABLEBUTTON, BID_SAVEDATA, FALSE);
}

void MainWindow::SaveDataClick() {
    OPENFILENAMEW ofn;
    WCHAR szFileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWindow;
    ofn.lpstrFilter = L"Valores Separados por V\u00EDrgula (*.csv)\0*.csv\0Todos os Arquivos (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream csvFile(ofn.lpstrFile);
        const auto delimiter = getUtf8(std::wstring(1, getListDelimiter()));
        csvFile.write((const char*)utf8BOM, 3);
        csvFile << "Timestamp" << delimiter << "Cor" << delimiter << "R" << delimiter << "G" << delimiter << "B" << std::endl;
        for (const auto& item : vColorItems) {
            writeCaptureLine(csvFile, item, delimiter);
        }
        csvFile.close();
    }
}

void MainWindow::DoCapture() {
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
    BitBlt(winDc, ScaleToDPI(310, dpi), ScaleToDPI(45, dpi), imWidth, imHeight, screen, area.left, area.top, SRCCOPY);
    // Reads image from screen
    HDC memDc = CreateCompatibleDC(winDc);
    HBITMAP hBitmap = CreateCompatibleBitmap(winDc, imWidth, imHeight);
    BITMAPINFOHEADER bihHeader = {0};
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
    GetDIBits(winDc, hBitmap, 0, imHeight, newImage.data(), (LPBITMAPINFO)&bihHeader, DIB_RGB_COLORS);
    // Compare if frames changed
    double diff = compareImages(vCaptureBuffer, newImage);
    bool imageChanged = diff > 0.01;
    if (csCapStatus == CaptureStatus::WaitingStillImage) {
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);
        if (imageChanged) {
            ftLastChangedImage = ftNow;
        } else if (getFileTimeDiff(ftNow, ftLastChangedImage) > iStillImageDuration * FILE_TIME_TO_MILLISECONDS) {
            csCapStatus = CaptureStatus::StillImage;
            CaptureItem newItem;
            SYSTEMTIME stUtcTimestamp;
            FileTimeToSystemTime(&ftNow, &stUtcTimestamp);
            SystemTimeToTzSpecificLocalTime(NULL, &stUtcTimestamp, &newItem.stTimestamp);
            newItem.cAvgColor = getAveragePixel(newImage);
            InsertCaptureItem(newItem);
            auto statusText = formatCaptureItem(newItem);
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusText.c_str());
        }
    } else if (csCapStatus == CaptureStatus::StillImage) {
        if (imageChanged) {
            FILETIME ftNow;
            GetSystemTimeAsFileTime(&ftNow);
            ftLastChangedImage = ftNow;
            csCapStatus = CaptureStatus::WaitingStillImage;
            std::wostringstream statusText;
            statusText << L"Esperando imagem... (" << diff << L")";
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusText.str().c_str());
        }
    }
    // Stores the new image
    vCaptureBuffer = std::move(newImage);
    DeleteObject(hBitmap);
    ReleaseDC(NULL, screen);
    ReleaseDC(hWindow, winDc);
    ReleaseDC(hWindow, memDc);
}

void MainWindow::GetMinMaxInfo(LPMINMAXINFO minMaxInfo) {
    const auto dpi = GetDpiForWindow(hWindow);
    LONG minWidth = ScaleToDPI(500, dpi);
    LONG minHeight = ScaleToDPI(400, dpi);
    minMaxInfo->ptMinTrackSize.x = (std::max)(minMaxInfo->ptMinTrackSize.x, minWidth);
    minMaxInfo->ptMinTrackSize.y = (std::max)(minMaxInfo->ptMinTrackSize.y, minHeight);
}

void MainWindow::SetupToolbar() {
    TBBUTTON tbButtons[5];
    ZeroMemory(tbButtons, sizeof(tbButtons));

    iToolbarTextIdx = (INT_PTR)SendMessageW(htbToolbar, TB_ADDSTRINGW, (WPARAM)MainWindow::hInstance, (LPARAM)IDS_TOOLBAR);

    tbButtons[0].iBitmap = MAKELONG(0, 0);
    tbButtons[0].fsState = TBSTATE_ENABLED;
    tbButtons[0].fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT;
    tbButtons[0].idCommand = BID_SETAREA;
    tbButtons[0].iString = iToolbarTextIdx;
    tbButtons[1].iBitmap = MAKELONG(1, 0);
    tbButtons[1].fsState = 0;
    tbButtons[1].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    tbButtons[1].idCommand = BID_STARTREC;
    tbButtons[1].iString = iToolbarTextIdx + 1;
    tbButtons[2].iBitmap = MAKELONG(3, 0);
    tbButtons[2].fsState = 0;
    tbButtons[2].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    tbButtons[2].idCommand = BID_SAVEDATA;
    tbButtons[2].iString = iToolbarTextIdx + 3;
    tbButtons[3].iBitmap = MAKELONG(4, 0);
    tbButtons[3].fsState = 0;
    tbButtons[3].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    tbButtons[3].idCommand = BID_CLEARDATA;
    tbButtons[3].iString = iToolbarTextIdx + 4;
    tbButtons[4].iBitmap = MAKELONG(5, 0);
    tbButtons[4].fsState = TBSTATE_ENABLED;
    tbButtons[4].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_WHOLEDROPDOWN;
    tbButtons[4].idCommand = BID_SETINTERVAL;
    tbButtons[4].iString = iToolbarTextIdx + 5;
    SendMessageW(htbToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessageW(htbToolbar, TB_ADDBUTTONS, 5, (LPARAM)&tbButtons);
    SendMessageW(htbToolbar, TB_AUTOSIZE, 0, 0);
}

void MainWindow::SetupToolbarImages() {
    auto dpi = GetDpiForWindow(hWindow);
    auto size = ScaleToDPI(24, dpi);
    const int iconIds[6] = {IDI_SELECT_AREA, IDI_START_CAPTURE, IDI_STOP_CAPTURE, IDI_SAVE_DATA, IDI_CLEAR_DATA, IDI_STILL_DURATION};
    HIMAGELIST hImageList = ImageList_Create(size, size, ILC_COLOR32, 0, 0);
    for (int i = 0; i < 6; i++) {
        HICON hIcon = (HICON)LoadImageW(MainWindow::hInstance, MAKEINTRESOURCEW(iconIds[i]), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
        ImageList_AddIcon(hImageList, hIcon);
        DestroyIcon(hIcon);
    }
    auto oldList = SendMessageW(htbToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImageList);
    SendMessageW(htbToolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(size, size));
    SendMessageW(htbToolbar, TB_AUTOSIZE, 0, 0);
    if (oldList) {
        DeleteObject((HIMAGELIST)oldList);
    }
}

void MainWindow::InsertCaptureItem(const CaptureItem& item) {
    auto listSize = (int)SendMessageW(hlvDataList, LVM_GETITEMCOUNT, 0, 0);
    auto timestamp = formatCaptureItemTimestamp(item);
    auto color = formatCaptureItemColor(item);
    LVITEMW lvItem;
    lvItem.mask = LVIF_TEXT | LVIF_STATE;
    lvItem.state = 0;
    lvItem.stateMask = 0;
    lvItem.iSubItem = 0;
    lvItem.iItem = listSize;
    lvItem.pszText = (LPWSTR)timestamp.c_str();
    auto index = (int)SendMessageW(hlvDataList, LVM_INSERTITEMW, 0, (LPARAM)&lvItem);
    lvItem.iSubItem = 1;
    lvItem.iItem = index;
    lvItem.pszText = (LPWSTR)color.c_str();
    SendMessageW(hlvDataList, LVM_SETITEMW, 0, (LPARAM)&lvItem);
    vColorItems.push_back(item);
    SendMessageW(htbToolbar, TB_ENABLEBUTTON, BID_CLEARDATA, TRUE);
    SendMessageW(htbToolbar, TB_ENABLEBUTTON, BID_SAVEDATA, TRUE);
}

void MainWindow::SetupColumns() {
    const auto dpi = GetDpiForWindow(hWindow);
    LVCOLUMNW lvColumn;
    lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvColumn.fmt = LVCFMT_LEFT;
    lvColumn.cx = ScaleToDPI(150, dpi);
    lvColumn.pszText = L"Timestamp";
    lvColumn.iSubItem = 0;
    SendMessageW(hlvDataList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvColumn);
    lvColumn.cx = ScaleToDPI(100, dpi);
    lvColumn.pszText = L"Cor M\u00E9dia";
    lvColumn.iSubItem = 1;
    SendMessageW(hlvDataList, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvColumn);
}

void MainWindow::UpdateChildrenPos(LPRECT clientArea) {
    const auto dpi = GetDpiForWindow(hWindow);
    if (dpi) {
        RECT statusBarPos;
        SendMessageW(hStatusBar, WM_SIZE, 0, 0);
        GetWindowRect(hStatusBar, &statusBarPos);
        auto statusBarHeight = statusBarPos.bottom - statusBarPos.top;
        SetWindowPos(hlvDataList, NULL, 0, ScaleToDPI(45, dpi), ScaleToDPI(300, dpi),
                     clientArea->bottom - clientArea->top - statusBarHeight - ScaleToDPI(45, dpi), SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void MainWindow::UpdateFont() {
    /*const auto dpi = GetDpiForWindow(hWindow);
    NONCLIENTMETRICSW ncMetrics;
    ncMetrics.cbSize = sizeof(ncMetrics);
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncMetrics), &ncMetrics, 0, dpi)) {
        HFONT font = CreateFontIndirectW(&ncMetrics.lfMessageFont);
        SendMessageW(hbtnSetArea, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(hbtnStartCapture, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(hbtnClearData, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(hbtnSaveData, WM_SETFONT, (WPARAM)font, TRUE);
        if (hCurrentFont) {
            DeleteObject(hCurrentFont);
        }
        hCurrentFont = font;
    }*/
}

void MainWindow::DestroyCleanup() {
    hWindow = 0;
    if (hCurrentFont) {
        DeleteObject(hCurrentFont);
        hCurrentFont = 0;
    }
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case BID_SETAREA:
            SelectAreaClick();
            return 0;
        case BID_STARTREC:
            ToggleCaptureClick();
            return 0;
        case BID_CLEARDATA:
            ClearDataClick();
            return 0;
        case BID_SAVEDATA:
            SaveDataClick();
            return 0;
        case IDM_STILL_DURATION_05:
            iStillImageDuration = 500;
            return 0;
        case IDM_STILL_DURATION_10:
            iStillImageDuration = 1000;
            return 0;
        case IDM_STILL_DURATION_20:
            iStillImageDuration = 2000;
            return 0;
        case IDM_STILL_DURATION_40:
            iStillImageDuration = 4000;
            return 0;
        case IDM_STILL_DURATION_50:
            iStillImageDuration = 5000;
            return 0;
        case IDM_STILL_DURATION_100:
            iStillImageDuration = 10000;
            return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->idFrom == TID_MAINTOOLBAR && nmhdr->code == TBN_DROPDOWN) {
            LPNMTOOLBARW nmtb = (LPNMTOOLBARW)lParam;
            RECT buttonRect;
            SendMessage(nmtb->hdr.hwndFrom, TB_GETRECT, (WPARAM)nmtb->iItem, (LPARAM)&buttonRect);
            MapWindowPoints(nmtb->hdr.hwndFrom, HWND_DESKTOP, (LPPOINT)&buttonRect, 2);
            HMENU hMenuLoaded = LoadMenuW(MainWindow::hInstance, MAKEINTRESOURCEW(IDM_STILL_DURATION));
            HMENU hPopupMenu = GetSubMenu(hMenuLoaded, 0);
            TPMPARAMS tpm;
            tpm.cbSize = sizeof(TPMPARAMS);
            tpm.rcExclude = buttonRect;
            switch (iStillImageDuration) {
            case 500:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_05, MF_BYCOMMAND | MF_CHECKED);
                break;
            case 1000:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_10, MF_BYCOMMAND | MF_CHECKED);
                break;
            case 2000:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_20, MF_BYCOMMAND | MF_CHECKED);
                break;
            case 3000:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_30, MF_BYCOMMAND | MF_CHECKED);
                break;
            case 4000:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_40, MF_BYCOMMAND | MF_CHECKED);
                break;
            case 5000:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_50, MF_BYCOMMAND | MF_CHECKED);
                break;
            case 10000:
                CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_100, MF_BYCOMMAND | MF_CHECKED);
                break;
            }
            CheckMenuItem(hPopupMenu, IDM_STILL_DURATION_05, MF_BYCOMMAND | (iStillImageDuration == 500 ? MF_CHECKED : MF_UNCHECKED));
            TrackPopupMenuEx(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, buttonRect.left, buttonRect.bottom, hWindow,
                             &tpm);

            DestroyMenu(hMenuLoaded);
        }
        break;
    }
    case WM_TIMER:
        if (wParam == TID_CAPTURE) {
            DoCapture();
        }
        return 0;
    case WM_WINDOWPOSCHANGED: {
        LPWINDOWPOS position = (LPWINDOWPOS)lParam;
        if ((position->flags & SWP_NOSIZE) == 0) {
            RECT windowCoords;
            GetClientRect(hWindow, &windowCoords);
            UpdateChildrenPos(&windowCoords);
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        GetMinMaxInfo((LPMINMAXINFO)lParam);
        return 0;
    }
    case WM_DESTROY: {
        DestroyCleanup();
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWindow, message, wParam, lParam);
}

void MainWindow::Register(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;
    MainWindow::hInstance = hInstance;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CAPGRAPH));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL; // MAKEINTRESOURCEW(IDC_GDICAPTURINGANIMAGE);
    wcex.lpszClassName = MainWindow::szClassName;
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CAPGRAPH));

    RegisterClassExW(&wcex);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CREATE) {
        LPCREATESTRUCTW createStruct = (LPCREATESTRUCTW)lParam;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)createStruct->lpCreateParams);
        return DefWindowProcW(hWnd, message, wParam, lParam);
    } else {
        MainWindow* window = (MainWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (window && window->hWindow == hWnd) {
            return window->HandleMessage(message, wParam, lParam);
        } else if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        } else {
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
    }
}