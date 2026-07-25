#include "ed060kd1_driver.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <string.h>

static const char* TAG = "ed060kd1";

static const EpdDisplay_t ED060KD1_DEFAULT_PANEL = {
    ED060KD1_WIDTH,
    ED060KD1_HEIGHT,
    8,
    20,
    &epdiy_ED060SCT,
    DISPLAY_TYPE_GENERIC,
};

ED060KD1Driver Epd;

static const uint8_t BAYER_8X8[8][8] = {
    {0, 48, 12, 60, 3, 51, 15, 63},
    {32, 16, 44, 28, 35, 19, 47, 31},
    {8, 56, 4, 52, 11, 59, 7, 55},
    {40, 24, 36, 20, 43, 27, 39, 23},
    {2, 50, 14, 62, 1, 49, 13, 61},
    {34, 18, 46, 30, 33, 17, 45, 29},
    {10, 58, 6, 54, 9, 57, 5, 53},
    {42, 26, 38, 22, 41, 25, 37, 21},
};

static const uint8_t FONT5X7_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
};

static const uint8_t FONT5X7_UPPER[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
};

bool ED060KD1Driver::begin() {
    return begin(ED060KD1_DEFAULT_WIDTH, ED060KD1_DEFAULT_HEIGHT);
}

bool ED060KD1Driver::begin(int panel_width, int panel_height) {
    if (panel_width < 64 || panel_height < 64 || (panel_width & 1) != 0) {
        panel_width = ED060KD1_DEFAULT_WIDTH;
        panel_height = ED060KD1_DEFAULT_HEIGHT;
    }

    gpio_set_direction(ED060KD1_PANEL_POWER_GPIO, GPIO_MODE_OUTPUT);
    powerOff();

    panel_ = ED060KD1_DEFAULT_PANEL;
    panel_.width = panel_width;
    panel_.height = panel_height;
    epd_init(&epd_board_v7, &panel_, EPD_LUT_64K);
    epd_set_vcom(1560);
    hl_ = epd_hl_init(EPD_BUILTIN_WAVEFORM);
    epd_set_rotation(EPD_ROT_LANDSCAPE);

    clearWhiteBuffer();
    markBuffersWhite();
    initialized_ = true;

    Serial.printf("ED060KD1 ready: %dx%d, framebuffer=%p\n", width(), height(), framebuffer());
    heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
    return framebuffer() != nullptr;
}

bool ED060KD1Driver::ready() const {
    return initialized_;
}

int ED060KD1Driver::width() const {
    return epd_rotated_display_width();
}

int ED060KD1Driver::height() const {
    return epd_rotated_display_height();
}

size_t ED060KD1Driver::frameBytes() const {
    return ((size_t)width() * (size_t)height()) / 2;
}

int ED060KD1Driver::temperature() {
    powerOn();
    int temp = epd_ambient_temperature();
    powerOff();
    return temp;
}

uint8_t* ED060KD1Driver::framebuffer() {
    return epd_hl_get_framebuffer(&hl_);
}

void ED060KD1Driver::powerOn() {
    gpio_set_level(ED060KD1_PANEL_POWER_GPIO, 1);
}

void ED060KD1Driver::powerOff() {
    gpio_set_level(ED060KD1_PANEL_POWER_GPIO, 0);
}

void ED060KD1Driver::panelPowerOff() {
    powerOff();
}

void ED060KD1Driver::clearWhiteBuffer() {
    memset(framebuffer(), 0xFF, frameBytes());
}

void ED060KD1Driver::markBuffersWhite() {
    size_t fb_bytes = frameBytes();
    memset(hl_.front_fb, 0xFF, fb_bytes);
    memset(hl_.back_fb, 0xFF, fb_bytes);
    memset(hl_.difference_fb, 0x00, fb_bytes * 2);
    memset(hl_.dirty_lines, 0x00, height() * sizeof(bool));
}

bool ED060KD1Driver::updateScreen(enum EpdDrawMode mode) {
    powerOn();
    enum EpdDrawError err = epd_hl_update_screen(&hl_, mode, epd_ambient_temperature());
    powerOff();
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "screen update error: %X", err);
        return false;
    }
    return true;
}

bool ED060KD1Driver::updateArea(enum EpdDrawMode mode, EpdRect area) {
    powerOn();
    enum EpdDrawError err = epd_hl_update_area(&hl_, mode, epd_ambient_temperature(), area);
    powerOff();
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "screen area update error: %X", err);
        return false;
    }
    return true;
}

void ED060KD1Driver::drawPixel(int x, int y, uint8_t color) {
    epd_draw_pixel(x, y, color, framebuffer());
}

void ED060KD1Driver::drawLine(int x0, int y0, int x1, int y1, uint8_t color) {
    epd_draw_line(x0, y0, x1, y1, color, framebuffer());
}

void ED060KD1Driver::drawRect(int x, int y, int width, int height, uint8_t color) {
    EpdRect rect = {x, y, width, height};
    epd_draw_rect(rect, color, framebuffer());
}

void ED060KD1Driver::fillRect(int x, int y, int width, int height, uint8_t color) {
    EpdRect rect = {x, y, width, height};
    epd_fill_rect(rect, color, framebuffer());
}

