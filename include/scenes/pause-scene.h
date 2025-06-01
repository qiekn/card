#pragma once

#include "scene.h"

class PauseScene : public Scene {
public:
  PauseScene() {}
  virtual ~PauseScene() {}

  void Update() override;
  void Draw() override;
};
