#include "FrontlightPanelActivity.h"

#include <CrossInkHalFrontlight.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "activities/home/BookActions.h"
#include "activities/settings/SettingsActivity.h"
#include "components/DrawerHandle.h"
#include "components/HeaderDate.h"
#include "components/RockerSwitch.h"
#include "components/TouchRegistry.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "components/icons/frontlightHeaderIcons.h"
#include "components/icons/listIcons.h"
#include "components/icons/readingStatsIcons.h"
#include "components/icons/tablerIcons.h"
#include "components/icons/touchscreenStateIcons.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_BRIGHTNESS = 1;
constexpr fui::ActionId ACTION_WARMTH = 2;
constexpr fui::ActionId ACTION_TOGGLE = 3;
constexpr fui::ActionId ACTION_BRIGHTNESS_STEP = 4;
constexpr fui::ActionId ACTION_WARMTH_STEP = 5;
constexpr fui::ActionId ACTION_QUICK = 6;
constexpr fui::ActionId ACTION_DISMISS = 7;
constexpr int BRIGHTNESS_STEP = 5;
constexpr int FINE_STEP = 1;
constexpr int HEADER_ICON_SIZE = 24;
constexpr int HEADER_BUTTON_WIDTH = 64;
constexpr int HEADER_CONTENT_BOTTOM_GAP = 8;
constexpr int ACTION_BAR_HEIGHT = 58;

// System 6 look for the drawer: boxed keys, a scroll-bar slider and an I/O
// switch, drawn straight to the renderer so they match the title bar above.
bool isSystem6() { return SETTINGS.uiTheme == CrossPointSettings::UI_THEME::SYSTEM6; }

void drawArrowBox(GfxRenderer& r, const fui::Rect& box, const int direction) {
  r.fillRect(box.x, box.y, box.width, box.height, false);
  r.drawRect(box.x, box.y, box.width, box.height);
  const int cx = box.x + box.width / 2;
  const int cy = box.y + box.height / 2;
  for (int k = 0; k <= 7; ++k) {
    const int x = direction < 0 ? cx - 5 + k : cx + 5 - k;
    r.drawLine(x, cy - k, x, cy + k);
  }
}

// Pixel-style glyphs for the drawer keys, drawn with lines and boxes like the
// Home screen's System 6 icons. `ink` is true for black on a white key.
enum class KeyGlyph { Stats, Sync, Dark, Prefs, Touch };

void drawKeyGlyph(GfxRenderer& r, const int cx, const int cy, const KeyGlyph glyph, const bool ink) {
  const int x = cx - 14;
  const int y = cy - 12;
  const int cx_ = cx;
  const int cy_ = cy;
  switch (glyph) {
    case KeyGlyph::Stats: {
      // Bar chart on a baseline.
      r.fillRect(x + 1, y + 14, 4, 9, ink);
      r.fillRect(x + 8, y + 8, 4, 15, ink);
      r.fillRect(x + 15, y + 11, 4, 12, ink);
      r.fillRect(x + 22, y + 2, 4, 21, ink);
      r.drawLine(x, y + 24, x + 27, y + 24, ink);
      r.drawLine(x, y + 25, x + 27, y + 25, ink);
      break;
    }
    case KeyGlyph::Sync: {
      // Two opposing arrows.
      r.fillRect(x + 1, y + 6, 20, 3, ink);
      for (int k = 0; k <= 6; ++k) r.drawLine(x + 21 + k, y + 7 - (6 - k), x + 21 + k, y + 7 + (6 - k), ink);
      r.fillRect(x + 6, y + 17, 20, 3, ink);
      for (int k = 0; k <= 6; ++k) r.drawLine(x + 5 - k, y + 18 - (6 - k), x + 5 - k, y + 18 + (6 - k), ink);
      break;
    }
    case KeyGlyph::Dark: {
      // The classic "contrast" tile: outlined square, left half solid.
      r.drawRect(x + 2, y, 24, 24, ink);
      r.drawRect(x + 3, y + 1, 22, 22, ink);
      r.fillRect(x + 2, y, 12, 24, ink);
      break;
    }
    case KeyGlyph::Prefs: {
      // Control-panel sliders.
      for (int row = 0; row < 3; ++row) {
        const int ly = y + 3 + row * 9;
        r.fillRect(x + 1, ly + 1, 26, 2, ink);
        const int knobX = x + 3 + ((row * 11 + 5) % 17);
        r.fillRect(knobX, ly - 2, 7, 8, ink);
      }
      break;
    }
    case KeyGlyph::Touch: {
      // Touch: a fingertip contact with ripples spreading out from it.
      r.fillRect(cx_ - 3, cy_ - 3, 7, 7, ink);
      for (const int radius : {9, 14}) {
        r.drawArc(radius, cx_, cy_, 1, 1, 2, ink);
        r.drawArc(radius, cx_, cy_, 1, -1, 2, ink);
        r.drawArc(radius, cx_, cy_, -1, 1, 2, ink);
        r.drawArc(radius, cx_, cy_, -1, -1, 2, ink);
      }
      break;
    }
  }
}

