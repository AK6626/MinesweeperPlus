/*
 * Windows XP Minesweeper Clone - 修复版 + 缩放支持
 * 支持150%/200%缩放，使用对应尺寸资源
 */

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <mmsystem.h>
#include <commctrl.h>
#include "resource_ids.h"

#define BASE_BORDER_SIZE    6   // 原版边框基础值
#define BASE_HEADER_HEIGHT  60  // 原版头部高度
#define BASE_TIMER_WIDTH    45  // 原版计时器宽度
#define BASE_COUNTER_WIDTH  45  // 原版计数器宽度
#define BASE_DIGIT_HEIGHT   27  // 原版数字高度

/* ========== 缩放比例定义 ========== */
#define SCALE_100   0
#define SCALE_150   1
#define SCALE_200   2

/* ========== 难度/状态常量 ========== */
#define DIFF_BEGINNER       0
#define DIFF_INTERMEDIATE   1
#define DIFF_EXPERT         2

#define STATE_READY         0
#define STATE_PLAYING       1
#define STATE_WON           2
#define STATE_LOST          3

#define CELL_CLOSED         0
#define CELL_OPENED         1
#define CELL_FLAGGED        2
#define CELL_MINE_WRONG     3

#define FACE_NORMAL         0
#define FACE_PRESSED        1
#define FACE_COOL           2
#define FACE_OH             3
#define FACE_DEAD           4

/* ========== 颜色常量 ========== */
#define C_BG            RGB(192,192,192)
#define C_DARK          RGB(128,128,128)
#define C_LIGHT         RGB(255,255,255)
#define C_BLACK         RGB(0,0,0)
#define C_RED           RGB(255,0,0)

static const COLORREF g_numberColors[9] = {
    0,
    RGB(0,0,255), RGB(0,128,0), RGB(255,0,0),
    RGB(0,0,128), RGB(128,0,0), RGB(0,128,128),
    RGB(0,0,0), RGB(128,128,128)
};

#define REG_PATH "Software\\Microsoft\\winmine"
#define REG_KEY_DIFFICULTY "Current_Difficulty"  // XP原版难度键
#define REG_KEY_SCALE "Current_Scale"            // 新增缩放键
#define REG_KEY_QUICKCLICK "QuickClickEnabled"

/* ========== 全局变量 ========== */
static HWND g_hwnd = NULL;
static HINSTANCE g_hinst = NULL;

// 游戏核心参数
static int g_width = 9, g_height = 9, g_mines = 10;
static int g_difficulty = DIFF_BEGINNER;
static int g_state = STATE_READY;
static int g_explodeX = -1;
static int g_explodeY = -1;
static int g_cellsLeft = 0, g_flags = 0, g_time = 0;
static BOOL g_firstClick = TRUE, g_timerOn = FALSE;
static UINT_PTR g_timerID = 0;

// 棋盘数据
static BYTE g_board[32][17];
static BYTE g_show[32][17];

// 界面状态
static int g_face = FACE_NORMAL;
static BOOL g_faceDown = FALSE;
static BOOL g_leftDown = FALSE, g_rightDown = FALSE, g_bothDown = FALSE;
static int g_hoverX = -1, g_hoverY = -1;
static int g_chordX = -1, g_chordY = -1;
// 2026-4-19 新增：记录左键按下起始格子
static int g_downX = -1, g_downY = -1;

// 高分榜
static struct { int time; char name[32]; } g_scores[3][3];
static int g_newRank = -1;
static char g_newName[32] = "玩家";

// 缩放控制
static int g_scaleMode = SCALE_150;  // 默认100%缩放
static float g_scaleFactor = 1.5f;   // 缩放因子（1.0/1.5/2.0）
// 动态尺寸（根据缩放计算）
static int g_cellSize;
static int g_faceSize;
static int g_borderSize;
static int g_headerHeight;
static int g_timerWidth;
static int g_counterWidth;
static int g_digitHeight;
static BOOL g_quickClickEnabled = FALSE; 

// 全局变量存储预加载的位图
// 在全局变量区域添加
// 预加载的位图资源
// [缩放级别][位图类型]
static HBITMAP g_cellBitmaps[3][16];    // 3种缩放，每种16个单元格位图
static HBITMAP g_faceBitmaps[3][5];     // 3种缩放，每种5个笑脸位图
static BOOL g_bitmapsLoaded = FALSE;    // 标记是否已加载

// 预创建3种缩放的字体，避免运行时创建开销
static HFONT g_hNumberFonts[3] = {NULL, NULL, NULL};  // [SCALE_100, SCALE_150, SCALE_200]
// 双缓冲缓存
static HDC g_hMemDC = NULL;           // 内存DC
static HBITMAP g_hMemBmp = NULL;      // 内存位图
static HBITMAP g_hOldBmp = NULL;      // 原位置位图（用于恢复）
static int g_bufferWidth = 0;         // 当前缓存宽度
static int g_bufferHeight = 0;        // 当前缓存高度


/* ========== 函数声明 ========== */
size_t strcpy_safe(char *dest, const char *src, size_t size);

static void InitGame(void);
static void SetDifficulty(int diff);
static void PlaceMines(int safeX, int safeY);
static int CountMines(int x, int y);
static void OpenCell(int x, int y);
static void ToggleMark(int x, int y);
static void DoBothClick(int x, int y);
static void CheckWin(void);
static void GameWin(void);
static void GameOver(void);
static void RestartGame(void);
static void StartTimer(void);
static void StopTimer(void);
static void LoadScores(void);
static void SaveScores(void);
static void CheckHighScore(void);
static void ShowScores(void);

// 加载所有位图资源（程序启动时调用一次）
static void LoadBitmapResources(void);
// 释放所有位图资源（程序退出时调用）
static void FreeBitmapResources(void);
// 预创建所有缩放级别的字体（极致流畅方案）
static void CreateAllNumberFonts(void);
static void FreeAllNumberFonts(void);
// 双缓冲
static void UpdateBackBuffer(HWND hwnd);
static void FreeBackBuffer(void);

static void DrawAll(HDC hdc);
static void DrawHeader(HDC hdc);
static void DrawBoard(HDC hdc);
static void DrawCell(HDC hdc, int x, int y);
static void DrawNumber(HDC hdc, int x, int y, int num);
static void DrawFace(HDC hdc);
static void DrawInsetFrame(HDC hdc, int x, int y, int w, int h);

static void GetCell(int px, int py, int *cx, int *cy);
static BOOL InBoard(int x, int y);
static void UpdateTitle(void);
static void PlaySoundEffect(int type);
static void InvalidateCell(int x, int y);
static void InvalidateAround(int x, int y);
static void UpdateScaleDimensions(void);  // 新增：更新缩放尺寸
static void SetScaleMode(int scaleMode);  // 新增：设置缩放模式
// 新增：根据缩放模式获取对应的资源ID
/*
static int GetCellResId(int baseId);
static int GetFaceResId(int baseId);
*/
// 调整：缩放绘制位图（适配不同原始尺寸资源）
static void DrawScaledBitmap(HDC hdc, int destX, int destY, int destW, int destH, HBITMAP hBmp, int srcW, int srcH);

static void LoadConfigFromReg(void);
static void SaveConfigToReg(void);

static inline RECT GetFaceRect(void) {
    RECT rc;
    int sz = g_faceSize;
    // 水平居中：窗口宽度的一半减去笑脸宽度的一半
    rc.left = (g_width * g_cellSize + g_borderSize * 2) / 2 - sz / 2;
    // 垂直居中：在头部区域内居中
    rc.top = (g_headerHeight - sz) / 2;
    rc.right = rc.left + sz;
    rc.bottom = rc.top + sz;
    return rc;
}

size_t strcpy_safe(char *dest, const char *src, size_t size) {
    if (size == 0) return strlen(src);
    
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    
    // 返回实际需要的长度（类似 strlcpy）
    while (src[i] != '\0') i++;
    return i;
}

