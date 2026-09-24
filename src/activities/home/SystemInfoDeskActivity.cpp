#include "SystemInfoDeskActivity.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SystemInfoDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  storageState_ = StorageState::NotStarted;
  requestUpdate();
}

void SystemInfoDeskActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void SystemInfoDeskActivity::loop() {
  // Checked first, and unconditionally, on every tick -- including the one
  // right after the "Calculating..." placeholder first appears -- so Back
  // always works even if the slow storage read hasn't run yet.
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (storageState_ == StorageState::Awaiting) {
    char storageText[48];
#ifndef SIMULATOR
    const uint64_t totalBytes = Storage.totalBytes();
    const uint64_t usedBytes = Storage.usedBytes();
    const uint64_t freeMb = totalBytes > usedBytes ? (totalBytes - usedBytes) / (1024ULL * 1024ULL) : 0;
    const uint64_t totalMb = totalBytes / (1024ULL * 1024ULL);
    std::snprintf(storageText, sizeof(storageText), tr(STR_SYSINFO_STORAGE_FREE), static_cast<unsigned long>(freeMb),
                 static_cast<unsigned long>(totalMb));
#else
    // The simulator's vendored HalStorage has no total/used byte accessors.
    std::snprintf(storageText, sizeof(storageText), "--");
#endif
    std::snprintf(storageText_, sizeof(storageText_), "%s", storageText);
    storageState_ = StorageState::Ready;
    requestUpdate();
  }
}

void SystemInfoDeskActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_SYSTEM_INFO), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_SYSTEM_INFO), nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;
  // No frame drawn here: System6Theme::drawHeader already draws the window
  // body all the way down to the button hints, grow box and all. Drawing
  // another one on top stacked a second window inside the first -- most
  // visible where its corner cut across the grow box.
  const int left = frameX + 20;
  const int width = frameW - 40;
  const int font = UI_10_FONT_ID;
  const int contentTop = top + 20;
  const int rowHeight = std::max(36, (bottom - top - 40) / 4);

  const uint16_t batteryPercent = powerManager.getBatteryPercentage();
  char batteryText[16];
  std::snprintf(batteryText, sizeof(batteryText), "%u%%", static_cast<unsigned>(batteryPercent));

  const char* storageValue = storageState_ == StorageState::Ready ? storageText_ : tr(STR_SYSINFO_CALCULATING);

  struct Row {
    const char* label;
    const char* value;
  };
  const Row rows[] = {
      {tr(STR_SYSINFO_FIRMWARE), CROSSINK_VERSION},
      {tr(STR_SYSINFO_STORAGE), storageValue},
      {tr(STR_SYSINFO_BATTERY), batteryText},
  };
  constexpr int rowCount = sizeof(rows) / sizeof(rows[0]);

  for (int i = 0; i < rowCount; ++i) {
    const int y = contentTop + i * rowHeight;
    renderer.drawText(font, left, y, rows[i].label, true, EpdFontFamily::BOLD);
    const int valueWidth = renderer.getTextWidth(font, rows[i].value);
    renderer.drawText(font, left + width - valueWidth, y, rows[i].value);
    if (i + 1 < rowCount) renderer.drawLine(left, y + rowHeight - 10, left + width, y + rowHeight - 10);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();

  // The placeholder is now actually on the panel -- safe to arm the slow
  // read for the next loop() tick, which checks Back before it ever runs.
  if (storageState_ == StorageState::NotStarted) storageState_ = StorageState::Awaiting;
}