// A key like the ones on System 6's bottom bar: white face, black outline and
// a drop shadow. A key showing an active state is drawn solid black.
void drawKeyBox(GfxRenderer& r, const fui::Rect& box, const char* label, const bool active) {
  r.fillRect(box.x + 3, box.y + 3, box.width, box.height);
  r.fillRect(box.x, box.y, box.width, box.height, active);
  r.drawRect(box.x, box.y, box.width, box.height);
  const int font = UI_10_FONT_ID;
  const int textW = r.getTextWidth(font, label);
  const int textX = box.x + std::max(2, (box.width - textW) / 2);
  const int textY = box.y + (box.height - r.getLineHeight(font)) / 2;
  r.drawText(font, textX, textY, label, !active);
}

void drawIconKey(GfxRenderer& r, const fui::Rect& box, const KeyGlyph glyph, const bool active) {
  r.fillRect(box.x + 3, box.y + 3, box.width, box.height);
  r.fillRect(box.x, box.y, box.width, box.height, active);
  r.drawRect(box.x, box.y, box.width, box.height);
  drawKeyGlyph(r, box.x + box.width / 2, box.y + box.height / 2, glyph, !active);
}

// The System 6 drawer floats over the current screen like an alert window, a
// little below the top edge so the desktop shows around it.
constexpr int kToastTop = 8;

// Height of the title bar region the drawer content starts below. System 6's
// bar is a fixed 40 px window title, so the drawer tucks right under it.
int panelHeaderBand(const ThemeMetrics& metrics, const MappedInputManager& input) {
  return isSystem6() ? kToastTop + 46 : TouchHeaderBackButton::height(metrics, input);
}

uint8_t percentFromPermille(const int16_t permille) {
  int value = (static_cast<int>(permille) * 100 + 500) / 1000;
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  return static_cast<uint8_t>(value);
}

fui::SheetProps frontlightSheetProps() {
  fui::SheetProps sheet;
  sheet.anchor = fui::SheetEdge::Top;
  sheet.dismissAction = ACTION_DISMISS;
  sheet.radius = 0;
  return sheet;
}
}  // namespace

FrontlightPanelActivity::FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 FrontlightPanelContext context)
    : Activity("FrontlightPanel", renderer, mappedInput),
      context(std::move(context)),
      drawerState(this->context.drawerState),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void FrontlightPanelActivity::onEnter() {
  Activity::onEnter();

  // The HAL is the live source of truth; SETTINGS only mirrors it for boot.
  brightness = Frontlight.brightness();
  warmth = Frontlight.warmth();
  lightOn = Frontlight.isOn();
  initialInversion = SETTINGS.screenInverted;
  initialTouchscreenDisabled = SETTINGS.disableReaderTouchscreen;
  pendingTouchscreenDisabled = initialTouchscreenDisabled;
  mappedInput.setReaderTouchscreenOverride(true);

  uiReady = false;
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_BRIGHTNESS, &FrontlightPanelActivity::onBrightnessEvent, this);
  app.on(ACTION_WARMTH, &FrontlightPanelActivity::onWarmthEvent, this);
  app.on(ACTION_TOGGLE, &FrontlightPanelActivity::onToggleEvent, this);
  app.on(ACTION_BRIGHTNESS_STEP, &FrontlightPanelActivity::onBrightnessStepEvent, this);
  app.on(ACTION_WARMTH_STEP, &FrontlightPanelActivity::onWarmthStepEvent, this);
  app.on(ACTION_QUICK, &FrontlightPanelActivity::onQuickActionEvent, this);
  app.on(ACTION_DISMISS, &FrontlightPanelActivity::onDismissEvent, this);
  app.setScreen(&FrontlightPanelActivity::panelScreen, this);
  prepareReaderDetailsLayout();
  requestUpdate();
}

void FrontlightPanelActivity::onExit() {
  // Debounced persistence: one SPIFFS write on close, never per slider tick.
  const bool inversionChanged = initialInversion != static_cast<bool>(SETTINGS.screenInverted);
  const bool touchscreenChanged = initialTouchscreenDisabled != pendingTouchscreenDisabled;
  SETTINGS.disableReaderTouchscreen = pendingTouchscreenDisabled ? 1 : 0;
  const bool frontlightChanged =
      !context.showReaderDetails && (SETTINGS.frontlightBrightness != brightness ||
                                     SETTINGS.frontlightWarmth != warmth || SETTINGS.frontlightOn != (lightOn ? 1 : 0));
  if (frontlightChanged || inversionChanged || touchscreenChanged) {
    if (!context.showReaderDetails) {
      SETTINGS.frontlightBrightness = brightness;
      SETTINGS.frontlightWarmth = warmth;
      SETTINGS.frontlightOn = lightOn ? 1 : 0;
    }
    // The drawer can be opened over a dictionary or another reader child.
    // Find the owning reader so its per-book values stay out of the global file.
    activityManager.persistGlobalSettings();
  }
  mappedInput.setReaderTouchscreenOverride(false);
  Activity::onExit();
}

