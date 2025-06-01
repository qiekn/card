#pragma once

enum class GameState {
  kLoading,
  kPlaying,
  kEnding,
};

class Game {
public:
  Game();
  virtual ~Game();

  void Init();
  void Update();
  void Draw();

private:
  /* data */
};
