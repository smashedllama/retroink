#include "RetroInkLibraryActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/CompactHeader.h"
#include "components/UITheme.h"
#include "FileBrowserActivity.h"
#include "RecentBookProgress.h"
#include "RecentBooksStore.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/EpubReaderUtils.h"
#include "fontIds.h"
#include "util/BookMoveUtils.h"

namespace {
constexpr int SPINE_GAP = 6;
constexpr int SPINE_MIN_W = 56;
constexpr int SPINE_MAX_COUNT = 8;
constexpr int BANNER_H = 40;
constexpr int BOARD_H = 6;
constexpr int DETAIL_H = 130;
// Icon view cells: a document icon with the title in up to two lines below.
constexpr int ICON_CELL_MIN_W = 128;
constexpr int ICON_CELL_H = 112;
constexpr int DOC_ICON_W = 34;
constexpr int DOC_ICON_H = 44;
constexpr int DOC_ICON_FOLD = 10;

const char* shelfLabel(RetroInkLibraryCatalog::Shelf shelf) {
  switch (shelf) {
    case RetroInkLibraryCatalog::Shelf::ToRead: return tr(STR_LIBRARY_TO_READ);
    case RetroInkLibraryCatalog::Shelf::Reading: return tr(STR_LIBRARY_READING);
    case RetroInkLibraryCatalog::Shelf::Finished: return tr(STR_LIBRARY_FINISHED);
    case RetroInkLibraryCatalog::Shelf::Favorites: return tr(STR_LIBRARY_FAVORITES);
    default: return tr(STR_LIBRARY_ALL);
  }
}
const char* sortLabel(RetroInkLibraryCatalog::Sort sort) {
  switch (sort) {
    case RetroInkLibraryCatalog::Sort::Filename: return tr(STR_LIBRARY_FILENAME_SORT);
    case RetroInkLibraryCatalog::Sort::Author: return tr(STR_LIBRARY_AUTHOR_SORT);
    case RetroInkLibraryCatalog::Sort::Recent: return tr(STR_LIBRARY_RECENT_SORT);
    default: return tr(STR_LIBRARY_TITLE_SORT);
  }
}

struct ConfirmActionRects {
  Rect scanNow;
  Rect back;
};

ConfirmActionRects getScanConfirmActionRects(const int x, const int w, const int buttonsTop) {
  constexpr int gap = 16;
  constexpr int buttonH = 60;
  const int buttonW = (w - gap) / 2;
  return {Rect{x, buttonsTop, buttonW, buttonH}, Rect{x + buttonW + gap, buttonsTop, buttonW, buttonH}};
}

bool containsPoint(const Rect& rect, const int x, const int y) {
  return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

void drawConfirmButton(const GfxRenderer& r, const Rect& rect, const char* label) {
  r.fillRect(rect.x + 3, rect.y + 3, rect.width, rect.height);
  r.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  r.drawRect(rect.x, rect.y, rect.width, rect.height);
  const int textWidth = r.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
  const int textHeight = r.getLineHeight(UI_12_FONT_ID);
  r.drawText(UI_12_FONT_ID, rect.x + (rect.width - textWidth) / 2, rect.y + (rect.height - textHeight) / 2, label,
            true, EpdFontFamily::BOLD);
}

void drawFitted(const GfxRenderer& r, int font, int x, int y, int width, const char* text, bool bold) {
  char line[224];
  snprintf(line, sizeof(line), "%s", text);
  if (r.getTextWidth(font, line) > width) {
    size_t len = strlen(line);
    while (len > 4 && r.getTextWidth(font, line) > width) line[--len] = '\0';
    if (len > 3) { line[len - 3] = '.'; line[len - 2] = '.'; line[len - 1] = '.'; }
  }
  r.drawText(font, x, y, line, true, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}

void drawTwoLineTitle(const GfxRenderer& r, int x, int y, int width, const char* title) {
  char first[224];
  snprintf(first, sizeof(first), "%s", title);
  size_t split = strlen(first);
  while (split > 1 && r.getTextWidth(UI_10_FONT_ID, first) > width) first[--split] = '\0';
  if (split < strlen(title)) {
    size_t space = split;
    while (space > split / 2 && title[space] != ' ') --space;
    if (space > split / 2) { split = space; first[split] = '\0'; }
  }
  r.drawText(UI_10_FONT_ID, x, y, first, true, EpdFontFamily::BOLD);
  const char* second = title + split;
  while (*second == ' ') ++second;
  if (*second) drawFitted(r, UI_10_FONT_ID, x, y + 21, width, second, true);
}

// A classic Macintosh document icon: a page with its top-right corner folded
// down. Selected icons draw inverted, the way the Finder darkens a selection.
void drawDocIcon(const GfxRenderer& r, const int x, const int y, const bool selected) {
  const int w = DOC_ICON_W, h = DOC_ICON_H, f = DOC_ICON_FOLD;
  if (selected) {
    r.fillRect(x, y + f, w + 1, h - f + 1, true);
    r.fillRect(x, y, w - f + 1, f, true);
    for (int i = 0; i < f; ++i) r.drawLine(x + w - f, y + i, x + w - f + i, y + i, true);
  }
  const bool ink = !selected;
  r.drawLine(x, y, x + w - f, y, true);
  r.drawLine(x + w - f, y, x + w, y + f, true);
  r.drawLine(x + w, y + f, x + w, y + h, true);
  r.drawLine(x, y + h, x + w, y + h, true);
  r.drawLine(x, y, x, y + h, true);
  // The fold itself.
  r.drawLine(x + w - f, y, x + w - f, y + f, ink);
  r.drawLine(x + w - f, y + f, x + w, y + f, ink);
  // Ruled lines of text on the page.
  for (int ly = y + 18; ly <= y + h - 8; ly += 6) r.drawLine(x + 6, ly, x + w - 6, ly, ink);
}

// Up to two centred lines of title under an icon, split at a space where it
// can be, the second line ellipsised. Selected labels are white on black, as
// the Finder highlights a selected icon's name.
void drawIconLabel(const GfxRenderer& r, const int cx, const int y, const int width, const char* title,
                   const bool selected) {
  const int lineH = r.getLineHeight(UI_10_FONT_ID);
  char first[224];
  snprintf(first, sizeof(first), "%s", title);
  size_t split = strlen(first);
  while (split > 1 && r.getTextWidth(UI_10_FONT_ID, first) > width) first[--split] = '\0';
  if (split < strlen(title)) {
    size_t space = split;
    while (space > 0 && title[space] != ' ') --space;
    if (space > 0) { split = space; first[split] = '\0'; }
  }
  const char* rest = title + split;
  while (*rest == ' ') ++rest;
  char second[224];
  snprintf(second, sizeof(second), "%s", rest);
  if (r.getTextWidth(UI_10_FONT_ID, second) > width) {
    size_t len = strlen(second);
    while (len > 4 && r.getTextWidth(UI_10_FONT_ID, second) > width) second[--len] = '\0';
    if (len > 3) { second[len - 3] = '.'; second[len - 2] = '.'; second[len - 1] = '.'; }
  }
  const char* lines[2] = {first, second};
  for (int i = 0; i < 2; ++i) {
    if (!lines[i][0]) continue;
    const int tw = r.getTextWidth(UI_10_FONT_ID, lines[i]);
    const int tx = cx - tw / 2;
    const int ty = y + i * lineH;
    if (selected) r.fillRect(tx - 3, ty, tw + 6, lineH, true);
    r.drawText(UI_10_FONT_ID, tx, ty, lines[i], !selected);
  }
}

std::string cacheFor(const std::string& path) {
  if (FsHelpers::hasEpubExtension(path)) return Epub::cachePathForFilePath(path, "/.crosspoint");
  if (FsHelpers::hasXtcExtension(path)) return Xtc(path, "/.crosspoint").getCachePath();
  return Txt(path, "/.crosspoint").getCachePath();
}

std::string progressFor(const std::string& path) {
  const RecentBook book{path, "", "", ""};
  float percent = FsHelpers::hasEpubExtension(path) ? RecentBookProgress::loadCachedEpubPercent(book)
                                                   : RecentBookProgress::loadPercent(book);
  char text[56];
  if (percent >= 0) {
    snprintf(text, sizeof(text), tr(STR_LIBRARY_PROGRESS_PERCENT), static_cast<unsigned>(percent + 0.5f));
    return text;
  }
  const std::string cache = cacheFor(path);
  if (FsHelpers::hasEpubExtension(path)) {
    EpubReaderUtils::Progress progress;
    if (EpubReaderUtils::readProgressFile("Library", cache + "/progress.bin", progress)) {
      snprintf(text, sizeof(text), tr(STR_LIBRARY_POSITION_CHAPTER_PAGE),
               static_cast<unsigned>(progress.spineIndex + 1), static_cast<unsigned>(progress.pageNumber + 1));
      return text;
    }
  } else {
    FsFile file;
    if (Storage.openFileForRead("Library", cache + "/progress.bin", file)) {
      uint8_t page[4] = {};
      const int count = file.read(page, sizeof(page)); file.close();
      if (count >= 2) {
        const unsigned p = static_cast<unsigned>(page[0]) | (static_cast<unsigned>(page[1]) << 8);
        snprintf(text, sizeof(text), tr(STR_LIBRARY_POSITION_PAGE), p + 1);
        return text;
      }
    }
  }
  return tr(STR_LIBRARY_PROGRESS_UNKNOWN);
}

std::string durationFor(const std::string& path) {
  char text[32];
  BookReadingStats::formatDuration(BookReadingStats::load(cacheFor(path)).totalReadingSeconds, text, sizeof(text));
  return text;
}
}  // namespace

void RetroInkLibraryActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  customShelves_.load();
  rebuildShelfCycle();
  shelfIndex_ = shelfIndexForToken(SETTINGS.libraryDefaultShelf);
  scanFinished_ = catalog_.openCached();
  awaitingScanConfirmation_ = !scanFinished_;
  selected_ = stripStart_ = 0;
  rowShelfIdValid_ = false;
  filteredRows_.clear();
  // openCached() already hands back the All Books / Title A-Z view for free;
  // anything else (including any custom shelf) needs its own view built.
  if (scanFinished_ && (shelfIndex_ != 0 || sort_ != RetroInkLibraryCatalog::Sort::Title)) rebuildView();
  requestUpdate();
}

void RetroInkLibraryActivity::onExit() {
  if (catalog_.isScanning()) catalog_.pauseScan();
  catalog_.close();
  rowShelfId_.clear();
  rowShelfId_.shrink_to_fit();
  filteredRows_.clear();
  filteredRows_.shrink_to_fit();
  shelfCycle_.clear();
  shelfCycle_.shrink_to_fit();
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void RetroInkLibraryActivity::rebuildShelfCycle() {
  const uint16_t previousCustom = shelfCycle_.empty() ? RetroInkCustomShelves::kNoShelf : activeShelf().customId;
  const auto previousBuiltin = shelfCycle_.empty() ? RetroInkLibraryCatalog::Shelf::All : activeShelf().builtin;
  shelfCycle_.clear();
  using Shelf = RetroInkLibraryCatalog::Shelf;
  for (const auto builtin : {Shelf::All, Shelf::ToRead, Shelf::Reading, Shelf::Finished, Shelf::Favorites})
    shelfCycle_.push_back({RetroInkCustomShelves::kNoShelf, builtin, shelfLabel(builtin)});
  for (const auto& shelf : customShelves_.shelves())
    shelfCycle_.push_back({shelf.id, Shelf::All, shelf.name});
  // Keep the user on the same shelf across a rebuild (e.g. after creating a
  // shelf elsewhere) when it still exists; otherwise fall back to All.
  shelfIndex_ = 0;
  for (uint32_t i = 0; i < shelfCycle_.size(); ++i) {
    if (shelfCycle_[i].customId == previousCustom &&
        (previousCustom != RetroInkCustomShelves::kNoShelf || shelfCycle_[i].builtin == previousBuiltin)) {
      shelfIndex_ = i;
      break;
    }
  }
}

uint32_t RetroInkLibraryActivity::shelfIndexForToken(const char* token) const {
  if (!token || !token[0] || strcmp(token, "all") == 0) return 0;
  if (strcmp(token, "toread") == 0) return 1;
  if (strcmp(token, "reading") == 0) return 2;
  if (strcmp(token, "finished") == 0) return 3;
  if (strcmp(token, "favorites") == 0) return 4;
  if (strncmp(token, "c:", 2) == 0) {
    const uint16_t wantId = static_cast<uint16_t>(strtoul(token + 2, nullptr, 10));
    for (uint32_t i = 5; i < shelfCycle_.size(); ++i)
      if (shelfCycle_[i].customId == wantId) return i;
  }
  return 0;  // deleted or unrecognized shelf: fall back to All, don't rewrite the setting
}

void RetroInkLibraryActivity::writeDefaultShelfToken() {
  char token[sizeof(SETTINGS.libraryDefaultShelf)];
  if (activeIsCustom()) {
    snprintf(token, sizeof(token), "c:%u", static_cast<unsigned>(activeShelf().customId));
  } else {
    static const char* kBuiltinTokens[] = {"all", "toread", "reading", "finished", "favorites"};
    snprintf(token, sizeof(token), "%s", kBuiltinTokens[static_cast<size_t>(activeShelf().builtin)]);
  }
  strncpy(SETTINGS.libraryDefaultShelf, token, sizeof(SETTINGS.libraryDefaultShelf) - 1);
  SETTINGS.libraryDefaultShelf[sizeof(SETTINGS.libraryDefaultShelf) - 1] = '\0';
  SETTINGS.saveToFile();
}

void RetroInkLibraryActivity::ensureRowShelfIds() {
  if (rowShelfIdValid_) return;
  rowShelfId_.assign(catalog_.viewCount(), RetroInkCustomShelves::kNoShelf);
  for (uint32_t row = 0; row < catalog_.viewCount(); ++row) {
    if (!catalog_.viewRecord(row, selectedRecord_)) break;
    if (selectedRecord_.fingerprint)
      rowShelfId_[row] = customShelves_.shelfForFingerprint(selectedRecord_.fingerprint);
  }
  rowShelfIdValid_ = true;
}

void RetroInkLibraryActivity::rebuildCustomFilter() {
  filteredRows_.clear();
  if (!activeIsCustom()) {
    rowShelfId_.clear();
    rowShelfId_.shrink_to_fit();
    rowShelfIdValid_ = false;
    return;
  }
  ensureRowShelfIds();
  const uint16_t want = activeShelf().customId;
  for (uint32_t row = 0; row < rowShelfId_.size(); ++row)
    if (rowShelfId_[row] == want) filteredRows_.push_back(row);
}

void RetroInkLibraryActivity::clampSelection() {
  const uint32_t count = shelfBookCount();
  if (!count) { selected_ = stripStart_ = 0; return; }
  if (selected_ >= count) selected_ = count - 1;
  const uint32_t perPage = itemsPerPage();
  stripStart_ = (selected_ / perPage) * perPage;
}

void RetroInkLibraryActivity::applyShelfChange(uint32_t newIndex) {
  const bool wasAllView = activeUsesAllView();
  shelfIndex_ = newIndex;
  selected_ = stripStart_ = 0;
  // Custom shelves (and All) all share the same underlying view, so hopping
  // between them is free - no re-sort, no SD re-read.
  if (wasAllView && activeUsesAllView()) {
    rebuildCustomFilter();
    requestUpdate();
  } else {
    rebuildView();
  }
}

int RetroInkLibraryActivity::spinesPerPage() const {
  const auto& m = UITheme::getInstance().getMetrics();
  const int innerW = renderer.getScreenWidth() - m.contentSidePadding * 2 - 18;
  int n = SPINE_MAX_COUNT;
  while (n > 1 && (innerW - SPINE_GAP * (n - 1)) / n < SPINE_MIN_W) --n;
  return n;
}

int RetroInkLibraryActivity::spineWidth() const {
  const auto& m = UITheme::getInstance().getMetrics();
  const int innerW = renderer.getScreenWidth() - m.contentSidePadding * 2 - 18;
  const int n = spinesPerPage();
  return (innerW - SPINE_GAP * (n - 1)) / n;
}

bool RetroInkLibraryActivity::iconView() const { return SETTINGS.libraryViewMode == 1; }

void RetroInkLibraryActivity::iconGrid(int& cols, int& rows) const {
  const auto& m = UITheme::getInstance().getMetrics();
  const int innerW = renderer.getScreenWidth() - m.contentSidePadding * 2 - 18;
  cols = std::max(1, innerW / ICON_CELL_MIN_W);
  // Same vertical budget render() uses: below the shelf banner, above the
  // detail panel. Fixed rather than following moveFailed_'s extra banner
  // line, so a failed move never changes the page size mid-browse.
  const int top = CompactHeader::contentTop(m) + 8;
  const int bottom = renderer.getScreenHeight() - m.buttonHintsHeight - 8;
  const int areaTop = top + 4 + BANNER_H + 12;
  const int areaBottom = bottom - 8 - DETAIL_H - 10;
  rows = std::max(1, (areaBottom - areaTop) / ICON_CELL_H);
}

uint32_t RetroInkLibraryActivity::itemsPerPage() const {
  if (!iconView()) return static_cast<uint32_t>(spinesPerPage());
  int cols, rows;
  iconGrid(cols, rows);
  return static_cast<uint32_t>(cols * rows);
}

void RetroInkLibraryActivity::toggleView() {
  SETTINGS.libraryViewMode = iconView() ? 0 : 1;
  SETTINGS.saveToFile();
  // Keep the same book selected; only the page around it changes size.
  const uint32_t perPage = itemsPerPage();
  stripStart_ = (selected_ / perPage) * perPage;
  requestUpdate();
}

void RetroInkLibraryActivity::rebuildView() {
  const auto builtin = activeIsCustom() ? RetroInkLibraryCatalog::Shelf::All : activeShelf().builtin;
  if (!catalog_.buildView(builtin, sort_)) { moveFailed_ = true; requestUpdate(); return; }
  rowShelfIdValid_ = false;  // row order (and, for a fresh scan, membership) just changed
  rebuildCustomFilter();
  clampSelection();
  requestUpdate();
}

void RetroInkLibraryActivity::refreshLibrary() {
  catalog_.close();
  scanFinished_ = false;
  awaitingScanConfirmation_ = true;
  selected_ = stripStart_ = 0;
  lastShownScanCount_ = UINT32_MAX;
  rowShelfIdValid_ = false;
  filteredRows_.clear();
  requestUpdate();
}

void RetroInkLibraryActivity::confirmAndBeginScan() {
  awaitingScanConfirmation_ = false;
  lastShownScanCount_ = UINT32_MAX;
  scanFinished_ = !catalog_.beginScan();
  if (scanFinished_) rebuildView();
  requestUpdate();
}

void RetroInkLibraryActivity::computeScanConfirmActionRects(Rect& scanNow, Rect& back) const {
  const auto& m = UITheme::getInstance().getMetrics();
  const int x = m.contentSidePadding;
  const int w = renderer.getScreenWidth() - x * 2;
  const int top = CompactHeader::contentTop(m) + 8;
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, tr(STR_LIBRARY_SCAN_EXPLANATION), w - 60, 6);
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const int textBottom = top + 90 + static_cast<int>(lines.size()) * lineHeight;
  const auto rects = getScanConfirmActionRects(x + 20, w - 40, textBottom + 20);
  scanNow = rects.scanNow;
  back = rects.back;
}

void RetroInkLibraryActivity::showShelves() {
  std::vector<std::string> choices;
  choices.reserve(shelfCycle_.size());
  for (const auto& entry : shelfCycle_) choices.push_back(entry.name);
  startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibraryShelves",
                                                           StrId::STR_LIBRARY_SHELVES, std::move(choices),
                                                           static_cast<uint8_t>(shelfIndex_)),
                         [this](const ActivityResult& result) {
                           if (const auto* choice = std::get_if<OptionSelectionResult>(&result.data))
                             if (choice->index < shelfCycle_.size()) applyShelfChange(choice->index);
                         });
}

