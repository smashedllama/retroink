#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// A tabletop tilt maze: roll a ball through a random maze into the hole by
// tilting the reader. Uses the motion sensor on hardware that has one (the X3);
// the front buttons tilt left/right and the side buttons tilt up/down, which is
// the only control on hardware without a sensor (the X4) and in the simulator.
//
// E-ink cannot animate like an LCD, so each frame repaints only the small
// window around the ball's old and new positions as a partial refresh, with a
// light cleanup refresh now and then to keep ghosting down.
class MarbleMazeDeskActivity final : public Activity {
  static constexpr int kCols = 6;
  static constexpr int kRows = 9;

  // Walls: hWall_[r][c] is the wall along the top of cell (r, c) (r == kRows
  // is the bottom edge); vWall_[r][c] is the wall along the left of cell
  // (r, c) (c == kCols is the right edge).
  bool hWall_[kRows + 1][kCols] = {};
  bool vWall_[kRows][kCols + 1] = {};
  std::vector<Rect> wallRects_;

  int cell_ = 0, originX_ = 0, originY_ = 0;
  int ballRadius_ = 0, holeRadius_ = 0;
  int holeX_ = 0, holeY_ = 0;

  // Physics state, owned by loop().
  float ballX_ = 0, ballY_ = 0, velX_ = 0, velY_ = 0;
  unsigned long lastStepMs_ = 0;
  unsigned long startedMs_ = 0;
  uint32_t solvedSeconds_ = 0;

  // Tilt calibration: whatever angle the reader is held at when the game
  // starts counts as level. Which sensor axis means "right" and which means
  // "away from you" is learned once by asking the player to tilt each way,
  // then saved to the SD card.
  enum class Setup : uint8_t { Right, Away, Done };
  std::atomic<Setup> setup_{Setup::Done};
  // X3 defaults, confirmed on hardware: tilting right raises the sensor's Y
  // reading and tilting the top away raises its X reading.
  uint8_t xAxis_ = 1, yAxis_ = 0;
  int8_t xSign_ = 1, ySign_ = 1;
  int setupHoldCount_ = 0;
  bool confirmLongPressed_ = false;
  bool sensorActive_ = false;
  bool calibrated_ = false;
  int calibrationSamples_ = 0;
  float baseX_ = 0, baseY_ = 0;
  unsigned long calibrateAfterMs_ = 0;

  // Handoff to the render task.
  std::atomic<int> targetX_{0}, targetY_{0};
  std::atomic<bool> fullRedraw_{true};
  std::atomic<bool> renderPending_{false};
  std::atomic<bool> solved_{false};
  // Where the ball was last drawn: written by the render task, read by loop().
  std::atomic<int> drawnX_{-1000}, drawnY_{-1000};

  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  void layout();
  void generateMaze();
  void buildWallRects();
  void resetBall();
  void newGame();
  bool collides(float x, float y) const;
  void step(float ax, float ay, float dt);
  bool readTilt(float& ax, float& ay);
  bool loadDirections();
  void saveDirections() const;
  void startDirectionSetup();
  void drawBoard() const;
  void drawBall(int x, int y, bool ink) const;
  void drawHole() const;

 public:
  MarbleMazeDeskActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("MarbleMazeDesk", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return !solved_.load(); }
};
