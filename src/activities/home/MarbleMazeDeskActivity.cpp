#include "MarbleMazeDeskActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#ifndef SIMULATOR
#include <HalDisplay.h>
#include <HalTiltSensor.h>
#endif
#include <HalStorage.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kWallThickness = 6;
// Acceleration in screen pixels per second squared for a full 1 g of tilt.
constexpr float kPixelsPerG = 2800.0f;
// The motion sensor reads much more tilt than a held button adds, so it gets
// its own, gentler gain.
constexpr float kTiltGain = 0.45f;
// How much tilt a held button adds, in g.
constexpr float kButtonTilt = 0.5f;
// Rolling friction: fraction of speed lost per second.
constexpr float kFriction = 0.8f;
constexpr float kMaxSpeed = 850.0f;
// Tilt below this (in g) is treated as level, so a resting hand doesn't drift.
constexpr float kDeadZone = 0.05f;
// Largest step the ball takes before collision is rechecked, so it can never
// tunnel through a wall however fast it rolls.
constexpr float kMaxSubstep = 1.5f;
// Only redraw once the ball has moved this far; smaller moves would spend a
// panel refresh on a change you can't see.
constexpr int kRedrawDistance = 4;
constexpr int kCalibrationSamples = 8;
constexpr unsigned long kSensorSettleMs = 350;

// Direction setup: a tilt past this (in g) held for this many samples counts
// as the player's "right" or "away" tilt.
constexpr float kSetupTilt = 0.3f;
constexpr int kSetupHoldSamples = 6;
constexpr unsigned long kLongPressMs = 900;
constexpr char kDirectionsPath[] = "/.crosspoint/marble.bin";
constexpr uint8_t kDirectionsVersion = 1;

bool circleOverlapsRect(const float cx, const float cy, const float r, const Rect& rect) {
  const float nearestX = std::clamp(cx, static_cast<float>(rect.x), static_cast<float>(rect.x + rect.width));
  const float nearestY = std::clamp(cy, static_cast<float>(rect.y), static_cast<float>(rect.y + rect.height));
  const float dx = cx - nearestX;
  const float dy = cy - nearestY;
  return dx * dx + dy * dy < r * r;
}

void fillDisc(const GfxRenderer& renderer, const int cx, const int cy, const int r, const bool ink) {
  for (int dy = -r; dy <= r; ++dy) {
    const int half = static_cast<int>(std::sqrt(static_cast<float>(r * r - dy * dy)));
    renderer.fillRect(cx - half, cy + dy, 2 * half + 1, 1, ink);
  }
}
}  // namespace

void MarbleMazeDeskActivity::layout() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing -
                     renderer.getLineHeight(UI_10_FONT_ID) - 12;
  const int availableW = pageWidth - 2 * metrics.contentSidePadding - 24;
  const int availableH = bottom - top - 12;
  cell_ = std::max(24, std::min(availableW / kCols, availableH / kRows));
  originX_ = (pageWidth - cell_ * kCols) / 2;
  originY_ = top + 6 + (availableH - cell_ * kRows) / 2;
  ballRadius_ = std::max(6, cell_ * 22 / 100);
  holeRadius_ = ballRadius_ + std::max(3, cell_ / 14);
}