void RetroInkLibraryActivity::showSort() {
  std::vector<std::string> choices = {tr(STR_LIBRARY_TITLE_SORT), tr(STR_LIBRARY_FILENAME_SORT),
                                      tr(STR_LIBRARY_AUTHOR_SORT), tr(STR_LIBRARY_RECENT_SORT)};
  startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibrarySort",
                                                           StrId::STR_LIBRARY_SORT, std::move(choices),
                                                           static_cast<uint8_t>(sort_)),
                         [this](const ActivityResult& result) {
                           if (const auto* choice = std::get_if<OptionSelectionResult>(&result.data)) {
                             sort_ = static_cast<RetroInkLibraryCatalog::Sort>(choice->index);
                             selected_ = stripStart_ = 0;
                             rebuildView();
                           }
                         });
}

void RetroInkLibraryActivity::showLetterJump() {
  std::vector<std::string> letters;
  letters.reserve(27);
  letters.push_back("#");
  for (char letter = 'A'; letter <= 'Z'; ++letter) letters.emplace_back(1, letter);
  startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibraryLetters",
                                                           StrId::STR_LIBRARY_JUMP, std::move(letters), 0),
                         [this](const ActivityResult& result) {
                           const auto* choice = std::get_if<OptionSelectionResult>(&result.data);
                           if (!choice) return;
                           const uint32_t count = shelfBookCount();
                           const uint32_t perPage = itemsPerPage();
                           for (uint32_t row = 0; row < count; ++row) {
                             if (!shelfRecord(row, selectedRecord_)) break;
                             const uint8_t first = static_cast<uint8_t>(selectedRecord_.title[0]);
                             const char upper = first >= 'a' && first <= 'z' ? first - 32 : first;
                             const bool isLetter = upper >= 'A' && upper <= 'Z';
                             if ((choice->index == 0 && !isLetter) ||
                                 (choice->index > 0 && upper == 'A' + choice->index - 1)) {
                               selected_ = row;
                               stripStart_ = (row / perPage) * perPage;
                               requestUpdate();
                               break;
                             }
                           }
                         });
}