void FrontlightPanelActivity::onBrightnessEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  if (event.dragPermille < 0) return;
  self->brightness = percentFromPermille(event.dragPermille);
  Frontlight.setBrightness(self->brightness);
  // Adjusting brightness while off is an obvious "I want light" intent.
  if (!self->lightOn) {
    self->lightOn = true;
    Frontlight.setOn(true);
  }
}

void FrontlightPanelActivity::onWarmthEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  if (event.dragPermille < 0) return;
  self->warmth = percentFromPermille(event.dragPermille);
  Frontlight.setWarmth(self->warmth);
}

void FrontlightPanelActivity::onBrightnessStepEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->adjustBrightness(event.value * FINE_STEP);
}

void FrontlightPanelActivity::onWarmthStepEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->adjustWarmth(event.value * FINE_STEP);
}

void FrontlightPanelActivity::onToggleEvent(const fui::ActionEvent&, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->toggleLight();
}

void FrontlightPanelActivity::onQuickActionEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->activateQuickAction(event.value);
}

void FrontlightPanelActivity::onDismissEvent(const fui::ActionEvent&, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->close();
}

void FrontlightPanelActivity::adjustBrightness(const int delta) {
  int next = static_cast<int>(brightness) + delta;
  if (next < 0) next = 0;
  if (next > 100) next = 100;
  if (next == brightness) return;
  brightness = static_cast<uint8_t>(next);
  Frontlight.setBrightness(brightness);
  if (!lightOn) {
    lightOn = true;
    Frontlight.setOn(true);
  }
  requestUpdate();
}

void FrontlightPanelActivity::adjustWarmth(const int delta) {
  int next = static_cast<int>(warmth) + delta;
  if (next < 0) next = 0;
  if (next > 100) next = 100;
  if (next == warmth) return;
  warmth = static_cast<uint8_t>(next);
  Frontlight.setWarmth(warmth);
  requestUpdate();
}

void FrontlightPanelActivity::toggleLight() {
  lightOn = !lightOn;
  Frontlight.setOn(lightOn);
  requestUpdate();
}

void FrontlightPanelActivity::toggleReaderTouchscreen() {
  pendingTouchscreenDisabled = !pendingTouchscreenDisabled;
  {
    RenderLock lock;
    BookActions::drawToast(renderer,
                           pendingTouchscreenDisabled ? tr(STR_TOUCHSCREEN_DISABLED) : tr(STR_TOUCHSCREEN_ENABLED));
  }
  delay(1000);
  requestUpdate();
}

void FrontlightPanelActivity::close() { finish(); }

void FrontlightPanelActivity::openReadingStats() {
  if (!context.readingStatsActivity && context.sourceActivity) {
    context.readingStatsActivity = context.sourceActivity->createFrontlightReadingStatsActivity();
  }
  if (!context.readingStatsActivity) return;
  startActivityForResult(std::move(context.readingStatsActivity), [this](const ActivityResult&) { close(); });
}

void FrontlightPanelActivity::openGlobalSettings() {
  // A drawer over Settings must not end the outer screen's edit session.
  const bool startedGlobalEdit = activityManager.beginGlobalSettingsEdit();
  auto settings = makeUniqueNoThrow<SettingsActivity>(renderer, mappedInput, true, true);
  if (!settings) {
    LOG_ERR("LIGHT", "OOM opening Settings from frontlight panel");
    if (startedGlobalEdit) activityManager.endGlobalSettingsEdit();
    return;
  }
  startActivityForResult(std::move(settings), [this, startedGlobalEdit](const ActivityResult&) {
    if (startedGlobalEdit) activityManager.endGlobalSettingsEdit();
    close();
  });
}

void FrontlightPanelActivity::openSyncDialog() {
  static constexpr std::array<StrId, 3> OPTIONS = {StrId::STR_SYNC_PROGRESS, StrId::STR_NEARBY_POSITION_SYNC,
                                                   StrId::STR_SEND_NEARBY_BOOK};
  drawerState.syncDialogOpen = true;
  const bool canSyncBookProgress = context.activeEpub;
  const bool canSendBook = !context.bookPath.empty();
  optionPopup.show(StrId::STR_SYNC_AND_TRANSFER, OPTIONS.data(), OPTIONS.size(), canSyncBookProgress ? 0 : 2,
                   [this](const int index) {
                     drawerState.syncDialogOpen = false;
                     FrontlightPanelResult panelResult;
                     panelResult.state = drawerState;
                     panelResult.activeEpub = context.activeEpub;
                     panelResult.bookPath = context.bookPath;
                     if (index == 0) panelResult.action = FrontlightPanelAction::SyncProgress;
                     if (index == 1) panelResult.action = FrontlightPanelAction::NearbyPositionSync;
                     if (index == 2) panelResult.action = FrontlightPanelAction::SendNearbyBook;
                     setResult(ActivityResult(std::move(panelResult)));
                     finish();
                   });
  optionPopup.setDisabledOptions({!canSyncBookProgress, !canSyncBookProgress, !canSendBook});
  optionPopup.setCancelCallback([this] { closeSyncDialog(); });
  requestUpdate();
}

