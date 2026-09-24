#include "DeskAccessoriesActivity.h"

#include <I18n.h>

#include <algorithm>
#include <memory>

#include "../reader/RetroInkFocusDeskActivity.h"
#include "ClockDeskActivity.h"
#include "DeskCalendarActivity.h"
#include "EarthPhaseDeskActivity.h"
#include "MoonPhaseDeskActivity.h"
#include "PuzzleDeskActivity.h"
#include "SystemInfoDeskActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/themes/BaseTheme.h"
#include "fontIds.h"

void DeskAccessoriesActivity::onEnter() {
  Activity::onEnter();
  selected_ = 0;
  requestUpdate();
}

void DeskAccessoriesActivity::activate() {
  switch (static_cast<DeskAction>(selected_)) {
    case DeskAction::FocusTimer:
      startActivityForResult(
          std::make_unique<RetroInkFocusDeskActivity>(renderer, mappedInput, RetroInkFocusDeskActivity::Entry::Home),
          [this](const ActivityResult&) { requestUpdate(); });
      break;
    case DeskAction::MoonPhase:
      startActivityForResult(std::make_unique<MoonPhaseDeskActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
      break;
    case DeskAction::Earth:
      startActivityForResult(std::make_unique<EarthPhaseDeskActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
      break;
    case DeskAction::Clock:
      startActivityForResult(std::make_unique<ClockDeskActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
      break;
    case DeskAction::Puzzle:
      startActivityForResult(std::make_unique<PuzzleDeskActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
      break;
    case DeskAction::Calendar:
      startActivityForResult(std::make_unique<DeskCalendarActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
      break;
    case DeskAction::SystemInfo:
      startActivityForResult(std::make_unique<SystemInfoDeskActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
      break;
  }
}

void DeskAccessoriesActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int tapped = -1;
  if (mappedInput.wasItemTapped(tapped) && tapped >= 0 && tapped < kItemCount) {
    selected_ = tapped;
    activate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate();
    return;
  }
  buttonNavigator_.onPreviousPress([this] {
    selected_ = ButtonNavigator::previousIndex(selected_, kItemCount);
    requestUpdate();
  });
  buttonNavigator_.onNextPress([this] {
    selected_ = ButtonNavigator::nextIndex(selected_, kItemCount);
    requestUpdate();
  });
}

void DeskAccessoriesActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const char* labels[kItemCount] = {tr(STR_FOCUS_SESSION), tr(STR_MOON_PHASE),    tr(STR_EARTH_PHASE),
                                    tr(STR_DESK_CLOCK),    tr(STR_PUZZLE),        tr(STR_DESK_CALENDAR),
                                    tr(STR_SYSTEM_INFO)};
  const UIIcon icons[kItemCount] = {Hourglass,  MoonPhaseIcon, EarthPhaseIcon, ClockIcon,
                                   PuzzleIcon, CalendarIcon,  SystemInfoIcon};

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_DESK_ACCESSORIES), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_DESK_ACCESSORIES), nullptr, false);
  }

  // A plain row list inside this screen's own frame, matching the other
  // desk-accessory screens -- not GUI.drawButtonMenu, which bakes in its own
  // secondary "MENU" pinstriped title box (meant for sharing a screen with
  // Home's cover carousel above it). Stacked under this screen's own header,
  // that second title bar read as a nested/redundant window.
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;
  // No frame drawn here: System6Theme::drawHeader already draws the window
  // body all the way down to the button hints, grow box and all. Drawing
  // another one on top stacked a second window inside the first -- most
  // visible where its corner cut across the grow box.

  const int rowX = frameX + 14;
  const int rowW = frameW - 28;
  const int rowHeight = std::min(64, (bottom - top - 20) / kItemCount);
  const int rowTop = top + 10;
  const int iconFont = UI_10_FONT_ID;
  for (int i = 0; i < kItemCount; ++i) {
    const int y = rowTop + i * rowHeight;
    const int rowH = rowHeight - 6;
    const bool active = i == selected_;
    if (active) renderer.fillRect(rowX, y, rowW, rowH, true);
    GUI.drawMenuIcon(renderer, icons[i], rowX + 10, y + (rowH - 26) / 2, !active);
    renderer.drawText(iconFont, rowX + 46, y + (rowH - renderer.getLineHeight(iconFont)) / 2, labels[i], !active);
    TouchRegistry::getInstance().add(Rect{rowX, y, rowW, rowH}, i, TouchRegistry::Item);
  }

  const auto btnLabels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
  renderer.displayBuffer();
}
