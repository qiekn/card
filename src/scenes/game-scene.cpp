#include "game-scene.h"
#include <raylib.h>
#include "constants.h"

void GameScene::Update() {}

void GameScene::Draw() {
  ClearBackground(Color{255, 0, 0, 255});
  DrawRectangle(kScreenWidth / 2, kScreenHeight / 2, kCardWidth, kCardHeight, RED);
}
