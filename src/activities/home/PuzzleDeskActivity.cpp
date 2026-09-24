#include "PuzzleDeskActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kPuzzleStatePath[] = "/.crosspoint/puzzle.bin";
constexpr uint8_t kPuzzleStateVersion = 1;
// version byte + 16 tiles + 4-byte move count.
constexpr size_t kPuzzleStateSize = 21;
}  // namespace

void PuzzleDeskActivity::shuffle() {
  for (int i = 0; i < 16; ++i) tiles_[i] = static_cast<uint8_t>((i + 1) % 16);
  for (int i = 15; i > 0; --i) {
    const int j = static_cast<int>(random(i + 1));
    std::swap(tiles_[i], tiles_[j]);
  }
  for (int i = 0; i < 16; ++i)
    if (tiles_[i] == 0) blankPos_ = i;

  if (!isSolvable()) {
    int a = -1, b = -1;
    for (int i = 0; i < 16 && b < 0; ++i) {
      if (tiles_[i] != 0) {
        if (a < 0)
          a = i;
        else
          b = i;
      }
    }
    std::swap(tiles_[a], tiles_[b]);
  }
  moves_ = 0;
  solved_ = false;
}

bool PuzzleDeskActivity::isSolvable() const {
  int inversions = 0;
  for (int i = 0; i < 16; ++i) {
    for (int j = i + 1; j < 16; ++j) {
      if (tiles_[i] && tiles_[j] && tiles_[i] > tiles_[j]) ++inversions;
    }
  }
  const int blankRowFromTop = blankPos_ / 4;
  const int blankRowFromBottom = 4 - blankRowFromTop;
  return (inversions + blankRowFromBottom) % 2 == 0;
}

void PuzzleDeskActivity::checkSolved() {
  for (int i = 0; i < 15; ++i) {
    if (tiles_[i] != i + 1) {
      solved_ = false;
      return;
    }
  }
  solved_ = tiles_[15] == 0;
}

void PuzzleDeskActivity::moveBlank(const int dRow, const int dCol) {
  if (solved_) return;
  const int blankRow = blankPos_ / 4;
  const int blankCol = blankPos_ % 4;
  const int tileRow = blankRow + dRow;
  const int tileCol = blankCol + dCol;
  if (tileRow < 0 || tileRow > 3 || tileCol < 0 || tileCol > 3) return;
  const int tilePos = tileRow * 4 + tileCol;
  std::swap(tiles_[blankPos_], tiles_[tilePos]);
  blankPos_ = tilePos;
  ++moves_;
  checkSolved();
  // Saved per move rather than on exit: the device can deep-sleep out from
  // under an open puzzle, and onExit() doesn't run on that path.
  saveState();
  requestUpdate();
}

void PuzzleDeskActivity::gridGeometry(int& cellSize, int& gridLeft, int& gridTop) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  // Leaves room below the grid, inside the frame, for the solved/hint caption.
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - 46;
  const int available = std::min(pageWidth - 2 * metrics.contentSidePadding - 24, bottom - top - 16);
  cellSize = std::max(20, available / 4);
  gridLeft = (pageWidth - cellSize * 4) / 2;
  gridTop = top + 16;
}

bool PuzzleDeskActivity::loadState() {
  FsFile f;
  if (!Storage.openFileForRead("PZL", kPuzzleStatePath, f)) return false;
  uint8_t data[kPuzzleStateSize] = {};
  const bool sizeOk = f.fileSize() == kPuzzleStateSize;
  const int n = sizeOk ? f.read(data, kPuzzleStateSize) : 0;
  f.close();
  if (!sizeOk || n != static_cast<int>(kPuzzleStateSize) || data[0] != kPuzzleStateVersion) return false;

  // Every value 0-15 exactly once, or the board isn't a real puzzle -- a
  // truncated or corrupt save would otherwise render duplicate tiles and
  // could leave blankPos_ pointing at a tile.
  bool seen[16] = {};
  for (int i = 0; i < 16; ++i) {
    const uint8_t tile = data[1 + i];
    if (tile > 15 || seen[tile]) return false;
    seen[tile] = true;
  }

  for (int i = 0; i < 16; ++i) {
    tiles_[i] = data[1 + i];
    if (tiles_[i] == 0) blankPos_ = i;
  }
  moves_ = static_cast<uint32_t>(data[17]) | (static_cast<uint32_t>(data[18]) << 8) |
           (static_cast<uint32_t>(data[19]) << 16) | (static_cast<uint32_t>(data[20]) << 24);
  checkSolved();
  return true;
}

