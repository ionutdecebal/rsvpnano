#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr int32_t GFX_SKIP_OUTPUT_BEGIN = -2;

struct GFXfont {};

class Arduino_GFX {
public:
    explicit Arduino_GFX(int16_t width = 320, int16_t height = 172) : width_(width), height_(height) {}
    virtual ~Arduino_GFX() = default;

    virtual bool begin(int32_t = -1) {
        return true;
    }
    virtual void writePixelPreclipped(int16_t, int16_t, uint16_t) {
        ++writes;
    }
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x >= 0 && y >= 0 && x < width() && y < height())
            writePixelPreclipped(x, y, color);
    }
    virtual void writeFastHLine(int16_t x, int16_t y, int16_t width, uint16_t color) {
        drawFastHLine(x, y, width, color);
    }
    virtual void writeFastVLine(int16_t x, int16_t y, int16_t height, uint16_t color) {
        drawFastVLine(x, y, height, color);
    }
    virtual void writeFillRectPreclipped(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) {
        fillRect(x, y, width, height, color);
    }

    virtual int16_t width() const {
        return width_;
    }
    virtual int16_t height() const {
        return height_;
    }
    virtual void setRotation(uint8_t rotation) {
        if (((rotation_ ^ rotation) & 1U) != 0)
            std::swap(width_, height_);
        rotation_ = rotation;
    }
    virtual void fillScreen(uint16_t) {
        ++writes;
    }
    virtual void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t color) {
        ++writes;
        lastFillColor = color;
    }
    virtual void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {
        ++writes;
    }
    virtual void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {
        ++writes;
    }
    virtual void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {
        ++writes;
    }
    virtual void drawFastHLine(int16_t, int16_t, int16_t, uint16_t) {
        ++writes;
        ++horizontalLines;
    }
    virtual void drawFastVLine(int16_t x, int16_t, int16_t height, uint16_t) {
        ++writes;
        ++verticalLines;
        maxVerticalX = std::max(maxVerticalX, x);
        if (verticalLines == 1)
            firstVerticalHeight = height;
        else if (verticalLines == 2)
            secondVerticalHeight = height;
        lastVerticalHeight = height;
    }
    virtual void drawCircle(int16_t x, int16_t y, int16_t, uint16_t) {
        ++writes;
        recordCircle(x, y);
    }
    virtual void fillCircle(int16_t x, int16_t y, int16_t, uint16_t) {
        ++writes;
        recordCircle(x, y);
    }
    virtual void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) {
        ++writes;
    }
    virtual void fillTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {
        ++writes;
    }
    virtual void setFont(const GFXfont*) {}
    virtual void setFont(const uint8_t*) {}
    virtual void setUTF8Print(bool) {}
    virtual void setTextSize(uint8_t size) {
        lastTextSize = size;
    }
    virtual void setTextWrap(bool) {}
    virtual void setTextBound(int16_t, int16_t, int16_t, int16_t) {}
    virtual void setTextColor(uint16_t) {
        ++transparentTextColors;
    }
    virtual void setTextColor(uint16_t, uint16_t) {
        ++opaqueTextColors;
    }
    virtual void setCursor(int16_t x, int16_t y) {
        cursorX = x;
        cursorY = y;
    }
    virtual void getTextBounds(const char* text, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* width,
                               uint16_t* height) {
        size_t codepoints = 0;
        for (const auto* byte = reinterpret_cast<const unsigned char*>(text); *byte != 0; ++byte)
            codepoints += (*byte & 0xC0U) != 0x80U;
        *x1 = x;
        *y1 = y;
        *width = static_cast<uint16_t>(codepoints * 6 * lastTextSize);
        *height = static_cast<uint16_t>(9 * lastTextSize);
    }
    virtual void draw16bitRGBBitmap(int16_t, int16_t, uint16_t*, int16_t, int16_t) {
        ++writes;
        ++bitmapWrites;
    }
    virtual size_t write(uint8_t) {
        ++writes;
        ++textWrites;
        return 1;
    }
    virtual void flush(bool = false) {
        ++flushes;
    }

    int writes = 0;
    int textWrites = 0;
    int bitmapWrites = 0;
    int transparentTextColors = 0;
    int opaqueTextColors = 0;
    uint8_t lastTextSize = 0;
    int flushes = 0;
    int horizontalLines = 0;
    int verticalLines = 0;
    int16_t maxVerticalX = 0;
    int16_t firstVerticalHeight = 0;
    int16_t secondVerticalHeight = 0;
    int16_t lastVerticalHeight = 0;
    int circleWrites = 0;
    int16_t firstCircleX = 0;
    int16_t firstCircleY = 0;
    int16_t lastCircleX = 0;
    int16_t lastCircleY = 0;
    int16_t cursorX = 0;
    int16_t cursorY = 0;
    uint16_t lastFillColor = 0;
    uint8_t rotation_ = 0;