void ED060KD1Driver::drawCircle(int x, int y, int radius, uint8_t color) {
    epd_draw_circle(x, y, radius, color, framebuffer());
}

void ED060KD1Driver::fillCircle(int x, int y, int radius, uint8_t color) {
    epd_fill_circle(x, y, radius, color, framebuffer());
}

void ED060KD1Driver::drawText(const char* text, int x, int y, int scale, uint8_t color) {
    drawScaledText(text, x, y, scale, color);
}

void ED060KD1Driver::whiteCleanCycle() {
    clearWhiteBuffer();
    updateScreen(MODE_GC16);
    delay(800);
    markBuffersWhite();
}

void ED060KD1Driver::blackWhiteCleanCycle() {
    memset(framebuffer(), 0x00, frameBytes());
    updateScreen(MODE_GC16);
    delay(800);
    whiteCleanCycle();
}

uint8_t ED060KD1Driver::grayscaleLevelToColor(uint8_t level) {
    return (uint8_t)((level & 0x0F) * 0x11);
}

uint8_t ED060KD1Driver::calibratedLevelColor(uint8_t level, int x, int y) {
    static const uint8_t dark_code[16] = {
        0, 0, 0, 0, 1, 1, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 15,
    };
    static const uint8_t light_code[16] = {
        0, 1, 1, 1, 2, 2, 2, 3,
        15, 15, 15, 15, 15, 15, 15, 15,
    };
    static const uint8_t light_fraction_64[16] = {
        0, 20, 38, 60, 20, 42, 0, 30,
        0, 5, 11, 17, 25, 35, 48, 0,
    };

    level &= 0x0F;
    if (level == 0 || level == 15) {
        return grayscaleLevelToColor(level);
    }

    uint8_t effective = dark_code[level];
    if (BAYER_8X8[y & 7][x & 7] < light_fraction_64[level]) {
        effective = light_code[level];
    }
    return grayscaleLevelToColor(effective);
}

bool ED060KD1Driver::displayPacked4bpp(const uint8_t* packed, size_t length, bool clean_first) {
    int w = width();
    int h = height();
    if (!ready() || packed == nullptr || length != frameBytes()) {
        return false;
    }

    if (clean_first) {
        whiteCleanCycle();
    }

    uint8_t* fb = framebuffer();
    for (int y = 0; y < h; y++) {
        int row_base = y * w;
        for (int x = 0; x < w; x++) {
            int idx = row_base + x;
            uint8_t packed_byte = packed[idx >> 1];
            uint8_t level = (idx & 1) ? (packed_byte >> 4) : (packed_byte & 0x0F);
            epd_draw_pixel(x, y, calibratedLevelColor(level, x, y), fb);
        }
        if ((y & 0x3F) == 0) {
            yield();
        }
    }

    return updateScreen(MODE_GC16);
}

uint8_t ED060KD1Driver::glyphRow(char c, int row) const {
    if (c >= '0' && c <= '9') {
        return FONT5X7_DIGITS[c - '0'][row];
    }
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if (c >= 'A' && c <= 'Z') {
        return FONT5X7_UPPER[c - 'A'][row];
    }
    switch (c) {
        case '-':
            return row == 3 ? 0x1F : 0x00;
        case '.':
            return row == 6 ? 0x04 : 0x00;
        case ':':
            return (row == 2 || row == 4) ? 0x04 : 0x00;
        case '/': {
            static const uint8_t slash[7] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
            return slash[row];
        }
        case '_':
            return row == 6 ? 0x1F : 0x00;
        default:
            return 0x00;
    }
}

void ED060KD1Driver::drawScaledText(const char* text, int x, int y, int scale, uint8_t color) {
    int cursor_x = x;
    int cursor_y = y;
    uint8_t* fb = framebuffer();

    for (const char* p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y += 9 * scale;
            continue;
        }

        for (int row = 0; row < 7; row++) {
            uint8_t bits = glyphRow(*p, row);
            for (int col = 0; col < 5; col++) {
                if ((bits & (1 << (4 - col))) != 0) {
                    EpdRect pixel = {cursor_x + col * scale, cursor_y + row * scale, scale, scale};
                    epd_fill_rect(pixel, color, fb);
                }
            }
        }
        cursor_x += 6 * scale;
    }
}

void ED060KD1Driver::showStatus(const char* line1, const char* line2, const char* line3) {
    clearWhiteBuffer();
    uint8_t* fb = framebuffer();
    int w = width();
    int h = height();
    EpdRect outer = {40, 40, w - 80, h - 80};
    epd_draw_rect(outer, 0x00, fb);
    drawScaledText("ESP32 ED060KD1 WIFI", 100, 120, 6, 0x00);
    drawScaledText(line1, 100, 260, 4, 0x00);
    drawScaledText(line2, 100, 350, 4, 0x00);
    drawScaledText(line3, 100, 440, 4, 0x00);
    char info[40];
    snprintf(info, sizeof(info), "UPLOAD %dX%d 16-GRAY IMAGE", w, h);
    drawScaledText(info, 100, h > 880 ? 820 : h - 140, 3, 0x00);
    updateScreen(MODE_GC16);
}
