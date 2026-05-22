#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 游戏状态枚举
enum GameState {
    STATE_COVER,
    STATE_DIFFICULTY,
    STATE_PLAYING
};

// 各种常量与控制变量
const int CELL_SIZE = 30;
const int OFFSET_X = 50;
const int OFFSET_Y = 50;
const int MAX_ROWS = 50; // 棋盘最大行数
const int MAX_COLS = 50; // 棋盘最大列数

int g_rows = 10;
int g_cols = 10;
int g_numMines = 10;

// 按钮ID常量
#define IDC_BTN_RESTART  1001
#define IDC_BTN_SAVE     1002
#define IDC_BTN_QUIT     1003
#define IDC_BTN_EASY     1004
#define IDC_BTN_MEDIUM   1005
#define IDC_BTN_HARD     1006
#define IDC_BTN_LOAD     1007 // 新增：加载存档按钮

//封装四个核心属性
struct Cell {
    bool isMine;
    bool isRevealed;
    bool isFlagged;
    int neighborMines;
};

struct MyImage {
    int width;
    int height;
    HBITMAP hBitmap;
};

static HWND g_hWnd = NULL;
// 游戏内按钮
static HWND g_btnRestart, g_btnSave, g_btnQuit;
// 难度选择及存档按钮
static HWND g_btnEasy, g_btnMed, g_btnHard, g_btnLoad;

// 图片资源
static MyImage g_bg = {0, 0, NULL};
static MyImage g_gameBg = {0, 0, NULL};
static MyImage g_flagImg = {0, 0, NULL};
static MyImage g_mineImg = {0, 0, NULL};

static GameState g_gameState = STATE_COVER;
static Cell g_board[MAX_ROWS][MAX_COLS];
static bool g_gameOver = false;
static bool g_gameWon = false;

// 绘制图片到指定DC的工具函数
static void DrawImageToDC(HDC hdcDest, MyImage* img, int x, int y, int width, int height) {
    if (!img->hBitmap) return;
    HDC memDC = CreateCompatibleDC(hdcDest);
    HGDIOBJ oldBmp = SelectObject(memDC, img->hBitmap);
    SetStretchBltMode(hdcDest, COLORONCOLOR);
    StretchBlt(hdcDest, x, y, width, height, memDC, 0, 0, img->width, img->height, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
}

// 初始化游戏数据
// 地图布设：随机布雷和计算邻雷数
static void InitGame() {
    srand((unsigned int)time(NULL));
    g_gameOver = false;
    g_gameWon = false;

    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            g_board[r][c] = {false, false, false, 0};
        }
    }

    // 随机布雷
    int placed = 0;
    while (placed < g_numMines) {
        int r = rand() % g_rows;
        int c = rand() % g_cols;
        if (!g_board[r][c].isMine) {
            g_board[r][c].isMine = true;
            placed++;
        }
    }

    // 计算每个非雷格子的邻雷数
    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            if (g_board[r][c].isMine) continue;
            int count = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr >= 0 && nr < g_rows && nc >= 0 && nc < g_cols && g_board[nr][nc].isMine) {
                        count++;
                    }
                }
            }
            g_board[r][c].neighborMines = count;
        }
    }
}

// 递归揭示空白区域（展开算法）
static void RevealCell(int r, int c) {
    if (r < 0 || r >= g_rows || c < 0 || c >= g_cols) return;
    if (g_board[r][c].isRevealed || g_board[r][c].isFlagged) return;

    g_board[r][c].isRevealed = true;

    if (g_board[r][c].isMine) {
        g_gameOver = true;
        return;
    }

    if (g_board[r][c].neighborMines == 0) {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr != 0 || dc != 0) RevealCell(r + dr, c + dc);
            }
        }
    }
}

// 检查胜利条件：所有非雷格子都被揭示
static void CheckWinCondition() {
    int revealedCount = 0;
    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            if (g_board[r][c].isRevealed && !g_board[r][c].isMine) {
                revealedCount++;
            }
        }
    }
    if (revealedCount == g_rows * g_cols - g_numMines) {
        g_gameWon = true;
        g_gameOver = true;
    }
}