/* ========== 缩放适配函数 ========== */
// 更新所有动态尺寸（基于缩放因子）
static void UpdateScaleDimensions(void) {
    // 根据缩放模式设置基础尺寸
    switch (g_scaleMode) {
        case SCALE_100:
            // 100%缩放：直接使用100%资源尺寸
            g_cellSize = BASE_CELL_SIZE_100;
            g_faceSize = BASE_FACE_SIZE_100;
            g_scaleFactor = 1.0f;
            break;
        case SCALE_150:
            // 150%缩放：直接使用150%资源尺寸
            g_cellSize = BASE_CELL_SIZE_150;
            g_faceSize = BASE_FACE_SIZE_150;
            g_scaleFactor = 1.5f;
            break;
        case SCALE_200:
            // 200%缩放：直接使用200%资源尺寸
            g_cellSize = BASE_CELL_SIZE_200;
            g_faceSize = BASE_FACE_SIZE_200;
            g_scaleFactor = 2.0f;
            break;
        default:
            g_cellSize = BASE_CELL_SIZE_100;
            g_faceSize = BASE_FACE_SIZE_100;
            g_scaleFactor = 1.0f;
            break;
    }

    // 其他尺寸按缩放因子比例计算
    g_borderSize = (int)(BASE_BORDER_SIZE * g_scaleFactor);
    g_headerHeight = (int)(BASE_HEADER_HEIGHT * g_scaleFactor);
    g_timerWidth = (int)(BASE_TIMER_WIDTH * g_scaleFactor);
    g_counterWidth = (int)(BASE_COUNTER_WIDTH * g_scaleFactor);
    g_digitHeight = (int)(BASE_DIGIT_HEIGHT * g_scaleFactor);
}

// 设置缩放模式并更新尺寸
static void SetScaleMode(int scaleMode) {
    g_scaleMode = scaleMode;
    UpdateScaleDimensions();

    int w = g_width * g_cellSize + g_borderSize*2;
    int h = g_height * g_cellSize + g_headerHeight + g_borderSize;
    RECT rc = {0,0,w,h};
    AdjustWindowRect(&rc, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, TRUE);
    SetWindowPos(g_hwnd, NULL, 0, 0, rc.right-rc.left, rc.bottom-rc.top,
        SWP_NOMOVE|SWP_NOZORDER|SWP_NOCOPYBITS);

    SaveConfigToReg();  // 切换缩放时保存
    InvalidateRect(g_hwnd, NULL, TRUE);
    UpdateTitle();
}

// 缩放绘制位图（适配不同原始尺寸资源）
static void DrawScaledBitmap(HDC hdc, int destX, int destY, int destW, int destH, HBITMAP hBmp, int srcW, int srcH) {
    if (!hBmp) return;
    
    // 创建兼容DC
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
    
    // 设置拉伸模式为线性插值（平滑放大/缩小）
    int oldStretchMode = SetStretchBltMode(hdc, HALFTONE);
    
    // 缩放绘制
    StretchBlt(hdc, destX, destY, destW, destH,
               memDC, 0, 0, srcW, srcH,
               SRCCOPY);
    
    // 恢复状态
    SetStretchBltMode(hdc, oldStretchMode);
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
}

/* ========== 游戏逻辑 (保留原有逻辑，仅调整InitGame中的缩放初始化) ========== */
static void InitGame(void) {
    // 完整重置 + 整个窗口重绘（用于难度切换或启动时）
    memset(g_board, 0, sizeof(g_board));
    memset(g_show, 0, sizeof(g_show));
    g_state = STATE_READY;
    g_firstClick = TRUE;
    g_cellsLeft = g_width * g_height - g_mines;
    g_flags = 0;
    g_time = 0;
    g_timerOn = FALSE;
    g_face = FACE_NORMAL;
    g_faceDown = FALSE;
    g_leftDown = FALSE;
    g_rightDown = FALSE;
    g_bothDown = FALSE;
    g_hoverX = g_hoverY = -1;
    g_chordX = g_chordY = -1;
    g_newRank = -1;
    g_explodeX = -1;
    g_explodeY = -1;
    
    if (g_timerID) {
        KillTimer(g_hwnd, g_timerID);
        g_timerID = 0;
    }
    
    // 整个客户区重绘（FALSE 避免闪烁）
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateTitle();
}

static void SetDifficulty(int diff) {
    g_difficulty = diff;
    switch (diff) {
        case DIFF_BEGINNER: g_width=9; g_height=9; g_mines=10; break;
        case DIFF_INTERMEDIATE: g_width=16; g_height=16; g_mines=40; break;
        case DIFF_EXPERT: g_width=30; g_height=16; g_mines=99; break;
    }
    
    // 调整窗口大小（难度改变时必须）
    int w = g_width * g_cellSize + g_borderSize*2;
    int h = g_height * g_cellSize + g_headerHeight + g_borderSize;
    RECT rc = {0,0,w,h};
    AdjustWindowRect(&rc, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, TRUE);
    SetWindowPos(g_hwnd, NULL, 0, 0, rc.right-rc.left, rc.bottom-rc.top,
        SWP_NOMOVE|SWP_NOZORDER|SWP_NOCOPYBITS);  // 添加 SWP_NOCOPYBITS 减少闪烁
    
    SaveConfigToReg();
    InitGame();  // 新难度需要完整初始化
}

static void PlaceMines(int safeX, int safeY) {
    srand((unsigned)time(NULL));
    int placed = 0;
    while (placed < g_mines) {
        int x = rand() % g_width;
        int y = rand() % g_height;
        if (abs(x-safeX)<=1 && abs(y-safeY)<=1) continue;
        if (g_board[x][y] != 9) {
            g_board[x][y] = 9;
            placed++;
        }
    }
    for (int y=0; y<g_height; y++)
        for (int x=0; x<g_width; x++)
            if (g_board[x][y] != 9)
                g_board[x][y] = CountMines(x,y);
}

static int CountMines(int x, int y) {
    int c=0;
    for (int dy=-1; dy<=1; dy++)
        for (int dx=-1; dx<=1; dx++)
            if (dx||dy) {
                int nx=x+dx, ny=y+dy;
                if (InBoard(nx,ny) && g_board[nx][ny]==9) c++;
            }
    return c;
}

static BOOL InBoard(int x, int y) {
    return x>=0 && x<g_width && y>=0 && y<g_height;
}

static void InvalidateCell(int x, int y) {
    if (!InBoard(x,y)) return;
    RECT rc;
    rc.left = g_borderSize + x*g_cellSize;
    rc.top = g_headerHeight + y*g_cellSize;
    rc.right = rc.left + g_cellSize;
    rc.bottom = rc.top + g_cellSize;
    InvalidateRect(g_hwnd, &rc, FALSE);
}

static void InvalidateAround(int x, int y) {
    for (int dy=-1; dy<=1; dy++)
        for (int dx=-1; dx<=1; dx++)
            InvalidateCell(x+dx, y+dy);
}

static void OpenCell(int x, int y) {
    if (!InBoard(x,y)) return;
    if (g_show[x][y]!=CELL_CLOSED) return;
    
    g_show[x][y] = CELL_OPENED;
    g_cellsLeft--;
    
    if (g_board[x][y]==0) {
        for (int dy=-1; dy<=1; dy++)
            for (int dx=-1; dx<=1; dx++)
                if (dx||dy) OpenCell(x+dx, y+dy);
    }
}

static void ToggleMark(int x, int y) {
    if (!InBoard(x,y)) return;
    if (g_show[x][y] == CELL_OPENED) return;
    
    if (g_show[x][y] == CELL_CLOSED) {
        if (g_flags < g_mines) {
            g_show[x][y] = CELL_FLAGGED;
            g_flags++;
            PlaySoundEffect(1);
        }
    } else if (g_show[x][y] == CELL_FLAGGED) {
        g_show[x][y] = CELL_CLOSED;
        g_flags--;
    }
}

