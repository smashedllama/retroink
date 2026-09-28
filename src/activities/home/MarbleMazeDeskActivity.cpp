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
#include <cstring>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kWallThickness = 6;
// Acceleration in screen pixels per second squared for a full 1 g of tilt.
constexpr float kPixelsPerG = 2800.0f;
// How strongly the motion sensor's tilt reads as ball acceleration.
constexpr float kTiltGain = 0.45f;
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

// Recalibrating averages this many readings over at least this long, and
// starts over if the reader moves more than this (in g) while it does.
constexpr int kRecalSamples = 20;
constexpr unsigned long kRecalMinMs = 800;
constexpr float kRecalMaxSpread = 0.12f;
// Saved settings. Version 3 replaces the direction-learning file from 0.5.1,
// which had a different size and is ignored.
constexpr char kSettingsPath[] = "/.crosspoint/marble.bin";
constexpr uint8_t kSettingsVersion = 3;
constexpr size_t kSettingsSize = 12;  // version, invert L/R, invert F/B, flat set, two floats
constexpr int kOptionCount = 3;

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
  if (flatSet_) {
    // A saved flat-surface calibration is the level.
    baseX_ = flatX_;
    baseY_ = flatY_;
    calibrated_ = true;
  } else {
    // Otherwise level on the angle the reader is held at now.
    calibrated_ = false;
    calibrationSamples_ = 0;
    baseX_ = baseY_ = 0;
    calibrateAfterMs_ = millis() + kSensorSettleMs;
  }
  fullRedraw_ = true;
  requestUpdate();
}

void MarbleMazeDeskActivity::loadSettings() {
  invertLeftRight_ = false;
  invertFrontBack_ = false;
  flatSet_ = false;
  FsFile f;
  if (!Storage.openFileForRead("MAZE", kSettingsPath, f)) return;
  uint8_t data[kSettingsSize] = {};
  const bool ok = f.fileSize() == kSettingsSize && f.read(data, kSettingsSize) == static_cast<int>(kSettingsSize);
  f.close();
  if (!ok || data[0] != kSettingsVersion) return;
  invertLeftRight_ = data[1] != 0;
  invertFrontBack_ = data[2] != 0;
  float flat[2] = {};
  std::memcpy(flat, data + 4, sizeof(flat));
  // Only trust a saved level that looks like a real reading (within a few g).
  if (data[3] != 0 && std::isfinite(flat[0]) && std::isfinite(flat[1]) && std::fabs(flat[0]) < 4.0f &&
      std::fabs(flat[1]) < 4.0f) {
    flatSet_ = true;
    flatX_ = flat[0];
    flatY_ = flat[1];
  }
}

void MarbleMazeDeskActivity::saveSettings() const {
  uint8_t data[kSettingsSize] = {};
  data[0] = kSettingsVersion;
  data[1] = invertLeftRight_ ? 1 : 0;
  data[2] = invertFrontBack_ ? 1 : 0;
  data[3] = flatSet_ ? 1 : 0;
  const float flat[2] = {flatX_, flatY_};
  std::memcpy(data + 4, flat, sizeof(flat));
  FsFile f;
  if (!Storage.openFileForWrite("MAZE", kSettingsPath, f)) return;
  f.write(data, sizeof(data));
  f.close();
}

// Switches screens. Leaving Options or Recalibrate returns to a fresh redraw.
void MarbleMazeDeskActivity::showMode(const Mode mode) {
  mode_ = mode;
  recalibrating_ = false;
  if (mode == Mode::Options) optionsSelected_ = 0;
  deepRefresh_ = true;
  fullRedraw_ = true;
  requestUpdate();
}

void MarbleMazeDeskActivity::beginRecalibrate() {
  recalibrating_ = true;
  recalSamples_ = 0;
  recalSumX_ = recalSumY_ = 0;
  recalStartMs_ = millis();
  calibrateAfterMs_ = millis() + kSensorSettleMs;
  fullRedraw_ = true;
  requestUpdate();
}