void RetroInkLibraryActivity::openSelectedBook() {
  if (shelfRecord(selected_, selectedRecord_) && Storage.exists(selectedRecord_.path))
    onSelectBook(selectedRecord_.path);
}

void RetroInkLibraryActivity::moveSelectedBook(const std::string& path) {
  startActivityForResult(std::make_unique<FileBrowserActivity>(renderer, mappedInput, "/",
                                                              FileBrowserActivity::Mode::PickDirectory),
                         [this, path](const ActivityResult& result) {
                           const auto* picked = std::get_if<FilePathResult>(&result.data);
                           if (!picked || picked->path.empty()) return;
                           const char* name = strrchr(path.c_str(), '/');
                           name = name ? name + 1 : path.c_str();
                           const std::string dst = (picked->path == "/" ? "" : picked->path) + "/" + name;
                           if (dst == path) return;
                           moveFailed_ = !BookMoveUtils::moveWithState(path, dst);
                           if (!moveFailed_) refreshLibrary();
                           requestUpdate();
                         });
}

void RetroInkLibraryActivity::reviewCandidate(uint32_t match) {
  if (!catalog_.recoveryCandidate(reviewCurrent_, match, reviewOld_)) return;
  const std::string oldProgress = progressFor(reviewOld_.path), oldTime = durationFor(reviewOld_.path);
  const std::string currentProgress = progressFor(reviewCurrent_.path), currentTime = durationFor(reviewCurrent_.path);
  char oldLabel[128], currentLabel[128];
  snprintf(oldLabel, sizeof(oldLabel), tr(STR_LIBRARY_RESTORE_WITH_PROGRESS), oldProgress.c_str(), oldTime.c_str());
  snprintf(currentLabel, sizeof(currentLabel), tr(STR_LIBRARY_KEEP_WITH_PROGRESS),
           currentProgress.c_str(), currentTime.c_str());
  std::vector<std::string> choices = {oldLabel, currentLabel};
  const std::string oldPath = reviewOld_.path, currentPath = reviewCurrent_.path;
  startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibraryRestore",
                                                           StrId::STR_LIBRARY_RECOVER, std::move(choices), 1),
                         [this, oldPath, currentPath](const ActivityResult& result) {
                           const auto* choice = std::get_if<OptionSelectionResult>(&result.data);
                           if (!choice || choice->index != 0) return;
                           moveFailed_ = !BookMoveUtils::restorePriorState(oldPath, currentPath);
                           if (!moveFailed_) {
                             if (reviewOld_.flags & 4) catalog_.setFavorite(catalogRow(selected_), true);
                             refreshLibrary();
                           }
                           requestUpdate();
                         });
}