static void DoBothClick(int x, int y) {
    if (!InBoard(x,y)) return;
    if (g_show[x][y] != CELL_OPENED) return;
    if (g_board[x][y] == 0) return;
    
    int flags=0, closed=0;
    for (int dy=-1; dy<=1; dy++)
        for (int dx=-1; dx<=1; dx++)
            if (dx||dy) {
                int nx=x+dx, ny=y+dy;
                if (InBoard(nx,ny)) {
                    if (g_show[nx][ny]==CELL_FLAGGED) flags++;
                    else if (g_show[nx][ny]==CELL_CLOSED) closed++;
                }
            }
    
    if (flags==g_board[x][y] && closed>0) {
        for (int dy=-1; dy<=1; dy++)
            for (int dx=-1; dx<=1; dx++)
                if (dx||dy) {
                    int nx=x+dx, ny=y+dy;
                    if (InBoard(nx,ny) && g_show[nx][ny]==CELL_CLOSED) {
                        if (g_board[nx][ny]==9) {
                            g_show[nx][ny] = CELL_OPENED;

                            // ======================================
                            // 必须加这两行！！！
                            g_explodeX = nx;
                            g_explodeY = ny;
                            // ======================================

                            GameOver();
                            return;
                        }
                        OpenCell(nx,ny);
                    }
                }
        CheckWin();
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void CheckWin(void) {
    if (g_cellsLeft==0) GameWin();
}

static void GameWin(void) {
    g_state = STATE_WON;
    g_face = FACE_COOL;
    StopTimer();
    for (int y=0; y<g_height; y++)
        for (int x=0; x<g_width; x++)
            if (g_board[x][y]==9 && g_show[x][y]!=CELL_FLAGGED)
                g_show[x][y] = CELL_FLAGGED;
    g_flags = g_mines;
    g_chordX = g_chordY = -1;
    PlaySoundEffect(2);
    CheckHighScore();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void GameOver(void) {
    g_state = STATE_LOST;
    g_face = FACE_DEAD;
    StopTimer();

    for (int y=0; y<g_height; y++)
    {
        for (int x=0; x<g_width; x++)
        {
            // 原来的逻辑：显示所有没被旗子挡住的地雷
            if (g_board[x][y]==9 && g_show[x][y]!=CELL_FLAGGED)
            {
                g_show[x][y] = CELL_OPENED;
            }

            // ==============================================
            // 新增：插了旗子 但 不是地雷 → 显示错误雷
            // ==============================================
            if (g_show[x][y] == CELL_FLAGGED && g_board[x][y] != 9)
            {
                g_show[x][y] = CELL_MINE_WRONG; // 你定义的错误状态
            }
        }
    }

    g_chordX = g_chordY = -1;
    PlaySoundEffect(3);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

// 新增：同难度重置游戏（不改变窗口大小，精准重绘）
static void RestartGame(void) {
    // 只重置游戏状态，不调整窗口
    memset(g_board, 0, sizeof(g_board));
    memset(g_show, 0, sizeof(g_show));
    g_state = STATE_READY;
    g_firstClick = TRUE;
    g_cellsLeft = g_width * g_height - g_mines;
    g_flags = 0;
    g_time = 0;
    g_timerOn = FALSE;
    g_face = FACE_NORMAL;
    g_faceDown = FALSE;
    g_leftDown = FALSE;
    g_rightDown = FALSE;
    g_bothDown = FALSE;
    g_hoverX = g_hoverY = -1;
    g_chordX = g_chordY = -1;
    g_newRank = -1;
    g_explodeX = -1;
    g_explodeY = -1;
    
    if (g_timerID) {
        KillTimer(g_hwnd, g_timerID);
        g_timerID = 0;
    }
    
    // 精准重绘：只重绘需要更新的区域
    // 1. 重绘左侧计数器
    RECT rcCounter = {
        g_borderSize, 
        (int)(18 * g_scaleFactor), 
        g_borderSize + g_counterWidth + (int)(4 * g_scaleFactor), 
        (int)(18 * g_scaleFactor) + g_digitHeight + (int)(4 * g_scaleFactor)
    };
    InvalidateRect(g_hwnd, &rcCounter, FALSE);
    
    // 2. 重绘右侧计时器
    int w = g_width * g_cellSize + g_borderSize * 2;
    RECT rcTimer = {
        w - g_borderSize - g_timerWidth - (int)(4 * g_scaleFactor),
        (int)(18 * g_scaleFactor),
        w - g_borderSize,
        (int)(18 * g_scaleFactor) + g_digitHeight + (int)(4 * g_scaleFactor)
    };
    InvalidateRect(g_hwnd, &rcTimer, FALSE);
    
    // 3. 重绘笑脸
    RECT rcFace = GetFaceRect();
    // 稍微扩大确保重绘完整
    RECT rcInvalidate = rcFace;
    InflateRect(&rcInvalidate, 2, 2);
    InvalidateRect(g_hwnd, &rcInvalidate, FALSE);
    
    // 4. 重绘整个雷区（使用 FALSE 避免背景擦除闪烁）
    RECT rcBoard = {
        g_borderSize,
        g_headerHeight,
        g_borderSize + g_width * g_cellSize,
        g_headerHeight + g_height * g_cellSize
    };
    InvalidateRect(g_hwnd, &rcBoard, FALSE);
    
    UpdateTitle();
}

static void StartTimer(void) {
    if (!g_timerOn) {
        g_timerOn = TRUE;
        g_timerID = SetTimer(g_hwnd, 1, 1000, NULL);
    }
}

static void StopTimer(void) {
    if (g_timerOn) {
        g_timerOn = FALSE;
        if (g_timerID) KillTimer(g_hwnd, g_timerID);
        g_timerID = 0;
    }
}

/* ========== 注册表操作 (保留原有逻辑) ========== */
static void LoadScores(void) {
    for (int d=0; d<3; d++)
        for (int i=0; i<3; i++) {
            g_scores[d][i].time = 999;
            strcpy_safe(g_scores[d][i].name, "匿名", 32);
        }
    
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey)==ERROR_SUCCESS) {
        for (int d=0; d<3; d++) {
            for (int i=0; i<3; i++) {
                char tkey[32], nkey[32];
                
                if (i == 0) {
                    const char *prefix = (d==0)?"":(d==1)?"Intermediate":"Expert";
                    sprintf(tkey, "%sTime%d", prefix, i+1);
                    sprintf(nkey, "%sName%d", prefix, i+1);
                } else {
                    const char *prefix = (d==0)?"Beginner":(d==1)?"Intermediate":"Expert";
                    sprintf(tkey, "%sTime%d_Extra", prefix, i+1);
                    sprintf(nkey, "%sName%d_Extra", prefix, i+1);
                }
                
                DWORD type, size;
                size = sizeof(int);
                RegQueryValueEx(hKey, tkey, NULL, &type, (LPBYTE)&g_scores[d][i].time, &size);
                
                char name[32];
                size = sizeof(name);
                if (RegQueryValueEx(hKey, nkey, NULL, &type, (LPBYTE)name, &size)==ERROR_SUCCESS)
                    strcpy_safe(g_scores[d][i].name, name, 32);
            }
        }
        RegCloseKey(hKey);
    }
}

static void SaveScores(void) {
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp)==ERROR_SUCCESS) {
        for (int d=0; d<3; d++)
            for (int i=0; i<3; i++) {
                char tkey[32], nkey[32];
                
                if (i == 0) {
                    const char *prefix = (d==0)?"":(d==1)?"Intermediate":"Expert";
                    sprintf(tkey, "%sTime%d", prefix, i+1);
                    sprintf(nkey, "%sName%d", prefix, i+1);
                } else {
                    const char *prefix = (d==0)?"Beginner":(d==1)?"Intermediate":"Expert";
                    sprintf(tkey, "%sTime%d_Extra", prefix, i+1);
                    sprintf(nkey, "%sName%d_Extra", prefix, i+1);
                }
                
                RegSetValueEx(hKey, tkey, 0, REG_DWORD, (LPBYTE)&g_scores[d][i].time, sizeof(int));
                RegSetValueEx(hKey, nkey, 0, REG_SZ, (LPBYTE)g_scores[d][i].name, strlen(g_scores[d][i].name)+1);
            }
        RegCloseKey(hKey);
    }
}