// Averages the sensor while the reader lies still, then saves that as level.
void MarbleMazeDeskActivity::stepRecalibrate() {
  if (!recalibrating_) return;
#ifndef SIMULATOR
  if (!sensorActive_ || millis() < calibrateAfterMs_) return;
  float bx = 0, by = 0, bz = 0;
  if (!halTiltSensor.readAccel(bx, by, bz)) return;
  if (recalSamples_ == 0) {
    recalMinX_ = recalMaxX_ = bx;
    recalMinY_ = recalMaxY_ = by;
  } else {
    recalMinX_ = std::min(recalMinX_, bx);
    recalMaxX_ = std::max(recalMaxX_, bx);
    recalMinY_ = std::min(recalMinY_, by);
    recalMaxY_ = std::max(recalMaxY_, by);
  }
  if (recalMaxX_ - recalMinX_ > kRecalMaxSpread || recalMaxY_ - recalMinY_ > kRecalMaxSpread) {
    // It moved: start over rather than save a level from a shaky reading.
    recalSamples_ = 0;
    recalSumX_ = recalSumY_ = 0;
    recalStartMs_ = millis();
    return;
  }
  recalSumX_ += bx;
  recalSumY_ += by;
  ++recalSamples_;
  if (recalSamples_ < kRecalSamples || millis() - recalStartMs_ < kRecalMinMs) return;
  flatX_ = recalSumX_ / static_cast<float>(recalSamples_);
  flatY_ = recalSumY_ / static_cast<float>(recalSamples_);
  LOG_DBG("MAZE", "Flat level set at x=%.3f y=%.3f", flatX_, flatY_);
#else
  flatX_ = flatY_ = 0;
#endif
  flatSet_ = true;
  saveSettings();
  recalibrating_ = false;
  mode_ = Mode::Play;
  newGame();
}

void MarbleMazeDeskActivity::activateOption(const int index) {
  switch (index) {
    case 0:
      showMode(Mode::Recalibrate);
      break;
    case 1:
      invertLeftRight_ = !invertLeftRight_;
      saveSettings();
      requestUpdate();
      break;
    case 2:
      invertFrontBack_ = !invertFrontBack_;
      saveSettings();
      requestUpdate();
      break;
    default:
      break;
  }
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
  loadSettings();
  mode_ = Mode::Play;
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
  // Tilting right rolls right; tilting the top away rolls the ball up. Either
  // direction can be inverted in Options.
  ax = kTiltGain * (invertLeftRight_ ? -1.0f : 1.0f) * delta[xAxis_];
  ay = -kTiltGain * (invertFrontBack_ ? -1.0f : 1.0f) * delta[yAxis_];
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
    if (mode_ != Mode::Play) {
      // Back from Options or Recalibrate returns to the maze, not out of it.
      showMode(Mode::Play);
      lastStepMs_ = millis();
      return;
    }
    finish();
    return;
  }

  const Mode mode = mode_;
  if (mode == Mode::Options) {
    buttonNavigator_.onNextPress([this] {
      optionsSelected_ = ButtonNavigator::nextIndex(optionsSelected_, kOptionCount);
      requestUpdate();
    });
    buttonNavigator_.onPreviousPress([this] {
      optionsSelected_ = ButtonNavigator::previousIndex(optionsSelected_, kOptionCount);
      requestUpdate();
    });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activateOption(optionsSelected_);
    return;
  }
  if (mode == Mode::Recalibrate) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && !recalibrating_) {
      beginRecalibrate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Right) && !recalibrating_) {
      // Auto: level at the start of each maze instead of on a flat surface.
      flatSet_ = false;
      saveSettings();
      showMode(Mode::Play);
      newGame();
    }
    stepRecalibrate();
    return;
  }

  // Playing. Select starts a new maze; the far-right button opens Options.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    newGame();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    showMode(Mode::Options);
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

