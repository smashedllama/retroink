#include "MinesweeperDeskActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void MinesweeperDeskActivity::newGame() {
  for (int i = 0; i < kCells; ++i) {
    cell_[i] = 0;
    near_[i] = 0;
  }
  minesPlaced_ = false;
  lost_ = false;
  won_ = false;
  hitCell_ = -1;
  seconds_ = 0;
  startedMs_ = 0;
}

// Mines are laid after the first dig so that square (and its neighbours, which
// makes the opening a clearing instead of a lone number) is never one.
void MinesweeperDeskActivity::placeMines(const int safeCell) {
  const int safeRow = safeCell / kCols;
  const int safeCol = safeCell % kCols;
  int placed = 0;
  while (placed < kMines) {
    const int pick = static_cast<int>(random(kCells));
    const int row = pick / kCols;
    const int col = pick % kCols;
    if (std::abs(row - safeRow) <= 1 && std::abs(col - safeCol) <= 1) continue;
    if (cell_[pick] & Mine) continue;
    cell_[pick] |= Mine;
    ++placed;
  }
  for (int i = 0; i < kCells; ++i) {
    const int row = i / kCols;
    const int col = i % kCols;
    uint8_t count = 0;
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        const int r = row + dr;
        const int c = col + dc;
        if (r < 0 || r >= kRows || c < 0 || c >= kCols) continue;
        if (cell_[r * kCols + c] & Mine) ++count;
      }
    }
    near_[i] = count;
  }
  minesPlaced_ = true;
  startedMs_ = millis();
}

// Opens a square and, when it has no mines around it, everything it connects to.
void MinesweeperDeskActivity::reveal(const int index) {
  int stack[kCells];
  int top = 0;
  stack[top++] = index;
  while (top > 0) {
    const int i = stack[--top];
    if (i < 0 || i >= kCells) continue;
    if (cell_[i] & (Revealed | Flagged)) continue;
    cell_[i] |= Revealed;
    if (near_[i] != 0 || (cell_[i] & Mine)) continue;
    const int row = i / kCols;
    const int col = i % kCols;
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        const int r = row + dr;
        const int c = col + dc;
        if (r < 0 || r >= kRows || c < 0 || c >= kCols) continue;
        if (top < kCells) stack[top++] = r * kCols + c;
      }
    }
  }
}

void MinesweeperDeskActivity::checkWon() {
  int closedSafe = 0;
  for (int i = 0; i < kCells; ++i) {
    if (!(cell_[i] & Mine) && !(cell_[i] & Revealed)) ++closedSafe;
  }
  if (closedSafe != 0) return;
  won_ = true;
  seconds_ = static_cast<uint32_t>((millis() - startedMs_) / 1000);
  // A cleared board shows every mine flagged.
  for (int i = 0; i < kCells; ++i) {
    if (cell_[i] & Mine) cell_[i] |= Flagged;
  }
}

void MinesweeperDeskActivity::dig(const int index) {
  if (lost_ || won_ || index < 0 || index >= kCells) return;
  if (cell_[index] & Flagged) return;
  if (cell_[index] & Revealed) {
    chord(index);
    return;
  }
  if (!minesPlaced_) placeMines(index);
  if (cell_[index] & Mine) {
    cell_[index] |= Revealed;
    hitCell_ = index;
    lost_ = true;
    seconds_ = static_cast<uint32_t>((millis() - startedMs_) / 1000);
    for (int i = 0; i < kCells; ++i) {
      if (cell_[i] & Mine) cell_[i] |= Revealed;
    }
    return;
  }
  reveal(index);
  checkWon();
}

// Tapping a number that already has that many flags around it opens the rest
// of its neighbours, which saves a lot of single taps.
void MinesweeperDeskActivity::chord(const int index) {
  if (near_[index] == 0) return;
  const int row = index / kCols;
  const int col = index % kCols;
  int flags = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      const int r = row + dr;
      const int c = col + dc;
      if (r < 0 || r >= kRows || c < 0 || c >= kCols) continue;
      if (cell_[r * kCols + c] & Flagged) ++flags;
    }
  }
  if (flags != near_[index]) return;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      const int r = row + dr;
      const int c = col + dc;
      if (r < 0 || r >= kRows || c < 0 || c >= kCols) continue;
      const int n = r * kCols + c;
      if (cell_[n] & (Flagged | Revealed)) continue;
      if (cell_[n] & Mine) {
        // A wrong flag: the mine goes off.
        cell_[n] |= Revealed;
        hitCell_ = n;
        lost_ = true;
        seconds_ = static_cast<uint32_t>((millis() - startedMs_) / 1000);
        for (int i = 0; i < kCells; ++i) {
          if (cell_[i] & Mine) cell_[i] |= Revealed;
        }
        return;
      }
      reveal(n);
    }
  }
  checkWon();
}

void MinesweeperDeskActivity::toggleFlag(const int index) {
  if (lost_ || won_ || index < 0 || index >= kCells) return;
  if (cell_[index] & Revealed) return;
  cell_[index] ^= Flagged;
}

int MinesweeperDeskActivity::flagsPlaced() const {
  int flags = 0;
  for (int i = 0; i < kCells; ++i) {
    if (cell_[i] & Flagged) ++flags;
  }
  return flags;
}

