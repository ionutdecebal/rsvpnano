#include "ui/Ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#if __has_include(<esp_log.h>)
#include <esp_log.h>
#endif

#include "fonts/UiFont6x9.h"
#include "text/UnicodeText.h"
#include "text/Utf8Text.h"

namespace ui {
    namespace {

        constexpr uint16_t kFallbackBlack = 0x0000;
        constexpr uint16_t kFallbackWhite = 0xFFFF;

        constexpr uint8_t kUiFontCellWidth = 6;
        constexpr uint8_t kUiFontHeight = 9;
        constexpr uint32_t kBuiltInUiScripts = UnicodeText::ScriptLatin | UnicodeText::ScriptCyrillic;
        constexpr uint8_t kTapCancelOutsideSamples = 2;

        class PortraitGfx final : public Arduino_GFX {
        public:
            explicit PortraitGfx(Arduino_GFX& output) : PortraitGfx(output, output.height(), output.width()) {}

            PortraitGfx(Arduino_GFX& output, int16_t width, int16_t height, int16_t offsetX = 0, int16_t offsetY = 0) :
                    Arduino_GFX(width, height),
                    output_(output),
                    portraitWidth_(width),
                    offsetX_(offsetX),
                    offsetY_(offsetY) {}

            bool begin(int32_t = -1) override {
                return true;
            }

            void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
                const int16_t mappedX = static_cast<int16_t>(y + offsetX_);
                const int16_t mappedY = static_cast<int16_t>(portraitWidth_ - 1 - x + offsetY_);
                if constexpr (Context::displayWriteAlignment() == 2)
                    output_.drawPixel(mappedX, mappedY, color);
                else
                    output_.writePixelPreclipped(mappedX, mappedY, color);
            }

            void writeFastHLine(int16_t x, int16_t y, int16_t width, uint16_t color) override {
                const int16_t mappedX = static_cast<int16_t>(y + offsetX_);
                const int16_t mappedY = static_cast<int16_t>(portraitWidth_ - x - width + offsetY_);
                if constexpr (Context::displayWriteAlignment() == 2)
                    output_.drawFastVLine(mappedX, mappedY, width, color);
                else
                    output_.writeFastVLine(mappedX, mappedY, width, color);
            }

            void writeFastVLine(int16_t x, int16_t y, int16_t height, uint16_t color) override {
                const int16_t mappedX = static_cast<int16_t>(y + offsetX_);
                const int16_t mappedY = static_cast<int16_t>(portraitWidth_ - 1 - x + offsetY_);
                if constexpr (Context::displayWriteAlignment() == 2)
                    output_.drawFastHLine(mappedX, mappedY, height, color);
                else
                    output_.writeFastHLine(mappedX, mappedY, height, color);
            }

            void writeFillRectPreclipped(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) override {
                const int16_t mappedX = static_cast<int16_t>(y + offsetX_);
                const int16_t mappedY = static_cast<int16_t>(portraitWidth_ - x - width + offsetY_);
                if constexpr (Context::displayWriteAlignment() == 2)
                    output_.fillRect(mappedX, mappedY, height, width, color);
                else
                    output_.writeFillRectPreclipped(mappedX, mappedY, height, width, color);
            }