// 从像素数据创建HBITMAP的工具函数
static HBITMAP CreateBitmapFromPixels(int width, int height, int channels, unsigned char* data) {
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pDest = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pDest, NULL, 0);
    ReleaseDC(NULL, hdc);

    if (hBitmap && pDest) {
        unsigned char* pDst = (unsigned char*)pDest;
        for (int i = 0; i < width * height; ++i) {
            pDst[i * 4 + 0] = data[i * 4 + 2];
            pDst[i * 4 + 1] = data[i * 4 + 1];
            pDst[i * 4 + 2] = data[i * 4 + 0];
            pDst[i * 4 + 3] = (channels == 4) ? data[i * 4 + 3] : 255;
        }
    }
    return hBitmap;
}

static bool LoadImageTo(MyImage* img, const char* path) {
    int channels = 0;
    unsigned char* data = stbi_load(path, &img->width, &img->height, &channels, 4);
    if (!data) return false;
    img->hBitmap = CreateBitmapFromPixels(img->width, img->height, 4, data);
    stbi_image_free(data);
    return img->hBitmap != NULL;
}

static void ShowDifficultyScreen() {
    g_gameState = STATE_DIFFICULTY;

    // 显示难度和读档按钮，隐藏游戏侧边按钮
    ShowWindow(g_btnRestart, SW_HIDE);
    ShowWindow(g_btnSave, SW_HIDE);
    ShowWindow(g_btnQuit, SW_HIDE);

    ShowWindow(g_btnEasy, SW_SHOW);
    ShowWindow(g_btnMed, SW_SHOW);
    ShowWindow(g_btnHard, SW_SHOW);
    ShowWindow(g_btnLoad, SW_SHOW);

    // 恢复窗口默认大小（根据需要）
    RECT rc = {0, 0, 600, 500};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(g_hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);

    InvalidateRect(g_hWnd, NULL, TRUE);
}