void FrontlightPanelActivity::closeSyncDialog() {
  drawerState.syncDialogOpen = false;
  close();
}

void FrontlightPanelActivity::activateQuickAction(const int index) {
  if (index < 0 || index >= 5) return;
  drawerState.selectedAction = static_cast<int8_t>(index);
  switch (index) {
    case 0:
      openReadingStats();
      return;
    case 1:
      openSyncDialog();
      return;
    case 2:
      SETTINGS.screenInverted = SETTINGS.screenInverted ? 0 : 1;
      display.setInverted(SETTINGS.screenInverted != 0);
      requestUpdate();
      return;
    case 3:
      openGlobalSettings();
      return;
    case 4:
      toggleReaderTouchscreen();
      return;
  }
}

bool FrontlightPanelActivity::handleHomeGesture() {
  if (context.activeReaderBook) {
    activityManager.goHome();
    return true;
  }
  close();
  return true;
}

void FrontlightPanelActivity::loop() {
  if (optionPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  // A power-button shortcut can change the light globally while this panel is
  // open. Keep the panel's state and eventual persistence in sync.
  if (!context.showReaderDetails && lightOn != Frontlight.isOn()) {
    lightOn = Frontlight.isOn();
    requestUpdate();
  }

  const Rect homeButton = homeButtonRect();
  if (showsHomeButton() &&
      mappedInput.wasTapInRect(homeButton.x, homeButton.y, homeButton.width, homeButton.height)) {
    activityManager.goHome();
    return;
  }

  if (DrawerHandle::wasDismissSwipe(mappedInput, drawerHandleRect, fui::SheetEdge::Top)) {
    close();
    return;
  }
  fui::InputSnapshot snap{};
  if (uiReady) {
    snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchHeld || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) {
        if (event.dragPermille >= 0) draggingSlider = true;
        return;
      }
    }
    if (draggingSlider) {
      // Drag ended (possibly off the slider): swallow the release's swipe so
      // it can't double as the left-edge back gesture and close the panel.
      if (!snap.touchHeld) draggingSlider = false;
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    close();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!context.showReaderDetails) toggleLight();
    return;
  }

  if (context.showReaderDetails) return;

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left},
                                       [this] { adjustBrightness(-BRIGHTNESS_STEP); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right},
                                       [this] { adjustBrightness(BRIGHTNESS_STEP); });
}

Rect FrontlightPanelActivity::homeButtonRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return Rect{renderer.getScreenWidth() - HEADER_BUTTON_WIDTH, metrics.topPadding, HEADER_BUTTON_WIDTH,
              TouchHeaderBackButton::height(metrics, mappedInput)};
}

bool FrontlightPanelActivity::showsHomeButton() const {
  return context.activeReaderBook &&
         !(SETTINGS.uiTheme == CrossPointSettings::UI_THEME::SYSTEM6 && mappedInput.hasHomeKey());
}

int FrontlightPanelActivity::computePanelBottom() const {
  // Mirror buildPanelScreen's takeTop/spacer sequence so the frame, content
  // margin, and dismiss threshold land on the same edge.
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto tokens = uiThemeTokens(uiTarget);
  const int16_t lh = uiTarget.lineHeight(tokens.bodyText.font);
  int y = metrics.topPadding + panelHeaderBand(metrics, mappedInput);
  if (context.showReaderDetails) {
    const int16_t titleLh = uiTarget.lineHeight(tokens.titleText.font);
    y += tokens.spaceLg * 2;
    y += titleLh * static_cast<int>(readerTitleLines.size());
    if (!readerTitleLines.empty()) y += tokens.spaceLg;
    if (!context.bookDetails.chapter.empty() || !context.bookDetails.author.empty()) {
      y += lh + tokens.spaceLg;
    }
    y += lh;
    y += tokens.spaceLg * 3 + ACTION_BAR_HEIGHT;
    const auto sheet = frontlightSheetProps();
    return y + DrawerHandle::bandHeight(sheet);
  }
  y += tokens.spaceLg;                     // leading spacer
  y += tokens.rowHeight + tokens.spaceSm;  // brightness label + frontlight toggle row
  y += tokens.rowHeight + tokens.spaceLg;  // brightness slider
  if (Frontlight.hasColorTemperature()) {
    y += lh + tokens.spaceSm + tokens.rowHeight + tokens.spaceLg;  // warmth label + slider
  }
  y += tokens.spaceLg + ACTION_BAR_HEIGHT;  // trailing padding + actions
  const auto sheet = frontlightSheetProps();
  return y + DrawerHandle::bandHeight(sheet);
}