void PuzzleDeskActivity::saveState() const {
  uint8_t data[kPuzzleStateSize] = {};
  data[0] = kPuzzleStateVersion;
  for (int i = 0; i < 16; ++i) data[1 + i] = tiles_[i];
  data[17] = static_cast<uint8_t>(moves_ & 0xFF);
  data[18] = static_cast<uint8_t>((moves_ >> 8) & 0xFF);
  data[19] = static_cast<uint8_t>((moves_ >> 16) & 0xFF);
  data[20] = static_cast<uint8_t>((moves_ >> 24) & 0xFF);

  FsFile f;
  if (!Storage.openFileForWrite("PZL", kPuzzleStatePath, f)) {
    LOG_ERR("PZL", "Could not open puzzle save for write");
    return;
  }
  const size_t written = f.write(data, kPuzzleStateSize);
  f.close();
  if (written != kPuzzleStateSize) LOG_ERR("PZL", "Short write saving puzzle state");
}

void PuzzleDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  if (!loadState()) shuffle();
  requestUpdate();
}

void PuzzleDeskActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void PuzzleDeskActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    shuffle();
    saveState();
    requestUpdate();
    return;
  }
  // Direction keys move tiles in that direction (the blank moves opposite).
  // Left/Right are the front buttons along the bottom bezel, which is where
  // the on-screen Up/Down hints sit, so they drive vertical moves; the side
  // rocker sits beside the screen and drives horizontal ones. Wiring these
  // the other way round meant the button labelled "Up" slid a tile sideways.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    moveBlank(1, 0);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    moveBlank(-1, 0);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    moveBlank(0, 1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    moveBlank(0, -1);
    return;
  }
  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    int cellSize, gridLeft, gridTop;
    gridGeometry(cellSize, gridLeft, gridTop);
    if (x < gridLeft || x >= gridLeft + cellSize * 4 || y < gridTop || y >= gridTop + cellSize * 4) return;
    const int col = (x - gridLeft) / cellSize;
    const int row = (y - gridTop) / cellSize;
    const int blankRow = blankPos_ / 4;
    const int blankCol = blankPos_ % 4;
    const bool adjacent = (row == blankRow && (col == blankCol + 1 || col == blankCol - 1)) ||
                         (col == blankCol && (row == blankRow + 1 || row == blankRow - 1));
    if (adjacent) moveBlank(row - blankRow, col - blankCol);
  }
}

void PuzzleDeskActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  char titleWithMoves[40];
  std::snprintf(titleWithMoves, sizeof(titleWithMoves), tr(STR_PUZZLE_MOVES), static_cast<unsigned>(moves_));

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_PUZZLE), false, 0, titleWithMoves);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_PUZZLE), titleWithMoves, false);
  }

  const int frameTop =
      metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int frameBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;
  renderer.drawRect(frameX, frameTop, frameW, frameBottom - frameTop);
  renderer.drawRect(frameX + 2, frameTop + 2, frameW - 4, frameBottom - frameTop - 4);

  int cellSize, gridLeft, gridTop;
  gridGeometry(cellSize, gridLeft, gridTop);
  const int font = UI_10_FONT_ID;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      const int pos = row * 4 + col;
      const int cellX = gridLeft + col * cellSize;
      const int cellY = gridTop + row * cellSize;
      renderer.drawRect(cellX, cellY, cellSize, cellSize);
      if (tiles_[pos] == 0) continue;
      char label[4];
      std::snprintf(label, sizeof(label), "%u", static_cast<unsigned>(tiles_[pos]));
      const int textWidth = renderer.getTextWidth(font, label);
      const int textHeight = renderer.getLineHeight(font);
      renderer.drawRect(cellX + 3, cellY + 3, cellSize - 6, cellSize - 6);
      renderer.drawText(font, cellX + (cellSize - textWidth) / 2, cellY + (cellSize - textHeight) / 2, label, true,
                        EpdFontFamily::BOLD);
    }
  }

  if (solved_) {
    renderer.drawCenteredText(UI_12_FONT_ID, gridTop + cellSize * 4 + 14, tr(STR_PUZZLE_SOLVED), true,
                              EpdFontFamily::BOLD);
  } else {
    // The hint bar only has room to label one direction pair (Up/Down
    // below) -- Left/Right and tapping a tile also work, so say so.
    renderer.drawCenteredText(UI_10_FONT_ID, gridTop + cellSize * 4 + 14, tr(STR_PUZZLE_HINT));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SHUFFLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
