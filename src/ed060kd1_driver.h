#pragma once

#include <stddef.h>
#include <stdint.h>

#include <epdiy.h>

#define ED060KD1_WIDTH 1448
#define ED060KD1_HEIGHT 1072
#define ED060KD1_FRAME_BYTES ((ED060KD1_WIDTH * ED060KD1_HEIGHT) / 2)

// Board-specific panel power enable. This GPIO must stay with this hardware.
#define ED060KD1_PANEL_POWER_GPIO GPIO_NUM_46

class ED060KD1Driver {
public:
    bool begin();
    bool ready() const;

    int width() const;
    int height() const;
    int temperature();

    uint8_t* framebuffer();

    void clearWhiteBuffer();
    void markBuffersWhite();
    void panelPowerOff();
    void whiteCleanCycle();
    void blackWhiteCleanCycle();
    bool updateScreen(enum EpdDrawMode mode);
    bool updateArea(enum EpdDrawMode mode, EpdRect area);

    void drawPixel(int x, int y, uint8_t color);
    void drawLine(int x0, int y0, int x1, int y1, uint8_t color);
    void drawRect(int x, int y, int width, int height, uint8_t color);
    void fillRect(int x, int y, int width, int height, uint8_t color);
    void drawCircle(int x, int y, int radius, uint8_t color);
    void fillCircle(int x, int y, int radius, uint8_t color);
    void drawText(const char* text, int x, int y, int scale, uint8_t color);
    void showStatus(const char* line1, const char* line2, const char* line3);
    bool displayPacked4bpp(const uint8_t* packed, size_t length, bool clean_first);

    static uint8_t grayscaleLevelToColor(uint8_t level);
    static uint8_t calibratedLevelColor(uint8_t level, int x, int y);

private:
    void powerOn();
    void powerOff();
    void drawScaledText(const char* text, int x, int y, int scale, uint8_t color);
    uint8_t glyphRow(char c, int row) const;

    bool initialized_ = false;
    EpdiyHighlevelState hl_;
};

extern ED060KD1Driver Epd;