void MarbleMazeDeskActivity::drawOptions() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen();
  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_MARBLE_OPTIONS), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_MARBLE_OPTIONS), nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int rowX = metrics.contentSidePadding + 14;
  const int rowW = renderer.getScreenWidth() - 2 * metrics.contentSidePadding - 28;
  constexpr int kRowStep = 58;
  constexpr int kRowHeight = 50;
  const char* labels[kOptionCount] = {tr(STR_MARBLE_RECALIBRATE), tr(STR_MARBLE_INVERT_LR), tr(STR_MARBLE_INVERT_FB)};
  const char* values[kOptionCount] = {flatSet_ ? tr(STR_MARBLE_FLAT) : tr(STR_MARBLE_AUTO),
                                      invertLeftRight_ ? tr(STR_STATE_ON) : tr(STR_STATE_OFF),
                                      invertFrontBack_ ? tr(STR_STATE_ON) : tr(STR_STATE_OFF)};
  const int selected = optionsSelected_;
  for (int i = 0; i < kOptionCount; ++i) {
    const int y = top + 10 + i * kRowStep;
    const bool active = i == selected;
    if (active) {
      renderer.fillRect(rowX, y, rowW, kRowHeight, true);
    } else {
      renderer.drawRect(rowX, y, rowW, kRowHeight);
    }
    renderer.drawText(UI_10_FONT_ID, rowX + 14, y + (kRowHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2, labels[i],
                      !active);
    const int valueW = renderer.getTextWidth(UI_12_FONT_ID, values[i], EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, rowX + rowW - 14 - valueW,
                      y + (kRowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, values[i], !active,
                      EpdFontFamily::BOLD);
  }

  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  fullRedraw_ = false;
  renderer.displayBuffer(deepRefresh_.exchange(false) ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
}

void MarbleMazeDeskActivity::drawRecalibrate() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen();
  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_MARBLE_RECALIBRATE), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_MARBLE_RECALIBRATE), nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int textW = renderer.getScreenWidth() - 2 * metrics.contentSidePadding - 60;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  int y = top + 60;
  if (recalibrating_) {
    renderer.drawCenteredText(UI_12_FONT_ID, y + 40, tr(STR_MARBLE_CALIBRATING), true, EpdFontFamily::BOLD);
  } else {
    for (const StrId id : {StrId::STR_MARBLE_RECAL_HELP, StrId::STR_MARBLE_RECAL_AUTO_HELP}) {
      for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, I18n::getInstance().get(id), textW, 5)) {
        renderer.drawCenteredText(UI_10_FONT_ID, y, line.c_str());
        y += lineHeight;
      }
      y += lineHeight / 2;
    }
    y += lineHeight;
    renderer.drawCenteredText(UI_10_FONT_ID, y,
                              flatSet_ ? tr(STR_MARBLE_LEVEL_FLAT) : tr(STR_MARBLE_LEVEL_AUTO), true,
                              EpdFontFamily::BOLD);
  }

  const auto hints = recalibrating_ ? mappedInput.mapLabels(tr(STR_BACK), "", "", "")
                                    : mappedInput.mapLabels(tr(STR_BACK), tr(STR_MARBLE_CALIBRATE), "", tr(STR_MARBLE_AUTO));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  fullRedraw_ = false;
  renderer.displayBuffer(deepRefresh_.exchange(false) ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
}

void MarbleMazeDeskActivity::render(RenderLock&&) {
  const Mode mode = mode_;
  if (mode == Mode::Options) {
    drawOptions();
    return;
  }
  if (mode == Mode::Recalibrate) {
    drawRecalibrate();
    return;
  }
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
    renderer.drawCenteredText(UI_10_FONT_ID, captionY, tr(STR_MARBLE_HINT_TILT));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MARBLE_NEW), "", tr(STR_MARBLE_OPTIONS));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  fullRedraw_ = false;
  renderPending_ = false;
  // A deeper refresh that also wipes accumulated ghost trails. It flashes, but
  // only ever at a pause in play.
  deepRefresh_ = false;
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