void RetroInkLibraryActivity::reviewRecovery() {
  if (!shelfRecord(selected_, reviewCurrent_)) return;
  const uint32_t count = catalog_.recoveryCount(reviewCurrent_);
  if (!count) return;
  if (count == 1) { reviewCandidate(0); return; }
  std::vector<std::string> choices;
  choices.reserve(std::min<uint32_t>(count, 32));
  for (uint32_t i = 0; i < count && i < 32; ++i) {
    if (!catalog_.recoveryCandidate(reviewCurrent_, i, reviewOld_)) break;
    choices.push_back(std::string(reviewOld_.path) + "  (" + progressFor(reviewOld_.path) + ")");
  }
  startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibraryMatches",
                                                           StrId::STR_LIBRARY_RECOVER, std::move(choices), 0),
                         [this](const ActivityResult& result) {
                           if (const auto* choice = std::get_if<OptionSelectionResult>(&result.data))
                             reviewCandidate(choice->index);
                         });
}

void RetroInkLibraryActivity::showActions() {
  if (!shelfBookCount() || !shelfRecord(selected_, selectedRecord_)) {
    std::vector<std::string> choices = {tr(STR_LIBRARY_SHELVES), tr(STR_LIBRARY_SORT), tr(STR_LIBRARY_SET_DEFAULT),
                                        tr(STR_LIBRARY_REFRESH),
                                        iconView() ? tr(STR_LIBRARY_VIEW_AS_SHELF) : tr(STR_LIBRARY_VIEW_AS_ICONS)};
    startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibraryActions",
                                                             StrId::STR_LIBRARY_BOOK_MENU, std::move(choices), 0),
                           [this](const ActivityResult& result) {
                             if (const auto* choice = std::get_if<OptionSelectionResult>(&result.data)) {
                               switch (choice->index) {
                                 case 0: showShelves(); break;
                                 case 1: showSort(); break;
                                 case 2: writeDefaultShelfToken(); break;
                                 case 3: refreshLibrary(); break;
                                 case 4: toggleView(); break;
                               }
                             }
                           });
    return;
  }
  const std::string path = selectedRecord_.path;
  const bool favorite = (selectedRecord_.flags & 4) != 0;
  std::vector<std::string> choices = {tr(STR_LIBRARY_OPEN),
                                      favorite ? tr(STR_LIBRARY_UNPIN) : tr(STR_LIBRARY_PIN),
                                      tr(STR_LIBRARY_MOVE), tr(STR_LIBRARY_SHELVES),
                                      tr(STR_LIBRARY_SORT), tr(STR_LIBRARY_JUMP),
                                      tr(STR_LIBRARY_SET_DEFAULT), tr(STR_LIBRARY_REFRESH),
                                      iconView() ? tr(STR_LIBRARY_VIEW_AS_SHELF) : tr(STR_LIBRARY_VIEW_AS_ICONS)};
  // Recover is conditional, so it stays last to keep every other index fixed.
  if (catalog_.recoveryCount(selectedRecord_)) choices.push_back(tr(STR_LIBRARY_RECOVER));
  startActivityForResult(std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "LibraryActions",
                                                           StrId::STR_LIBRARY_BOOK_MENU, std::move(choices), 0),
                         [this, path, favorite](const ActivityResult& result) {
                           const auto* choice = std::get_if<OptionSelectionResult>(&result.data);
                           if (!choice) return;
                           switch (choice->index) {
                             case 0: openSelectedBook(); break;
                             case 1:
                               if (catalog_.setFavorite(catalogRow(selected_), !favorite)) rebuildView();
                               break;
                             case 2: moveSelectedBook(path); break;
                             case 3: showShelves(); break;
                             case 4: showSort(); break;
                             case 5: showLetterJump(); break;
                             case 6: writeDefaultShelfToken(); break;
                             case 7: refreshLibrary(); break;
                             case 8: toggleView(); break;
                             case 9: reviewRecovery(); break;
                           }
                         });
}

void RetroInkLibraryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (catalog_.isScanning()) catalog_.pauseScan();
    finish(); return;
  }
  if (awaitingScanConfirmation_) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { confirmAndBeginScan(); return; }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      Rect scanNowRect, backRect;
      computeScanConfirmActionRects(scanNowRect, backRect);
      if (containsPoint(scanNowRect, x, y)) { confirmAndBeginScan(); return; }
      if (containsPoint(backRect, x, y)) { finish(); return; }
    }
    return;
  }
  if (catalog_.isScanning()) {
    if (!catalog_.stepScan()) {
      scanFinished_ = !catalog_.isScanning();
      requestUpdate();
      return;
    }
    if (!catalog_.isScanning()) { scanFinished_ = true; rebuildView(); return; }
    const uint32_t count = catalog_.scannedCount();
    if (count != lastShownScanCount_ && (count % 24 == 0 || lastShownScanCount_ == UINT32_MAX)) {
      lastShownScanCount_ = count; requestUpdate();
    }
    return;
  }
  if (!scanFinished_ || catalog_.failed()) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { showActions(); return; }

  // Up/Down: previous/next shelf, wrapping across the whole cycle (5
  // built-ins then any custom shelves). Release-only, no held-repeat - a
  // held Up could queue SD re-sorts faster than the panel can repaint them.
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    applyShelfChange(shelfIndex_ ? shelfIndex_ - 1 : static_cast<uint32_t>(shelfCycle_.size()) - 1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    applyShelfChange((shelfIndex_ + 1) % static_cast<uint32_t>(shelfCycle_.size()));
    return;
  }

  // Left/Right: previous/next book within this shelf, wrapping. The visible
  // window is page-aligned (a strip of spines, or a page of icons), so it
  // only jumps when the selection crosses a page edge.
  const uint32_t count = shelfBookCount();
  const uint32_t perPage = itemsPerPage();
  if (count && mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    selected_ = selected_ ? selected_ - 1 : count - 1;
    stripStart_ = (selected_ / perPage) * perPage;
    requestUpdate(); return;
  }
  if (count && mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    selected_ = (selected_ + 1) % count;
    stripStart_ = (selected_ / perPage) * perPage;
    requestUpdate();
  }
}

void RetroInkLibraryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  CompactHeader::drawTitle(renderer, tr(STR_RETRO_LIBRARY));
  const auto& m = UITheme::getInstance().getMetrics();
  const int x = m.contentSidePadding;
  const int w = renderer.getScreenWidth() - x * 2;
  const int top = CompactHeader::contentTop(m) + 8;
  const int bottom = renderer.getScreenHeight() - m.buttonHintsHeight - 8;
  // No window drawn here: CompactHeader::drawTitle routes to
  // System6Theme::drawHeader, which already lays down the window body (white
  // fill, border, drop shadow and grow box) from the header down to the button
  // hints. Drawing a second one inside it stacked two windows, clipping the
  // grow box in the bottom-right corner.
  if (awaitingScanConfirmation_) {
    renderer.drawCenteredText(UI_12_FONT_ID, top + 35, tr(STR_LIBRARY_SCAN_CONFIRM_TITLE), true, EpdFontFamily::BOLD);
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, tr(STR_LIBRARY_SCAN_EXPLANATION), w - 60, 6);
    const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 6;
    int ly = top + 90;
    for (const auto& line : lines) {
      renderer.drawCenteredText(UI_10_FONT_ID, ly, line.c_str());
      ly += lineHeight;
    }
    // Real, legible buttons in the content area (not just the tiny bottom
    // hint bar, which also disappears entirely on touch devices) -- this is
    // a decision the user should not have to guess at from a cramped hint.
    Rect scanNowRect, backRect;
    computeScanConfirmActionRects(scanNowRect, backRect);
    drawConfirmButton(renderer, scanNowRect, tr(STR_LIBRARY_SCAN_NOW));
    drawConfirmButton(renderer, backRect, tr(STR_BACK));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_LIBRARY_SCAN_NOW_SHORT), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (catalog_.isScanning()) {
    renderer.drawCenteredText(UI_12_FONT_ID, top + 35, tr(STR_LIBRARY_SCAN), true, EpdFontFamily::BOLD);
    const uint32_t scanned = catalog_.scannedCount();
    const uint32_t estimatedTotal = catalog_.previousCount();
    char count[48];
    if (estimatedTotal > 0) {
      snprintf(count, sizeof(count), "~%lu / %lu", static_cast<unsigned long>(scanned),
               static_cast<unsigned long>(estimatedTotal));
    } else {
      snprintf(count, sizeof(count), "%lu", static_cast<unsigned long>(scanned));
    }
    renderer.drawCenteredText(UI_12_FONT_ID, top + 95, count, true, EpdFontFamily::BOLD);
    if (estimatedTotal > 0) {
      // Books added/removed since the last scan make this an estimate, not
      // an exact percentage -- clamp so a library that grew doesn't draw a
      // bar past full.
      GUI.drawProgressBar(renderer, Rect{x + 30, top + 125, w - 60, m.progressBarHeight},
                          std::min(scanned, estimatedTotal), estimatedTotal);
    }
    drawFitted(renderer, UI_10_FONT_ID, x + 20, top + 170, w - 40, catalog_.currentPath().c_str(), false);
  } else if (catalog_.failed()) {
    renderer.drawCenteredText(UI_10_FONT_ID, top + 100, tr(STR_LIBRARY_SD_ERROR));
  } else {
    const int innerX = x + 9;
    const int innerW = w - 18;
    const int bannerY = top + 4;

    // --- shelf banner: BootActivity.cpp's pinstripe-title-strip idiom ---
    for (int sy = bannerY + 6; sy < bannerY + BANNER_H - 6; sy += 4)
      renderer.drawLine(innerX, sy, innerX + innerW - 1, sy);
    const std::string shelfName = activeShelf().name;
    const int nameWidth = renderer.getTextWidth(UI_12_FONT_ID, shelfName.c_str());
    const int nameX = innerX + (innerW - nameWidth) / 2;
    renderer.fillRect(nameX - 10, bannerY + 2, nameWidth + 20, BANNER_H - 4, false);
    renderer.drawText(UI_12_FONT_ID, nameX, bannerY + (BANNER_H - renderer.getLineHeight(UI_12_FONT_ID)) / 2,
                       shelfName.c_str(), true, EpdFontFamily::BOLD);
    char posBuf[16];
    snprintf(posBuf, sizeof(posBuf), tr(STR_LIBRARY_SHELF_OF), static_cast<unsigned>(shelfIndex_ + 1),
             static_cast<unsigned>(shelfCycle_.size()));
    const int posWidth = renderer.getTextWidth(SMALL_FONT_ID, posBuf, EpdFontFamily::BOLD);
    renderer.fillRect(innerX + innerW - posWidth - 22, bannerY + 4, posWidth + 20, BANNER_H - 8, false);
    renderer.drawText(SMALL_FONT_ID, innerX + innerW - posWidth - 12,
                       bannerY + (BANNER_H - renderer.getLineHeight(SMALL_FONT_ID)) / 2, posBuf, true,
                       EpdFontFamily::BOLD);
    if (moveFailed_) {
      drawFitted(renderer, UI_10_FONT_ID, innerX + 4, bannerY + BANNER_H + 14, innerW - 8,
                 tr(STR_LIBRARY_MOVE_FAILED), true);
    }

    // --- spine strip + shelf board ---
    const int stripTop = bannerY + BANNER_H + (moveFailed_ ? 30 : 12);
    const int detailY = bottom - 8 - DETAIL_H;
    const int boardY = detailY - 14 - BOARD_H;
    const int spineAreaH = std::max(80, boardY - stripTop);
    const uint32_t count = shelfBookCount();
    if (!count) {
      renderer.drawCenteredText(UI_10_FONT_ID, stripTop + spineAreaH / 2, tr(STR_LIBRARY_NO_BOOKS));
    } else if (iconView()) {
      // --- Finder icon view: a page of document icons, titles upright ---
      int cols, rows;
      iconGrid(cols, rows);
      const int cellW = innerW / cols;
      const int gridTop = bannerY + BANNER_H + 12;
      const int perPage = cols * rows;
      for (int i = 0; i < perPage && stripStart_ + static_cast<uint32_t>(i) < count; ++i) {
        const uint32_t idx = stripStart_ + static_cast<uint32_t>(i);
        if (!shelfRecord(idx, selectedRecord_)) break;
        const bool isSelected = idx == selected_;
        const int cellX = innerX + (i % cols) * cellW;
        const int cellY = gridTop + (i / cols) * ICON_CELL_H;
        drawDocIcon(renderer, cellX + (cellW - DOC_ICON_W) / 2, cellY + 6, isSelected);
        drawIconLabel(renderer, cellX + cellW / 2, cellY + 6 + DOC_ICON_H + 8, cellW - 10, selectedRecord_.title,
                      isSelected);
      }
    } else {
      const int n = spinesPerPage();
      const int sw = spineWidth();
      for (int i = 0; i < n && stripStart_ + static_cast<uint32_t>(i) < count; ++i) {
        const uint32_t idx = stripStart_ + static_cast<uint32_t>(i);
        if (!shelfRecord(idx, selectedRecord_)) break;
        const bool isSelected = idx == selected_;
        const int sx = innerX + i * (sw + SPINE_GAP);
        const uint64_t fp = selectedRecord_.fingerprint;
        int sh = spineAreaH - static_cast<int>((fp ^ (fp >> 32)) % 5) * 10;
        if (isSelected) sh = spineAreaH + 14;  // stands the tallest, pulled out of the shelf
        sh = std::max(sh, 60);
        const int sy = boardY - sh;
        renderer.fillRect(sx + 3, sy + 3, sw, sh, true);
        renderer.fillRect(sx, sy, sw, sh, false);
        renderer.drawRect(sx, sy, sw, sh, isSelected ? 2 : 1, true);
        if (isSelected) {
          renderer.drawRect(sx + 5, sy + 5, sw - 10, sh - 10, 1, true);
          renderer.fillRect(sx + 8, sy + 10, sw - 16, 5, true);
        }
        const std::string label =
            renderer.truncatedText(UI_10_FONT_ID, selectedRecord_.title, sh - 50, EpdFontFamily::BOLD);
        const int textHeight = renderer.getTextHeight(UI_10_FONT_ID);
        const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label.c_str(), EpdFontFamily::BOLD);
        const int tx = sx + (sw - textHeight) / 2;
        const int ty = sy + (sh + textWidth) / 2;
        renderer.drawTextRotated90CW(UI_10_FONT_ID, tx, ty, label.c_str(), true, EpdFontFamily::BOLD);
      }
    }
    // The shelf board only belongs under spines.
    if (!iconView()) {
      renderer.fillRect(innerX, boardY, innerW, BOARD_H, true);
      renderer.fillRect(innerX + 6, boardY + BOARD_H, innerW - 12, 3, true);
    }

    // --- detail panel for the selected book: spine labels are necessarily
    // terse, so this is where the current selection is actually readable ---
    renderer.drawRect(x + 9, detailY, innerW, DETAIL_H);
    if (count && shelfRecord(selected_, selectedRecord_)) {
      drawTwoLineTitle(renderer, x + 22, detailY + 10, innerW - 26, selectedRecord_.title);
      drawFitted(renderer, UI_10_FONT_ID, x + 22, detailY + 56, innerW - 26,
                 selectedRecord_.author[0] ? selectedRecord_.author : selectedRecord_.path, false);
      char bookOf[48];
      snprintf(bookOf, sizeof(bookOf), tr(STR_LIBRARY_BOOK_OF), static_cast<unsigned>(selected_ + 1),
               static_cast<unsigned>(count));
      char metaLine[160];
      snprintf(metaLine, sizeof(metaLine), "%s  /  %s", bookOf, sortLabel(sort_));
      drawFitted(renderer, SMALL_FONT_ID, x + 22, detailY + 84, innerW - 26, metaLine, false);
      const std::string progress = progressFor(selectedRecord_.path);
      drawFitted(renderer, SMALL_FONT_ID, x + 22, detailY + 106, innerW - 26, progress.c_str(), false);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, detailY + DETAIL_H / 2, tr(STR_LIBRARY_NO_BOOKS));
    }
  }
  const bool ready = !catalog_.isScanning() && !catalog_.failed();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), ready ? tr(STR_ACTIONS) : "",
                                             ready && shelfBookCount() ? tr(STR_LIBRARY_PREV) : "",
                                             ready && shelfBookCount() ? tr(STR_LIBRARY_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  renderer.displayBuffer();
}