void MinesweeperDeskActivity::geometry(int& cellSize, int& left, int& top) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int areaTop = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  // Room under the board for the status line.
  const int areaBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - 40;
  const int byWidth = (pageWidth - 2 * metrics.contentSidePadding - 16) / kCols;
  const int byHeight = (areaBottom - areaTop - 12) / kRows;
  cellSize = std::max(24, std::min(byWidth, byHeight));
  left = (pageWidth - cellSize * kCols) / 2;
  top = areaTop + 8;
}

int MinesweeperDeskActivity::cellAt(const int x, const int y) const {
  int size, left, top;
  geometry(size, left, top);
  if (x < left || x >= left + size * kCols || y < top || y >= top + size * kRows) return -1;
  return ((y - top) / size) * kCols + (x - left) / size;
}

void MinesweeperDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  flagMode_ = false;
  newGame();
  requestUpdate();
}

void MinesweeperDeskActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void MinesweeperDeskActivity::loop() {
  if (menu_.isActive()) {
    menu_.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    // New Game sits behind the Menu so it can't be tapped in the middle of a
    // round by accident.
    const std::vector<std::string> choices = {tr(STR_MINES_NEW_GAME), tr(STR_EXIT)};
    menu_.show(tr(STR_MINES_MENU), choices, 0, [this](const int index) {
      if (index == 0) {
        newGame();
      } else if (index == 1) {
        finish();
        return;
      }
      requestUpdate();
    });
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    flagMode_ = !flagMode_;
    requestUpdate();
    return;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const int index = cellAt(x, y);
    if (index < 0) return;
    if (flagMode_ && !(cell_[index] & Revealed)) {
      toggleFlag(index);
    } else {
      dig(index);
    }
    requestUpdate();
  }
}

// The marks are drawn from lines and boxes, in the same pixel style as the
// desk accessory icons.
void MinesweeperDeskActivity::drawCell(const int index, const int x, const int y, const int size) const {
  const uint8_t state = cell_[index];
  const bool revealed = state & Revealed;
  const int cx = x + size / 2;
  const int cy = y + size / 2;

  if (!revealed) {
    // Raised, unopened square.
    renderer.fillRectDither(x + 1, y + 1, size - 2, size - 2, Color::LightGray);
    renderer.drawRect(x, y, size, size);
    renderer.drawLine(x + 1, y + size - 2, x + size - 2, y + size - 2);
    renderer.drawLine(x + size - 2, y + 1, x + size - 2, y + size - 2);
    if (state & Flagged) {
      // A flag on a pole.
      const int pole = cx - size / 8;
      renderer.fillRect(pole, cy - size / 3, 3, size * 2 / 3, true);
      for (int k = 0; k < size / 4; ++k) {
        renderer.drawLine(pole + 3, cy - size / 3 + k, pole + 3 + (size / 4) - k, cy - size / 3 + k);
      }
      renderer.fillRect(pole - size / 8, cy + size / 3 - 2, size / 3, 4, true);
    }
    return;
  }

  const bool hit = index == hitCell_;
  renderer.fillRect(x, y, size, size, hit);
  renderer.drawRect(x, y, size, size);
  const bool ink = !hit;
  if (state & Mine) {
    // A mine: a solid centre with spokes.
    const int r = size / 4;
    renderer.fillRect(cx - r + 1, cy - r + 1, r * 2 - 1, r * 2 - 1, ink);
    const int spoke = r + size / 8;
    renderer.drawLine(cx - spoke, cy, cx + spoke, cy, ink);
    renderer.drawLine(cx, cy - spoke, cx, cy + spoke, ink);
    renderer.drawLine(cx - spoke + 2, cy - spoke + 2, cx + spoke - 2, cy + spoke - 2, ink);
    renderer.drawLine(cx - spoke + 2, cy + spoke - 2, cx + spoke - 2, cy - spoke + 2, ink);
    return;
  }
  if (near_[index] > 0) {
    char digit[2] = {static_cast<char>('0' + near_[index]), 0};
    const int font = UI_12_FONT_ID;
    const int textW = renderer.getTextWidth(font, digit, EpdFontFamily::BOLD);
    renderer.drawText(font, cx - textW / 2, cy - renderer.getLineHeight(font) / 2, digit, true, EpdFontFamily::BOLD);
  }
}

void MinesweeperDeskActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();

  char minesLeft[32];
  std::snprintf(minesLeft, sizeof(minesLeft), tr(STR_MINES_LEFT), static_cast<int>(kMines - flagsPlaced()));
  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_MINESWEEPER), false, 0, minesLeft);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_MINESWEEPER), minesLeft, false);
  }

  int size, left, top;
  geometry(size, left, top);
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      drawCell(row * kCols + col, left + col * size, top + row * size, size);
    }
  }

  const int captionY = top + size * kRows + 12;
  char caption[48];
  if (won_) {
    std::snprintf(caption, sizeof(caption), tr(STR_MINES_CLEARED), static_cast<unsigned>(seconds_));
    renderer.drawCenteredText(UI_12_FONT_ID, captionY, caption, true, EpdFontFamily::BOLD);
  } else if (lost_) {
    renderer.drawCenteredText(UI_12_FONT_ID, captionY, tr(STR_MINES_BOOM), true, EpdFontFamily::BOLD);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, captionY, flagMode_ ? tr(STR_MINES_HINT_FLAG) : tr(STR_MINES_HINT));
  }
  (void)pageHeight;
  (void)metrics;

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MINES_MENU), "",
                                            flagMode_ ? tr(STR_MINES_DIG) : tr(STR_MINES_FLAG));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (menu_.processRender(renderer, mappedInput)) return;
  renderer.displayBuffer();
}