static void AdjustGameWindowAndButtons() {
    // 按照列数调整菜单位置
    int btnX = OFFSET_X + g_cols * CELL_SIZE + 50;
    SetWindowPos(g_btnRestart, NULL, btnX, OFFSET_Y,       0, 0, SWP_NOSIZE | SWP_NOZORDER);
    SetWindowPos(g_btnSave,    NULL, btnX, OFFSET_Y + 50,  0, 0, SWP_NOSIZE | SWP_NOZORDER);
    SetWindowPos(g_btnQuit,    NULL, btnX, OFFSET_Y + 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    // 显示游戏侧边按钮
    ShowWindow(g_btnRestart, SW_SHOW);
    ShowWindow(g_btnSave, SW_SHOW);
    ShowWindow(g_btnQuit, SW_SHOW);

    // 动态调整窗口适应棋盘
    int windowWidth = btnX + 150;
    int windowHeight = OFFSET_Y + g_rows * CELL_SIZE + 100;
    if (windowWidth < 600) windowWidth = 600;
    if (windowHeight < 500) windowHeight = 500;

    RECT rc = {0, 0, windowWidth, windowHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(g_hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);

    InvalidateRect(g_hWnd, NULL, TRUE);
}

static void StartPlay(int rows, int cols, int mines) {
    g_rows = rows;
    g_cols = cols;
    g_numMines = mines;
    g_gameState = STATE_PLAYING;
    InitGame();

    // 隐藏难度按钮
    ShowWindow(g_btnEasy, SW_HIDE);
    ShowWindow(g_btnMed, SW_HIDE);
    ShowWindow(g_btnHard, SW_HIDE);
    ShowWindow(g_btnLoad, SW_HIDE);

    AdjustGameWindowAndButtons();
}

// 存档和读档功能：将游戏状态写入/读取到文件
static void SaveGame() {
    FILE* fp = fopen("save.dat", "wb");
    if (fp) {
        fwrite(&g_rows, sizeof(g_rows), 1, fp);
        fwrite(&g_cols, sizeof(g_cols), 1, fp);
        fwrite(&g_numMines, sizeof(g_numMines), 1, fp);
        fwrite(&g_board, sizeof(g_board), 1, fp);
        fwrite(&g_gameOver, sizeof(g_gameOver), 1, fp);
        fwrite(&g_gameWon, sizeof(g_gameWon), 1, fp);
        fclose(fp);
        MessageBox(g_hWnd, "游戏状态已存档至 save.dat", "存档成功", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(g_hWnd, "存档失败！", "错误", MB_OK | MB_ICONERROR);
    }
}

static void LoadGame() {
    FILE* fp = fopen("save.dat", "rb");
    if (fp) {
        fread(&g_rows, sizeof(g_rows), 1, fp);
        fread(&g_cols, sizeof(g_cols), 1, fp);
        fread(&g_numMines, sizeof(g_numMines), 1, fp);
        fread(&g_board, sizeof(g_board), 1, fp);
        fread(&g_gameOver, sizeof(g_gameOver), 1, fp);
        fread(&g_gameWon, sizeof(g_gameWon), 1, fp);
        fclose(fp);

        g_gameState = STATE_PLAYING;

        // 隐藏难度选择界面按钮
        ShowWindow(g_btnEasy, SW_HIDE);
        ShowWindow(g_btnMed, SW_HIDE);
        ShowWindow(g_btnHard, SW_HIDE);
        ShowWindow(g_btnLoad, SW_HIDE);

        AdjustGameWindowAndButtons();
        MessageBox(g_hWnd, "读取存档成功！", "读档", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(g_hWnd, "未找到相应的存档文件！", "错误", MB_OK | MB_ICONERROR);
    }
}

// 绘制游戏棋盘：根据每个格子的状态绘制不同的颜色、数字、旗帜或雷
static void DrawBoard(HDC hdc) {
    HFONT hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            int x = OFFSET_X + c * CELL_SIZE;
            int y = OFFSET_Y + r * CELL_SIZE;
            RECT rect = {x, y, x + CELL_SIZE, y + CELL_SIZE};

            HBRUSH brush;
            if (g_board[r][c].isRevealed) {
                if (g_board[r][c].isMine) brush = CreateSolidBrush(RGB(255, 100, 100));
                else brush = CreateSolidBrush(RGB(220, 220, 220));
            } else {
                brush = CreateSolidBrush(RGB(160, 160, 160));
            }

            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            FrameRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

            SetBkMode(hdc, TRANSPARENT);
            char text[2] = {0};

            if (g_board[r][c].isRevealed) {
                if (g_board[r][c].isMine) {
                    if (g_mineImg.hBitmap) {
                        DrawImageToDC(hdc, &g_mineImg, x, y, CELL_SIZE, CELL_SIZE);
                    } else {
                        text[0] = '*';
                    }
                } else if (g_board[r][c].neighborMines > 0) {
                    text[0] = '0' + g_board[r][c].neighborMines;
                    SetTextColor(hdc, RGB(0, 0, 255));
                }
            } else if (g_board[r][c].isFlagged) {
                if (g_flagImg.hBitmap) {
                    DrawImageToDC(hdc, &g_flagImg, x, y, CELL_SIZE, CELL_SIZE);
                } else {
                    text[0] = 'F';
                    SetTextColor(hdc, RGB(255, 0, 0));
                }
            }

            if (text[0] != 0) {
                DrawText(hdc, text, 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    if (g_gameOver) {
        SetTextColor(hdc, g_gameWon ? RGB(0, 128, 0) : RGB(255, 0, 0));
        const char* msg = g_gameWon ? "游戏胜利！" : "游戏失败！";
        TextOut(hdc, OFFSET_X, OFFSET_Y + g_rows * CELL_SIZE + 20, msg, lstrlen(msg));
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_btnRestart = CreateWindow("BUTTON", "再来一局", WS_CHILD | BS_PUSHBUTTON, 0, 0, 100, 35, hwnd, (HMENU)IDC_BTN_RESTART, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            g_btnSave    = CreateWindow("BUTTON", "存档",     WS_CHILD | BS_PUSHBUTTON, 0, 0, 100, 35, hwnd, (HMENU)IDC_BTN_SAVE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            g_btnQuit    = CreateWindow("BUTTON", "退出",     WS_CHILD | BS_PUSHBUTTON, 0, 0, 100, 35, hwnd, (HMENU)IDC_BTN_QUIT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            g_btnEasy   = CreateWindow("BUTTON", "初级 (9x9 10颗雷)", WS_CHILD | BS_PUSHBUTTON, 200, 150, 200, 40, hwnd, (HMENU)IDC_BTN_EASY,   ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            g_btnMed    = CreateWindow("BUTTON", "中级 (16x16 40颗雷)",WS_CHILD | BS_PUSHBUTTON, 200, 220, 200, 40, hwnd, (HMENU)IDC_BTN_MEDIUM, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            g_btnHard   = CreateWindow("BUTTON", "高级 (16x30 99颗雷)",WS_CHILD | BS_PUSHBUTTON, 200, 290, 200, 40, hwnd, (HMENU)IDC_BTN_HARD,   ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            g_btnLoad   = CreateWindow("BUTTON", "加载存档",         WS_CHILD | BS_PUSHBUTTON, 200, 360, 200, 40, hwnd, (HMENU)IDC_BTN_LOAD,   ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            return 0;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_BTN_EASY:   StartPlay(9, 9, 10); break;
                case IDC_BTN_MEDIUM: StartPlay(16, 16, 40); break;
                case IDC_BTN_HARD:   StartPlay(16, 30, 99); break;
                case IDC_BTN_LOAD:   LoadGame(); break;

                case IDC_BTN_RESTART:
                    ShowDifficultyScreen();
                    SetFocus(hwnd);
                    break;
                case IDC_BTN_SAVE:
                    SaveGame();
                    SetFocus(hwnd);
                    break;
                case IDC_BTN_QUIT:
                    PostQuitMessage(0);
                    break;
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            if (g_gameState == STATE_COVER) {
                if (g_bg.hBitmap) {
                    DrawImageToDC(hdc, &g_bg, 0, 0, clientRect.right, clientRect.bottom);
                } else {
                    FillRect(hdc, &clientRect, (HBRUSH)(COLOR_WINDOW + 1));
                    TextOut(hdc, 250, 200, "加载封面失败，点击任意位置开始", 30);
                }
            } else if (g_gameState == STATE_DIFFICULTY) {
                if (g_gameBg.hBitmap) {
                    DrawImageToDC(hdc, &g_gameBg, 0, 0, clientRect.right, clientRect.bottom);
                } else {
                    FillRect(hdc, &clientRect, (HBRUSH)(COLOR_WINDOW + 1));
                }

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(0, 0, 0));
                HFONT titleFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
                HFONT old = (HFONT)SelectObject(hdc, titleFont);
                const char* title = "请选 择 游 戏 难 度";
                TextOut(hdc, 220, 80, title, lstrlen(title));
                SelectObject(hdc, old);
                DeleteObject(titleFont);
            } else if (g_gameState == STATE_PLAYING) {
                if (g_gameBg.hBitmap) {
                    DrawImageToDC(hdc, &g_gameBg, 0, 0, clientRect.right, clientRect.bottom);
                } else {
                    FillRect(hdc, &clientRect, (HBRUSH)(COLOR_WINDOW + 1));
                }
                DrawBoard(hdc);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (g_gameState == STATE_COVER) {
                ShowDifficultyScreen();
            } else if (g_gameState == STATE_PLAYING) {
                if (g_gameOver) return 0;
                int xPos = LOWORD(lParam);
                int yPos = HIWORD(lParam);
                int c = (xPos - OFFSET_X) / CELL_SIZE;
                int r = (yPos - OFFSET_Y) / CELL_SIZE;

                if (r >= 0 && r < g_rows && c >= 0 && c < g_cols && xPos >= OFFSET_X && yPos >= OFFSET_Y) {
                    RevealCell(r, c);
                    if (!g_gameOver) CheckWinCondition();
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }
        case WM_RBUTTONDOWN: {
            if (g_gameState != STATE_PLAYING || g_gameOver) return 0;

            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            int c = (xPos - OFFSET_X) / CELL_SIZE;
            int r = (yPos - OFFSET_Y) / CELL_SIZE;

            if (r >= 0 && r < g_rows && c >= 0 && c < g_cols && xPos >= OFFSET_X && yPos >= OFFSET_Y) {
                if (!g_board[r][c].isRevealed) {
                    g_board[r][c].isFlagged = !g_board[r][c].isFlagged;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (g_gameState == STATE_COVER) {
                ShowDifficultyScreen();
            } else if (g_gameState == STATE_PLAYING && g_gameOver) {
                if (wParam == 'R' || wParam == 'r') {
                    ShowDifficultyScreen();
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 程序入口点：注册窗口类，创建主窗口，加载资源，进入消息循环
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const char* CLASS_NAME = "MinesweeperClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, CLASS_NAME, "Minesweeper Windows",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
        NULL, NULL, hInstance, NULL
    );
    if (!g_hWnd) return 0;

    LoadImageTo(&g_bg, "start.jpg");
    LoadImageTo(&g_gameBg, "Background.png");
    LoadImageTo(&g_flagImg, "flag.png");
    LoadImageTo(&g_mineImg, "mine.png");

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}






