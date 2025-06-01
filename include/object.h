#pragma once

// every thing you see in this game is an object
class Object {
public:
  Object() : x(0), y(0), w(100), h(100), s(1.0f), r(0.0f) {}
  virtual ~Object() {}

public:
  int x, y;  // start point
  int w, h;  // width & height
  float s;   // scale
  float r;   // rotation angle
};
