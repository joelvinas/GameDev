#include "Navigation.h"
#include "MapGeneration.h"
#include "Constants.h"
#include <queue>
#include <cmath>
#include <algorithm>

// Node structure for A* algorithm
struct Node {
    Point pos;
    float gCost, hCost;
    Point parent;

    float fCost() const { return gCost + hCost; }
    bool operator>(const Node& other) const { return fCost() > other.fCost(); }
};

// ... Rest of the implementations ...
// Bresenham's line algorithm for line of sight
bool lineOfSight(Point p0, Point p1, int agentId) {
    int x0 = p0.x, y0 = p0.y;
    int x1 = p1.x, y1 = p1.y;
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int x = x0, y = y0;
    int n = 1 + dx + dy;
    int x_inc = (x1 > x0) ? 1 : -1;
    int y_inc = (y1 > y0) ? 1 : -1;
    int error = dx - dy;
    dx *= 2;
    dy *= 2;

    for (; n > 0; --n) {
        if (!isPassable(x, y, agentId)) return false;
        if (getMoveCost(x, y) > 1.0f) return false;

        if (error > 0) {
            x += x_inc;
            error -= dy;
        } else if (error < 0) {
            y += y_inc;
            error += dx;
        } else {
            x += x_inc;
            error -= dy;
            y += y_inc;
            error += dx;
            --n; 
        }
    }
    return true;
}

// Catmull-Rom spline interpolation
float getSplineVal(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

// String pulling algorithm to smooth paths
std::vector<Point> stringPull(const std::vector<Point>& path, int agentId) {
    if (path.size() <= 2) return path;
    std::vector<Point> smoothPath;
    smoothPath.push_back(path.front());
    size_t currentIndex = 0;
    while (currentIndex + 1 < path.size()) {
        size_t farthestVisible = currentIndex + 1;
        for (size_t i = currentIndex + 2; i < path.size(); i++) {
            if (lineOfSight(path[currentIndex], path[i], agentId)) {
                farthestVisible = i;
            }
        }
        smoothPath.push_back(path[farthestVisible]);
        currentIndex = farthestVisible;
    }
    return smoothPath;
}

// Get movement cost for a cell
float getMoveCost(int x, int y) {
    float baseCost = 1.0f;
    CellType t = grid[y][x].type;
    if (t == SWAMP) baseCost *= 2.0f;
    if (t == MOUNTAIN) {
        float alt = grid[y][x].altitude;
        baseCost *= (1.0f + (alt - 0.7f) * 10.0f); // increasingly expensive
    }
    return baseCost;
}

// Get Euclidean distance between two points
float getDistance(Point p1, Point p2) {
    return std::sqrt(std::pow((float)p1.x - p2.x, 2) + std::pow((float)p1.y - p2.y, 2));
}

// Check if a cell is passable
bool isPassable(int x, int y, int agentId) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return false;
    CellType t = grid[y][x].type;
    if (t == OBSTACLE) {
        if (agentId != -1) {
            auto it = agentHouses.find(agentId);
            if (it != agentHouses.end() && it->second.x == x && it->second.y == y) {
                return true;
            }
        }
        return false;
    }
    if (t == WATER || t == TREE) return false;
    if (t == MOUNTAIN && grid[y][x].altitude > 0.85f) return false;
    return true;
}


// A* Implementation
std::vector<Point> findPath(Point start, Point end, int agentId) {
    if (!isPassable(end.x, end.y, agentId)) return {};

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
    std::vector<std::vector<float>> gCosts(GRID_SIZE, std::vector<float>(GRID_SIZE, INFINITY));
    std::vector<std::vector<Point>> parents(GRID_SIZE, std::vector<Point>(GRID_SIZE, { -1, -1 }));

    gCosts[start.y][start.x] = 0;
    openSet.push({ start, 0, getDistance(start, end), { -1, -1 }});

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
                if (isPassable(neighbor.x, neighbor.y, agentId)) {
                    float moveCost = (dx == 0 || dy == 0) ? 1.0f : 1.414f;
                    moveCost *= getMoveCost(neighbor.x, neighbor.y);
                    float newGCost = gCosts[current.pos.y][current.pos.x] + moveCost;

                    if (newGCost < gCosts[neighbor.y][neighbor.x]) {
                        gCosts[neighbor.y][neighbor.x] = newGCost;
                        parents[neighbor.y][neighbor.x] = current.pos;
                        openSet.push({ neighbor, newGCost, getDistance(neighbor, end), current.pos});
                    }
                }
            }
        }
    }

    return {};
}