void FrontlightPanelActivity::prepareReaderDetailsLayout() {
  readerTitleLines.clear();
  if (!context.showReaderDetails || context.bookDetails.title.empty()) return;
  const auto tokens = uiThemeTokens(uiTarget);
  const int16_t sideInset = static_cast<int16_t>(tokens.spaceLg * 2);
  const int width = std::max<int>(1, renderer.getScreenWidth() - sideInset * 2);
  readerTitleLines =
      renderer.wrappedText(UI_12_FONT_ID, context.bookDetails.title.c_str(), width, 2, EpdFontFamily::BOLD);
}

void FrontlightPanelActivity::drawReaderDetails(fui::Screen<20>& screen) {
  const auto& theme = screen.theme();
  const fui::Insets sideInset{0, static_cast<int16_t>(theme.spaceLg * 2), 0, static_cast<int16_t>(theme.spaceLg * 2)};
  screen.spacer(static_cast<int16_t>(theme.spaceLg * 2));

  fui::TextStyle title = theme.titleText;
  title.bold = true;
  title.align = fui::TextAlign::Center;
  const int16_t titleLh = screen.target().lineHeight(title.font);
  for (const std::string& line : readerTitleLines) {
    screen.target().text(screen.takeTop(titleLh).inset(sideInset), line.c_str(), title);
  }
  if (!readerTitleLines.empty()) screen.spacer(theme.spaceLg);

  const char* subtitle =
      !context.bookDetails.chapter.empty() ? context.bookDetails.chapter.c_str() : context.bookDetails.author.c_str();
  fui::TextStyle body = theme.bodyText;
  body.align = fui::TextAlign::Center;
  const int16_t bodyLh = screen.target().lineHeight(body.font);
  if (subtitle[0] != '\0') {
    const std::string visible =
        renderer.truncatedText(UI_10_FONT_ID, subtitle, screen.body().width - sideInset.left - sideInset.right);
    screen.target().text(screen.takeTop(bodyLh, theme.spaceLg).inset(sideInset), visible.c_str(), body);
  }

  char progress[48];
  std::snprintf(progress, sizeof(progress), "%d%% %s", context.bookDetails.progressPercent, tr(STR_COMPLETE));
  screen.target().text(screen.takeTop(bodyLh).inset(sideInset), progress, body);
  screen.spacer(static_cast<int16_t>(theme.spaceLg * 3));
}

void FrontlightPanelActivity::panelScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->buildPanelScreen(screen);
}