static void ShowScores(void) {
    char buf[1024] = "扫雷高分榜\n\n";
    const char *diffNames[3] = {"初级", "中级", "高级"};
    
    for (int d = 0; d < 3; d++) {
        char section[256];
        sprintf(section, "--- %s ---\n", diffNames[d]);
        strcat(buf, section);
        
        for (int i = 0; i < 3; i++) {
            char line[64];
            sprintf(line, "%d. %3d 秒  %s\n", 
                i+1, g_scores[d][i].time, g_scores[d][i].name);
            strcat(buf, line);
        }
        strcat(buf, "\n");
    }
    
    MessageBox(g_hwnd, buf, "高分榜", MB_OK | MB_ICONINFORMATION);
}

static int ShowNameInputDialog(int difficulty, int time) {
    const char *diffNames[3] = {"初级", "中级", "高级"};
    
    // 获取父窗口的字体
    HFONT hParentFont = (HFONT)SendMessage(g_hwnd, WM_GETFONT, 0, 0);
    if (hParentFont == NULL) {
        // 如果父窗口没有设置字体，使用默认系统字体
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
        hParentFont = CreateFontIndirect(&ncm.lfMessageFont);
    }
    
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "#32770",
        "扫雷",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CENTER,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 180,
        g_hwnd, NULL, g_hinst, NULL
    );
    
    if (!hDlg) return IDCANCEL;
    
    char prompt[128];
    sprintf(prompt, "新的%s高分！\n你用了%d秒！", diffNames[difficulty], time);
    
    HWND hStatic = CreateWindow("STATIC", prompt,
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 15, 
        280, 40, 
        hDlg, NULL, g_hinst, NULL);
    SendMessage(hStatic, WM_SETFONT, (WPARAM)hParentFont, TRUE);
    
    HWND hEdit = CreateWindow("EDIT", g_newName,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER,
        80, 65, 
        160, 24, 
        hDlg, (HMENU)1001, g_hinst, NULL);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hParentFont, TRUE);
    
    HWND hBtnOK = CreateWindow("BUTTON", "确定",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        120, 110, 
        80, 26, 
        hDlg, (HMENU)IDOK, g_hinst, NULL);
    SendMessage(hBtnOK, WM_SETFONT, (WPARAM)hParentFont, TRUE);
    
    SetFocus(hEdit);
    SendMessage(hEdit, EM_SETSEL, 0, -1);
    
    RECT rcDlg, rcOwner;
    GetWindowRect(g_hwnd, &rcOwner);
    GetWindowRect(hDlg, &rcDlg);
    int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hDlg, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE);
    
    EnableWindow(g_hwnd, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    
    MSG msg;
    int result = IDCANCEL;
    BOOL running = TRUE;
    
    while (running && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) {
            HWND hFocus = GetFocus();
            if (hFocus == hEdit)
                SetFocus(hBtnOK);
            else
                SetFocus(hEdit);
            continue;
        }
        
        if (msg.message == WM_LBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hDlg, &pt);
            RECT rc;
            GetClientRect(hBtnOK, &rc);
            MapWindowPoints(hBtnOK, hDlg, (LPPOINT)&rc, 2);
            if (PtInRect(&rc, pt)) {
                GetWindowText(hEdit, g_newName, sizeof(g_newName));
                if (strlen(g_newName) == 0) strcpy_safe(g_newName, "匿名", 32);
                result = IDOK;
                running = FALSE;
                continue;
            }
        }
        
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            GetWindowText(hEdit, g_newName, sizeof(g_newName));
            if (strlen(g_newName) == 0) strcpy_safe(g_newName, "匿名", 32);
            result = IDOK;
            running = FALSE;
            continue;
        }
        
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            strcpy_safe(g_newName, "匿名", 32);
            result = IDCANCEL;
            running = FALSE;
            continue;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    EnableWindow(g_hwnd, TRUE);
    SetForegroundWindow(g_hwnd);
    DestroyWindow(hDlg);
    
    // 释放字体资源
    if (hParentFont != NULL) {
        DeleteObject(hParentFont);
    }
    
    return result;
}

static void CheckHighScore(void) {
    int d = g_difficulty;
    int t = g_time;
    int rank = -1;
    
    for (int i = 0; i < 3; i++)
        if (t < g_scores[d][i].time) { rank = i; break; }
    
    if (rank >= 0) {
        int dlgResult = ShowNameInputDialog(d, t);
        
        if (dlgResult == IDOK) {
            for (int i = 2; i > rank; i--) 
                g_scores[d][i] = g_scores[d][i-1];
            
            g_scores[d][rank].time = t;
            strcpy_safe(g_scores[d][rank].name, g_newName, 32);
            SaveScores();
        }
    }
}

/* ========== 绘制函数 (修改资源加载和绘制逻辑) ========== */
static void DrawInsetFrame(HDC hdc, int x, int y, int w, int h) {
    int penWidth = (int)(2 * g_scaleFactor);
    HPEN dark = CreatePen(PS_SOLID, penWidth, C_DARK);
    HPEN light = CreatePen(PS_SOLID, penWidth, C_LIGHT);
    
    SelectObject(hdc, dark);
    MoveToEx(hdc, x, y+h-1, NULL);
    LineTo(hdc, x, y);
    LineTo(hdc, x+w-1, y);
    
    SelectObject(hdc, light);
    MoveToEx(hdc, x+w-1, y, NULL);
    LineTo(hdc, x+w-1, y+h-1);
    LineTo(hdc, x, y+h-1);
    
    DeleteObject(dark);
    DeleteObject(light);
}

static void DrawNumber(HDC hdc, int x, int y, int num) {
    char str[4];
    if (num > 999) num = 999;
    if (num < -99) num = -99;
    sprintf(str, "%03d", num);
    
    int margin = (int)(2 * g_scaleFactor);
    RECT rc = {x + margin, y + margin, x + g_counterWidth - margin, y + g_digitHeight - margin};
    
    // 填充黑色背景
    HBRUSH br = CreateSolidBrush(C_BLACK);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    
    // 设置文字属性
    SetTextColor(hdc, C_RED);
    SetBkMode(hdc, TRANSPARENT);
    
    // 直接使用预创建的字体（零开销，极致流畅）
    HFONT oldFont = (HFONT)SelectObject(hdc, g_hNumberFonts[g_scaleMode]);
    
    // 计算居中位置
    SIZE textSize;
    GetTextExtentPoint32(hdc, str, 3, &textSize);
    
    int innerWidth = g_counterWidth - 2 * margin;
    int innerHeight = g_digitHeight - 2 * margin;
    int textX = x + margin + (innerWidth - textSize.cx) / 2;
    int textY = y + margin + (innerHeight - textSize.cy) / 2 + (int)(1 * g_scaleFactor);
    
    // 边界保护
    if (textX < x + margin) textX = x + margin;
    if (textY < y + margin) textY = y + margin;
    if (textY + textSize.cy > y + g_digitHeight - margin)
        textY = y + g_digitHeight - margin - textSize.cy;
    
    // 绘制文字
    TextOut(hdc, textX, textY, str, 3);
    
    // 恢复原字体（不删除！）
    SelectObject(hdc, oldFont);
}

static void DrawFace(HDC hdc) {
    RECT rcFace = GetFaceRect();
    int x = rcFace.left;
    int y = rcFace.top;
    int sz = g_faceSize;
    
    int faceIndex;
    switch (g_face) {
        case FACE_NORMAL:   faceIndex = 0; break;
        case FACE_PRESSED:  faceIndex = 1; break;
        case FACE_COOL:     faceIndex = 2; break;
        case FACE_OH:       faceIndex = 3; break;
        case FACE_DEAD:     faceIndex = 4; break;
        default:            faceIndex = 0; break;
    }
    
    // 直接使用预加载的位图
    HBITMAP hBmp = g_faceBitmaps[g_scaleMode][faceIndex];
    if (hBmp) {
        int srcW, srcH;
        switch (g_scaleMode) {
            case SCALE_100: srcW = BASE_FACE_SIZE_100; srcH = BASE_FACE_SIZE_100; break;
            case SCALE_150: srcW = BASE_FACE_SIZE_150; srcH = BASE_FACE_SIZE_150; break;
            case SCALE_200: srcW = BASE_FACE_SIZE_200; srcH = BASE_FACE_SIZE_200; break;
            default: srcW = BASE_FACE_SIZE_100; srcH = BASE_FACE_SIZE_100; break;
        }
        DrawScaledBitmap(hdc, x, y, sz, sz, hBmp, srcW, srcH);
    } else {
        // 备用绘制...
    }
}

