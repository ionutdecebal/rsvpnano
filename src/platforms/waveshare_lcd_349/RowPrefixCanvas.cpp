#if !defined(LITTLE_FOOT_PRINT)

#include "RowPrefixCanvas.h"

#include <algorithm>
#include <cstdlib>
#include <esp_heap_caps.h>

namespace {
    constexpr int tileRows = 8;

    // The landscape page and portrait panel have perpendicular scanlines.
    // Keep a tile's source rows in cache and write each native span in order.
    void copyLandscapeBitmap(const uint16_t* bitmap, int width, int height, uint16_t* framebuffer,
                             int x, int y, int screenWidth, int screenHeight, bool clockwise) {
        constexpr int tileColumns = 16;
        const int sourceStride = clockwise ? -width : width;
        for (int left = 0; left < width; left += tileColumns) {
            const int right = std::min(left + tileColumns, width);
            for (int top = 0; top < height; top += tileRows) {
                const int sourceRow = clockwise ? top + tileRows - 1 : top;
                const int nativeX = clockwise ? screenHeight - y - top - tileRows : y + top;
                for (int col = left; col < right; ++col) {
                    const int nativeY = clockwise ? x + col : screenWidth - 1 - x - col;
                    auto* dest = framebuffer + nativeY * screenHeight + nativeX;
                    const auto* source = bitmap + sourceRow * width + col;
#pragma GCC unroll 8
                    for (int row = 0; row < tileRows; ++row)
                        dest[row] = source[row * sourceStride];
                }
            }
        }
    }
}

RowPrefixCanvas::RowPrefixCanvas(int16_t w, int16_t h, Arduino_G& output, int16_t output_x, int16_t output_y,
                                 uint8_t r) :
        Arduino_GFX(w, h),
        _output(output),
        _output_x(output_x),
        _output_y(output_y) {
    MAX_X = WIDTH - 1;
    MAX_Y = HEIGHT - 1;
    setRotation(r);
}

RowPrefixCanvas::~RowPrefixCanvas() {
    if (_framebuffer) {
        free(_framebuffer);
    }
}

bool RowPrefixCanvas::begin(int32_t speed) {
    if (speed != GFX_SKIP_OUTPUT_BEGIN) {
        if (!_output.begin(speed)) {
            return false;
        }
    }

    if (!_framebuffer) {
        size_t s = _width * _height * 2;
#if defined(BOARD_HAS_PSRAM)
        _framebuffer = static_cast<uint16_t*>(
            heap_caps_aligned_alloc(16, s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#elif defined(ESP32)
        _framebuffer = (uint16_t*) aligned_alloc(16, s);
#else
        _framebuffer = (uint16_t*) malloc(s);
#endif
        if (!_framebuffer) {
            return false;
        }
    }

    return true;
}

void RowPrefixCanvas::writePixelPreclipped(int16_t x, int16_t y, uint16_t color) {
    markDirtyRect(x, y, 1, 1);

    uint16_t* fb = _framebuffer;
    switch (_rotation) {
    case 1:
        fb += (int32_t) x * _height;
        fb += _max_y - y;
        *fb = color;
        break;
    case 2:
        fb += (int32_t) (_max_y - y) * _width;
        fb += _max_x - x;
        *fb = color;
        break;
    case 3:
        fb += (int32_t) (_max_x - x) * _height;
        fb += y;
        *fb = color;
        break;
    default: // case 0:
        fb += (int32_t) y * _width;
        fb += x;
        *fb = color;
    }
}

void RowPrefixCanvas::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    markDirtyRect(x, y, 1, h);

    switch (_rotation) {
    case 1:
        writeFastHLineCore(_height - y - h, x, h, color);
        break;
    case 2:
        writeFastVLineCore(_max_x - x, _height - y - h, h, color);
        break;
    case 3:
        writeFastHLineCore(y, _max_x - x, h, color);
        break;
    default: // case 0:
        writeFastVLineCore(x, y, h, color);
    }
}

void RowPrefixCanvas::writeFastVLineCore(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (_ordered_in_range(x, 0, MAX_X) && h) { // X on screen, nonzero height
        if (h < 0) { // If negative height...
            y += h + 1; //   Move Y to top edge
            h = -h; //   Use positive height
        }
        if (y <= MAX_Y) { // Not off bottom
            int16_t y2 = y + h - 1;
            if (y2 >= 0) { // Not off top
                // Line partly or fully overlaps screen
                if (y < 0) {
                    y = 0;
                    h = y2 + 1;
                } // Clip top
                if (y2 > MAX_Y) {
                    h = MAX_Y - y + 1;
                } // Clip bottom

                uint16_t* fb = _framebuffer + ((int32_t) y * WIDTH) + x;
                while (h--) {
                    *fb = color;
                    fb += WIDTH;
                }
            }
        }
    }
}

void RowPrefixCanvas::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    markDirtyRect(x, y, w, 1);

    switch (_rotation) {
    case 1:
        writeFastVLineCore(_max_y - y, x, w, color);
        break;
    case 2:
        writeFastHLineCore(_width - x - w, _max_y - y, w, color);
        break;
    case 3:
        writeFastVLineCore(y, _width - x - w, w, color);
        break;
    default: // case 0:
        writeFastHLineCore(x, y, w, color);
    }
}

