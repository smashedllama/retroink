#include "BootActivity.h"

#include <Bitmap.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <algorithm>
#include <cstdio>

#include "AppVersion.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "ImageFolderIndex.h"
#include "components/DesktopPattern.h"
#include "fontIds.h"
#include "images/Logo120.h"

namespace {
void drawRetroInkDesktop(const GfxRenderer& renderer, const int pageWidth, const int pageHeight) {
  DesktopPattern::fill(renderer, 0, 0, pageWidth, pageHeight, SETTINGS.desktopPattern);
}

void drawRetroInkMac(const GfxRenderer& renderer, const int x, const int y, const int scale, const int eyeOffset = 0,
                     const bool sleeping = false) {
  renderer.drawRect(x, y, 24 * scale, 29 * scale);
  renderer.drawRect(x + 3 * scale, y + 3 * scale, 18 * scale, 17 * scale);
  if (sleeping) {
    renderer.drawLine(x + 6 * scale, y + 9 * scale, x + 10 * scale, y + 9 * scale);
    renderer.drawLine(x + 14 * scale, y + 9 * scale, x + 18 * scale, y + 9 * scale);
    renderer.drawLine(x + 9 * scale, y + 15 * scale, x + 15 * scale, y + 15 * scale);
  } else {
    const int boundedEyeOffset = std::clamp(eyeOffset, -2, 2);
    renderer.fillRect(x + (7 + boundedEyeOffset) * scale, y + 8 * scale, 2 * scale, 2 * scale);
    renderer.fillRect(x + (15 + boundedEyeOffset) * scale, y + 8 * scale, 2 * scale, 2 * scale);
    renderer.drawLine(x + 8 * scale, y + 14 * scale, x + 15 * scale, y + 14 * scale);
  }
  renderer.drawLine(x + 10 * scale, y + 24 * scale, x + 20 * scale, y + 24 * scale);
  renderer.drawRect(x + 2 * scale, y + 30 * scale, 20 * scale, 3 * scale);
}

void drawRetroInkFloppy(const GfxRenderer& renderer, const int x, const int y, const int scale) {
  renderer.drawRect(x, y, 24 * scale, 29 * scale);
  renderer.drawRect(x + 3 * scale, y + 3 * scale, 18 * scale, 9 * scale);
  renderer.fillRect(x + 15 * scale, y + 4 * scale, 4 * scale, 7 * scale);
  renderer.drawRect(x + 4 * scale, y + 17 * scale, 16 * scale, 9 * scale);
  renderer.drawLine(x + 7 * scale, y + 20 * scale, x + 17 * scale, y + 20 * scale);
  renderer.drawLine(x + 7 * scale, y + 23 * scale, x + 17 * scale, y + 23 * scale);
}

// A malformed/hostile BMP header can otherwise report implausible
// dimensions; this is well beyond any real panel and only guards the draw
// math below, not a supported image size.
constexpr int MAX_BOOT_IMAGE_DIMENSION = 4000;

bool isBootImagePath(const std::string& path) { return FsHelpers::hasBmpExtension(path); }

// Letterboxed/centered draw matching BmpViewerActivity's simple centering
// (no crop or invert options — boot has no settings surface of its own).
void drawCenteredBootBitmap(const GfxRenderer& renderer, const Bitmap& bitmap) {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  int x, y;

  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    const float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);
    if (ratio > screenRatio) {
      x = 0;
      y = static_cast<int>(std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2));
    } else {
      x = static_cast<int>(std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2));
      y = 0;
    }
  } else {
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  renderer.clearScreen();
  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0);
}

bool tryDrawCustomBootImage(const GfxRenderer& renderer, const std::string& path) {
  FsFile file;
  if (!Storage.openFileForRead("BOOT", path, file)) {
    LOG_ERR("BOOT", "Failed to open custom boot image: %s", path.c_str());
    return false;
  }

  Bitmap bitmap(file, true);
  const BmpReaderError parseResult = bitmap.parseHeaders();
  if (parseResult != BmpReaderError::Ok) {
    LOG_ERR("BOOT", "Failed to parse custom boot BMP %s: %s", path.c_str(), Bitmap::errorToString(parseResult));
    file.close();
    return false;
  }
  if (bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0 || bitmap.getWidth() > MAX_BOOT_IMAGE_DIMENSION ||
      bitmap.getHeight() > MAX_BOOT_IMAGE_DIMENSION) {
    LOG_ERR("BOOT", "Custom boot BMP %s has implausible dimensions %dx%d", path.c_str(), bitmap.getWidth(),
            bitmap.getHeight());
    file.close();
    return false;
  }

  drawCenteredBootBitmap(renderer, bitmap);
  file.close();
  return true;
}