        private:
            Arduino_GFX& output_;
            int16_t portraitWidth_;
            int16_t offsetX_;
            int16_t offsetY_;
        };

    } // namespace

    Context::Context(Arduino_GFX& gfx) : gfx_(gfx) {}

    Rect Context::paintBounds(Rect rect) const {
        if constexpr (displayWriteAlignment() == 1)
            return rect;
        const int x1 = rect.x & ~1;
        const int y1 = rect.y & ~1;
        const int x2 = (rect.x + std::max<int16_t>(0, rect.w)) & ~1;
        const int y2 = (rect.y + std::max<int16_t>(0, rect.h)) & ~1;
        // Snap shared boundaries alike. Clip transfers later so offscreen content does not reflow.
        return {static_cast<int16_t>(x1), static_cast<int16_t>(y1), static_cast<int16_t>(std::max(0, x2 - x1)),
                static_cast<int16_t>(std::max(0, y2 - y1))};
    }

    Arduino_Canvas* Context::paintBuffer() {
#ifdef RSVP_BOARD_CONFIG_HEADER
        if constexpr (displayWriteAlignment() > 1) {
            // aligned_alloc used by Arduino_Canvas requires a multiple-of-16 allocation size.
            constexpr int16_t pitch = (Board::Config::PANEL_NATIVE_WIDTH + 3) & ~3;
            constexpr int16_t rows = Board::Config::DISPLAY_BUFFER_ROWS;
            static_assert(displayWriteAlignment() == 1 || (rows > 0 && rows % displayWriteAlignment() == 0));
            static Arduino_Canvas buffer(pitch, rows, nullptr);
            if (gfx_.width() <= pitch && buffer.begin(GFX_SKIP_OUTPUT_BEGIN))
                return &buffer;
        }
#endif
        return nullptr;
    }

    int16_t Context::textWidth(std::string_view text, uint8_t size) {
        const int32_t width =
            static_cast<int32_t>(Utf8Text::count(text)) * kUiFontCellWidth * std::max<uint8_t>(1, size);
        return static_cast<int16_t>(std::min<int32_t>(width, INT16_MAX));
    }

    int16_t Context::textHeight(uint8_t size) {
        return static_cast<int16_t>(kUiFontHeight * std::max<uint8_t>(1, size));
    }

    const locales::UiAssets* Context::fontAssetsFor(std::string_view text, std::string_view textLocale) const {
        if (languageAssets_.owns(text) && !languageAssets_.font.empty())
            return &languageAssets_;
        const uint32_t requiredScripts = UnicodeText::scriptsIn(text) & ~kBuiltInUiScripts;
        if (requiredScripts == 0 || languageFilesystem_ == nullptr || languageCatalog_ == nullptr
            || languageFontLoader_ == nullptr)
            return nullptr;
        const std::string_view preferredLocale = textLocale.empty() ? std::string_view{locale_} : textLocale;
        const locales::InstalledPack* pack =
            locales::findPackForScripts(*languageCatalog_, preferredLocale, requiredScripts);
        if (pack == nullptr)
            return nullptr;
        if (pack->locale == locale_ && !languageAssets_.font.empty())
            return &languageAssets_;
        const auto cached = std::ranges::find(contentFonts_, pack->id, [](const auto& entry) -> const std::string& {
            return entry.first;
        });
        if (cached != contentFonts_.end())
            return cached->second.font.empty() ? nullptr : &cached->second;

        auto& [id, assets] = contentFonts_.emplace_back(pack->id, locales::UiAssets{.direction = pack->direction});
        auto loaded = languageFontLoader_(*languageFilesystem_, *pack);
        if (loaded)
            assets.font = std::move(*loaded);
#if __has_include(<esp_log.h>)
        else
            ESP_LOGW("ui", "content font unavailable pack=%s: %s", id.c_str(), loaded.error().c_str());
#endif
        return assets.font.empty() ? nullptr : &assets;
    }

    int16_t Context::textWidthFor(std::string_view text, uint8_t size, std::string_view textLocale) const {
        const locales::UiAssets* assets = fontAssetsFor(text, textLocale);
        const uint8_t cellWidth = assets == nullptr ? kUiFontCellWidth : locales::uiFontCellWidth(assets->font);
        const int32_t width = static_cast<int32_t>(Utf8Text::count(text)) * cellWidth * std::max<uint8_t>(1, size);
        return static_cast<int16_t>(std::min<int32_t>(width, INT16_MAX));
    }

    int16_t Context::textHeightFor(std::string_view text, uint8_t size, std::string_view textLocale) const {
        const locales::UiAssets* assets = fontAssetsFor(text, textLocale);
        const uint8_t height = assets == nullptr ? kUiFontHeight : locales::uiFontHeight(assets->font);
        return static_cast<int16_t>(height * std::max<uint8_t>(1, size));
    }

    void Context::setTheme(const ui::themes::Theme& theme) {
        // Installing/removing themes can move another theme into the same catalog address.
        theme_ = &theme;
        invalidate();
    }

    void Context::setLanguageCatalog(fs::FS* filesystem, const locales::Catalog* catalog,
                                     LanguageFontLoader fontLoader) {
        languageFilesystem_ = filesystem;
        languageCatalog_ = catalog;
        languageFontLoader_ = fontLoader;
        contentFonts_.clear();
        invalidate();
    }

    void Context::setLanguageAssets(locales::UiAssets assets) {
        languageAssets_ = std::move(assets);
        contentFonts_.clear();
        invalidate();
    }

    void Context::setLocale(std::string_view locale) {
        if (locale.empty())
            locale = Localization::kDefaultLocale;
        if (locale_ != locale) {
            locale_ = locale;
            invalidate();
        }
    }

    void Context::setOrientation(Orientation orientation) {
        if (touchOrientation_ == orientation)
            return;
        touchOrientation_ = orientation;
        if constexpr (displayWriteAlignment() == 1)
            gfx_.setRotation(static_cast<uint8_t>(orientation));
        resetTouchGesture();
        invalidate();
    }

    std::string_view Context::text(UiText key) const {
        const std::string_view translated = languageAssets_.text(static_cast<size_t>(key));
        if (!translated.empty())
            return translated;
        return Localization::text(key);
    }

    void Context::setTouchSource(TouchSource source) {
        touchSource_ = source;
        resetTouchGesture();
    }

    bool Context::pollTouch(uint32_t nowMs) {
        touchPending_ = false;
        if (touchSource_.poll == nullptr)
            return false;

        TouchContact contact;
        switch (touchSource_.poll(contact)) {
        case TouchSampleResult::None:
            return false;
        case TouchSampleResult::Reset:
            if (!touchActive_) {
                resetTouchGesture();
                return false;
            }
            touchActive_ = false;
            touchHoldEmitted_ = false;
            touchOutsideSamples_ = 0;
            touchStartedAtMs_ = 0;
            touchEvent_ = {TouchRelease, touchLastX_, touchLastY_};
            return touchPending_ = true;
        case TouchSampleResult::Contact:
            break;
        }

        const uint32_t sampledAtMs = contact.sampledAtMs == 0 ? nowMs : contact.sampledAtMs;
        touchLastPollMs_ = sampledAtMs;
        return updateTouch(contact, sampledAtMs);
    }

    void Context::beginFrame(uint8_t screen) {
        nextSlot_ = 0;
        drew_ = false;
        if (screen_ != screen) {
            screen_ = screen;
            gridPage_ = 0;
            rotaryDragging_ = false;
            contentFonts_.clear();
            invalid_ = true;
            capturedSlot_ = kSlotCapacity;
        }
        if (invalid_) {
            invalid_ = false;
            if constexpr (displayWriteAlignment() == 1)
                gfx_.fillScreen(color(ui::themes::ColorRole::Background));
            else
                clear({0, 0, width(), height()});
            markDrawn();
            for (Slot& slot: slots_) {
                slot.valid = false;
            }
            slotCount_ = 0;
            drew_ = true;
        }
    }

    void Context::endFrame() {
        for (size_t index = nextSlot_; index < slotCount_; ++index) {
            if (slots_[index].valid) {
                if (slots_[index].kind != Kind::Touch)
                    clear(slots_[index].rect);
                slots_[index].valid = false;
            }
        }
        if (capturedSlot_ >= nextSlot_) {
            capturedSlot_ = kSlotCapacity;
        }
        slotCount_ = std::min(nextSlot_, kSlotCapacity);
        if (drew_)
            gfx_.flush();
        touchPending_ = false;
    }

    void Context::invalidate() {
        invalid_ = true;
    }

    void Context::prepareTextFont(std::string_view text, std::string_view textLocale) const {
        (void) fontAssetsFor(text, textLocale);
    }

    void Context::label(Rect rect, std::string_view text, uint8_t textSize, ui::themes::ColorRole role, TextAlign align,
                        uint8_t textLines, std::string_view textLocale, uint8_t alpha) {
        rect = paintBounds(rect);
        uint32_t state = combine(signature(text), textSize);
        state = combine(state, role);
        state = combine(state, static_cast<uint8_t>(align));
        state = combine(state, textLines);
        state = signature(textLocale, state);
        state = combine(state, alpha);
        if (!claim(Kind::Label, rect, state).changed) {
            return;
        }
        drawText(rect, text, textSize, blend(role, alpha), align, textLines, textLocale);
    }

    void Context::separator(Rect rect, std::string_view text) {
        rect = paintBounds(rect);
        if (!claim(Kind::Separator, rect, signature(text)).changed)
            return;

        const int16_t labelWidth = std::min<int16_t>(rect.w, textWidthFor(text, 1));
        const auto label = prepareText({0, 0, labelWidth, rect.h}, text, 1);
        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            output.fillRect(rect.x, rect.y, rect.w, rect.h, color(ui::themes::ColorRole::Background));
            drawText(output, label, color(ui::themes::ColorRole::Muted), rect.x, rect.y);
            const int16_t lineX = static_cast<int16_t>(rect.x + labelWidth + 6);
            if (lineX < rect.x + rect.w)
                output.drawFastHLine(lineX, static_cast<int16_t>(rect.y + rect.h / 2),
                                     static_cast<int16_t>(rect.x + rect.w - lineX),
                                     blend(ui::themes::ColorRole::Muted, 96));
            markDrawn();
        });
    }

    bool Context::setting(Rect rect, std::string_view label, std::string_view value, SettingLayout layout) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        uint32_t state = signature(value, signature(label));
        state = combine(state, static_cast<uint8_t>(layout));
        if (claim(Kind::Setting, rect, state).changed) {
            TextLayout labelText, valueText;
            const int16_t textWidth = std::max<int16_t>(0, static_cast<int16_t>(rect.w - 14));
            if (layout == SettingLayout::Inline) {
                const int16_t labelRequired = textWidthFor(label, 2);
                uint8_t valueSize = 2;
                int16_t valueRequired = textWidthFor(value, 2);
                if (labelRequired + valueRequired + 8 > textWidth) {
                    valueSize = 1;
                    valueRequired = textWidthFor(value, 1);
                }
                const int16_t labelWidth = labelRequired + valueRequired + 8 <= textWidth
                                             ? labelRequired
                                             : std::min<int16_t>(labelRequired, textWidth / 2);
                const int16_t valueWidth = std::max<int16_t>(0, static_cast<int16_t>(textWidth - labelWidth - 8));
                labelText = prepareText({7, 0, labelWidth, rect.h}, label, 2);
                valueText = prepareText({static_cast<int16_t>(rect.w - valueWidth - 7), 0, valueWidth, rect.h}, value,
                                        valueSize, TextAlign::Right);
            } else {
                const bool largeValue = textWidthFor(value, 2) <= textWidth;
                labelText = prepareText({7, 3, textWidth, 8}, label, 1);
                valueText =
                    prepareText({7, 11, textWidth, static_cast<int16_t>(std::max<int16_t>(0, rect.h - 13))}, value,
                                largeValue ? 2 : 1, TextAlign::Start, !largeValue && rect.h >= 32 ? 2 : 1);
            }
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 5, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 5, color(ui::themes::ColorRole::Outline));
                drawText(output, labelText, color(layout == SettingLayout::Inline ? themes::Foreground : themes::Muted),
                         rect.x, rect.y);
                drawText(output, valueText, color(themes::Accent), rect.x, rect.y);
            });
        }
        return tapped(slot, rect);
    }

    bool Context::toggle(Rect rect, std::string_view label, bool& enabled) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        uint32_t state = combine(signature(label), enabled);
        if (claim(Kind::Toggle, rect, state).changed) {
            constexpr int16_t switchWidth = 34;
            const auto labelText =
                prepareText({7, 0, static_cast<int16_t>(std::max<int16_t>(0, rect.w - switchWidth - 21)), rect.h},
                            label, 2);
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 5, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 5, color(ui::themes::ColorRole::Outline));
                const int16_t switchX = static_cast<int16_t>(rect.x + rect.w - switchWidth - 7);
                const int16_t switchY = static_cast<int16_t>(rect.y + (rect.h - 16) / 2);
                output.fillRoundRect(switchX, switchY, switchWidth, 16, 8,
                                     color(enabled ? ui::themes::ColorRole::Accent
                                                   : ui::themes::ColorRole::ProgressTrack));
                output.fillCircle(static_cast<int16_t>(switchX + (enabled ? switchWidth - 8 : 8)),
                                  static_cast<int16_t>(switchY + 8), 6, color(ui::themes::ColorRole::Foreground));
                drawText(output, labelText, color(themes::Foreground), rect.x, rect.y);
            });
        }
        if (!tapped(slot, rect))
            return false;
        enabled = !enabled;
        return true;
    }

    bool Context::tap(Rect rect, bool enabled) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        claim(Kind::Touch, rect, enabled);
        return tapped(slot, rect, enabled);
    }

    bool Context::button(Rect rect, std::string_view text, bool enabled, Icon icon, uint8_t textLines,
                         std::string_view detailLeft, std::string_view detailRight) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        const bool activated = tapped(slot, rect, enabled);
        uint32_t state = combine(signature(text), enabled);
        state = combine(state, static_cast<uint8_t>(icon));
        state = combine(state, textLines);
        state = signature(detailLeft, state);
        state = signature(detailRight, state);
        const Claim widget = claim(Kind::Button, rect, state);
        if (widget.changed) {
            const int16_t iconWidth = icon == Icon::None ? 0 : std::min<int16_t>(34, rect.w / 3);
            const bool hasDetail = !detailLeft.empty() || !detailRight.empty();
            const int16_t textHeight = hasDetail ? static_cast<int16_t>(rect.h - 18) : rect.h;
            const Rect textRect{6, 0, static_cast<int16_t>(std::max<int16_t>(0, rect.w - iconWidth - 12)), textHeight};
            const auto title = prepareText(textRect, text, 2, TextAlign::Center, textLines);
            TextLayout detail;
            if (hasDetail) {
                const int16_t detailY = static_cast<int16_t>(rect.h - 20);
                if (detailLeft.empty() || detailRight.empty()) {
                    detail = prepareText({textRect.x, detailY, textRect.w, 16},
                                         detailLeft.empty() ? detailRight : detailLeft, 2,
                                         detailLeft.empty() ? TextAlign::Right : TextAlign::Left);
                } else {
                    const int16_t detailWidth = static_cast<int16_t>((textRect.w - 8) / 2);
                    detail.lines.reserve(2);
                    appendText(detail, {textRect.x, detailY, detailWidth, 16}, detailLeft, 2, TextAlign::Start, 1, {});
                    appendText(detail,
                               {static_cast<int16_t>(textRect.x + textRect.w - detailWidth), detailY, detailWidth, 16},
                               detailRight, 2, TextAlign::Right, 1, {});
                }
            }
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 5, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 5,
                                     color(enabled ? ui::themes::ColorRole::Outline
                                                   : ui::themes::ColorRole::ProgressTrack));
                if (enabled && rect.w > 16 && rect.h >= 28)
                    output.fillRect(static_cast<int16_t>(rect.x + 8), static_cast<int16_t>(rect.y + rect.h - 3),
                                    static_cast<int16_t>(rect.w - 16), 2, color(ui::themes::ColorRole::Accent));
                drawText(output, title, color(enabled ? themes::Foreground : themes::Muted), rect.x, rect.y);
                drawText(output, detail, color(themes::Muted), rect.x, rect.y);
                if (icon != Icon::None)
                    drawIcon(output, {static_cast<int16_t>(rect.x + rect.w - iconWidth), rect.y, iconWidth, rect.h},
                             icon, color(enabled ? ui::themes::ColorRole::Accent : ui::themes::ColorRole::Muted),
                             surface);
            });
        }
        return activated;
    }

    bool Context::iconButton(Rect rect, Icon icon) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        const bool activated = tapped(slot, rect);
        const uint32_t state = static_cast<uint8_t>(icon);
        const Claim widget = claim(Kind::Button, rect, state);
        if (widget.changed) {
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 7, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 7, color(ui::themes::ColorRole::Outline));
                drawIcon(output, rect, icon, color(ui::themes::ColorRole::Muted), surface);
            });
        }
        return activated;
    }

    bool Context::tab(Rect rect, std::string_view text, bool active, Icon icon) {
        rect = paintBounds(rect);
        uint32_t state = combine(signature(text), active);
        state = combine(state, static_cast<uint8_t>(icon));
        const Claim widget = claim(Kind::Tab, rect, state);
        if (widget.changed) {
            const int16_t iconWidth = icon == Icon::None ? 0 : std::min<int16_t>(26, rect.w / 3);
            const auto label = prepareText({static_cast<int16_t>(iconWidth + 8), 0,
                                            static_cast<int16_t>(rect.w - iconWidth - 12), rect.h},
                                           text, 2, TextAlign::Center);
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const uint16_t surface =
                    color(active ? ui::themes::ColorRole::Surface : ui::themes::ColorRole::SurfaceMuted);
                output.fillRect(rect.x, rect.y, rect.w, rect.h, surface);
                output.drawRect(rect.x, rect.y, rect.w, rect.h, color(ui::themes::ColorRole::Outline));
                if (active) {
                    output.fillRect(rect.x, static_cast<int16_t>(rect.y + 5), 3, static_cast<int16_t>(rect.h - 10),
                                    color(ui::themes::ColorRole::Accent));
                }
                const uint16_t ink = color(active ? ui::themes::ColorRole::Foreground : ui::themes::ColorRole::Muted);
                if (icon != Icon::None)
                    drawIcon(output, {static_cast<int16_t>(rect.x + 7), rect.y, iconWidth, rect.h}, icon, ink, surface);
                drawText(output, label, ink, rect.x, rect.y);
            });
        }
        return tapped(widget.index, rect);
    }

    Context::BatteryLayout Context::batteryLayout(Rect rect, std::string_view labelText, bool showIcon) const {
        constexpr int16_t iconWidth = 29;
        constexpr int16_t iconHeight = 13;
        constexpr int16_t labelGap = 7;

        const int16_t iconAreaWidth = showIcon ? iconWidth + labelGap : 0;
        const int16_t labelWidth =
            std::min<int16_t>(textWidth(labelText, 2), std::max<int16_t>(0, rect.w - iconAreaWidth));
        const int16_t totalWidth = static_cast<int16_t>(iconAreaWidth + labelWidth);
        const int16_t x = std::max<int16_t>(rect.x, static_cast<int16_t>(rect.x + rect.w - totalWidth));

        return {{x, static_cast<int16_t>(rect.y + std::max<int16_t>(0, (rect.h - iconHeight) / 2)), iconWidth,
                 iconHeight},
                {static_cast<int16_t>(x + iconAreaWidth), rect.y, labelWidth, rect.h}};
    }

    void Context::battery(Rect rect, uint8_t percent, bool charging, std::string_view labelText, bool showIcon,
                          uint8_t iconAlpha, uint8_t labelAlpha) {
        rect = paintBounds(rect);
        percent = std::min<uint8_t>(percent, 100);
        uint32_t state = combine(signature(labelText), percent);
        state = combine(state, charging);
        state = combine(state, showIcon);
        state = combine(state, iconAlpha);
        state = combine(state, labelAlpha);
        if (!claim(Kind::Battery, rect, state).changed)
            return;
        const auto layout = batteryLayout({0, 0, rect.w, rect.h}, labelText, showIcon);
        const auto label = prepareText(layout.label, labelText, 2);
        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            if (!showIcon && labelText.empty())
                return;
            if (showIcon)
                drawBatteryIcon(output,
                                {static_cast<int16_t>(layout.icon.x + rect.x),
                                 static_cast<int16_t>(layout.icon.y + rect.y), layout.icon.w, layout.icon.h},
                                percent, charging, blend(themes::Muted, iconAlpha), color(themes::Background));
            drawText(output, label, blend(themes::Muted, labelAlpha), rect.x, rect.y);
        });
    }

    void Context::progress(Rect rect, int value, int minimum, int maximum) {
        rect = paintBounds(rect);
        value = std::clamp(value, minimum, maximum);
        uint32_t state = combine(static_cast<uint32_t>(value), static_cast<uint32_t>(minimum));
        state = combine(state, static_cast<uint32_t>(maximum));
        if (!claim(Kind::Progress, rect, state).changed) {
            return;
        }
        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            output.fillRect(rect.x, rect.y, rect.w, rect.h, color(ui::themes::ColorRole::ProgressTrack));
            if (maximum > minimum && rect.w > 2 && rect.h > 2) {
                const int16_t fill =
                    static_cast<int16_t>((static_cast<int32_t>(rect.w - 2) * (value - minimum)) / (maximum - minimum));
                output.fillRect(static_cast<int16_t>(rect.x + 1), static_cast<int16_t>(rect.y + 1), fill,
                                static_cast<int16_t>(rect.h - 2), color(ui::themes::ColorRole::Accent));
            }
        });
    }

    void Context::steps(Rect rect, uint8_t current, uint8_t total, ui::themes::ColorRole activeRole) {
        rect = paintBounds(rect);
        current = std::min(current, total);
        uint32_t state = combine(current, total);
        state = combine(state, activeRole);
        if (!claim(Kind::Steps, rect, state).changed)
            return;

        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            if (total == 0)
                return;
            const bool vertical = rect.h > rect.w;
            const int16_t crossSize = vertical ? rect.w : rect.h;
            const int16_t radius =
                std::max<int16_t>(2, std::min<int16_t>(4, static_cast<int16_t>((crossSize - 2) / 2)));
            const int16_t spacing = static_cast<int16_t>(radius * 2 + 5);
            const int16_t length = static_cast<int16_t>((total - 1) * spacing + radius * 2);
            const int16_t first = static_cast<int16_t>((vertical ? rect.y : rect.x)
                                                       + ((vertical ? rect.h : rect.w) - length) / 2 + radius);
            const int16_t center = static_cast<int16_t>((vertical ? rect.x : rect.y) + crossSize / 2);
            for (uint8_t index = 0; index < total; ++index) {
                const int16_t position = static_cast<int16_t>(first + index * spacing);
                const int16_t x = vertical ? center : position;
                const int16_t y = vertical ? position : center;
                if (index < current)
                    output.fillCircle(x, y, radius, color(activeRole));
                else
                    output.drawCircle(x, y, radius, color(ui::themes::ColorRole::Outline));
            }
        });
    }

    bool Context::sliderValue(Rect rect, std::string_view label, int& value, int minimum, int maximum, int step,
                              std::string_view suffix, ui::themes::ColorRole activeRole) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        const Touch* event = touch();
        const bool labeled = !label.empty();
        const int16_t visualHeight = labeled ? std::min<int16_t>(50, rect.h) : rect.h;
        const int16_t trackInset = labeled ? 8 : (displayWriteAlignment() == 2 ? 7 : 0);
        const Rect visual{rect.x, static_cast<int16_t>(rect.y + (rect.h - visualHeight) / 2), rect.w, visualHeight};
        const Rect track{static_cast<int16_t>(visual.x + trackInset),
                         static_cast<int16_t>(visual.y + (labeled ? visual.h - 8 : visual.h / 2 - 1)),
                         static_cast<int16_t>(visual.w - 2 * trackInset), 3};
        const bool started = event != nullptr && hasTouch(*event, TouchStart) && contains(rect, event->x, event->y);
        if (started && slot < kSlotCapacity) {
            capturedSlot_ = slot;
            capturedScalarInitialValue_ = std::clamp(value, minimum, maximum);
            capturedScalarValue_ = valueAt(track, event->x, minimum, maximum, step);
        }

        int displayedValue = std::clamp(value, minimum, maximum);
        bool changed = false;
        const bool moving =
            event != nullptr
            && (hasTouch(*event, TouchStart) || hasTouch(*event, TouchMove) || hasTouch(*event, TouchRelease));
        if (capturedSlot_ == slot && moving) {
            capturedScalarValue_ = valueAt(track, event->x, minimum, maximum, step);
        }
        if (capturedSlot_ == slot)
            displayedValue = capturedScalarValue_;
        if (capturedSlot_ == slot && event != nullptr && hasTouch(*event, TouchRelease))
            changed = displayedValue != capturedScalarInitialValue_;

        uint32_t state = signature(suffix, signature(label));
        state = combine(state, static_cast<uint32_t>(displayedValue));
        state = combine(state, static_cast<uint32_t>(minimum));
        state = combine(state, static_cast<uint32_t>(maximum));
        state = combine(state, static_cast<uint32_t>(step));
        state = combine(state, activeRole);
        if (claim(Kind::Slider, rect, state).changed) {
            const Rect bounds = rect;
            TextLayout labelText, valueLayout;
            if (labeled) {
                char valueText[24];
                std::snprintf(valueText, sizeof(valueText), "%d%.*s", displayedValue, static_cast<int>(suffix.size()),
                              suffix.data());
                const std::string_view valueView{valueText};
                const int16_t headerWidth = static_cast<int16_t>(visual.w - 14);
                const bool largeInline = visual.h >= 40 && textHeightFor(label, 3) <= visual.h - 10
                                      && textHeightFor(valueView, 3) <= visual.h - 10
                                      && textWidthFor(label, 3) + textWidthFor(valueView, 3) + 8 <= headerWidth;
                if (largeInline) {
                    const int16_t valueWidth = textWidthFor(valueView, 3);
                    const int16_t labelWidth = static_cast<int16_t>(headerWidth - valueWidth - 8);
                    const int16_t textHeight = static_cast<int16_t>(visual.h - 10);
                    labelText = prepareText({static_cast<int16_t>(visual.x + 7), static_cast<int16_t>(visual.y + 1),
                                             labelWidth, textHeight},
                                            label, 3);
                    valueLayout = prepareText({static_cast<int16_t>(visual.x + visual.w - valueWidth - 7),
                                               static_cast<int16_t>(visual.y + 1), valueWidth, textHeight},
                                              valueView, 3, TextAlign::Right);
                } else if (visual.h >= 44) {
                    labelText = prepareText({static_cast<int16_t>(visual.x + 7), static_cast<int16_t>(visual.y + 2),
                                             headerWidth, 16},
                                            label, 2);
                    valueLayout = prepareText({static_cast<int16_t>(visual.x + 7), static_cast<int16_t>(visual.y + 18),
                                               headerWidth, 16},
                                              valueView, 2, TextAlign::Right);
                } else {
                    uint8_t labelSize = visual.h >= 30 ? 2 : 1;
                    uint8_t valueSize = labelSize;
                    int16_t valueWidth = textWidthFor(valueView, valueSize);
                    if (headerWidth < textWidthFor(label, labelSize) + valueWidth + 8) {
                        valueSize = 1;
                        valueWidth = textWidthFor(valueView, 1);
                    }
                    if (headerWidth < textWidthFor(label, labelSize) + valueWidth + 8)
                        labelSize = 1;
                    const int16_t labelWidth = std::max<int16_t>(0, static_cast<int16_t>(headerWidth - valueWidth - 8));
                    const int16_t textY = static_cast<int16_t>(visual.y + 2);
                    labelText =
                        prepareText({static_cast<int16_t>(visual.x + 7), textY, labelWidth, 16}, label, labelSize);
                    valueLayout =
                        prepareText({static_cast<int16_t>(visual.x + visual.w - valueWidth - 7), textY, valueWidth, 16},
                                    valueView, valueSize, TextAlign::Right);
                }
            }
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const int16_t dx = rect.x - bounds.x, dy = rect.y - bounds.y;
                const Rect visual{rect.x, static_cast<int16_t>(rect.y + (rect.h - visualHeight) / 2), rect.w,
                                  visualHeight};
                const Rect track{static_cast<int16_t>(visual.x + trackInset),
                                 static_cast<int16_t>(visual.y + (labeled ? visual.h - 8 : visual.h / 2 - 1)),
                                 static_cast<int16_t>(visual.w - 2 * trackInset), 3};
                if (labeled) {
                    const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                    output.fillRoundRect(visual.x, visual.y, visual.w, visual.h, 5, surface);
                    output.drawRoundRect(visual.x, visual.y, visual.w, visual.h, 5,
                                         color(ui::themes::ColorRole::Outline));
                    drawText(output, labelText, color(themes::Foreground), dx, dy);
                    drawText(output, valueLayout, color(activeRole), dx, dy);
                }
                output.fillRect(track.x, track.y, track.w, track.h, color(ui::themes::ColorRole::ProgressTrack));
                const int16_t knobX =
                    maximum == minimum
                        ? track.x
                        : static_cast<int16_t>(track.x
                                               + (static_cast<int32_t>(track.w - 1) * (displayedValue - minimum))
                                                     / (maximum - minimum));
                const int16_t trackCenterY = static_cast<int16_t>(track.y + track.h / 2);
                if (step > 0 && maximum > minimum) {
                    const int intervalCount = (maximum - minimum + step - 1) / step;
                    const int tickStride = std::max(1, (intervalCount + 9) / 10);
                    for (int interval = 0;; interval = std::min(interval + tickStride, intervalCount)) {
                        const int tickValue = std::min(minimum + interval * step, maximum);
                        const int16_t tickX =
                            static_cast<int16_t>(track.x
                                                 + (static_cast<int32_t>(track.w - 1) * (tickValue - minimum))
                                                       / (maximum - minimum));
                        output.drawFastVLine(tickX, static_cast<int16_t>(trackCenterY - 3), 7,
                                             color(ui::themes::ColorRole::Outline));
                        if (interval == intervalCount)
                            break;
                    }
                }
                output.fillRect(track.x, track.y, static_cast<int16_t>(knobX - track.x + 1), track.h,
                                color(activeRole));
                const int16_t knobRadius = labeled ? 5 : 7;
                output.fillCircle(knobX, trackCenterY, knobRadius, color(activeRole));
                output.drawCircle(knobX, trackCenterY, knobRadius, color(ui::themes::ColorRole::OnAccent));
            });
        }

        if (capturedSlot_ == slot && event != nullptr && hasTouch(*event, TouchRelease)) {
            capturedSlot_ = kSlotCapacity;
        }
        if (changed)
            value = displayedValue;
        return changed;
    }

    bool Context::stepperValue(Rect rect, std::string_view label, int& value, int minimum, int maximum, int step,
                               std::string_view suffix, ui::themes::ColorRole activeRole) {
        rect = paintBounds(rect);
        const size_t slot = nextSlot_;
        const int safeStep = std::max(1, step);
        const int16_t buttonWidth = std::min<int16_t>(42, std::max<int16_t>(16, rect.w / 5));
        const Rect decrement{rect.x, rect.y, buttonWidth, rect.h};
        const Rect increment{static_cast<int16_t>(rect.x + rect.w - buttonWidth), rect.y, buttonWidth, rect.h};
        const Touch* event = touch();

        if (event != nullptr && hasTouch(*event, TouchStart) && slot < kSlotCapacity) {
            const int8_t direction = contains(decrement, event->x, event->y) ? -1
                                   : contains(increment, event->x, event->y) ? 1
                                                                             : 0;
            if (direction != 0) {
                capturedSlot_ = slot;
                capturedScalarInitialValue_ = std::clamp(value, minimum, maximum);
                capturedScalarValue_ = capturedScalarInitialValue_;
                capturedStepperDirection_ = direction;
            }
        }

        int displayedValue = std::clamp(value, minimum, maximum);
        bool changed = false;
        if (capturedSlot_ == slot) {
            const Rect target = capturedStepperDirection_ < 0 ? decrement : increment;
            const bool overTarget = event != nullptr && contains(target, event->x, event->y);
            if (overTarget && event != nullptr && hasTouch(*event, TouchRelease) && hasTouch(*event, TouchTap)) {
                capturedScalarValue_ =
                    std::clamp(capturedScalarInitialValue_ + capturedStepperDirection_ * safeStep, minimum, maximum);
            } else if (overTarget && touchActive_
                       && touchLastPollMs_ - touchStartedAtMs_ >= touchSource_.timing.holdMs) {
                constexpr uint32_t repeatMs = 120;
                const uint32_t repeats = (touchLastPollMs_ - touchStartedAtMs_ - touchSource_.timing.holdMs) / repeatMs;
                const int delta = safeStep * (1 + static_cast<int>(repeats));
                capturedScalarValue_ =
                    std::clamp(capturedScalarInitialValue_ + capturedStepperDirection_ * delta, minimum, maximum);
            }
            displayedValue = capturedScalarValue_;
            changed = displayedValue != value;
            if (event != nullptr && hasTouch(*event, TouchRelease)) {
                capturedSlot_ = kSlotCapacity;
                capturedStepperDirection_ = 0;
            }
        }

        uint32_t state = signature(suffix, signature(label));
        state = combine(state, static_cast<uint32_t>(displayedValue));
        state = combine(state, static_cast<uint32_t>(minimum));
        state = combine(state, static_cast<uint32_t>(maximum));
        state = combine(state, activeRole);
        if (claim(Kind::Stepper, rect, state).changed) {
            const Rect bounds = rect;
            const auto minusText = prepareText(decrement, "-", 2, TextAlign::Center);
            const auto plusText = prepareText(increment, "+", 2, TextAlign::Center);
            char valueText[24];
            std::snprintf(valueText, sizeof(valueText), "%d%.*s", displayedValue, static_cast<int>(suffix.size()),
                          suffix.data());
            const Rect middle{static_cast<int16_t>(decrement.x + decrement.w + 6), rect.y,
                              static_cast<int16_t>(rect.w - buttonWidth * 2 - 12), rect.h};
            TextLayout labelText, valueLayout;
            if (rect.h >= 44) {
                labelText = prepareText({middle.x, static_cast<int16_t>(middle.y + 2), middle.w, 16}, label, 2,
                                        TextAlign::Center);
                valueLayout = prepareText({middle.x, static_cast<int16_t>(middle.y + 20), middle.w,
                                           static_cast<int16_t>(middle.h - 20)},
                                          valueText, 2, TextAlign::Center);
            } else {
                const int16_t valueWidth = std::min<int16_t>(middle.w / 2, textWidth(valueText, 2));
                labelText = prepareText({middle.x, middle.y, static_cast<int16_t>(middle.w - valueWidth - 6), middle.h},
                                        label, 2);
                valueLayout = prepareText({static_cast<int16_t>(middle.x + middle.w - valueWidth), middle.y, valueWidth,
                                           middle.h},
                                          valueText, 2, TextAlign::Right);
            }
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const int16_t dx = rect.x - bounds.x, dy = rect.y - bounds.y;
                const uint16_t surface = color(ui::themes::ColorRole::SurfaceMuted);
                const uint16_t outline = color(ui::themes::ColorRole::Outline);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 5, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 5, outline);
                output.drawFastVLine(static_cast<int16_t>(rect.x + buttonWidth), static_cast<int16_t>(rect.y + 4),
                                     static_cast<int16_t>(rect.h - 8), outline);
                output.drawFastVLine(static_cast<int16_t>(rect.x + rect.w - buttonWidth),
                                     static_cast<int16_t>(rect.y + 4), static_cast<int16_t>(rect.h - 8), outline);

                const uint16_t muted = color(ui::themes::ColorRole::Muted);
                drawText(output, minusText, displayedValue > minimum ? color(activeRole) : muted, dx, dy);
                drawText(output, plusText, displayedValue < maximum ? color(activeRole) : muted, dx, dy);
                drawText(output, labelText, color(themes::Foreground), dx, dy);
                drawText(output, valueLayout, color(activeRole), dx, dy);
            });
        }
        if (changed)
            value = displayedValue;
        return changed;
    }

    void Context::dial(Rect rect, int value, int minimum, int maximum, std::string_view labelText) {
        rect = paintBounds(rect);
        value = std::clamp(value, minimum, maximum);
        uint32_t state = combine(signature(labelText), static_cast<uint32_t>(value));
        state = combine(state, static_cast<uint32_t>(minimum));
        state = combine(state, static_cast<uint32_t>(maximum));
        if (!claim(Kind::Dial, rect, state).changed) {
            return;
        }
        const int16_t radius = std::max<int16_t>(2, std::min(rect.w, rect.h) / 2 - 2);
        const int16_t cx = static_cast<int16_t>(rect.x + rect.w / 2);
        const int16_t cy = static_cast<int16_t>(rect.y + rect.h / 2);
        int16_t needleX = cx, needleY = cy;
        if (maximum > minimum) {
            constexpr float kPi = 3.14159265358979323846f;
            const float angle = (-135.0f + 270.0f * (value - minimum) / (maximum - minimum)) * kPi / 180.0f;
            needleX = static_cast<int16_t>(cx + std::cos(angle) * (radius - 4));
            needleY = static_cast<int16_t>(cy + std::sin(angle) * (radius - 4));
        }
        const auto label = prepareText({rect.x, static_cast<int16_t>(cy + radius / 2), rect.w, textHeight(1)},
                                       labelText, 1, TextAlign::Center);
        const Rect bounds = rect;
        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            const int16_t dx = rect.x - bounds.x, dy = rect.y - bounds.y;
            output.drawCircle(cx + dx, cy + dy, radius, color(ui::themes::ColorRole::ProgressTrack));
            if (maximum > minimum)
                output.drawLine(cx + dx, cy + dy, needleX + dx, needleY + dy, color(ui::themes::ColorRole::Accent));
            drawText(output, label, color(themes::Muted), dx, dy);
        });
    }

    void Context::hourglass(Rect rect, uint16_t progress, bool paused, bool complete, ui::themes::ColorRole sandRole,
                            bool reversed, std::string_view time) {
        rect = paintBounds(rect);
        progress = std::min<uint16_t>(progress, 1000);
        uint32_t state = combine(progress, paused);
        state = combine(state, complete);
        state = combine(state, sandRole);
        state = combine(state, reversed);
        if constexpr (displayWriteAlignment() == 2)
            state = signature(time, state);
        const bool visualChanged = claim(Kind::Hourglass, rect, state).changed;
        const auto drawTime = [&] {
            if (time.empty())
                return;
            const Rect timeRect{static_cast<int16_t>(rect.x + (rect.w - 120) / 2), rect.y, 120, 28};
            if (claim(Kind::Label, timeRect, signature(time, state)).changed)
                drawText(timeRect, time, 3, color(sandRole), TextAlign::Center);
        };
        if (!visualChanged) {
            if constexpr (displayWriteAlignment() == 1)
                drawTime();
            return;
        }

        TextLayout timeText;
        if constexpr (displayWriteAlignment() == 2)
            timeText = prepareText({static_cast<int16_t>((rect.w - 120) / 2), 0, 120, 28}, time, 3, TextAlign::Center);
        constexpr int16_t segments = 14;
        const int16_t inset = std::max<int16_t>(3, std::min(rect.w, rect.h) / 12);
        const int16_t chamberWidth = std::max<int16_t>(1, (rect.w - 2 * inset - 1) / 2);
        const int16_t chamberHeight = std::max<int16_t>(3, (rect.h - 2 * inset - 1) / 2);
        const int16_t waist = std::max<int16_t>(2, chamberHeight / 14);
        const int16_t capWidth = std::max<int16_t>(8, std::min<int16_t>(16, rect.h / 8));
        std::array<int16_t, segments + 1> profile{};
        std::array<std::array<int16_t, 4>, segments * 4> outlineSegments{};
        if (rect.w > rect.h) {
            for (int16_t step = 0; step <= segments; ++step) {
                const int16_t offset = static_cast<int16_t>(chamberWidth * step / segments);
                int16_t curve = static_cast<int16_t>(offset * 100 / chamberWidth);
                curve = static_cast<int16_t>(curve * curve / 100);
                curve = static_cast<int16_t>(curve * curve / 100);
                profile[step] = static_cast<int16_t>(chamberHeight - (chamberHeight - waist) * curve / 100);
            }
            const int16_t right = rect.w - inset - 1;
            const int16_t centerY = inset + (rect.h - 2 * inset - 1) / 2;
            for (int16_t step = 1; step <= segments; ++step) {
                const int16_t previousOffset = chamberWidth * (step - 1) / segments;
                const int16_t offset = chamberWidth * step / segments;
                const int16_t previousHalf = profile[step - 1], half = profile[step];
                outlineSegments[(step - 1) * 4] = {static_cast<int16_t>(inset + previousOffset),
                                                   static_cast<int16_t>(centerY - previousHalf),
                                                   static_cast<int16_t>(inset + offset),
                                                   static_cast<int16_t>(centerY - half)};
                outlineSegments[(step - 1) * 4 + 1] = {static_cast<int16_t>(inset + previousOffset),
                                                       static_cast<int16_t>(centerY + previousHalf),
                                                       static_cast<int16_t>(inset + offset),
                                                       static_cast<int16_t>(centerY + half)};
                outlineSegments[(step - 1) * 4 + 2] = {static_cast<int16_t>(right - previousOffset),
                                                       static_cast<int16_t>(centerY - previousHalf),
                                                       static_cast<int16_t>(right - offset),
                                                       static_cast<int16_t>(centerY - half)};
                outlineSegments[(step - 1) * 4 + 3] = {static_cast<int16_t>(right - previousOffset),
                                                       static_cast<int16_t>(centerY + previousHalf),
                                                       static_cast<int16_t>(right - offset),
                                                       static_cast<int16_t>(centerY + half)};
            }
        }
        const int16_t pileWidth = static_cast<int16_t>(chamberWidth * 65 / 100);
        const uint32_t sourceTarget = static_cast<uint32_t>(1000 - progress) * pileWidth * pileWidth;
        int16_t sourceColumns = 0;
        if (rect.w > rect.h)
            while (sourceColumns < pileWidth
                   && static_cast<uint32_t>(sourceColumns + 1) * (sourceColumns + 1) * 1000U <= sourceTarget)
                ++sourceColumns;
        const int16_t receivedColumns =
            static_cast<int16_t>((static_cast<uint32_t>(pileWidth) * progress + 999U) / 1000U);
        const int16_t plateauColumns = receivedColumns == 0 ? 0 : std::max<int16_t>(1, receivedColumns / 6);
        const int16_t slopeColumns = std::max<int16_t>(1, receivedColumns - plateauColumns);
        const auto halfAt = [&](int16_t offsetFromBase) {
            const int32_t scaled =
                static_cast<int32_t>(std::clamp<int16_t>(offsetFromBase, 0, chamberWidth)) * segments;
            const int16_t step = static_cast<int16_t>(scaled / chamberWidth);
            if (step >= segments)
                return profile[segments];
            const int32_t remainder = scaled - static_cast<int32_t>(step) * chamberWidth;
            return static_cast<int16_t>(profile[step]
                                        + static_cast<int32_t>(profile[step + 1] - profile[step]) * remainder
                                              / chamberWidth);
        };
        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            const auto drawStripTime = [&] {
                if constexpr (displayWriteAlignment() == 2)
                    drawText(output, timeText, color(sandRole), rect.x, rect.y);
            };
            const uint16_t ink = color(paused ? ui::themes::ColorRole::Muted : sandRole);
            const uint16_t outline = color(ui::themes::ColorRole::Foreground);
            const int16_t left = static_cast<int16_t>(rect.x + inset);
            const int16_t right = static_cast<int16_t>(rect.x + rect.w - inset - 1);
            const int16_t top = static_cast<int16_t>(rect.y + inset);
            const int16_t bottom = static_cast<int16_t>(rect.y + rect.h - inset - 1);
            const int16_t centerX = static_cast<int16_t>(left + (right - left) / 2);
            const int16_t centerY = static_cast<int16_t>(top + (bottom - top) / 2);

            if (rect.w > rect.h) {
                const int16_t baseTop = static_cast<int16_t>(rect.y + 2);
                const int16_t baseHeight = static_cast<int16_t>(rect.h - 4);
                const uint16_t base = color(ui::themes::ColorRole::SurfaceActive);
                output.fillRoundRect(static_cast<int16_t>(left - capWidth / 2), baseTop, capWidth, baseHeight,
                                     capWidth / 2, base);
                output.drawRoundRect(static_cast<int16_t>(left - capWidth / 2), baseTop, capWidth, baseHeight,
                                     capWidth / 2, outline);
                output.fillRoundRect(static_cast<int16_t>(right - capWidth / 2), baseTop, capWidth, baseHeight,
                                     capWidth / 2, base);
                output.drawRoundRect(static_cast<int16_t>(right - capWidth / 2), baseTop, capWidth, baseHeight,
                                     capWidth / 2, outline);

                for (const auto& segment: outlineSegments) {
                    const int16_t x0 = rect.x + segment[0], y0 = rect.y + segment[1];
                    const int16_t x1 = rect.x + segment[2], y1 = rect.y + segment[3];
                    // Reject whole segments without changing Bresenham endpoints or thick-edge pixels.
                    if (std::max(x0, x1) < 0 || std::min(x0, x1) >= output.width() || std::max(y0, y1) + 1 < 0
                        || std::min(y0, y1) - 1 >= output.height())
                        continue;
                    for (int16_t thickness = -1; thickness <= 1; ++thickness) {
                        output.drawLine(x0, static_cast<int16_t>(y0 + thickness), x1,
                                        static_cast<int16_t>(y1 + thickness), outline);
                    }
                }
                const int16_t leftGlassEdge = static_cast<int16_t>(left + capWidth / 2 + 2);
                const int16_t rightGlassEdge = static_cast<int16_t>(right - capWidth / 2 - 2);
                for (int16_t column = 1; column <= sourceColumns; ++column) {
                    const int16_t x =
                        reversed ? static_cast<int16_t>(centerX + column) : static_cast<int16_t>(centerX - column);
                    if (x < 0 || x >= output.width())
                        continue;
                    const int16_t half = halfAt(static_cast<int16_t>(chamberWidth - column));
                    output.drawFastVLine(x, static_cast<int16_t>(centerY - half + 3),
                                         std::max<int16_t>(1, static_cast<int16_t>(half * 2 - 5)), ink);
                }
                const int16_t receivedBaseHalf = receivedColumns == 0 ? 0 : chamberHeight;
                for (int16_t column = 1; column <= receivedColumns; ++column) {
                    const int16_t x = reversed ? static_cast<int16_t>(leftGlassEdge + column)
                                               : static_cast<int16_t>(rightGlassEdge - column);
                    if (x < 0 || x >= output.width())
                        continue;
                    const int16_t pileHalf =
                        column <= plateauColumns
                            ? receivedBaseHalf
                            : static_cast<int16_t>(receivedBaseHalf * (receivedColumns - column) / slopeColumns);
                    const int16_t half = std::min(halfAt(column), pileHalf);
                    output.drawFastVLine(x, static_cast<int16_t>(centerY - half + 3),
                                         std::max<int16_t>(1, static_cast<int16_t>(half * 2 - 5)), ink);
                }
                if (!paused && progress > 0 && progress < 1000) {
                    if (reversed) {
                        const int16_t streamX = static_cast<int16_t>(left + receivedColumns + 1);
                        output.drawFastHLine(streamX, centerY,
                                             std::max<int16_t>(1, static_cast<int16_t>(centerX - streamX)), ink);
                    } else {
                        output.drawFastHLine(static_cast<int16_t>(centerX + 1), centerY,
                                             std::max<int16_t>(1, static_cast<int16_t>(right - receivedColumns - centerX
                                                                                       - 2)),
                                             ink);
                    }
                }
                if (paused) {
                    output.fillRect(static_cast<int16_t>(centerX - 6), static_cast<int16_t>(bottom - 17), 4, 13, ink);
                    output.fillRect(static_cast<int16_t>(centerX + 2), static_cast<int16_t>(bottom - 17), 4, 13, ink);
                } else if (complete) {
                    output.drawLine(static_cast<int16_t>(centerX - 7), static_cast<int16_t>(bottom - 11),
                                    static_cast<int16_t>(centerX - 2), static_cast<int16_t>(bottom - 6), ink);
                    output.drawLine(static_cast<int16_t>(centerX - 2), static_cast<int16_t>(bottom - 6),
                                    static_cast<int16_t>(centerX + 8), static_cast<int16_t>(bottom - 17), ink);
                }
                markDrawn();
                drawStripTime();
                return;
            }

            const int16_t chamberHeight = std::max<int16_t>(1, static_cast<int16_t>(centerY - top - 3));

            output.drawFastHLine(left, top, static_cast<int16_t>(right - left + 1), outline);
            output.drawFastHLine(left, bottom, static_cast<int16_t>(right - left + 1), outline);
            output.drawLine(left, static_cast<int16_t>(top + 1), centerX, centerY, outline);
            output.drawLine(right, static_cast<int16_t>(top + 1), centerX, centerY, outline);
            output.drawLine(centerX, centerY, left, static_cast<int16_t>(bottom - 1), outline);
            output.drawLine(centerX, centerY, right, static_cast<int16_t>(bottom - 1), outline);

            const int16_t topRows = static_cast<int16_t>(chamberHeight * (1000 - progress) / 1000);
            for (int16_t row = 0; row < topRows; ++row) {
                const int16_t y = static_cast<int16_t>(centerY - 2 - row);
                const int16_t half =
                    std::max<int16_t>(1, static_cast<int16_t>((right - left) * (row + 1) / (2 * chamberHeight)));
                output.drawFastHLine(static_cast<int16_t>(centerX - half), y, static_cast<int16_t>(half * 2 + 1), ink);
            }
            const int16_t bottomRows = static_cast<int16_t>(chamberHeight * progress / 1000);
            for (int16_t row = 0; row < bottomRows; ++row) {
                const int16_t y = static_cast<int16_t>(bottom - 2 - row);
                const int16_t half = std::max<int16_t>(1, static_cast<int16_t>((right - left) * (bottomRows - row)
                                                                               / (2 * chamberHeight)));
                output.drawFastHLine(static_cast<int16_t>(centerX - half), y, static_cast<int16_t>(half * 2 + 1), ink);
            }
            if (!paused && progress > 0 && progress < 1000)
                output.drawFastVLine(centerX, static_cast<int16_t>(centerY + 1),
                                     std::max<int16_t>(1, static_cast<int16_t>(bottom - bottomRows - centerY - 2)),
                                     ink);
            markDrawn();
            drawStripTime();
        });
        if constexpr (displayWriteAlignment() == 1)
            drawTime();
    }

    bool Context::redraw(Rect rect, uint32_t state, bool opaque) {
        return claim(opaque ? Kind::Opaque : Kind::Custom, rect, state).changed;
    }

    uint16_t Context::color(ui::themes::ColorRole role) const {
        if (theme_ == nullptr) {
            return role == ui::themes::ColorRole::Background ? kFallbackBlack : kFallbackWhite;
        }
        return ui::themes::color(theme_->definition.colors, role);
    }

    uint16_t Context::blend(ui::themes::ColorRole role, uint8_t alpha) const {
        const uint16_t foreground = color(role);
        const uint16_t background = color(ui::themes::ColorRole::Background);
        const uint8_t fgR = static_cast<uint8_t>((foreground >> 11) & 0x1F);
        const uint8_t fgG = static_cast<uint8_t>((foreground >> 5) & 0x3F);
        const uint8_t fgB = static_cast<uint8_t>(foreground & 0x1F);
        const uint8_t bgR = static_cast<uint8_t>((background >> 11) & 0x1F);
        const uint8_t bgG = static_cast<uint8_t>((background >> 5) & 0x3F);
        const uint8_t bgB = static_cast<uint8_t>(background & 0x1F);
        return static_cast<uint16_t>((((fgR * alpha + bgR * (255 - alpha)) / 255) << 11)
                                     | (((fgG * alpha + bgG * (255 - alpha)) / 255) << 5)
                                     | ((fgB * alpha + bgB * (255 - alpha)) / 255));
    }

    uint32_t Context::signature(std::string_view text, uint32_t seed) {
        return Fnv1a::append(seed, text);
    }

    uint32_t Context::combine(uint32_t seed, uint32_t value) {
        return (seed ^ value) * Fnv1a::kPrime;
    }

    Context::Claim Context::claim(Kind kind, Rect rect, uint32_t state) {
        rect = paintBounds(rect);
        const size_t index = nextSlot_++;
        const bool clearNew = kind != Kind::Touch && (displayWriteAlignment() == 1 || kind == Kind::Custom);
        if (index >= kSlotCapacity) {
            if (clearNew)
                clear(rect);
            return {index, true};
        }

        Slot& slot = slots_[index];
        const bool structureChanged = slot.valid && (slot.kind != kind || !(slot.rect == rect));
        if (structureChanged && capturedSlot_ == index) {
            capturedSlot_ = kSlotCapacity;
        }
        const bool changed = !slot.valid || structureChanged || slot.signature != state;
        if (changed) {
            if (slot.valid && slot.kind != Kind::Touch && (!(slot.rect == rect) || kind == Kind::Touch)) {
                clear(slot.rect);
            }
            if (clearNew)
                clear(rect);
            slot = {rect, state, kind, true};
        }
        slotCount_ = std::max(slotCount_, index + 1);
        return {index, changed};
    }

    void Context::clear(Rect rect) {
        if (rect.w <= 0 || rect.h <= 0) {
            return;
        }
        paint(rect, [&](Arduino_GFX& output, Rect bounds) {
            output.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, color(ui::themes::ColorRole::Background));
        });
        markDrawn();
    }

    void Context::markDrawn() {
        drew_ = true;
    }

    void Context::drawText(Rect rect, std::string_view text, uint8_t textSize, uint16_t textColor, TextAlign align,
                           uint8_t maxLines, std::string_view textLocale) {
        rect = paintBounds(rect);
        const auto layout = prepareText(rect, text, textSize, align, maxLines, textLocale);
        paint(rect, [&](Arduino_GFX& output, Rect translated) {
            drawText(output, layout, textColor, translated.x - rect.x, translated.y - rect.y);
        });
    }

    void Context::portraitText(Rect rect, std::string_view text, uint8_t textSize, uint16_t textColor, TextAlign align,
                               uint8_t maxLines, std::string_view textLocale) {
        Rect destination{};
        if constexpr (displayWriteAlignment() == 2) {
            destination = paintBounds(rotateClockwise(rect, height()));
            rect = {static_cast<int16_t>(height() - destination.y - destination.h), destination.x, destination.h,
                    destination.w};
        }
        const auto layout = prepareText(rect, text, textSize, align, maxLines, textLocale);
        if constexpr (displayWriteAlignment() == 2) {
            paint(destination, [&](Arduino_GFX& output, Rect translated) {
                PortraitGfx portrait{output, height(), width(), static_cast<int16_t>(translated.x - destination.x),
                                     static_cast<int16_t>(translated.y - destination.y)};
                drawText(portrait, layout, textColor);
            });
        } else {
            PortraitGfx portrait{gfx_};
            drawText(portrait, layout, textColor);
        }
    }

    void Context::portraitVerticalText(Rect rect, std::string_view text, uint8_t textSize, uint16_t textColor,
                                       std::string_view textLocale) {
        const int16_t lineHeight = textHeightFor(text, textSize);
        if (lineHeight <= 0)
            return;
        Rect destination{};
        if constexpr (displayWriteAlignment() == 2) {
            destination = paintBounds(rotateClockwise(rect, height()));
            rect = {static_cast<int16_t>(height() - destination.y - destination.h), destination.x, destination.h,
                    destination.w};
        }
        TextLayout layout;
        layout.lines.reserve(std::min<size_t>(Utf8Text::count(text), std::max<int16_t>(0, rect.h) / lineHeight));
        for (int16_t y = rect.y; !text.empty() && y + lineHeight <= rect.y + rect.h;
             y = static_cast<int16_t>(y + lineHeight)) {
            const char* glyph = text.data();
            const size_t remaining = text.size();
            uint32_t codepoint = 0;
            Utf8Text::next(text, codepoint);
            appendText(layout, {rect.x, y, rect.w, lineHeight}, {glyph, remaining - text.size()}, textSize,
                       TextAlign::Center, 1, textLocale);
        }
        if constexpr (displayWriteAlignment() == 2) {
            paint(destination, [&](Arduino_GFX& output, Rect translated) {
                PortraitGfx portrait{output, height(), width(), static_cast<int16_t>(translated.x - destination.x),
                                     static_cast<int16_t>(translated.y - destination.y)};
                drawText(portrait, layout, textColor);
            });
        } else {
            PortraitGfx portrait{gfx_};
            drawText(portrait, layout, textColor);
        }
    }

    void Context::portraitBattery(Rect rect, uint8_t percent, bool charging, std::string_view labelText,
                                  bool showIcon) {
        if (!showIcon && labelText.empty())
            return;
        Rect destination{};
        if constexpr (displayWriteAlignment() == 2) {
            destination = paintBounds(rotateClockwise(rect, height()));
            rect = {static_cast<int16_t>(height() - destination.y - destination.h), destination.x, destination.h,
                    destination.w};
        }
        constexpr int16_t iconWidth = 29;
        constexpr int16_t iconHeight = 13;
        constexpr int16_t labelGap = 5;
        const int16_t iconAreaWidth = showIcon ? iconWidth + labelGap : 0;
        const uint16_t ink = color(ui::themes::ColorRole::Muted);
        const uint16_t surface = color(ui::themes::ColorRole::Background);
        const int16_t iconY = static_cast<int16_t>(rect.y + std::max<int16_t>(0, (rect.h - iconHeight) / 2));
        const auto label = prepareText({static_cast<int16_t>(rect.x + iconAreaWidth), rect.y,
                                        static_cast<int16_t>(std::max<int16_t>(0, rect.w - iconAreaWidth)), rect.h},
                                       labelText, 2, TextAlign::Left);
        const auto draw = [&](Arduino_GFX& output) {
            if (showIcon)
                drawBatteryIcon(output, {rect.x, iconY, iconWidth, iconHeight}, percent, charging, ink, surface);
            drawText(output, label, ink);
        };
        if constexpr (displayWriteAlignment() == 2) {
            paint(destination, [&](Arduino_GFX& output, Rect translated) {
                PortraitGfx portrait{output, height(), width(), static_cast<int16_t>(translated.x - destination.x),
                                     static_cast<int16_t>(translated.y - destination.y)};
                draw(portrait);
            });
        } else {
            PortraitGfx portrait{gfx_};
            draw(portrait);
        }
    }

    TextLayout Context::prepareText(Rect rect, std::string_view text, uint8_t textSize, TextAlign align,
                                    uint8_t maxLines, std::string_view textLocale) {
        TextLayout layout;
        if (rect.w <= 0 || rect.h <= 0 || text.empty())
            return layout;
        layout.lines.reserve(std::min<uint8_t>(maxLines, 2));
        appendText(layout, rect, text, textSize, align, maxLines, textLocale);
        return layout;
    }

    void Context::appendText(TextLayout& layout, Rect rect, std::string_view text, uint8_t textSize, TextAlign align,
                             uint8_t maxLines, std::string_view textLocale) {
        if (rect.w <= 0 || rect.h <= 0 || text.empty())
            return;
        const locales::UiAssets* assets = fontAssetsFor(text, textLocale);
        const bool externalFont = assets != nullptr;
        const uint8_t cellWidth = externalFont ? locales::uiFontCellWidth(assets->font) : kUiFontCellWidth;
        const uint8_t fontHeight = externalFont ? locales::uiFontHeight(assets->font) : kUiFontHeight;
        const size_t codepoints = Utf8Text::count(text);
        uint8_t size = std::max<uint8_t>(1, textSize);
        while (size > 1) {
            const size_t columns = static_cast<size_t>(rect.w) / (cellWidth * size);
            const size_t lines = columns == 0 ? SIZE_MAX : (codepoints + columns - 1) / columns;
            if (lines <= maxLines && static_cast<size_t>(fontHeight) * size * lines <= static_cast<size_t>(rect.h))
                break;
            --size;
        }
        const size_t capacity = static_cast<size_t>(std::max<int16_t>(0, rect.w) / (cellWidth * size));
        if (capacity == 0)
            return;
        const uint8_t* font = externalFont ? assets->font.data() : u8g2_font_rsvpnano_ui_6x9_tf;
        gfx_.setFont(font);
        gfx_.setUTF8Print(true);
        gfx_.setTextSize(size);
        gfx_.setTextWrap(false);
        const bool rightToLeft = (externalFont ? assets->direction : languageAssets_.direction) == TextDirection::rtl;
        if (align == TextAlign::Start)
            align = rightToLeft ? TextAlign::Right : TextAlign::Left;

        std::string_view first = text;
        std::string_view second;
        if (maxLines > 1 && codepoints > capacity) {
            size_t split = Utf8Text::prefixBytes(text, capacity);
            const size_t space = text.rfind(' ', split);
            if (space != std::string_view::npos && Utf8Text::count(text.substr(0, space)) >= capacity / 2)
                split = space;
            first = text.substr(0, split);
            second = text.substr(split);
            while (!second.empty() && second.front() == ' ')
                second.remove_prefix(1);
        }

        const uint8_t lineCount = second.empty() ? 1 : 2;
        const int16_t lineHeight = static_cast<int16_t>(fontHeight * size);
        const int16_t firstY =
            static_cast<int16_t>(rect.y + std::max<int16_t>(0, (rect.h - lineHeight * lineCount) / 2));
        const auto appendLine = [&](std::string_view line, size_t lineCodepoints, int16_t y) {
            const bool truncated = lineCodepoints > capacity;
            const size_t length = truncated && capacity > 3 ? Utf8Text::prefixBytes(line, capacity - 3)
                                : truncated                 ? 0
                                                            : line.size();
            const size_t dots = truncated ? std::min<size_t>(3, capacity) : 0;
            const std::string_view visible = line.substr(0, length);
            const bool bidiReady = rightToLeft && bidiAnalysis_.reset(visible, TextDirection::rtl)
                                && bidiAnalysis_.resolve({0, visible.size()}, bidiLine_);
            if (bidiReady)
                BidiText::visualCodepoints(visible, bidiLine_, bidiCodepoints_);

            const auto appendVisual = [&](std::string& output) {
                if (!bidiReady) {
                    output.append(visible);
                    return;
                }
                std::array<char, 4> encoded{};
                for (const BidiText::Codepoint& codepoint: bidiCodepoints_)
                    output.append(encoded.data(), Utf8Text::encode(codepoint.value, encoded));
            };

            std::string rendered;
            rendered.reserve(visible.size() + dots);
            if (rightToLeft)
                rendered.append(dots, '.');
            appendVisual(rendered);
            if (!rightToLeft)
                rendered.append(dots, '.');

            int16_t inkX = 0;
            int16_t inkY = 0;
            uint16_t inkWidth = 0;
            uint16_t inkHeight = 0;
            gfx_.getTextBounds(rendered.c_str(), 0, 0, &inkX, &inkY, &inkWidth, &inkHeight);
            const int16_t left = static_cast<int16_t>(rect.x - inkX);
            const int16_t x = align == TextAlign::Center
                                ? std::max<int16_t>(left, static_cast<int16_t>(rect.x + (rect.w - inkWidth) / 2 - inkX))
                            : align == TextAlign::Right
                                ? std::max<int16_t>(left, static_cast<int16_t>(rect.x + rect.w - inkWidth - inkX))
                                : left;
            const int16_t baseline = static_cast<int16_t>(y + lineHeight - size);
            // A literal newline returns to the viewport edge, not this line's cursor origin.
            const Rect ink = rendered.find('\n') == std::string::npos
                               ? Rect{static_cast<int16_t>(x + inkX), static_cast<int16_t>(baseline + inkY),
                                      static_cast<int16_t>(inkWidth), static_cast<int16_t>(inkHeight)}
                               : Rect{};
            layout.lines.push_back({std::move(rendered), font, size, x, baseline, ink});
        };
        appendLine(first, second.empty() ? codepoints : Utf8Text::count(first), firstY);
        if (!second.empty())
            appendLine(second, Utf8Text::count(second), static_cast<int16_t>(firstY + lineHeight));
    }

    void Context::drawText(Arduino_GFX& output, const TextLayout& layout, uint16_t textColor, int16_t dx, int16_t dy) {
        if (layout.lines.empty())
            return;
        output.setUTF8Print(true);
        output.setTextWrap(false);
        output.setTextColor(textColor);
        for (const auto& line: layout.lines) {
            const Rect ink{static_cast<int16_t>(line.ink.x + dx), static_cast<int16_t>(line.ink.y + dy), line.ink.w,
                           line.ink.h};
            const Rect visible = intersection(ink, {0, 0, output.width(), output.height()});
            if (line.ink.w > 0 && line.ink.h > 0 && (visible.w <= 0 || visible.h <= 0))
                continue;
            output.setFont(line.font);
            output.setTextSize(line.size);
            output.setCursor(static_cast<int16_t>(line.x + dx), static_cast<int16_t>(line.y + dy));
            for (const char byte: line.text)
                output.write(static_cast<uint8_t>(byte));
        }
        drew_ = true;
    }

    int Context::valueAt(Rect rect, uint16_t x, int minimum, int maximum, int step) const {
        if (rect.w <= 1 || maximum <= minimum) {
            return minimum;
        }
        const int clampedX = std::clamp<int>(x, rect.x, rect.x + rect.w - 1) - rect.x;
        int value = minimum + static_cast<int>((static_cast<int64_t>(maximum - minimum) * clampedX) / (rect.w - 1));
        if (step > 1) {
            value = minimum + ((value - minimum + step / 2) / step) * step;
        }
        return std::clamp(value, minimum, maximum);
    }

    bool Context::tapped(size_t slot, Rect rect, bool enabled) {
        if (!enabled) {
            if (capturedSlot_ == slot)
                capturedSlot_ = kSlotCapacity;
            return false;
        }
        const Touch* event = touch();
        if (event == nullptr || slot >= kSlotCapacity)
            return false;
        if (hasTouch(*event, TouchStart) && contains(rect, event->x, event->y))
            capturedSlot_ = slot;
        if (!hasTouch(*event, TouchRelease) || capturedSlot_ != slot)
            return false;
        capturedSlot_ = kSlotCapacity;
        return hasTouch(*event, TouchTap);
    }

    void Context::resetTouchGesture() {
        rotaryDragging_ = false;
        touchActive_ = false;
        touchHoldEmitted_ = false;
        touchOutsideSamples_ = 0;
        touchPending_ = false;
        touchStartedAtMs_ = 0;
        touchStartX_ = 0;
        touchStartY_ = 0;
        touchLastX_ = 0;
        touchLastY_ = 0;
        capturedSlot_ = kSlotCapacity;
    }

    TouchContact Context::mapTouch(TouchContact contact) const {
        const uint16_t maxX = std::max(touchSource_.surface.width, uint16_t{1}) - 1;
        const uint16_t maxY = std::max(touchSource_.surface.height, uint16_t{1}) - 1;
        const uint16_t rawX = std::clamp<uint16_t>(contact.x, 0, maxX);
        const uint16_t rawY = std::clamp<uint16_t>(contact.y, 0, maxY);
        switch (touchOrientation_) {
        case Orientation::LandscapeFlipped:
            return {true, rawY, static_cast<uint16_t>(maxX - rawX)};
        case Orientation::PortraitFlipped:
            return {true, static_cast<uint16_t>(maxX - rawX), static_cast<uint16_t>(maxY - rawY)};
        case Orientation::Landscape:
            return {true, static_cast<uint16_t>(maxY - rawY), rawX};
        default:
            return {true, rawX, rawY};
        }
    }

    bool Context::updateTouch(const TouchContact& contact, uint32_t nowMs) {
        if (!contact.touched) {
            if (!touchActive_)
                return false;
            const bool tapped = !touchHoldEmitted_ && nowMs - touchStartedAtMs_ <= touchSource_.timing.tapMaxDurationMs
                             && touchOutsideSamples_ < kTapCancelOutsideSamples;
            touchActive_ = false;
            touchEvent_ = {static_cast<uint8_t>(TouchRelease | (tapped ? TouchTap : TouchNone)), touchLastX_,
                           touchLastY_};
            return touchPending_ = true;
        }

        const TouchContact mapped = mapTouch(contact);
        if (!touchActive_) {
            touchActive_ = true;
            touchHoldEmitted_ = false;
            touchOutsideSamples_ = 0;
            touchStartedAtMs_ = nowMs;
            touchStartX_ = touchLastX_ = mapped.x;
            touchStartY_ = touchLastY_ = mapped.y;
            touchEvent_ = {TouchStart, mapped.x, mapped.y};
            return touchPending_ = true;
        }

        touchLastX_ = mapped.x;
        touchLastY_ = mapped.y;
        uint8_t actions = TouchMove;
        const uint16_t dx = std::max(touchLastX_, touchStartX_) - std::min(touchLastX_, touchStartX_);
        const uint16_t dy = std::max(touchLastY_, touchStartY_) - std::min(touchLastY_, touchStartY_);
        const bool outside = dx > touchSource_.timing.tapMoveTolerancePx || dy > touchSource_.timing.tapMoveTolerancePx;
        if (outside) {
            touchOutsideSamples_ = std::min<uint8_t>(touchOutsideSamples_ + 1, kTapCancelOutsideSamples);
        } else if (touchOutsideSamples_ < kTapCancelOutsideSamples) {
            touchOutsideSamples_ = 0;
        }
        if (!touchHoldEmitted_ && touchOutsideSamples_ < kTapCancelOutsideSamples
            && nowMs - touchStartedAtMs_ >= touchSource_.timing.holdMs) {
            touchHoldEmitted_ = true;
            actions |= TouchHold;
        }
        touchEvent_ = {actions, mapped.x, mapped.y};
        return touchPending_ = true;
    }

} // namespace ui