void MarbleMazeDeskActivity::generateMaze() {
  for (auto& row : hWall_) std::fill(std::begin(row), std::end(row), true);
  for (auto& row : vWall_) std::fill(std::begin(row), std::end(row), true);

  // Iterative depth-first "recursive backtracker": long winding corridors,
  // exactly one path between any two cells.
  bool visited[kRows][kCols] = {};
  std::vector<int> stack;
  stack.reserve(kRows * kCols);
  stack.push_back(0);
  visited[0][0] = true;
  while (!stack.empty()) {
    const int cellIndex = stack.back();
    const int r = cellIndex / kCols;
    const int c = cellIndex % kCols;
    int options[4];
    int count = 0;
    if (r > 0 && !visited[r - 1][c]) options[count++] = 0;
    if (c < kCols - 1 && !visited[r][c + 1]) options[count++] = 1;
    if (r < kRows - 1 && !visited[r + 1][c]) options[count++] = 2;
    if (c > 0 && !visited[r][c - 1]) options[count++] = 3;
    if (count == 0) {
      stack.pop_back();
      continue;
    }
    const int dir = options[random(count)];
    int nr = r, nc = c;
    switch (dir) {
      case 0: hWall_[r][c] = false; nr = r - 1; break;
      case 1: vWall_[r][c + 1] = false; nc = c + 1; break;
      case 2: hWall_[r + 1][c] = false; nr = r + 1; break;
      default: vWall_[r][c] = false; nc = c - 1; break;
    }
    visited[nr][nc] = true;
    stack.push_back(nr * kCols + nc);
  }

  // Put the hole in the cell farthest (by path) from the start.
  int distance[kRows * kCols];
  std::fill(std::begin(distance), std::end(distance), -1);
  std::vector<int> queue;
  queue.reserve(kRows * kCols);
  queue.push_back(0);
  distance[0] = 0;
  int farthest = 0;
  for (size_t head = 0; head < queue.size(); ++head) {
    const int cellIndex = queue[head];
    const int r = cellIndex / kCols;
    const int c = cellIndex % kCols;
    if (distance[cellIndex] > distance[farthest]) farthest = cellIndex;
    const int neighbours[4][3] = {{r - 1, c, !hWall_[r][c]},
                                  {r, c + 1, !vWall_[r][c + 1]},
                                  {r + 1, c, !hWall_[r + 1][c]},
                                  {r, c - 1, !vWall_[r][c]}};
    for (const auto& n : neighbours) {
      if (!n[2] || n[0] < 0 || n[0] >= kRows || n[1] < 0 || n[1] >= kCols) continue;
      const int next = n[0] * kCols + n[1];
      if (distance[next] >= 0) continue;
      distance[next] = distance[cellIndex] + 1;
      queue.push_back(next);
    }
  }
  holeX_ = originX_ + (farthest % kCols) * cell_ + cell_ / 2;
  holeY_ = originY_ + (farthest / kCols) * cell_ + cell_ / 2;
}

void MarbleMazeDeskActivity::buildWallRects() {
  wallRects_.clear();
  const int half = kWallThickness / 2;
  for (int r = 0; r <= kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      if (!hWall_[r][c]) continue;
      wallRects_.push_back(Rect{originX_ + c * cell_ - half, originY_ + r * cell_ - half, cell_ + kWallThickness,
                                kWallThickness});
    }
  }
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c <= kCols; ++c) {
      if (!vWall_[r][c]) continue;
      wallRects_.push_back(Rect{originX_ + c * cell_ - half, originY_ + r * cell_ - half, kWallThickness,
                                cell_ + kWallThickness});
    }
  }
}

void MarbleMazeDeskActivity::resetBall() {
  ballX_ = static_cast<float>(originX_ + cell_ / 2);
  ballY_ = static_cast<float>(originY_ + cell_ / 2);
  velX_ = velY_ = 0;
  targetX_ = static_cast<int>(ballX_);
  targetY_ = static_cast<int>(ballY_);
}

void MarbleMazeDeskActivity::newGame() {
  generateMaze();
  buildWallRects();
  resetBall();
  solved_ = false;
  solvedSeconds_ = 0;
  startedMs_ = millis();
  lastStepMs_ = millis();
  // Re-level on the angle the reader is held at now.
  calibrated_ = false;
  calibrationSamples_ = 0;
  baseX_ = baseY_ = 0;
  calibrateAfterMs_ = millis() + kSensorSettleMs;
  fullRedraw_ = true;
  requestUpdate();
}

bool MarbleMazeDeskActivity::loadDirections() {
  FsFile f;
  if (!Storage.openFileForRead("MAZE", kDirectionsPath, f)) return false;
  uint8_t data[5] = {};
  const bool ok = f.fileSize() == sizeof(data) && f.read(data, sizeof(data)) == static_cast<int>(sizeof(data));
  f.close();
  if (!ok || data[0] != kDirectionsVersion || data[1] > 1 || data[3] > 1 || data[1] == data[3]) return false;
  xAxis_ = data[1];
  xSign_ = data[2] ? 1 : -1;
  yAxis_ = data[3];
  ySign_ = data[4] ? 1 : -1;
  return true;
}