bool tryDrawPinnedBootImage(const GfxRenderer& renderer) {
  const std::string& favorite = APP_STATE.favoriteBootImagePath;
  if (favorite.empty() || !isBootImagePath(favorite) || !Storage.exists(favorite.c_str())) return false;

  LOG_INF("BOOT", "Loading pinned boot image: %s", favorite.c_str());
  return tryDrawCustomBootImage(renderer, favorite);
}

bool tryDrawRotatingBootImage(const GfxRenderer& renderer) {
  std::string bootDir;
  if (!ImageFolderIndex::resolveBootScreenDirectory(bootDir)) return false;

  const auto pickAndDraw = [&](const bool validateBmpHeaders) {
    ImageFolderIndex::Selection selection;
    if (!ImageFolderIndex::select(bootDir, false, validateBmpHeaders, APP_STATE.recentBootImages,
                                  CrossPointState::BOOT_RECENT_COUNT, APP_STATE.recentBootPos, APP_STATE.recentBootFill,
                                  std::min(APP_STATE.recentBootFill, CrossPointState::BOOT_RECENT_COUNT), selection)) {
      return false;
    }
    if (!tryDrawCustomBootImage(renderer, selection.path)) return false;

    APP_STATE.pushRecentBoot(selection.index);
    APP_STATE.saveToFile();
    return true;
  };

  if (pickAndDraw(false)) return true;

  // A corrupt BMP shouldn't strand an otherwise valid folder on the fallback
  // logo forever; re-scan with header validation once, same recovery sleep
  // screens use for their own folder.
  return pickAndDraw(true);
}

void drawDefaultBootLogo(const GfxRenderer& renderer) {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSINK), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSINK_VERSION);
}

}  // namespace


