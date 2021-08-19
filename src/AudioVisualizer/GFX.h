#pragma once

void fillRoundRect(Image<RGB565> img, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color, bool fillInner);
void drawTextCenterX(Image<RGB565> img, const char* text, int y, int w, RGB565 color, const ILI9341_t3_font_t& font);
void drawNumber(Image<RGB565> img, long long_num, int poX, int poY, RGB565 color, ILI9341_t3_font_t fnt);
void drawFloat(Image<RGB565> img, float floatNumber, int dp, int poX, int poY, RGB565 color, ILI9341_t3_font_t fnt);
void fillCircleHelper(Image<RGB565> img, int16_t x0, int16_t y0, int16_t r, int16_t cornername, int16_t delta, uint16_t color);