void RowPrefixCanvas::writeFastHLineCore(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (_ordered_in_range(y, 0, MAX_Y) && w) { // Y on screen, nonzero width
        if (w < 0) { // If negative width...
            x += w + 1; //   Move X to left edge
            w = -w; //   Use positive width
        }
        if (x <= MAX_X) { // Not off right
            int16_t x2 = x + w - 1;
            if (x2 >= 0) { // Not off left
                // Line partly or fully overlaps screen
                if (x < 0) {
                    x = 0;
                    w = x2 + 1;
                } // Clip left
                if (x2 > MAX_X) {
                    w = MAX_X - x + 1;
                } // Clip right

                uint16_t* fb = _framebuffer + ((int32_t) y * WIDTH) + x;
                while (w--) {
                    *(fb++) = color;
                }
            }
        }
    }
}

void RowPrefixCanvas::writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    markDirtyRect(x, y, w, h);

    if (_rotation > 0) {
        int16_t t = x;
        switch (_rotation) {
        case 1:
            x = WIDTH - y - h;
            y = t;
            t = w;
            w = h;
            h = t;
            break;
        case 2:
            x = WIDTH - x - w;
            y = HEIGHT - y - h;
            break;
        case 3:
            x = y;
            y = HEIGHT - t - w;
            t = w;
            w = h;
            h = t;
            break;
        }
    }
    uint16_t* row = _framebuffer;
    row += y * WIDTH;
    row += x;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            row[i] = color;
        }
        row += WIDTH;
    }
}

void RowPrefixCanvas::drawIndexedBitmap(int16_t x, int16_t y, uint8_t* bitmap, uint16_t* color_index, int16_t w,
                                        int16_t h, int16_t x_skip) {
    markDirtyRect(x, y, w, h);

    if (_rotation > 0) {
        Arduino_GFX::drawIndexedBitmap(x, y, bitmap, color_index, w, h, x_skip);
    } else {
        if (((x + w - 1) < 0) || // Outside left
            ((y + h - 1) < 0) || // Outside top
            (x > _max_x) || // Outside right
            (y > _max_y) // Outside bottom
        ) {
            return;
        } else {
            if ((y + h - 1) > _max_y) {
                h -= (y + h - 1) - _max_y;
            }
            if (y < 0) {
                bitmap -= y * (w + x_skip);
                h += y;
                y = 0;
            }
            if ((x + w - 1) > _max_x) {
                x_skip += (x + w - 1) - _max_x;
                w -= (x + w - 1) - _max_x;
            }
            if (x < 0) {
                bitmap -= x;
                x_skip -= x;
                w += x;
                x = 0;
            }
            uint16_t* row = _framebuffer;
            row += y * _width;
            row += x;
            int16_t i;
            int16_t wi;
            while (h--) {
                i = 0;
                wi = w;
                while (wi >= 4) {
                    uint32_t b32 = *((uint32_t*) bitmap);
                    row[i++] = color_index[(b32 & 0xff)];
                    row[i++] = color_index[(b32 & 0xff00) >> 8];
                    row[i++] = color_index[(b32 & 0xff0000) >> 16];
                    row[i++] = color_index[(b32 & 0xff000000) >> 24];
                    wi -= 4;
                    bitmap += 4;
                }
                while (i < w) {
                    row[i++] = color_index[*bitmap++];
                }
                bitmap += x_skip;
                row += _width;
            }
        }
    }
}

