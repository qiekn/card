#pragma once

#include "scene.h"

class LogoScene : public Scene {
public:
  LogoScene() {}
  virtual ~LogoScene() {}

  void Update() override;
  void Draw() override;
};
