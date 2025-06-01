#pragma once

#include "scene.h"

class GameScene : public Scene {
public:
  GameScene() {}
  virtual ~GameScene() {}

  void Update() override;
  void Draw() override;
};