void RowPrefixCanvas::drawIndexedBitmap(int16_t x, int16_t y, uint8_t* bitmap, uint16_t* color_index,
                                        uint8_t chroma_key, int16_t w, int16_t h, int16_t x_skip) {
    markDirtyRect(x, y, w, h);

    if (_rotation > 0) {
        Arduino_GFX::drawIndexedBitmap(x, y, bitmap, color_index, chroma_key, w, h, x_skip);
    } else {
        if (((x + w - 1) < 0) || // Outside left
            ((y + h - 1) < 0) || // Outside top
            (x > _max_x) || // Outside right
            (y > _max_y) // Outside bottom
        ) {
            return;
        } else {
            if ((y + h - 1) > _max_y) {
                h -= (y + h - 1) - _max_y;
            }
            if (y < 0) {
                bitmap -= y * (w + x_skip);
                h += y;
                y = 0;
            }
            if ((x + w - 1) > _max_x) {
                x_skip += (x + w - 1) - _max_x;
                w -= (x + w - 1) - _max_x;
            }
            if (x < 0) {
                bitmap -= x;
                x_skip -= x;
                w += x;
                x = 0;
            }
            uint16_t* row = _framebuffer;
            row += y * _width;
            row += x;
            int16_t i;
            int16_t wi;
            uint8_t color_key;
            while (h--) {
                i = 0;
                wi = w;
                while (wi >= 4) {
                    uint32_t b32 = *((uint32_t*) bitmap);
                    color_key = (b32 & 0xff);
                    if (color_key != chroma_key) {
                        row[i] = color_index[color_key];
                    }
                    ++i;
                    color_key = (b32 & 0xff00) >> 8;
                    if (color_key != chroma_key) {
                        row[i] = color_index[color_key];
                    }
                    ++i;
                    color_key = (b32 & 0xff0000) >> 16;
                    if (color_key != chroma_key) {
                        row[i] = color_index[color_key];
                    }
                    ++i;
                    color_key = (b32 & 0xff000000) >> 24;
                    if (color_key != chroma_key) {
                        row[i] = color_index[color_key];
                    }
                    ++i;
                    wi -= 4;
                    bitmap += 4;
                }
                while (i < w) {
                    color_key = *bitmap++;
                    if (color_key != chroma_key) {
                        row[i] = color_index[color_key];
                    }
                    ++i;
                }
                bitmap += x_skip;
                row += _width;
            }
        }
    }
}

void RowPrefixCanvas::draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) {
    markDirtyRect(x, y, w, h);

    if ((_rotation == 1 || _rotation == 3) && w >= 32 && h >= tileRows
        && x >= 0 && y >= 0 && x + w <= _width && y + h <= _height) {
        const int tiledRows = h - h % tileRows;
        copyLandscapeBitmap(bitmap, w, tiledRows, _framebuffer, x, y, _width, _height, _rotation == 1);
        if (tiledRows == h)
            return;
        bitmap += tiledRows * w;
        y += tiledRows;
        h -= tiledRows;
    }

    switch (_rotation) {
    case 1:
        gfx_draw_bitmap_to_framebuffer_rotate_1(bitmap, w, h, _framebuffer, x, y, _width, _height);
        break;
    case 2:
        gfx_draw_bitmap_to_framebuffer_rotate_2(bitmap, w, h, _framebuffer, x, y, _width, _height);
        break;
    case 3:
        gfx_draw_bitmap_to_framebuffer_rotate_3(bitmap, w, h, _framebuffer, x, y, _width, _height);
        break;
    default: // case 0:
        gfx_draw_bitmap_to_framebuffer(bitmap, w, h, _framebuffer, x, y, _width, _height);
    }
}

void RowPrefixCanvas::draw16bitRGBBitmapWithTranColor(int16_t x, int16_t y, uint16_t* bitmap,
                                                      uint16_t transparent_color, int16_t w, int16_t h) {
    markDirtyRect(x, y, w, h);

    if (_rotation > 0) {
        Arduino_GFX::draw16bitRGBBitmapWithTranColor(x, y, bitmap, transparent_color, w, h);
    } else {
        if (((x + w - 1) < 0) || // Outside left
            ((y + h - 1) < 0) || // Outside top
            (x > _max_x) || // Outside right
            (y > _max_y) // Outside bottom
        ) {
            return;
        } else {
            int16_t x_skip = 0;
            if ((y + h - 1) > _max_y) {
                h -= (y + h - 1) - _max_y;
            }
            if (y < 0) {
                bitmap -= y * w;
                h += y;
                y = 0;
            }
            if ((x + w - 1) > _max_x) {
                x_skip = (x + w - 1) - _max_x;
                w -= x_skip;
            }
            if (x < 0) {
                bitmap -= x;
                x_skip -= x;
                w += x;
                x = 0;
            }
            uint16_t* row = _framebuffer;
            row += y * _width;
            row += x;
            int16_t i;
            int16_t wi;
            uint16_t p;
            while (h--) {
                i = 0;
                wi = w;
                while (wi >= 4) {
                    uint32_t b32 = *((uint32_t*) bitmap);
                    p = (b32 & 0xffff);
                    if (p != transparent_color) {
                        row[i] = p;
                    }
                    ++i;
                    p = (b32 & 0xffff0000) >> 16;
                    if (p != transparent_color) {
                        row[i] = p;
                    }
                    ++i;
                    wi -= 2;
                    bitmap += 2;
                }
                while (i < w) {
                    p = *bitmap++;
                    if (p != transparent_color) {
                        row[i] = p;
                    }
                    ++i;
                }
                bitmap += x_skip;
                row += _width;
            }
        }
    }
}