static void DrawCell(HDC hdc, int x, int y) {
    int px = g_borderSize + x * g_cellSize;
    int py = g_headerHeight + y * g_cellSize;
    BYTE st = g_show[x][y];
    BYTE val = g_board[x][y];

    BOOL showAsBlank = FALSE;
    // 单左键按住时的按下高亮（新增）
    if (g_leftDown && !g_bothDown && 
        (g_state == STATE_READY || g_state == STATE_PLAYING)) {
        if (x == g_hoverX && y == g_hoverY && st == CELL_CLOSED) {
            showAsBlank = TRUE;
        }
    }

    if (g_chordX >= 0 && g_chordY >= 0) {
        if (abs(x - g_chordX) <= 1 && abs(y - g_chordY) <= 1 && !(x == g_chordX && y == g_chordY)) {
            if (st == CELL_CLOSED) {
                showAsBlank = TRUE;
            }
        }
    }

    int bmpIndex = -1;
    if (st == CELL_MINE_WRONG) {
        bmpIndex = 13;  // IDB_CELL*_MINE_WRONG 的索引
    }
    else if (st == CELL_CLOSED || st == CELL_FLAGGED) {
        if (showAsBlank) {
            bmpIndex = 0;  // IDB_CELL*_0
        } else if (st == CELL_FLAGGED) {
            bmpIndex = 10; // IDB_CELL*_FLAG
        } else {
            bmpIndex = 9;  // IDB_CELL*_UNREVEALED
        }
    } else {
        if (g_state == STATE_LOST && val == 9) {
            if (x == g_explodeX && y == g_explodeY) {
                bmpIndex = 12; // IDB_CELL*_MINE_RED
            } else {
                bmpIndex = 11; // IDB_CELL*_MINE
            }
        } else if (val >= 0 && val <= 8) {
            bmpIndex = val;  // IDB_CELL*_0 到 IDB_CELL*_8
        } else {
            bmpIndex = 0;
        }
    }

    // 直接使用预加载的位图，不再调用 LoadImage！
    HBITMAP hBmp = g_cellBitmaps[g_scaleMode][bmpIndex];
    if (hBmp) {
        int srcW, srcH;
        switch (g_scaleMode) {
            case SCALE_100: srcW = BASE_CELL_SIZE_100; srcH = BASE_CELL_SIZE_100; break;
            case SCALE_150: srcW = BASE_CELL_SIZE_150; srcH = BASE_CELL_SIZE_150; break;
            case SCALE_200: srcW = BASE_CELL_SIZE_200; srcH = BASE_CELL_SIZE_200; break;
            default: srcW = BASE_CELL_SIZE_100; srcH = BASE_CELL_SIZE_100; break;
        }
        DrawScaledBitmap(hdc, px, py, g_cellSize, g_cellSize, hBmp, srcW, srcH);
    }
}

static void DrawBoard(HDC hdc) {
    for (int y=0; y<g_height; y++)
        for (int x=0; x<g_width; x++)
            DrawCell(hdc, x, y);
}

static void DrawHeader(HDC hdc) {
    int w = g_width*g_cellSize + g_borderSize*2;
    
    RECT rc = {0, 0, w, g_headerHeight};
    HBRUSH br = CreateSolidBrush(C_BG);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    
    int insetX = g_borderSize - (int)(4 * g_scaleFactor);
    int insetY = (int)(10 * g_scaleFactor);
    int insetW = w - 2*(g_borderSize - (int)(4 * g_scaleFactor));
    int insetH = g_headerHeight - (int)(20 * g_scaleFactor);
    DrawInsetFrame(hdc, insetX, insetY, insetW, insetH);
    
    // 计算数字面板的垂直居中位置
    int digitY = (g_headerHeight - g_digitHeight) / 2;
    
    HBRUSH black = CreateSolidBrush(C_BLACK);
    int offset = (int)(4 * g_scaleFactor);
    
    // 左侧计数器 - 使用 digitY 垂直居中
    RECT rc1 = {
        g_borderSize + offset, 
        digitY, 
        g_borderSize + g_counterWidth + offset, 
        digitY + g_digitHeight
    };
    
    // 右侧计时器 - 使用 digitY 垂直居中
    RECT rc2 = {
        w - g_borderSize - g_timerWidth - offset, 
        digitY,
        w - g_borderSize - offset, 
        digitY + g_digitHeight
    };
    
    FrameRect(hdc, &rc1, black);
    FrameRect(hdc, &rc2, black);
    InflateRect(&rc1, -offset/2, -offset/2); 
    FillRect(hdc, &rc1, black);
    InflateRect(&rc2, -offset/2, -offset/2); 
    FillRect(hdc, &rc2, black);
    DeleteObject(black);
    
    // 绘制数字 - 使用 digitY
    DrawNumber(hdc, g_borderSize + offset, digitY, g_mines - g_flags);
    DrawNumber(hdc, w - g_borderSize - g_timerWidth - offset, digitY, g_time);
    
    DrawFace(hdc);
}

// 加载所有位图资源（程序启动时调用一次）
static void LoadBitmapResources(void) {
    if (g_bitmapsLoaded) return;  // 防止重复加载
    
    // 定义资源ID映射表
    int cellResIds[3][16] = {
        // SCALE_100 (100%)
        {
            IDB_CELL100_0, IDB_CELL100_1, IDB_CELL100_2, IDB_CELL100_3,
            IDB_CELL100_4, IDB_CELL100_5, IDB_CELL100_6, IDB_CELL100_7,
            IDB_CELL100_8, IDB_CELL100_UNREVEALED, IDB_CELL100_FLAG,
            IDB_CELL100_MINE, IDB_CELL100_MINE_RED, IDB_CELL100_MINE_WRONG,
            0, 0  // 补齐16个，最后两个未使用
        },
        // SCALE_150 (150%)
        {
            IDB_CELL150_0, IDB_CELL150_1, IDB_CELL150_2, IDB_CELL150_3,
            IDB_CELL150_4, IDB_CELL150_5, IDB_CELL150_6, IDB_CELL150_7,
            IDB_CELL150_8, IDB_CELL150_UNREVEALED, IDB_CELL150_FLAG,
            IDB_CELL150_MINE, IDB_CELL150_MINE_RED, IDB_CELL150_MINE_WRONG,
            0, 0
        },
        // SCALE_200 (200%)
        {
            IDB_CELL200_0, IDB_CELL200_1, IDB_CELL200_2, IDB_CELL200_3,
            IDB_CELL200_4, IDB_CELL200_5, IDB_CELL200_6, IDB_CELL200_7,
            IDB_CELL200_8, IDB_CELL200_UNREVEALED, IDB_CELL200_FLAG,
            IDB_CELL200_MINE, IDB_CELL200_MINE_RED, IDB_CELL200_MINE_WRONG,
            0, 0
        }
    };
    
    int faceResIds[3][5] = {
        // SCALE_100
        {IDB_FACE100_NORMAL, IDB_FACE100_PRESSED, IDB_FACE100_COOL, IDB_FACE100_OH, IDB_FACE100_DEAD},
        // SCALE_150
        {IDB_FACE150_NORMAL, IDB_FACE150_PRESSED, IDB_FACE150_COOL, IDB_FACE150_OH, IDB_FACE150_DEAD},
        // SCALE_200
        {IDB_FACE200_NORMAL, IDB_FACE200_PRESSED, IDB_FACE200_COOL, IDB_FACE200_OH, IDB_FACE200_DEAD}
    };
    
    // 加载所有单元格位图
    for (int scale = 0; scale < 3; scale++) {
        for (int i = 0; i < 16; i++) {
            if (cellResIds[scale][i] != 0) {
                g_cellBitmaps[scale][i] = (HBITMAP)LoadImage(
                    g_hinst,
                    MAKEINTRESOURCE(cellResIds[scale][i]),
                    IMAGE_BITMAP,
                    0, 0,
                    LR_DEFAULTCOLOR
                );
            } else {
                g_cellBitmaps[scale][i] = NULL;
            }
        }
    }
    
    // 加载所有笑脸位图
    for (int scale = 0; scale < 3; scale++) {
        for (int i = 0; i < 5; i++) {
            g_faceBitmaps[scale][i] = (HBITMAP)LoadImage(
                g_hinst,
                MAKEINTRESOURCE(faceResIds[scale][i]),
                IMAGE_BITMAP,
                0, 0,
                LR_DEFAULTCOLOR
            );
        }
    }
    
    g_bitmapsLoaded = TRUE;
}