void MarbleMazeDeskActivity::saveDirections() const {
  const uint8_t data[5] = {kDirectionsVersion, xAxis_, static_cast<uint8_t>(xSign_ > 0), yAxis_,
                           static_cast<uint8_t>(ySign_ > 0)};
  FsFile f;
  if (!Storage.openFileForWrite("MAZE", kDirectionsPath, f)) return;
  f.write(data, sizeof(data));
  f.close();
}

void MarbleMazeDeskActivity::startDirectionSetup() {
  setup_ = Setup::Right;
  setupHoldCount_ = 0;
  newGame();
}

void MarbleMazeDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
#ifndef SIMULATOR
  // Half-length panel waveform while playing: about twice as many ball
  // updates per second on the X3. Restored in onExit().
  display.setAnimationWaveform(true);
  if (halTiltSensor.isAvailable()) {
    halTiltSensor.setHoldAwake(true);
    sensorActive_ = true;
  }
#endif
  layout();
  // A saved setup (from holding New) overrides the X3 defaults.
  loadDirections();
  newGame();
}

void MarbleMazeDeskActivity::onExit() {
#ifndef SIMULATOR
  if (sensorActive_) halTiltSensor.setHoldAwake(false);
  display.setAnimationWaveform(false);
#endif
  sensorActive_ = false;
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

bool MarbleMazeDeskActivity::readTilt(float& ax, float& ay) {
  ax = ay = 0;
#ifndef SIMULATOR
  if (!sensorActive_) return false;
  float bx = 0, by = 0, bz = 0;
  if (!halTiltSensor.readAccel(bx, by, bz)) return false;
  if (!calibrated_) {
    if (millis() < calibrateAfterMs_) return false;
    baseX_ += bx;
    baseY_ += by;
    if (++calibrationSamples_ >= kCalibrationSamples) {
      baseX_ /= calibrationSamples_;
      baseY_ /= calibrationSamples_;
      calibrated_ = true;
      LOG_DBG("MAZE", "Level set at ax=%.2f ay=%.2f", baseX_, baseY_);
    }
    return false;
  }
  const float delta[2] = {bx - baseX_, by - baseY_};
  if (setup_ != Setup::Done) {
    // Learn the axis and direction the player uses for "right", then the
    // other axis for "away from you".
    const bool learningRight = setup_ == Setup::Right;
    int axis = std::fabs(delta[0]) >= std::fabs(delta[1]) ? 0 : 1;
    if (!learningRight) axis = 1 - xAxis_;
    if (std::fabs(delta[axis]) < kSetupTilt) {
      setupHoldCount_ = 0;
      return false;
    }
    if (++setupHoldCount_ < kSetupHoldSamples) return false;
    setupHoldCount_ = 0;
    if (learningRight) {
      xAxis_ = static_cast<uint8_t>(axis);
      xSign_ = delta[axis] > 0 ? 1 : -1;
      setup_ = Setup::Away;
    } else {
      yAxis_ = static_cast<uint8_t>(axis);
      ySign_ = delta[axis] > 0 ? 1 : -1;
      setup_ = Setup::Done;
      saveDirections();
      // Re-level: the player is still tilted from the setup.
      calibrated_ = false;
      calibrationSamples_ = 0;
      baseX_ = baseY_ = 0;
      calibrateAfterMs_ = millis() + 800;
    }
    LOG_DBG("MAZE", "Direction setup: x=axis%u%+d y=axis%u%+d", xAxis_, xSign_, yAxis_, ySign_);
    fullRedraw_ = true;
    requestUpdate();
    return false;
  }
  // Tilting right rolls right; tilting the top away rolls the ball up.
  ax = kTiltGain * xSign_ * delta[xAxis_];
  ay = -kTiltGain * ySign_ * delta[yAxis_];
  if (std::fabs(ax) < kDeadZone * kTiltGain) ax = 0;
  if (std::fabs(ay) < kDeadZone * kTiltGain) ay = 0;
  return true;
#else
  return false;
#endif
}

bool MarbleMazeDeskActivity::collides(const float x, const float y) const {
  const float r = static_cast<float>(ballRadius_);
  for (const auto& wall : wallRects_) {
    if (circleOverlapsRect(x, y, r, wall)) return true;
  }
  return false;
}

void MarbleMazeDeskActivity::step(const float ax, const float ay, const float dt) {
  velX_ += ax * kPixelsPerG * dt;
  velY_ += ay * kPixelsPerG * dt;
  const float damping = std::max(0.0f, 1.0f - kFriction * dt);
  velX_ *= damping;
  velY_ *= damping;
  velX_ = std::clamp(velX_, -kMaxSpeed, kMaxSpeed);
  velY_ = std::clamp(velY_, -kMaxSpeed, kMaxSpeed);

  float moveX = velX_ * dt;
  float moveY = velY_ * dt;
  const int substeps =
      std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(moveX), std::fabs(moveY)) / kMaxSubstep)));
  const float sx = moveX / substeps;
  const float sy = moveY / substeps;
  for (int i = 0; i < substeps; ++i) {
    // Move each axis separately so the ball slides along a wall rather than
    // sticking to it, and bounces a little off whatever it hits.
    if (sx != 0 && !collides(ballX_ + sx, ballY_)) {
      ballX_ += sx;
    } else if (sx != 0) {
      velX_ = -velX_ * 0.25f;
    }
    if (sy != 0 && !collides(ballX_, ballY_ + sy)) {
      ballY_ += sy;
    } else if (sy != 0) {
      velY_ = -velY_ * 0.25f;
    }
  }

  const float dx = ballX_ - holeX_;
  const float dy = ballY_ - holeY_;
  const float captureRadius = static_cast<float>(holeRadius_ - ballRadius_ / 2);
  if (dx * dx + dy * dy <= captureRadius * captureRadius) {
    ballX_ = static_cast<float>(holeX_);
    ballY_ = static_cast<float>(holeY_);
    velX_ = velY_ = 0;
    solvedSeconds_ = (millis() - startedMs_) / 1000;
    solved_ = true;
    fullRedraw_ = true;
  }
}

void MarbleMazeDeskActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  // Hold Select to redo the tilt direction setup; a quick press is a new maze.
  if (sensorActive_ && !confirmLongPressed_ && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= kLongPressMs) {
    confirmLongPressed_ = true;
    startDirectionSetup();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (confirmLongPressed_) {
      confirmLongPressed_ = false;
    } else {
      newGame();
    }
    return;
  }
  if (solved_) return;

  const unsigned long now = millis();
  // Capped so a stall doesn't launch the ball, but high enough that a slow
  // loop tick still moves it at full speed.
  const float dt = std::min(0.1f, static_cast<float>(now - lastStepMs_) / 1000.0f);
  lastStepMs_ = now;
  if (dt <= 0) return;

  float ax = 0, ay = 0;
  readTilt(ax, ay);
  // Buttons tilt the board too: the only control without a motion sensor,
  // and a handy nudge with one.
  if (mappedInput.isPressed(MappedInputManager::Button::Left)) ax -= kButtonTilt;
  if (mappedInput.isPressed(MappedInputManager::Button::Right)) ax += kButtonTilt;
  if (mappedInput.isPressed(MappedInputManager::Button::Up)) ay -= kButtonTilt;
  if (mappedInput.isPressed(MappedInputManager::Button::Down)) ay += kButtonTilt;

  step(ax, ay, dt);

  const int x = static_cast<int>(std::lround(ballX_));
  const int y = static_cast<int>(std::lround(ballY_));
  targetX_ = x;
  targetY_ = y;
  if (solved_) {
    requestUpdate();
    return;
  }
  const int movedX = std::abs(x - drawnX_);
  const int movedY = std::abs(y - drawnY_);
  if (!renderPending_ && (movedX >= kRedrawDistance || movedY >= kRedrawDistance)) {
    renderPending_ = true;
    requestUpdate();
  }
}