void RowPrefixCanvas::draw16bitBeRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) {
    markDirtyRect(x, y, w, h);

    if (_rotation > 0) {
        Arduino_GFX::draw16bitBeRGBBitmap(x, y, bitmap, w, h);
    } else {
        if (((x + w - 1) < 0) || // Outside left
            ((y + h - 1) < 0) || // Outside top
            (x > _max_x) || // Outside right
            (y > _max_y) // Outside bottom
        ) {
            return;
        } else {
            int16_t x_skip = 0;
            if ((y + h - 1) > _max_y) {
                h -= (y + h - 1) - _max_y;
            }
            if (y < 0) {
                bitmap -= y * w;
                h += y;
                y = 0;
            }
            if ((x + w - 1) > _max_x) {
                x_skip = (x + w - 1) - _max_x;
                w -= x_skip;
            }
            if (x < 0) {
                bitmap -= x;
                x_skip -= x;
                w += x;
                x = 0;
            }
            uint16_t* row = _framebuffer;
            row += y * _width;
            row += x;
            uint16_t color;
            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    color = *bitmap++;
                    MSB_16_SET(row[i], color);
                }
                bitmap += x_skip;
                row += _width;
            }
        }
    }
}

void RowPrefixCanvas::flush(bool force_flush) {
    if (force_flush) {
        markDirtyRect(0, 0, width(), height());
    }

    if (_framebuffer && hasDirty()) {
        const int16_t rows = dirtyNativeRowEnd();
        if (rows > 0) {
            _output.draw16bitRGBBitmap(_output_x, _output_y, _framebuffer, WIDTH, rows);
        }
        clearDirty();
    }
}

uint16_t* RowPrefixCanvas::getFramebuffer() {
    return _framebuffer;
}

void RowPrefixCanvas::markDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (w < 0) {
        x = static_cast<int16_t>(x + w + 1);
        w = static_cast<int16_t>(-w);
    }
    if (h < 0) {
        y = static_cast<int16_t>(y + h + 1);
        h = static_cast<int16_t>(-h);
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    const int16_t left = std::max<int16_t>(0, x);
    const int16_t top = std::max<int16_t>(0, y);
    const int16_t right = std::min<int16_t>(width(), static_cast<int16_t>(x + w));
    const int16_t bottom = std::min<int16_t>(height(), static_cast<int16_t>(y + h));
    if (left >= right || top >= bottom) {
        return;
    }

    if (!hasDirty()) {
        _dirty_left = left;
        _dirty_top = top;
        _dirty_right = right;
        _dirty_bottom = bottom;
        return;
    }

    _dirty_left = std::min<int16_t>(_dirty_left, left);
    _dirty_top = std::min<int16_t>(_dirty_top, top);
    _dirty_right = std::max<int16_t>(_dirty_right, right);
    _dirty_bottom = std::max<int16_t>(_dirty_bottom, bottom);
}

bool RowPrefixCanvas::hasDirty() const {
    return _dirty_left >= 0;
}

int16_t RowPrefixCanvas::dirtyNativeRowEnd() const {
    // Flushes native rows 0..N only. This avoids arbitrary panel windows while
    // still honoring rotation.
    switch (_rotation) {
    case 1:
        return _dirty_right;
    case 2:
        return static_cast<int16_t>(HEIGHT - _dirty_top);
    case 3:
        return static_cast<int16_t>(HEIGHT - _dirty_left);
    default:
        return _dirty_bottom;
    }
}

void RowPrefixCanvas::clearDirty() {
    _dirty_left = -1;
    _dirty_top = -1;
    _dirty_right = -1;
    _dirty_bottom = -1;
}

#endif // !defined(LITTLE_FOOT_PRINT)