// 释放所有位图资源（程序退出时调用）
static void FreeBitmapResources(void) {
    if (!g_bitmapsLoaded) return;
    
    for (int scale = 0; scale < 3; scale++) {
        for (int i = 0; i < 16; i++) {
            if (g_cellBitmaps[scale][i]) {
                DeleteObject(g_cellBitmaps[scale][i]);
                g_cellBitmaps[scale][i] = NULL;
            }
        }
    }
    
    for (int scale = 0; scale < 3; scale++) {
        for (int i = 0; i < 5; i++) {
            if (g_faceBitmaps[scale][i]) {
                DeleteObject(g_faceBitmaps[scale][i]);
                g_faceBitmaps[scale][i] = NULL;
            }
        }
    }
    
    g_bitmapsLoaded = FALSE;
}

// 预创建所有缩放级别的字体（极致流畅方案）
static void CreateAllNumberFonts(void) {
    // 对应的缩放因子
    float scaleFactors[3] = {1.0f, 1.5f, 2.0f};
    
    for (int i = 0; i < 3; i++) {
        if (g_hNumberFonts[i] != NULL) continue;  // 已存在则跳过
        
        int fontSize = (int)(45 * scaleFactors[i] / 2);
        int fontWidth = (int)(27 * scaleFactors[i] / 2);
        
        g_hNumberFonts[i] = CreateFont(
            fontSize,                   // 高度
            fontWidth,                  // 宽度（0表示自动选择）
            0, 0,                       // 转义角度、方向
            FW_BOLD,                    // 粗体（Segoe UI推荐用BOLD而非HEAVY）
            FALSE, FALSE, FALSE,        // 斜体、下划线、删除线
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,            // 可以用ANTIALIASED_QUALITY抗锯齿
            DEFAULT_PITCH | FF_SWISS,   // Segoe UI是无衬线字体
            "Courier New"                  // 字体名称（更像计算器）
        );
        
        // 如果Segoe UI创建失败，回退到Tahoma
        if (g_hNumberFonts[i] == NULL) {
            g_hNumberFonts[i] = CreateFont(
                fontSize, fontWidth, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Tahoma"
            );
        }
    }
}

// 释放所有字体（程序退出时调用）
static void FreeAllNumberFonts(void) {
    for (int i = 0; i < 3; i++) {
        if (g_hNumberFonts[i] != NULL) {
            DeleteObject(g_hNumberFonts[i]);
            g_hNumberFonts[i] = NULL;
        }
    }
}

// 创建或更新双缓冲缓存（窗口大小改变时调用）
static void UpdateBackBuffer(HWND hwnd) {
    // 获取新的客户区大小
    RECT rc;
    GetClientRect(hwnd, &rc);
    int newWidth = rc.right;
    int newHeight = rc.bottom;
    
    // 如果尺寸没变且已存在，直接返回
    if (g_hMemDC != NULL && g_bufferWidth == newWidth && g_bufferHeight == newHeight) {
        return;
    }
    
    // 销毁旧缓存
    if (g_hMemDC != NULL) {
        SelectObject(g_hMemDC, g_hOldBmp);  // 恢复原位图
        DeleteObject(g_hMemBmp);
        DeleteDC(g_hMemDC);
        g_hMemDC = NULL;
        g_hMemBmp = NULL;
        g_hOldBmp = NULL;
    }
    
    // 创建新DC
    HDC hdcScreen = GetDC(hwnd);
    g_hMemDC = CreateCompatibleDC(hdcScreen);
    
    // 创建新位图
    g_hMemBmp = CreateCompatibleBitmap(hdcScreen, newWidth, newHeight);
    g_hOldBmp = (HBITMAP)SelectObject(g_hMemDC, g_hMemBmp);
    
    ReleaseDC(hwnd, hdcScreen);
    
    g_bufferWidth = newWidth;
    g_bufferHeight = newHeight;
}

// 释放双缓冲缓存（程序退出时调用）
static void FreeBackBuffer(void) {
    if (g_hMemDC != NULL) {
        SelectObject(g_hMemDC, g_hOldBmp);
        DeleteObject(g_hMemBmp);
        DeleteDC(g_hMemDC);
        g_hMemDC = NULL;
        g_hMemBmp = NULL;
        g_hOldBmp = NULL;
        g_bufferWidth = 0;
        g_bufferHeight = 0;
    }
}

static void DrawAll(HDC hdc) {
    // 确保缓存有效（尺寸匹配）
    UpdateBackBuffer(g_hwnd);
    
    if (g_hMemDC == NULL) return;  // 安全检查
    
    // 直接在缓存DC上绘制（不需要创建/删除任何东西！）
    HBRUSH br = CreateSolidBrush(C_BG);
    RECT rc = {0, 0, g_bufferWidth, g_bufferHeight};
    FillRect(g_hMemDC, &rc, br);
    DeleteObject(br);
    
    DrawHeader(g_hMemDC);
    
    int boardX = g_borderSize;
    int boardY = g_headerHeight;
    int boardW = g_width * g_cellSize;
    int boardH = g_height * g_cellSize;

    DrawInsetFrame(g_hMemDC, boardX - (int)(1*g_scaleFactor), boardY - (int)(1*g_scaleFactor), 
                   boardW + (int)(2*g_scaleFactor), boardH + (int)(2*g_scaleFactor));
    
    DrawBoard(g_hMemDC);
    
    // 一次性拷贝到屏幕（零延迟）
    BitBlt(hdc, 0, 0, g_bufferWidth, g_bufferHeight, g_hMemDC, 0, 0, SRCCOPY);
}

/* ========== 辅助函数 (保留原有逻辑) ========== */
static void GetCell(int px, int py, int *cx, int *cy) {
    *cx = (px - g_borderSize) / g_cellSize;
    *cy = (py - g_headerHeight) / g_cellSize;
}

static void UpdateTitle(void) {
    const char *names[3] = {"Beginner", "Intermediate", "Expert"};
    const char *scaleNames[3] = {"100%", "150%", "200%"};
    char buf[64];
    sprintf(buf, "MinesweeperPLus - %s (%s)", names[g_difficulty], scaleNames[g_scaleMode]);
    sprintf(buf, "MinesweeperPLus");
    SetWindowText(g_hwnd, buf);
}

static void PlaySoundEffect(int type) {
    switch (type) {
        case 1:
            // 插旗 → 清脆短音
            // PlaySound(TEXT("MenuPopup"), NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
            break;
        case 2:
            // 胜利 → 上升提示音
            // PlaySound(TEXT("MailBeep"), NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
            break;
        case 3:
            // 爆炸失败 → 低沉爆炸感
            // PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
            break;
    }
}