private:
    void recordCircle(int16_t x, int16_t y) {
        if (circleWrites++ == 0) {
            firstCircleX = x;
            firstCircleY = y;
        }
        lastCircleX = x;
        lastCircleY = y;
    }

    int16_t width_;
    int16_t height_;
};

// Pixel-recording test double for the dependency's canvas; never compiled into firmware.
class Arduino_Canvas : public Arduino_GFX {
public:
    Arduino_Canvas(int16_t width, int16_t height, Arduino_GFX*, int16_t = 0, int16_t = 0, uint8_t rotation = 0) :
            Arduino_GFX(width, height),
            physicalWidth_(width),
            physicalHeight_(height),
            pixels_(width * height) {
        setRotation(rotation);
    }
    uint16_t* getFramebuffer() {
        return pixels_.data();
    }
    void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
        assert(x >= 0 && y >= 0 && x < width() && y < height());
        int px = x, py = y;
        switch (rotation_) {
        case 1:
            px = physicalWidth_ - 1 - y;
            py = x;
            break;
        case 2:
            px = physicalWidth_ - 1 - x;
            py = physicalHeight_ - 1 - y;
            break;
        case 3:
            px = y;
            py = physicalHeight_ - 1 - x;
            break;
        default:
            break;
        }
        pixels_[py * physicalWidth_ + px] = color;
    }
    void fillScreen(uint16_t color) override {
        std::fill(pixels_.begin(), pixels_.end(), color);
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        const int x2 = std::min<int>(width(), x + w), y2 = std::min<int>(height(), y + h);
        for (int py = std::max<int>(0, y); py < y2; ++py)
            for (int px = std::max<int>(0, x); px < x2; ++px)
                writePixelPreclipped(px, py, color);
    }
    void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
        fillRect(x, y, w, h, c);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override {
        fillRect(x, y, w, 1, c);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override {
        fillRect(x, y, 1, h, c);
    }
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override {
        drawFastHLine(x, y, w, c);
    }
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override {
        drawFastVLine(x, y, h, c);
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
        drawFastHLine(x, y, w, c);
        drawFastHLine(x, y + h - 1, w, c);
        drawFastVLine(x, y, h, c);
        drawFastVLine(x + w - 1, y, h, c);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) override {
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        while (true) {
            drawPixel(x0, y0, c);
            if (x0 == x1 && y0 == y1)
                break;
            const int twice = 2 * error;
            if (twice >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twice <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t c) override {
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (dx * dx + dy * dy <= r * r)
                    drawPixel(x + dx, y + dy, c);
    }
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t c) override {
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                const int d = dx * dx + dy * dy;
                if (d <= r * r && d > (r - 1) * (r - 1))
                    drawPixel(x + dx, y + dy, c);
            }
    }
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) override {
        r = std::max<int16_t>(0, std::min<int16_t>(r, std::min(w, h) / 2));
        for (int py = y; py < y + h; ++py)
            for (int px = x; px < x + w; ++px) {
                const int cx = std::clamp(px, x + r - 1, x + w - r), cy = std::clamp(py, y + r - 1, y + h - r);
                if ((px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r)
                    drawPixel(px, py, c);
            }
    }
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t, uint16_t c) override {
        drawRect(x, y, w, h, c);
    }
    void fillTriangle(int16_t ax, int16_t ay, int16_t bx, int16_t by, int16_t cx, int16_t cy, uint16_t c) override {
        const auto side = [](int x, int y, int x0, int y0, int x1, int y1) {
            return (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
        };
        for (int y = std::max<int>(0, std::min({ay, by, cy})); y <= std::min<int>(height() - 1, std::max({ay, by, cy}));
             ++y)
            for (int x = std::max<int>(0, std::min({ax, bx, cx}));
                 x <= std::min<int>(width() - 1, std::max({ax, bx, cx})); ++x) {
                const int a = side(x, y, ax, ay, bx, by), b = side(x, y, bx, by, cx, cy),
                          d = side(x, y, cx, cy, ax, ay);
                if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0))
                    drawPixel(x, y, c);
            }
    }
    void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* data, int16_t w, int16_t h) override {
        for (int row = 0; row < h; ++row)
            for (int col = 0; col < w; ++col)
                drawPixel(x + col, y + row, data[row * w + col]);
    }

private:
    int16_t physicalWidth_, physicalHeight_;
    std::vector<uint16_t> pixels_;
};