void FrontlightPanelActivity::buildPanelScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  // System 6 paints its window (and title bar) first and wipes everything under
  // it, so the sheet starts at the title bar's lower edge and is drawn as a
  // window of the same width: framed, shadowed, and inside the desktop margin.
  const bool framed = SETTINGS.uiTheme == CrossPointSettings::UI_THEME::SYSTEM6;
  const int16_t sheetTop = framed ? static_cast<int16_t>(metrics.topPadding + kToastTop + 38) : 0;
  const int16_t sideMargin = framed ? 8 : 0;
  const fui::Rect sheetRect{sideMargin, sheetTop, static_cast<int16_t>(renderer.getScreenWidth() - sideMargin * 2),
                            static_cast<int16_t>(panelBottom - sheetTop)};
  fui::SheetProps sheet = frontlightSheetProps();
  if (framed) {
    sheet.ruleWidth = 0;  // the frame below replaces the plain bottom rule
    sheet.grabber = fui::Paint::solid(fui::Color::White);  // replaced by a title-bar tab
    // Drop shadow first, so the sheet's own fill sits on top of it.
    renderer.fillRect(sheetRect.x + 3, sheetRect.y - 38 + 3, sheetRect.width, sheetRect.height + 38);
  }
  fui::sheet(screen.frame(), sheetRect, sheet);
  if (framed) {
    renderer.drawRect(sheetRect.x, sheetRect.y, sheetRect.width, sheetRect.height);
    renderer.drawRect(sheetRect.x + 2, sheetRect.y - 1, sheetRect.width - 4, sheetRect.height - 1);
    // A pull tab in the frame's lower edge, striped like a title bar.
    const int tabW = 64;
    const int tabH = 12;
    const int tabX = sheetRect.x + (sheetRect.width - tabW) / 2;
    const int tabY = sheetRect.y + sheetRect.height - tabH - 7;
    renderer.fillRect(tabX, tabY, tabW, tabH, false);
    renderer.drawRect(tabX, tabY, tabW, tabH);
    for (int ly = tabY + 3; ly < tabY + tabH - 2; ly += 3) renderer.drawLine(tabX + 4, ly, tabX + tabW - 5, ly);
  }
  const fui::Rect sheetContent = fui::sheetContentRect(sheetRect, sheet);
  drawerHandleRect = DrawerHandle::registerTap(screen.frame(), sheetContent, sheet, ACTION_DISMISS);
  const int16_t bottomInset = static_cast<int16_t>(renderer.getScreenHeight() - sheetContent.bottom());
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + panelHeaderBand(metrics, mappedInput)), sideMargin,
                  bottomInset, sideMargin});

  const fui::Rect actionBar = screen.takeBottom(ACTION_BAR_HEIGHT);
  const int16_t slotWidth = static_cast<int16_t>(actionBar.width / 5);
  const std::array<fui::BitmapRef, 5> icons = {
      fui::bitmapFromIcon(icon_reading_stats_24), fui::bitmapFromIcon(icon_transfer_24),
      fui::bitmapFromIcon(icon_tabler_moon_filled_24), fui::bitmapFromIcon(icon_sliders_horizontal_24),
      fui::bitmapFromIcon(pendingTouchscreenDisabled ? icon_device_tablet_off_24 : icon_device_tablet_24)};
  const bool system6 = isSystem6();
  const KeyGlyph keyGlyphs[5] = {KeyGlyph::Stats, KeyGlyph::Sync, KeyGlyph::Dark, KeyGlyph::Prefs, KeyGlyph::Touch};
  const bool keyActive[5] = {false, false, SETTINGS.screenInverted != 0, false, pendingTouchscreenDisabled};
  for (int16_t i = 0; i < 5; ++i) {
    const int16_t x = static_cast<int16_t>(actionBar.x + i * slotWidth);
    const int16_t width = i == 4 ? static_cast<int16_t>(actionBar.right() - x) : slotWidth;
    const fui::Rect slot{x, actionBar.y, width, actionBar.height};
    screen.frame().hit(slot, ACTION_QUICK, i);
    if (system6) {
      const fui::Rect key{static_cast<int16_t>(slot.x + 5), static_cast<int16_t>(slot.y + 8),
                          static_cast<int16_t>(slot.width - 14), static_cast<int16_t>(slot.height - 18)};
      drawIconKey(renderer, key, keyGlyphs[i], keyActive[i]);
    } else {
      screen.target().bitmap(slot, icons[static_cast<size_t>(i)], fui::BitmapMode::Center);
    }
  }
  if (!system6) {
    screen.target().fill(fui::Rect{actionBar.x, actionBar.y, actionBar.width, 1}, fui::Paint::solid(fui::Color::Black));
  }

  if (context.showReaderDetails) {
    drawReaderDetails(screen);
    return;
  }

  const int16_t lh = screen.target().lineHeight(theme.bodyText.font);
  const int16_t rowH = theme.rowHeight;
  const fui::Insets sideInset{0, static_cast<int16_t>(theme.spaceLg * 2), 0, static_cast<int16_t>(theme.spaceLg * 2)};
  char line[48];

  screen.spacer(theme.spaceLg);

  // Header row: "Brightness NN%" on the left, a tappable bulb icon on the right
  // that toggles the light — `lightbulb` when on, `lightbulb-off` when off (the
  // icon itself is the state indicator). Sharing a row with the label frees the
  // whole bottom toggle row, shrinking the panel.
  const fui::Rect headerRow = screen.takeTop(rowH, theme.spaceSm).inset(sideInset);
  snprintf(line, sizeof(line), "%s  %u%%", tr(STR_BRIGHTNESS), static_cast<unsigned>(brightness));
  const fui::BitmapRef lightIcon = fui::bitmapFromIcon(lightOn ? icon_lightbulb_28 : icon_lightbulb_off_28);
  const int16_t iconW = static_cast<int16_t>(lightIcon.width);
  const int16_t iconH = static_cast<int16_t>(lightIcon.height);
  const fui::Rect iconRect{static_cast<int16_t>(headerRow.x + headerRow.width - iconW),
                           static_cast<int16_t>(headerRow.y + (rowH - iconH) / 2), iconW, iconH};
  const fui::Rect labelRect{headerRow.x, static_cast<int16_t>(headerRow.y + (rowH - lh) / 2),
                            static_cast<int16_t>(headerRow.width - iconW - theme.spaceMd), lh};
  screen.target().text(labelRect, line, theme.bodyText);
  // Keep the visible glyph aligned to the content inset, but let its touch
  // target fill the otherwise blank right edge and the adjacent row gaps.
  // This makes the control more forgiving without reaching the brightness
  // slider below.
  const int16_t hitW = static_cast<int16_t>(iconW + theme.spaceLg * 4);
  const fui::Rect hitRect{static_cast<int16_t>(headerRow.right() - hitW),
                          static_cast<int16_t>(headerRow.y - theme.spaceSm),
                          static_cast<int16_t>(hitW + sideInset.right), static_cast<int16_t>(rowH + theme.spaceSm * 2)};
  screen.frame().hit(hitRect, ACTION_TOGGLE);
  if (system6) {
    RockerSwitch::paint(renderer, headerRow.right() - RockerSwitch::kWidth,
                        headerRow.y + (rowH - RockerSwitch::kHeight) / 2, lightOn);
  } else {
    screen.target().bitmap(iconRect, lightIcon, fui::BitmapMode::Center);
  }

  addStepSlider(screen, screen.takeTop(theme.rowHeight, theme.spaceLg).inset(sideInset), brightness, ACTION_BRIGHTNESS,
                ACTION_BRIGHTNESS_STEP);

  if (Frontlight.hasColorTemperature()) {
    snprintf(line, sizeof(line), "%s  %u%%", tr(STR_WARMTH), static_cast<unsigned>(warmth));
    screen.target().text(screen.takeTop(lh, theme.spaceSm).inset(sideInset), line, theme.bodyText);
    addStepSlider(screen, screen.takeTop(theme.rowHeight, theme.spaceLg).inset(sideInset), warmth, ACTION_WARMTH,
                  ACTION_WARMTH_STEP);
  }

  screen.spacer(theme.spaceLg);
}