// 从注册表加载配置
static void LoadConfigFromReg(void) {
    HKEY hKey;
    DWORD type, size, value;

    // 默认值
    g_difficulty = DIFF_BEGINNER;
    g_scaleMode = SCALE_150;
    g_quickClickEnabled = FALSE;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // 读取难度
        size = sizeof(DWORD);
        if (RegQueryValueEx(hKey, REG_KEY_DIFFICULTY, NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            if (value >= DIFF_BEGINNER && value <= DIFF_EXPERT) {
                g_difficulty = value;
            }
        }

        // 读取缩放模式
        size = sizeof(DWORD);
        if (RegQueryValueEx(hKey, REG_KEY_SCALE, NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            if (value >= SCALE_100 && value <= SCALE_200) {
                g_scaleMode = value;
            }
        }

        // 新增：读取快速点击设置
        size = sizeof(DWORD);
        if (RegQueryValueEx(hKey, REG_KEY_QUICKCLICK, NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            g_quickClickEnabled = (value != 0) ? TRUE : FALSE;
        }
        
        RegCloseKey(hKey);
    }
}

// 保存配置到注册表（难度+缩放）
static void SaveConfigToReg(void) {
    HKEY hKey;
    DWORD disp;

    if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS) {
        
        // 保存难度（复用XP原版键值）
        DWORD diff = g_difficulty;
        RegSetValueEx(hKey, REG_KEY_DIFFICULTY, 0, REG_DWORD, (LPBYTE)&diff, sizeof(DWORD));
        
        // 保存缩放模式（新增）
        DWORD scale = g_scaleMode;
        RegSetValueEx(hKey, REG_KEY_SCALE, 0, REG_DWORD, (LPBYTE)&scale, sizeof(DWORD));
        
        // 新增：保存快速点击设置
        DWORD quickClick = g_quickClickEnabled ? 1 : 0;
        RegSetValueEx(hKey, REG_KEY_QUICKCLICK, 0, REG_DWORD, (LPBYTE)&quickClick, sizeof(DWORD));        
        RegCloseKey(hKey);
    }
}

/* ========== 窗口过程 (修改默认缩放初始化) ========== */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_hwnd = hwnd;
            g_hinst = ((LPCREATESTRUCT)lParam)->hInstance;
            
            // 先加载位图资源
            LoadBitmapResources();
            // 预创建所有字体（极致流畅）
            CreateAllNumberFonts();
            
            // 设置窗口图标（确保任务栏和标题栏显示正确）
            HICON hIcon = LoadIcon(g_hinst, MAKEINTRESOURCE(ICON_MINE));
            HICON hIconSmall = LoadIcon(g_hinst, MAKEINTRESOURCE(ICON_MINE_16));
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
            
            // 加载配置
            LoadConfigFromReg();  // 设置 g_difficulty 和 g_scaleMode
    
            // 先应用缩放（设置 g_cellSize 等）
            UpdateScaleDimensions();
    
            // 再应用难度（设置 g_width/g_height/g_mines）
            switch (g_difficulty) {
                case DIFF_BEGINNER: g_width=9; g_height=9; g_mines=10; break;
                case DIFF_INTERMEDIATE: g_width=16; g_height=16; g_mines=40; break;
                case DIFF_EXPERT: g_width=30; g_height=16; g_mines=99; break;
            }
            
            // 现在尺寸正确了，调整窗口
            int w = g_width * g_cellSize + g_borderSize*2;
            int h = g_height * g_cellSize + g_headerHeight + g_borderSize;
            RECT rc = {0,0,w,h};
            AdjustWindowRect(&rc, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, TRUE);
            SetWindowPos(g_hwnd, NULL, 0, 0, rc.right-rc.left, rc.bottom-rc.top,
                SWP_NOMOVE|SWP_NOZORDER|SWP_NOCOPYBITS);
            
            LoadScores();
            InitGame();  // 现在 g_width/g_height 是正确的
            UpdateBackBuffer(hwnd);
            
            // 初始化快速点击菜单状态
            {
                HMENU hMenu = GetMenu(hwnd);
                HMENU hGameMenu = GetSubMenu(hMenu, 0);
                CheckMenuItem(hGameMenu, 104, 
                    MF_BYCOMMAND | (g_quickClickEnabled ? MF_CHECKED : MF_UNCHECKED));
            }            
            
            return 0;
        
        case WM_SIZE:
            // 窗口大小改变时，下次DrawAll会自动更新
            // 也可以立即调用 UpdateBackBuffer(hwnd) 预创建
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;            
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 101: // Beginner
                    if (g_difficulty != DIFF_BEGINNER) {
                        SetDifficulty(DIFF_BEGINNER);  // 难度改变，调整窗口
                    } else {
                        RestartGame();  // 难度相同，精准重绘
                    }
                    break;
                case 102: // Intermediate
                    if (g_difficulty != DIFF_INTERMEDIATE) {
                        SetDifficulty(DIFF_INTERMEDIATE);
                    } else {
                        RestartGame();
                    }
                    break;
                case 103: // Expert
                    if (g_difficulty != DIFF_EXPERT) {
                        SetDifficulty(DIFF_EXPERT);
                    } else {
                        RestartGame();
                    }
                    break;
                case 104: // 切换快速点击
                    g_quickClickEnabled = !g_quickClickEnabled;
                    {
                        HMENU hMenu = GetMenu(g_hwnd);
                        HMENU hGameMenu = GetSubMenu(hMenu, 0);
                        CheckMenuItem(hGameMenu, 104, 
                            MF_BYCOMMAND | (g_quickClickEnabled ? MF_CHECKED : MF_UNCHECKED));
                    }
                    SaveConfigToReg();
                    break;                    
                case 201: ShowScores(); break;
                case 203:
                    if (MessageBox(g_hwnd,
                        "确定要重置所有高分吗？",
                        "确认重置", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
                    {
                        for (int d = 0; d < 3; d++)
                            for (int i = 0; i < 3; i++) {
                                g_scores[d][i].time = 999;
                                strcpy_safe(g_scores[d][i].name, "匿名", 32);
                            }
                        SaveScores();
                        MessageBox(g_hwnd, "高分已重置。",
                            "重置完成", MB_OK | MB_ICONINFORMATION);
                    }
                    break;
                case 202: DestroyWindow(hwnd); break;
                case 300: SetScaleMode(SCALE_100); break;
                case 301: SetScaleMode(SCALE_150); break;
                case 302: SetScaleMode(SCALE_200); break;
            }
            return 0;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawAll(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
            
            RECT rcFace = GetFaceRect();
            // 扩大一点点击范围，提高用户体验（各边扩展2像素）
            RECT rcHit = rcFace;
            InflateRect(&rcHit, 2, 2);
            
            if (PtInRect(&rcHit, (POINT){x, y})) {
                g_faceDown = TRUE;
                g_face = FACE_PRESSED;
                // 只重绘笑脸区域
                InvalidateRect(hwnd, &rcFace, FALSE);
                return 0;
            }
            
            if (y>=g_headerHeight && y<g_headerHeight+g_height*g_cellSize &&
                x>=g_borderSize && x<g_borderSize+g_width*g_cellSize) {
                g_leftDown = TRUE;
                GetCell(x, y, &g_hoverX, &g_hoverY);
                // 记录按下起始格子
                g_downX = g_hoverX;
                g_downY = g_hoverY;
                
                if (g_rightDown || 
                    (g_quickClickEnabled && g_show[g_hoverX][g_hoverY] == CELL_OPENED && 
                    g_board[g_hoverX][g_hoverY] > 0 && g_board[g_hoverX][g_hoverY] <= 8)
                ) {
                    g_bothDown = TRUE;
                    g_face = FACE_OH;
                    g_chordX = g_hoverX;
                    g_chordY = g_hoverY;
                    InvalidateAround(g_chordX, g_chordY);
                } else if (g_state==STATE_READY || g_state==STATE_PLAYING) {
                    g_face = FACE_OH;
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
            if (y>=g_headerHeight && y<g_headerHeight+g_height*g_cellSize &&
                x>=g_borderSize && x<g_borderSize+g_width*g_cellSize) {
                g_rightDown = TRUE;
                int cx, cy; GetCell(x, y, &cx, &cy);
                
                if (g_leftDown) {
                    g_bothDown = TRUE;
                    g_face = FACE_OH;
                    g_chordX = cx;
                    g_chordY = cy;
                    InvalidateAround(g_chordX, g_chordY);
                } else if (g_state==STATE_READY || g_state==STATE_PLAYING) {
                    ToggleMark(cx, cy);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

            if (g_faceDown) {
                RECT rcFace = GetFaceRect();
                RECT rcHit = rcFace;
                InflateRect(&rcHit, 2, 2);
                
                if (PtInRect(&rcHit, (POINT){x, y})) {
                    RestartGame();
                }
                g_faceDown = FALSE;
                g_face = FACE_NORMAL;
                // 只重绘笑脸区域
                InvalidateRect(hwnd, &rcFace, FALSE);
                return 0;
            }

            if (g_leftDown) {
                g_leftDown = FALSE;

                if (g_chordX >= 0 && g_chordY >= 0) {
                    int oldChordX = g_chordX, oldChordY = g_chordY;
                    g_chordX = g_chordY = -1;
                    InvalidateAround(oldChordX, oldChordY);
                }

                // 获取松开时的实际位置
                int cx, cy;
                GetCell(x, y, &cx, &cy);
                if (!InBoard(cx, cy)) {
                    cx = -1; 
                    cy = -1;
                }

                if (g_rightDown || 
                    (g_quickClickEnabled && g_show[g_hoverX][g_hoverY] == CELL_OPENED && 
                    g_board[g_hoverX][g_hoverY] > 0 && g_board[g_hoverX][g_hoverY] <= 8)
                ) {
                    g_bothDown = FALSE;
                    g_rightDown = FALSE;
                    if (cx >= 0 && cy >= 0) {
                        DoBothClick(cx, cy);
                    }
                    g_face = (g_state==STATE_WON)?FACE_COOL:(g_state==STATE_LOST)?FACE_DEAD:FACE_NORMAL;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                if (g_state==STATE_LOST || g_state==STATE_WON) {
                    g_face = (g_state==STATE_WON)?FACE_COOL:FACE_DEAD;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (y>=g_headerHeight && y<g_headerHeight+g_height*g_cellSize &&
                    x>=g_borderSize && x<g_borderSize+g_width*g_cellSize) {
                    int cx, cy;
                    GetCell(x, y, &cx, &cy);
                    if (InBoard(cx,cy)) {
                        if (g_state==STATE_READY) {
                            g_state = STATE_PLAYING;
                            PlaceMines(cx, cy);
                            StartTimer();
                        }
                        if (g_show[cx][cy]==CELL_CLOSED) {
                            if (g_board[cx][cy]==9) {
                                g_show[cx][cy] = CELL_OPENED;
                                g_explodeX = cx;
                                g_explodeY = cy;
                                GameOver();
                            } else {
                                OpenCell(cx, cy);
                                CheckWin();
                            }
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                    }
                }
                g_face = (g_state==STATE_WON)?FACE_COOL:(g_state==STATE_LOST)?FACE_DEAD:FACE_NORMAL;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        
        case WM_RBUTTONUP: {
            if (g_rightDown) {
                g_rightDown = FALSE;
                
                if (g_chordX >= 0 && g_chordY >= 0) {
                    int oldChordX = g_chordX, oldChordY = g_chordY;
                    g_chordX = g_chordY = -1;
                    InvalidateAround(oldChordX, oldChordY);
                }
                
                if (g_bothDown) {
                    g_bothDown = FALSE;
                    g_leftDown = FALSE;
                    
                    // 获取松开时的实际位置
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(hwnd, &pt);
                    int cx, cy;
                    GetCell(pt.x, pt.y, &cx, &cy);                    
                    
                    if (InBoard(cx, cy)) {
                        DoBothClick(cx, cy);
                    }
                    
                    g_face = (g_state==STATE_WON)?FACE_COOL:(g_state==STATE_LOST)?FACE_DEAD:FACE_NORMAL;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (g_leftDown && !g_bothDown && (g_state == STATE_READY || g_state == STATE_PLAYING)) {
                int cx, cy, x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
                GetCell(x, y, &cx, &cy);
                
                if (InBoard(cx, cy)) {
                    if (cx != g_hoverX || cy != g_hoverY) {
                        int oldX = g_hoverX, oldY = g_hoverY;
                        g_hoverX = cx; 
                        g_hoverY = cy;
                        g_face = FACE_OH;
                        
                        // 精准局部重绘：只刷新旧格子和新格子（如果你愿意用全量重绘也可以）
                        InvalidateCell(oldX, oldY);
                        InvalidateCell(cx, cy);
                        // 或者保守一点继续用：InvalidateRect(hwnd, NULL, FALSE);
                    }
                } else {
                    // 鼠标移出棋盘区域，取消按下效果（新增）
                    if (g_hoverX != -1 || g_hoverY != -1) {
                        int oldX = g_hoverX, oldY = g_hoverY;
                        g_hoverX = -1; 
                        g_hoverY = -1;
                        g_face = FACE_NORMAL;
                        InvalidateCell(oldX, oldY);
                    }
                }
            }
            if ((g_bothDown && g_leftDown && g_rightDown) ||
                (g_bothDown && g_quickClickEnabled && g_leftDown)
            ) {
                int cx, cy, x=GET_X_LPARAM(lParam), y=GET_Y_LPARAM(lParam);
                GetCell(x, y, &cx, &cy);
                if (InBoard(cx,cy) && (cx!=g_chordX || cy!=g_chordY)) {
                    int oldX = g_chordX, oldY = g_chordY;
                    g_chordX = cx; g_chordY = cy;
                    InvalidateAround(oldX, oldY);
                    InvalidateAround(g_chordX, g_chordY);
                }
            }
            return 0;
        }
        
        case WM_TIMER: {
            if (g_timerOn && g_state==STATE_PLAYING) {
                g_time++;
                if (g_time >= 999) {
                    StopTimer();
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1;  // 阻止系统自动擦除背景
        
        case WM_DESTROY: {
            StopTimer();
            SaveConfigToReg();
            FreeBitmapResources();  // 释放位图资源
            FreeAllNumberFonts();       // 释放字体（新增）
            FreeBackBuffer();  // 释放双缓冲（新增）
            PostQuitMessage(0);
            return 0;
        }
        
case WM_KEYDOWN:
    switch (wParam) {
        case VK_F1: SetDifficulty(DIFF_BEGINNER); break;
        case VK_F2: SetDifficulty(DIFF_INTERMEDIATE); break;
        case VK_F3: SetDifficulty(DIFF_EXPERT); break;
        case VK_F4: ShowScores(); break;
        case VK_F5: SetScaleMode(SCALE_100); break;
        case VK_F6: SetScaleMode(SCALE_150); break;
        case VK_F7: SetScaleMode(SCALE_200); break;
        case 'Q':
            if (GetKeyState(VK_CONTROL) < 0) {
                PostMessage(g_hwnd, WM_COMMAND, 104, 0);
            }
            break;        
    }
    return 0;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

/* ========== 入口函数 (保留原有逻辑) ========== */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    g_hinst = hInst;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    // 加载自定义图标
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(ICON_MINE));      // 48x48 用于任务栏/Alt+Tab
    wc.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(ICON_MINE_16)); // 16x16 用于标题栏
    
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = "MinesweeperPLus";
    RegisterClassEx(&wc);

    // ========== 你原版菜单 100% 还原 ==========
    HMENU menu = CreateMenu();
    HMENU gameMenu = CreatePopupMenu();
    AppendMenu(gameMenu, MF_STRING, 101, "初级(&B)\t\tF1");
    AppendMenu(gameMenu, MF_STRING, 102, "中级(&I)\t\tF2");
    AppendMenu(gameMenu, MF_STRING, 103, "高级(&E)\t\tF3");
    AppendMenu(gameMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(gameMenu, MF_STRING | (g_quickClickEnabled ? MF_CHECKED : MF_UNCHECKED), 
               104, "快速点击\tCtrl+Q");
    AppendMenu(gameMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(gameMenu, MF_STRING, 201, "扫雷英雄榜(&T)...\tF4");
    AppendMenu(gameMenu, MF_STRING, 203, "重置分数(&R)");
    AppendMenu(gameMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(gameMenu, MF_STRING, 202, "退出(&X)");

    HMENU scaleMenu = CreatePopupMenu();
    AppendMenu(scaleMenu, MF_STRING, 300, "100% 缩放\tF5");
    AppendMenu(scaleMenu, MF_STRING, 301, "150% 缩放\tF6");
    AppendMenu(scaleMenu, MF_STRING, 302, "200% 缩放\tF7");

    AppendMenu(menu, MF_POPUP, (UINT_PTR)gameMenu, "游戏(&G)");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)scaleMenu, "缩放(&C)");

    HWND hwnd = CreateWindowEx(0, "MinesweeperPLus", "MinesweeperPLus",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 400, NULL, menu, hInst, NULL);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
