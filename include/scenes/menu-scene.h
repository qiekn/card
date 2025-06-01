
#pragma once

#include "scene.h"

class MenuScene : public Scene {
public:
  MenuScene() {}
  virtual ~MenuScene() {}

  void Update() override;
  void Draw() override;
};