void FrontlightPanelActivity::drawHeader() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int headerHeight = TouchHeaderBackButton::height(metrics, mappedInput);
  const Rect header{0, metrics.topPadding, renderer.getScreenWidth(), headerHeight};

  char date[16] = {};
  const char* title = context.showReaderDetails ? "" : tr(STR_FRONTLIGHT);
  int titleFontId = UI_12_FONT_ID;
  if (context.showReaderDetails) {
    if (formatHeaderDateText(date, sizeof(date))) title = date;
    titleFontId = UI_10_FONT_ID;
  } else if (context.activeReaderBook && !context.bookTitle.empty()) {
    title = context.bookTitle.c_str();
    titleFontId = UI_10_FONT_ID;
  } else if (formatHeaderDateText(date, sizeof(date))) {
    title = date;
    titleFontId = UI_10_FONT_ID;
  }

  if (SETTINGS.uiTheme == CrossPointSettings::UI_THEME::SYSTEM6) {
    // A System 6 window title bar for the floating drawer: close box on the
    // left (Back), striped bar, centred title. The screen underneath stays put.
    const int bx = 8;
    const int by = metrics.topPadding + kToastTop;
    const int bw = renderer.getScreenWidth() - 16;
    constexpr int bh = 40;
    renderer.fillRect(bx, by, bw, bh, false);
    renderer.drawRect(bx, by, bw, bh);
    renderer.drawRect(bx + 2, by + 2, bw - 4, bh - 4);
    for (int dy = 7; dy < bh - 5; dy += 4) renderer.drawLine(bx + 6, by + dy, bx + bw - 7, by + dy);

    constexpr int box = 30;
    const int cbx = bx + 6;
    const int cby = by + (bh - box) / 2;
    renderer.fillRect(cbx, cby, box, box, false);
    renderer.drawRect(cbx, cby, box, box);
    const int cx = cbx + box / 2 + 4;
    const int cy = cby + box / 2;
    for (int i = 0; i < 9; ++i) renderer.drawLine(cx - i, cy - 8 + i, cx - i, cy + 8 - i);
    TouchRegistry::getInstance().add(Rect{0, std::max(0, by - 6), 76, bh + 12}, 0, TouchRegistry::Back);

    const int titleLeft = cbx + box + 14;
    const int titleRight = bx + bw - 16;
    const std::string visibleTitle = renderer.truncatedText(UI_12_FONT_ID, title, std::max(1, titleRight - titleLeft));
    const int titleW = renderer.getTextWidth(UI_12_FONT_ID, visibleTitle.c_str());
    const int titleX = std::clamp(bx + (bw - titleW) / 2, titleLeft, std::max(titleLeft, titleRight - titleW));
    const int titleY = by + (bh - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.fillRect(titleX - 7, by + 4, titleW + 14, bh - 8, false);
    renderer.drawText(UI_12_FONT_ID, titleX, titleY, visibleTitle.c_str());
    return;
  }

  // Reuse the normal header chrome so the drawer retains its separator, clock,
  // and battery indicator. The date remains deliberately smaller than a page
  // title, matching its secondary status role.
  // The panel is a global overlay even when it was opened from a reader. Its
  // clock and battery therefore follow the outside-reader visibility settings.
  GUI.drawHeader(renderer, header, "", nullptr, false);
  // Keep the title/date in the header's lower lane. The status chrome above
  // owns the clock and battery, so centering across the whole header crowds it.
  const int headerBottom = header.y + header.height;
  const int titleY = headerBottom - HEADER_CONTENT_BOTTOM_GAP - renderer.getLineHeight(titleFontId);
  const bool showBookTitle = !context.showReaderDetails && context.activeReaderBook && !context.bookTitle.empty();
  if (showBookTitle) {
    const auto tokens = uiThemeTokens(uiTarget);
    const Rect homeButton = homeButtonRect();
    const int titleX = header.x + tokens.headerSidePadding;
    const int titleRight = homeButton.x - tokens.spaceSm;
    const int titleWidth = std::max(0, titleRight - titleX);
    const std::string visibleTitle = renderer.truncatedText(titleFontId, title, titleWidth, EpdFontFamily::REGULAR);
    renderer.drawText(titleFontId, titleX, titleY, visibleTitle.c_str(), true);
  } else {
    UITheme::drawCenteredText(renderer, header, titleFontId, titleY, title, true);
  }

  if (context.activeReaderBook) {
    const Rect button = homeButtonRect();
    uiTarget.bitmap(fui::Rect{static_cast<int16_t>(button.x + (button.width - HEADER_ICON_SIZE) / 2),
                              static_cast<int16_t>(headerBottom - HEADER_CONTENT_BOTTOM_GAP - HEADER_ICON_SIZE),
                              HEADER_ICON_SIZE, HEADER_ICON_SIZE},
                    fui::bitmapFromIcon(icon_home_24), fui::BitmapMode::Center);
  }
}

