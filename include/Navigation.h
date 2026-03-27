//Fully encapsulates the A*, 
// line-of-sight, 
// and Catmull-Rom spline curves.

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include "Constants.h"

bool lineOfSight(Point p0, Point p1);
float getSplineVal(float p0, float p1, float p2, float p3, float t);
std::vector<Point> findPath(Point start, Point end);
std::vector<Point> stringPull(const std::vector<Point>& path);
float getMoveCost(int x, int y);
float getDistance(Point a, Point b);
bool isPassable(int x, int y);

#endif