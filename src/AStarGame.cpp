#include <windows.h>
#include <vector>
#include <queue>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>

// Constants
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;
const int MAP_SIZE = 720;
const int GRID_SIZE = 30;
const int CELL_SIZE = MAP_SIZE / GRID_SIZE; // 24
const float SQUISH_RADIUS = 20.0f;

// Colors
const COLORREF COLOR_GRASS = RGB(34, 139, 34);
const COLORREF COLOR_OBSTACLE = RGB(101, 67, 33);
const COLORREF COLOR_SQUISH = RGB(0, 0, 255);
const COLORREF COLOR_PATH = RGB(255, 255, 0);
const COLORREF COLOR_SIDEBAR = RGB(50, 50, 50);

struct Point {
    int x, y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

struct Node {
    Point pos;
    float gCost, hCost;
    Point parent;
    bool obstacle;

    float fCost() const { return gCost + hCost; }

    bool operator>(const Node& other) const {
        return fCost() > other.fCost();
    }
};

// Global State
Point squishPos = { 10, 10 }; // Grid coordinates
Point targetPos = { 10, 10 };
std::vector<Point> currentPath;
bool isMoving = false;
float moveProgress = 0.0f;
size_t currentPathIndex = 0;
bool grid[GRID_SIZE][GRID_SIZE] = { false };
Point realSquishPos = { 10 * CELL_SIZE + CELL_SIZE / 2, 10 * CELL_SIZE + CELL_SIZE / 2 };

// Helper to get Euclidean distance
float getDistance(Point p1, Point p2) {
    return std::sqrt(std::pow((float)p1.x - p2.x, 2) + std::pow((float)p1.y - p2.y, 2));
}

// A* Implementation
std::vector<Point> findPath(Point start, Point end) {
    if (end.x < 0 || end.x >= GRID_SIZE || end.y < 0 || end.y >= GRID_SIZE || grid[end.y][end.x]) {
        return {};
    }

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
    std::vector<std::vector<float>> gCosts(GRID_SIZE, std::vector<float>(GRID_SIZE, INFINITY));
    std::vector<std::vector<Point>> parents(GRID_SIZE, std::vector<Point>(GRID_SIZE, { -1, -1 }));

    gCosts[start.y][start.x] = 0;
    openSet.push({ start, 0, getDistance(start, end), { -1, -1 }, false });

    while (!openSet.empty()) {
        Node current = openSet.top();
        openSet.pop();

        if (current.pos == end) {
            std::vector<Point> path;
            Point currPos = end;
            while (currPos.x != -1) {
                path.push_back(currPos);
                currPos = parents[currPos.y][currPos.x];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                Point neighbor = { current.pos.x + dx, current.pos.y + dy };
                if (neighbor.x >= 0 && neighbor.x < GRID_SIZE && neighbor.y >= 0 && neighbor.y < GRID_SIZE && !grid[neighbor.y][neighbor.x]) {
                    float moveCost = (dx == 0 || dy == 0) ? 1.0f : 1.414f;
                    float newGCost = gCosts[current.pos.y][current.pos.x] + moveCost;

                    if (newGCost < gCosts[neighbor.y][neighbor.x]) {
                        gCosts[neighbor.y][neighbor.x] = newGCost;
                        parents[neighbor.y][neighbor.x] = current.pos;
                        openSet.push({ neighbor, newGCost, getDistance(neighbor, end), current.pos, false });
                    }
                }
            }
        }
    }

    return {};
}

// Win32 Drawing
void DrawGame(HDC hdc) {
    // Double buffering (simplified for single call here, but ideally using a backbuffer)
    HBRUSH grassBrush = CreateSolidBrush(COLOR_GRASS);
    HBRUSH obstacleBrush = CreateSolidBrush(COLOR_OBSTACLE);
    HBRUSH sidebarBrush = CreateSolidBrush(COLOR_SIDEBAR);
    HPEN pathPen = CreatePen(PS_SOLID, 3, COLOR_PATH);

    // Draw Map
    RECT mapRect = { 0, 0, MAP_SIZE, MAP_SIZE };
    FillRect(hdc, &mapRect, grassBrush);

    // Draw Obstacles
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (grid[y][x]) {
                RECT cellRect = { x * CELL_SIZE, y * CELL_SIZE, (x + 1) * CELL_SIZE, (y + 1) * CELL_SIZE };
                FillRect(hdc, &cellRect, obstacleBrush);
            }
        }
    }

    // Draw Sidebar
    RECT sidebarRect = { MAP_SIZE, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    FillRect(hdc, &sidebarRect, sidebarBrush);

    // Draw Path
    if (!currentPath.empty()) {
        SelectObject(hdc, pathPen);
        MoveToEx(hdc, currentPath[0].x * CELL_SIZE + CELL_SIZE / 2, currentPath[0].y * CELL_SIZE + CELL_SIZE / 2, NULL);
        for (size_t i = 1; i < currentPath.size(); i++) {
            LineTo(hdc, currentPath[i].x * CELL_SIZE + CELL_SIZE / 2, currentPath[i].y * CELL_SIZE + CELL_SIZE / 2);
        }
    }

    // Draw Squish
    HBRUSH squishBrush = CreateSolidBrush(COLOR_SQUISH);
    SelectObject(hdc, squishBrush);
    Ellipse(hdc, realSquishPos.x - SQUISH_RADIUS, realSquishPos.y - SQUISH_RADIUS, realSquishPos.x + SQUISH_RADIUS, realSquishPos.y + SQUISH_RADIUS);

    // Sidebar Text
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, MAP_SIZE + 20, 20, "A* Pathfinder", 13);
    TextOutA(hdc, MAP_SIZE + 20, 50, "Left-Click: Set Target", 23);
    TextOutA(hdc, MAP_SIZE + 20, 80, "Right-Click: Placeholder", 24);

    DeleteObject(grassBrush);
    DeleteObject(obstacleBrush);
    DeleteObject(sidebarBrush);
    DeleteObject(squishBrush);
    DeleteObject(pathPen);
}

// Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        if (x < MAP_SIZE && y < MAP_SIZE) {
            targetPos = { x / CELL_SIZE, y / CELL_SIZE };
            currentPath = findPath(squishPos, targetPos);
            if (!currentPath.empty()) {
                isMoving = true;
                currentPathIndex = 0;
                moveProgress = 0.0f;
            }
        }
        return 0;
    }
    case WM_RBUTTONDOWN:
        MessageBoxA(hwnd, "Right-click functionality coming soon!", "Info", MB_OK);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        // Double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
        SelectObject(memDC, memBitmap);

        DrawGame(memDC);

        BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memDC, 0, 0, SRCCOPY);
        
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Seed and generate random obstacles
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, GRID_SIZE - 1);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (chance(rng) < 0.15f) grid[i][j] = true;
        }
    }
    // Ensure start position is clear
    grid[10][10] = false;

    // Register Window Class
    const char CLASS_NAME[] = "AStarGameClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    // Create Window
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "A* Pathfinding Showcase", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH + 16, WINDOW_HEIGHT + 39, // Adjust for borders
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Game Loop
    MSG msg = {};
    auto lastTime = std::chrono::steady_clock::now();
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            auto currentTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (isMoving && currentPathIndex < currentPath.size() - 1) {
                moveProgress += deltaTime * 5.0f; // Speed constant
                if (moveProgress >= 1.0f) {
                    moveProgress = 0.0f;
                    currentPathIndex++;
                    squishPos = currentPath[currentPathIndex];
                    if (currentPathIndex >= currentPath.size() - 1) {
                        isMoving = false;
                        currentPath.clear();
                    }
                }

                if (isMoving) {
                    Point p1 = currentPath[currentPathIndex];
                    Point p2 = currentPath[currentPathIndex + 1];
                    realSquishPos.x = (int)((p1.x + (p2.x - p1.x) * moveProgress) * CELL_SIZE + CELL_SIZE / 2);
                    realSquishPos.y = (int)((p1.y + (p2.y - p1.y) * moveProgress) * CELL_SIZE + CELL_SIZE / 2);
                }
            } else {
                realSquishPos.x = squishPos.x * CELL_SIZE + CELL_SIZE / 2;
                realSquishPos.y = squishPos.y * CELL_SIZE + CELL_SIZE / 2;
            }

            InvalidateRect(hwnd, NULL, FALSE);
        }
    }

    return 0;
}