void FrontlightPanelActivity::addStepSlider(UiApp::ScreenType& screen, const fui::Rect& row, const uint8_t value,
                                            const fui::ActionId sliderAction, const fui::ActionId stepAction) {
  const auto& theme = screen.theme();
  const int16_t stepWidth = row.height;
  const fui::Rect minusHit{row.x, row.y, stepWidth, row.height};
  const fui::Rect plusHit{static_cast<int16_t>(row.right() - stepWidth), row.y, stepWidth, row.height};

  fui::TextStyle glyph = theme.bodyText;
  glyph.align = fui::TextAlign::Center;
  const int16_t lineHeight = screen.target().lineHeight(glyph.font);
  const int16_t glyphY = static_cast<int16_t>(row.y + (row.height - lineHeight) / 2);
  const bool system6 = isSystem6();
  if (system6) {
    // Mac scroll bar: an arrow box at each end of a dithered trough, with a
    // square thumb that rides along it.
    const int16_t boxH = static_cast<int16_t>(std::min<int>(row.height, 34));
    const int16_t boxY = static_cast<int16_t>(row.y + (row.height - boxH) / 2);
    drawArrowBox(renderer, fui::Rect{minusHit.x, boxY, boxH, boxH}, -1);
    drawArrowBox(renderer, fui::Rect{static_cast<int16_t>(plusHit.right() - boxH), boxY, boxH, boxH}, +1);
  } else {
    screen.target().text(fui::Rect{minusHit.x, glyphY, stepWidth, lineHeight}, "-", glyph);
    screen.target().text(fui::Rect{plusHit.x, glyphY, stepWidth, lineHeight}, "+", glyph);
  }
  screen.frame().hit(minusHit, stepAction, -1, fui::InputTouch);
  screen.frame().hit(plusHit, stepAction, +1, fui::InputTouch);

  fui::SliderProps slider;
  if (system6) {
    const int16_t boxH = static_cast<int16_t>(std::min<int>(row.height, 34));
    slider.track = fui::Paint::dither(fui::Color::LightGray);
    slider.fill = fui::Paint::dither(fui::Color::LightGray);
    slider.knob = fui::Paint::solid(fui::Color::White);
    slider.border = fui::Paint::solid(fui::Color::Black);
    slider.trackHeight = boxH;
    slider.knobWidth = boxH;
    slider.knobHeight = boxH;
    slider.horizontalPadding = 0;
  }
  slider.value = value;
  slider.max = 100;
  slider.action = sliderAction;
  slider.inputMask = fui::InputTouch | fui::InputDrag;
  const int16_t sideGap = static_cast<int16_t>(stepWidth + theme.spaceSm);
  const fui::Rect sliderRect = row.inset(fui::Insets{0, sideGap, 0, sideGap});
  fui::slider(screen.frame(), sliderRect, slider);
  if (system6) {
    const int16_t boxH = static_cast<int16_t>(std::min<int>(row.height, 34));
    renderer.drawRect(sliderRect.x, row.y + (row.height - boxH) / 2, sliderRect.width, boxH);
  }
}

void FrontlightPanelActivity::render(RenderLock&&) {
  // Overlay drop-down: keep the framebuffer content (the reader/menu we opened
  // over) intact below the panel; only the top third is repainted. No full
  // clearScreen — same as the theme popups draw over the current frame.
  panelBottom = computePanelBottom();
  const bool system6 = SETTINGS.uiTheme == CrossPointSettings::UI_THEME::SYSTEM6;
  uiReady = false;
  app.render();
  uiReady = true;
  (void)system6;
  drawHeader();

  // The dialog is the topmost layer.
  if (optionPopup.isActive()) optionPopup.render(renderer);

  renderer.displayBuffer();
}
