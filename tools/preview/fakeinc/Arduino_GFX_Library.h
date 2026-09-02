#pragma once
// Host-side stand-in for Arduino_GFX. It implements exactly the primitives
// radar_ui.cpp uses, against a plain RGB565 framebuffer, using the same 5x7
// font the device uses — so a preview PNG is what the panel will show.

#include <stdint.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "font5x7.h"

class Arduino_GFX {
   public:
    Arduino_GFX(int w, int h) : w_(w), h_(h), px_(w * h, 0) {}

    int width() const { return w_; }
    int height() const { return h_; }
    const std::vector<uint16_t>& pixels() const { return px_; }

    void drawPixel(int x, int y, uint16_t c) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        px_[y * w_ + x] = c;
    }

    void fillScreen(uint16_t c) {
        for (auto& p : px_) p = c;
    }

    void fillRect(int x, int y, int w, int h, uint16_t c) {
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) drawPixel(x + i, y + j, c);
    }

    void drawFastHLine(int x, int y, int w, uint16_t c) {
        for (int i = 0; i < w; i++) drawPixel(x + i, y, c);
    }

    void drawFastVLine(int x, int y, int h, uint16_t c) {
        for (int i = 0; i < h; i++) drawPixel(x, y + i, c);
    }

    // Midpoint circle, matching Arduino_GFX's output.
    void drawCircle(int x0, int y0, int r, uint16_t c) {
        int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
        drawPixel(x0, y0 + r, c);
        drawPixel(x0, y0 - r, c);
        drawPixel(x0 + r, y0, c);
        drawPixel(x0 - r, y0, c);
        while (x < y) {
            if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
            x++; ddF_x += 2; f += ddF_x;
            drawPixel(x0 + x, y0 + y, c); drawPixel(x0 - x, y0 + y, c);
            drawPixel(x0 + x, y0 - y, c); drawPixel(x0 - x, y0 - y, c);
            drawPixel(x0 + y, y0 + x, c); drawPixel(x0 - y, y0 + x, c);
            drawPixel(x0 + y, y0 - x, c); drawPixel(x0 - y, y0 - x, c);
        }
    }

    void fillCircle(int x0, int y0, int r, uint16_t c) {
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++)
                if (x * x + y * y <= r * r) drawPixel(x0 + x, y0 + y, c);
    }

    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
        int minx = std::min(x0, std::min(x1, x2)), maxx = std::max(x0, std::max(x1, x2));
        int miny = std::min(y0, std::min(y1, y2)), maxy = std::max(y0, std::max(y1, y2));
        auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
            return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        };
        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                int e0 = edge(x0, y0, x1, y1, x, y);
                int e1 = edge(x1, y1, x2, y2, x, y);
                int e2 = edge(x2, y2, x0, y0, x, y);
                bool neg = (e0 <= 0 && e1 <= 0 && e2 <= 0);
                bool pos = (e0 >= 0 && e1 >= 0 && e2 >= 0);
                if (neg || pos) drawPixel(x, y, c);
            }
        }
    }

    void setTextSize(int s) { size_ = s < 1 ? 1 : s; }
    void setTextColor(uint16_t c) { fg_ = c; }
    void setCursor(int x, int y) { cx_ = x; cy_ = y; }

    void print(const char* s) {
        for (const char* p = s; *p; p++) {
            if (*p == '\n') { cy_ += 8 * size_; cx_ = 0; continue; }
            drawChar(cx_, cy_, (unsigned char)*p);
            cx_ += 6 * size_;
        }
    }

   private:
    // Same glyph walk as Arduino_GFX::drawChar for the classic font.
    void drawChar(int x, int y, unsigned char ch) {
        if (ch >= FONT5X7_CHARS) return;
        for (int i = 0; i < 5; i++) {
            uint8_t line = FONT5X7[ch * 5 + i];
            for (int j = 0; j < 8; j++, line >>= 1) {
                if (!(line & 1)) continue;
                if (size_ == 1) drawPixel(x + i, y + j, fg_);
                else fillRect(x + i * size_, y + j * size_, size_, size_, fg_);
            }
        }
    }

    int w_, h_;
    std::vector<uint16_t> px_;
    uint16_t fg_ = 0xFFFF;
    int size_ = 1;
    int cx_ = 0, cy_ = 0;
};

using Arduino_Canvas = Arduino_GFX;