void MarbleMazeDeskActivity::drawHole() const {
  // A dark ring with a black centre: reads as a hole at a glance, and stays
  // visible around the ball once it drops in.
  fillDisc(renderer, holeX_, holeY_, holeRadius_, true);
  fillDisc(renderer, holeX_, holeY_, holeRadius_ - 3, false);
  fillDisc(renderer, holeX_, holeY_, holeRadius_ - 6, true);
}

void MarbleMazeDeskActivity::drawBall(const int x, const int y, const bool ink) const {
  if (!ink) {
    fillDisc(renderer, x, y, ballRadius_, false);
    return;
  }
  // Black ball with a small highlight so it reads as round, not as a dot.
  fillDisc(renderer, x, y, ballRadius_, true);
  fillDisc(renderer, x - ballRadius_ / 3, y - ballRadius_ / 3, std::max(2, ballRadius_ / 4), false);
}

void MarbleMazeDeskActivity::drawBoard() const {
  for (const auto& wall : wallRects_) renderer.fillRect(wall.x, wall.y, wall.width, wall.height, true);
  drawHole();
}

void MarbleMazeDeskActivity::render(RenderLock&&) {
  const int x = targetX_;
  const int y = targetY_;

  if (!fullRedraw_ && !solved_ && drawnX_ > -1000) {
    // Partial frame: erase the old ball, restore the hole it may have been
    // crossing, draw the new ball, and push only that patch to the panel.
    drawBall(drawnX_, drawnY_, false);
    drawHole();
    drawBall(x, y, true);
    const int pad = ballRadius_ + 2;
    const int oldX = drawnX_;
    const int oldY = drawnY_;
    const int left = std::min(oldX, x) - pad;
    const int right = std::max(oldX, x) + pad;
    const int top = std::min(oldY, y) - pad;
    const int bottom = std::max(oldY, y) + pad;
    // The controller only accepts windows on 8-pixel column boundaries.
    const int alignedLeft = std::max(0, left & ~7);
    const int alignedRight = std::min(renderer.getScreenWidth(), (right + 8) & ~7);
    drawnX_ = x;
    drawnY_ = y;
    // Never a full-panel flash while the ball is moving: a mid-game refresh
    // blanks the screen for over a second. Ghost trails are cleared by the
    // full refresh below instead, at a natural pause (a new maze, finishing,
    // or opening the screen).
    renderer.displayWindow(alignedLeft, std::max(0, top), alignedRight - alignedLeft,
                           std::min(renderer.getScreenHeight(), bottom) - std::max(0, top));
    renderPending_ = false;
    return;
  }

  renderer.clearScreen();
  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_MARBLE_MAZE), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_MARBLE_MAZE), nullptr, false);
  }
  drawBoard();
  drawBall(x, y, true);
  drawnX_ = x;
  drawnY_ = y;

  const int captionY = originY_ + cell_ * kRows + 12;
  if (solved_) {
    char solvedText[48];
    std::snprintf(solvedText, sizeof(solvedText), tr(STR_MARBLE_SOLVED), static_cast<unsigned>(solvedSeconds_));
    renderer.drawCenteredText(UI_12_FONT_ID, captionY, solvedText, true, EpdFontFamily::BOLD);
  } else {
    const Setup setup = setup_;
    const char* caption = !sensorActive_          ? tr(STR_MARBLE_HINT_BUTTONS)
                          : setup == Setup::Right ? tr(STR_MARBLE_SETUP_RIGHT)
                          : setup == Setup::Away  ? tr(STR_MARBLE_SETUP_AWAY)
                                                  : tr(STR_MARBLE_HINT_TILT);
    renderer.drawCenteredText(UI_10_FONT_ID, captionY, caption, true,
                              setup == Setup::Done ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MARBLE_NEW), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  fullRedraw_ = false;
  renderPending_ = false;
  // A deeper refresh that also wipes accumulated ghost trails. It flashes, but
  // only ever at a pause in play.
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