void RetroInkBoot::drawScreen(const GfxRenderer& renderer, const int pageWidth, const int pageHeight,
                              const int eyeOffset) {
  drawRetroInkDesktop(renderer, pageWidth, pageHeight);

  const int windowWidth = std::min(350, pageWidth - 48);
  constexpr int windowHeight = 270;
  const int windowX = (pageWidth - windowWidth) / 2;
  const int windowY = std::max(30, (pageHeight - windowHeight) / 2 - 24);
  renderer.fillRect(windowX + 5, windowY + 5, windowWidth, windowHeight);
  renderer.fillRect(windowX, windowY, windowWidth, windowHeight, false);
  renderer.drawRect(windowX, windowY, windowWidth, windowHeight);
  renderer.drawRect(windowX + 3, windowY + 3, windowWidth - 6, windowHeight - 6);

  constexpr int titleHeight = 44;
  for (int y = windowY + 9; y < windowY + titleHeight - 5; y += 4) {
    renderer.drawLine(windowX + 8, y, windowX + windowWidth - 9, y);
  }
  const char* brand = tr(STR_SYSTEM6_DESKTOP);
  const int brandWidth = renderer.getTextWidth(UI_12_FONT_ID, brand);
  const int brandX = windowX + (windowWidth - brandWidth) / 2;
  renderer.fillRect(brandX - 10, windowY + 5, brandWidth + 20, titleHeight - 8, false);
  renderer.drawText(UI_12_FONT_ID, brandX, windowY + (titleHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, brand);
  renderer.drawLine(windowX + 4, windowY + titleHeight, windowX + windowWidth - 5, windowY + titleHeight);

  constexpr int macScale = 4;
  constexpr int macWidth = 24 * macScale;
  const int macX = windowX + (windowWidth - macWidth) / 2;
  drawRetroInkMac(renderer, macX, windowY + 67, macScale, eyeOffset);
  renderer.drawCenteredText(UI_10_FONT_ID, windowY + 221, tr(STR_RETROINK_BY_ALTFLOW));
}

void RetroInkBoot::drawSleepScreen(const GfxRenderer& renderer, const int pageWidth, const int pageHeight) {
  drawRetroInkDesktop(renderer, pageWidth, pageHeight);
  const int width = std::min(430, pageWidth - 40);
  constexpr int height = 210;
  const int x = (pageWidth - width) / 2;
  const int y = (pageHeight - height) / 2;
  renderer.fillRect(x + 5, y + 5, width, height);
  renderer.fillRect(x, y, width, height, false);
  renderer.drawRect(x, y, width, height);
  renderer.drawRect(x + 3, y + 3, width - 6, height - 6);

  constexpr int titleHeight = 42;
  for (int stripeY = y + 8; stripeY < y + titleHeight - 5; stripeY += 4) {
    renderer.drawLine(x + 8, stripeY, x + width - 9, stripeY);
  }
  const char* title = tr(STR_RETROINK_SLEEP_TITLE);
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title);
  const int titleX = x + (width - titleWidth) / 2;
  renderer.fillRect(titleX - 10, y + 5, titleWidth + 20, titleHeight - 8, false);
  renderer.drawText(UI_12_FONT_ID, titleX, y + (titleHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, title);
  renderer.drawLine(x + 4, y + titleHeight, x + width - 5, y + titleHeight);

  constexpr int macScale = 3;
  drawRetroInkMac(renderer, x + 38, y + 74, macScale, 0, true);
  renderer.drawText(UI_10_FONT_ID, x + 104, y + 68, "z");
  renderer.drawText(UI_10_FONT_ID, x + 122, y + 51, "Z");
  renderer.drawText(UI_12_FONT_ID, x + 150, y + 76, tr(STR_RETROINK_RESTING));
  renderer.drawText(UI_12_FONT_ID, x + 150, y + 112, tr(STR_RETROINK_BETWEEN_CHAPTERS));
}

void RetroInkBoot::drawFunnySleepScreen(const GfxRenderer& renderer, const int pageWidth, const int pageHeight,
                                        const uint8_t sleepMode) {
  const char* title = tr(STR_SLEEP_SYSTEM_NAP);
  const char* line1 = tr(STR_SLEEP_NAP_LINE1);
  const char* line2 = tr(STR_SLEEP_NAP_LINE2);
  if (sleepMode == CrossPointSettings::RETROINK_ERROR_404_SLEEP) {
    title = tr(STR_SLEEP_ERROR_404);
    line1 = tr(STR_SLEEP_404_LINE1);
    line2 = tr(STR_SLEEP_404_LINE2);
  } else if (sleepMode == CrossPointSettings::RETROINK_INSERT_BOOKMARK_SLEEP) {
    title = tr(STR_SLEEP_INSERT_BOOKMARK);
    line1 = tr(STR_SLEEP_BOOKMARK_LINE1);
    line2 = tr(STR_SLEEP_BOOKMARK_LINE2);
  }

  drawRetroInkDesktop(renderer, pageWidth, pageHeight);
  const int width = std::min(430, pageWidth - 40);
  constexpr int height = 270;
  const int x = (pageWidth - width) / 2;
  const int y = (pageHeight - height) / 2;
  renderer.fillRect(x + 5, y + 5, width, height);
  renderer.fillRect(x, y, width, height, false);
  renderer.drawRect(x, y, width, height);
  renderer.drawRect(x + 3, y + 3, width - 6, height - 6);
  constexpr int titleHeight = 42;
  for (int stripeY = y + 8; stripeY < y + titleHeight - 5; stripeY += 4)
    renderer.drawLine(x + 8, stripeY, x + width - 9, stripeY);
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title);
  const int titleX = x + (width - titleWidth) / 2;
  renderer.fillRect(titleX - 10, y + 5, titleWidth + 20, titleHeight - 8, false);
  renderer.drawText(UI_12_FONT_ID, titleX, y + (titleHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, title);
  renderer.drawLine(x + 4, y + titleHeight, x + width - 5, y + titleHeight);

  constexpr int iconScale = 3;
  const int iconX = x + (width - 24 * iconScale) / 2;
  if (sleepMode == CrossPointSettings::RETROINK_INSERT_BOOKMARK_SLEEP) {
    drawRetroInkFloppy(renderer, iconX, y + 62, iconScale);
  } else {
    drawRetroInkMac(renderer, iconX, y + 62, iconScale, 0, sleepMode == CrossPointSettings::RETROINK_SYSTEM_NAP_SLEEP);
  }
  renderer.drawCenteredText(UI_12_FONT_ID, y + 175, line1, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, y + 214, line2);
}

void RetroInkBoot::drawChargingScreen(const GfxRenderer& renderer, const int pageWidth, const int pageHeight,
                                      const int batteryPercent, const int lineIndex,
                                      const int realBatteryPercent) {
  drawRetroInkDesktop(renderer, pageWidth, pageHeight);
  const int width = std::min(430, pageWidth - 40);
  constexpr int height = 270;
  const int x = (pageWidth - width) / 2;
  const int y = (pageHeight - height) / 2;
  renderer.fillRect(x + 5, y + 5, width, height);
  renderer.fillRect(x, y, width, height, false);
  renderer.drawRect(x, y, width, height);
  renderer.drawRect(x + 3, y + 3, width - 6, height - 6);

  constexpr int titleHeight = 42;
  for (int stripeY = y + 8; stripeY < y + titleHeight - 5; stripeY += 4) {
    renderer.drawLine(x + 8, stripeY, x + width - 9, stripeY);
  }
  const char* title = tr(STR_RETROINK_CHARGING_TITLE);
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title);
  const int titleX = x + (width - titleWidth) / 2;
  renderer.fillRect(titleX - 10, y + 5, titleWidth + 20, titleHeight - 8, false);
  renderer.drawText(UI_12_FONT_ID, titleX, y + (titleHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, title);

  // Real gauge percentage, knocked out of the pinstripes on the right side
  // of the title bar (same pattern as the Library shelf position counter).
  char percentBuf[8];
  snprintf(percentBuf, sizeof(percentBuf), "%d%%", std::clamp(realBatteryPercent, 0, 100));
  const int percentWidth = renderer.getTextWidth(SMALL_FONT_ID, percentBuf, EpdFontFamily::BOLD);
  renderer.fillRect(x + width - percentWidth - 24, y + 7, percentWidth + 16, titleHeight - 12, false);
  renderer.drawText(SMALL_FONT_ID, x + width - percentWidth - 16,
                    y + (titleHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2, percentBuf, true,
                    EpdFontFamily::BOLD);

  renderer.drawLine(x + 4, y + titleHeight, x + width - 5, y + titleHeight);

  constexpr int macScale = 3;
  const int groupWidth = 24 * macScale + 24 + 54 + 12;
  const int macX = x + (width - groupWidth) / 2;
  const int macY = y + 62;
  drawRetroInkMac(renderer, macX, macY, macScale);

  // Battery glyph beside the Mac: outline, a small terminal nub, and a fill
  // bar that cycles with batteryPercent (cosmetic, not the real gauge, which
  // already has its own header indicator).
  const int battX = macX + 24 * macScale + 24;
  constexpr int battW = 54;
  constexpr int battH = 26;
  const int battY = macY + 15 * macScale - battH / 2;
  renderer.drawRect(battX, battY, battW, battH);
  renderer.fillRect(battX + battW, battY + battH / 2 - 4, 5, 8);
  const int fillW = std::clamp((battW - 6) * batteryPercent / 100, 0, battW - 6);
  if (fillW > 0) renderer.fillRect(battX + 3, battY + 3, fillW, battH - 6);

  // Lightning bolt straddling the top edge of the battery, matching the
  // classic charge-in-progress glyph.
  const int boltCx = battX + battW / 2;
  renderer.drawLine(boltCx + 5, battY - 12, boltCx - 5, battY + 3);
  renderer.drawLine(boltCx - 5, battY + 3, boltCx + 3, battY + 3);
  renderer.drawLine(boltCx + 3, battY + 3, boltCx - 5, battY + 18);

  const char* lines1[] = {tr(STR_RETROINK_CHARGING_LINE1_A), tr(STR_RETROINK_CHARGING_LINE1_B),
                          tr(STR_RETROINK_CHARGING_LINE1_C)};
  const char* line1 = lines1[((lineIndex % 3) + 3) % 3];
  renderer.drawCenteredText(UI_12_FONT_ID, y + 175, line1, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, y + 214, tr(STR_RETROINK_CHARGING_LINE2));
}

void BootActivity::onEnter() {
  Activity::onEnter();

  const bool drewCustom =
      SETTINGS.customBootscreenEnabled && (tryDrawPinnedBootImage(renderer) || tryDrawRotatingBootImage(renderer));
  if (drewCustom) {
    renderer.displayBuffer();
    return;
  }
  if (SETTINGS.uiTheme == CrossPointSettings::SYSTEM6) {
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();
    // Three restrained frames make the Macintosh glance around without storing
    // animation art. Each blocking e-ink update leaves a stable visible pose.
#ifdef SIMULATOR
    RetroInkBoot::drawScreen(renderer, pageWidth, pageHeight);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
#else
    constexpr int eyePositions[] = {-2, 2, 0};
    for (const int eyePosition : eyePositions) {
      renderer.clearScreen();
      RetroInkBoot::drawScreen(renderer, pageWidth, pageHeight, eyePosition);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      if (eyePosition != 0) delay(140);
    }
#endif
    return;
  }
  drawDefaultBootLogo(renderer);
  renderer.displayBuffer();
}
