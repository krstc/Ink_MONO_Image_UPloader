#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <time.h>

#include "ed060kd1_driver.h"
#include "logo_image.h"
#include "web_page.h"

static const char* AP_SSID = "ED060KD1-WIFI";
static const int SLOT_COUNT = 12;
static const uint32_t DEFAULT_CAROUSEL_INTERVAL_S = 60;
static const uint32_t MIN_CAROUSEL_INTERVAL_S = 10;
static const uint32_t MAX_CAROUSEL_INTERVAL_S = 30UL * 24UL * 60UL * 60UL;
static const uint32_t STARTUP_AUTOPLAY_SECONDS = 5;
static const uint16_t MIN_DISPLAY_WIDTH = 320;
static const uint16_t MIN_DISPLAY_HEIGHT = 240;
static const uint16_t MAX_DISPLAY_WIDTH = 1872;
static const uint16_t MAX_DISPLAY_HEIGHT = 1404;
static const uint32_t MAX_DISPLAY_PIXELS = 2800000UL;

static WebServer server(80);
static Preferences prefs;
static File upload_file;
static uint8_t* frame_buffer = nullptr;

static size_t upload_offset = 0;
static int upload_slot = -1;
static int pending_slot = -1;
static int carousel_index = -1;
static int startup_slot = -1;
static bool upload_ok = false;
static bool upload_in_progress = false;
static bool pending_show = false;
static bool display_busy = false;
static bool clean_before_show = true;
static bool fs_ready = false;
static bool carousel_enabled = false;
static uint32_t carousel_interval_s = DEFAULT_CAROUSEL_INTERVAL_S;
static uint32_t last_carousel_ms = 0;
static uint32_t sta_connect_start_ms = 0;
static uint32_t last_wifi_status_check_ms = 0;
static String status_message = "Booting";
static String stored_ssid;
static String stored_pass;
static String last_sta_ip;
static wl_status_t last_sta_status = WL_IDLE_STATUS;
static bool sta_connecting = false;
static bool pending_wifi_status_screen = false;
static bool startup_sequence_active = false;
static bool startup_auto_cancelled = false;
static uint16_t configured_width = ED060KD1_DEFAULT_WIDTH;
static uint16_t configured_height = ED060KD1_DEFAULT_HEIGHT;

static const uint32_t STA_CONNECT_TIMEOUT_MS = 30000;

static void serviceWiFiStatus();

static String slotPath(int slot) {
    char path[24];
    snprintf(path, sizeof(path), "/slot%02d.gray4", slot);
    return String(path);
}

static bool validSlot(int slot) {
    return slot >= 0 && slot < SLOT_COUNT;
}

static uint32_t displayWidth() {
    return Epd.ready() ? (uint32_t)Epd.width() : configured_width;
}

static uint32_t displayHeight() {
    return Epd.ready() ? (uint32_t)Epd.height() : configured_height;
}

static uint32_t displayFrameBytes() {
    return (displayWidth() * displayHeight()) / 2;
}

static bool slotExists(int slot) {
    if (!validSlot(slot) || !fs_ready) {
        return false;
    }
    String path = slotPath(slot);
    if (!LittleFS.exists(path)) {
        return false;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        return false;
    }
    bool ok = f.size() == displayFrameBytes();
    f.close();
    return ok;
}

static uint32_t theoreticalSlotCapacity() {
    if (!fs_ready) {
        return 0;
    }
    uint32_t frame_bytes = displayFrameBytes();
    return frame_bytes == 0 ? 0 : LittleFS.totalBytes() / frame_bytes;
}

static uint32_t additionalSlotCapacity() {
    uint32_t max_slots = theoreticalSlotCapacity();
    return max_slots > SLOT_COUNT ? max_slots - SLOT_COUNT : 0;
}

static void markUserOperation() {
    startup_auto_cancelled = true;
}

static bool validDisplayConfig(uint32_t width, uint32_t height) {
    if (width < MIN_DISPLAY_WIDTH || height < MIN_DISPLAY_HEIGHT) {
        return false;
    }
    if (width > MAX_DISPLAY_WIDTH || height > MAX_DISPLAY_HEIGHT) {
        return false;
    }
    if ((width & 7) != 0) {
        return false;
    }
    return width * height <= MAX_DISPLAY_PIXELS;
}

static String jsonEscape(const String& input) {
    String out;
    out.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        if (c == '\n' || c == '\r') {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

static String staIpString() {
    return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("");
}

static void queueWifiStatusScreen() {
    pending_wifi_status_screen = true;
}

static uint32_t clampCarouselInterval(uint32_t interval) {
    if (interval < MIN_CAROUSEL_INTERVAL_S) {
        return MIN_CAROUSEL_INTERVAL_S;
    }
    if (interval > MAX_CAROUSEL_INTERVAL_S) {
        return MAX_CAROUSEL_INTERVAL_S;
    }
    return interval;
}

static void showWifiStatusScreen() {
    String sta = staIpString();
    if (!sta.isEmpty()) {
        String ip_line = String("IP ") + sta;
        Epd.showStatus("STA CONNECTED", ip_line.c_str(), "AP 192.168.4.1");
        return;
    }

    if (sta_connecting) {
        Epd.showStatus("STA CONNECTING", "AP 192.168.4.1", "WAITING");
        return;
    }

    if (stored_ssid.isEmpty()) {
        Epd.showStatus("STA NOT SET", "AP 192.168.4.1", "OPEN WIFI");
        return;
    }

    Epd.showStatus("STA NOT CONNECTED", "AP 192.168.4.1", "CHECK PASSWORD");
}

static void saveCarouselConfig() {
    prefs.begin("carousel", false);
    prefs.putBool("enabled", carousel_enabled);
    prefs.putUInt("interval", carousel_interval_s);
    prefs.end();
}

static void saveLastDisplayedSlot() {
    prefs.begin("carousel", false);
    prefs.putInt("lastSlot", carousel_index);
    prefs.end();
}

static void saveStartupSlot() {
    prefs.begin("carousel", false);
    prefs.putInt("startupSlot", startup_slot);
    prefs.end();
}

static void loadCarouselConfig() {
    prefs.begin("carousel", true);
    carousel_enabled = prefs.getBool("enabled", false);
    carousel_interval_s = clampCarouselInterval(prefs.getUInt("interval", DEFAULT_CAROUSEL_INTERVAL_S));
    carousel_index = prefs.getInt("lastSlot", -1);
    startup_slot = prefs.getInt("startupSlot", -1);
    prefs.end();
    if (!validSlot(carousel_index)) {
        carousel_index = -1;
    }
    if (!validSlot(startup_slot)) {
        startup_slot = -1;
    }
}

static void loadWifiConfig() {
    prefs.begin("wifi", true);
    stored_ssid = prefs.getString("ssid", "");
    stored_pass = prefs.getString("pass", "");
    prefs.end();
}

static void loadDisplayConfig() {
    prefs.begin("display", true);
    uint32_t width = prefs.getUInt("width", ED060KD1_DEFAULT_WIDTH);
    uint32_t height = prefs.getUInt("height", ED060KD1_DEFAULT_HEIGHT);
    prefs.end();
    if (!validDisplayConfig(width, height)) {
        width = ED060KD1_DEFAULT_WIDTH;
        height = ED060KD1_DEFAULT_HEIGHT;
    }
    configured_width = (uint16_t)width;
    configured_height = (uint16_t)height;
}

static void saveDisplayConfig(uint32_t width, uint32_t height) {
    prefs.begin("display", false);
    prefs.putUInt("width", width);
    prefs.putUInt("height", height);
    prefs.end();
    configured_width = (uint16_t)width;
    configured_height = (uint16_t)height;
}

static void saveWifiConfig(const String& ssid, const String& pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    stored_ssid = ssid;
    stored_pass = pass;
}

static void connectSTA(bool wait_short) {
    if (stored_ssid.isEmpty()) {
        WiFi.disconnect(false, false);
        sta_connecting = false;
        status_message = "AP ready, STA not configured";
        return;
    }

    WiFi.begin(stored_ssid.c_str(), stored_pass.c_str());
    sta_connecting = true;
    sta_connect_start_ms = millis();
    status_message = "STA connecting";
    Serial.printf("STA connecting: %s\n", stored_ssid.c_str());

    if (!wait_short) {
        return;
    }

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
        sta_connecting = false;
        status_message = String("STA connected ") + staIpString();
    } else {
        status_message = "STA connecting";
    }
}

static bool ensureFrameBuffer() {
    if (frame_buffer != nullptr) {
        return true;
    }
    frame_buffer = (uint8_t*)heap_caps_malloc(displayFrameBytes(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (frame_buffer == nullptr) {
        status_message = "No PSRAM frame buffer";
        Serial.println(status_message);
        return false;
    }
    return true;
}

static bool loadSlotToFrame(int slot) {
    if (!slotExists(slot) || !ensureFrameBuffer()) {
        return false;
    }

    File f = LittleFS.open(slotPath(slot), "r");
    if (!f) {
        return false;
    }
    size_t read_len = f.read(frame_buffer, displayFrameBytes());
    f.close();
    return read_len == displayFrameBytes();
}

static void whiteCleanCycles(int cycles) {
    for (int i = 0; i < cycles; i++) {
        Epd.whiteCleanCycle();
        delay(200);
    }
}

static void cleanBeforeImageDisplay(int cycles) {
    if (cycles < 1) {
        return;
    }
    whiteCleanCycles(cycles);
    Epd.panelPowerOff();
}

static bool displaySlotWithCleanCycles(int slot, int clean_cycles) {
    if (!slotExists(slot) || !ensureFrameBuffer()) {
        status_message = "Slot load failed";
        return false;
    }
    char msg[48];
    snprintf(msg, sizeof(msg), "Displaying slot %02d clean=%d", slot + 1, clean_cycles);
    status_message = msg;
    Serial.println(status_message);
    if (clean_cycles > 0) {
        cleanBeforeImageDisplay(clean_cycles);
    }
    if (!loadSlotToFrame(slot)) {
        status_message = "Slot load failed";
        return false;
    }
    bool ok = Epd.displayPacked4bpp(frame_buffer, displayFrameBytes(), false);
    Epd.panelPowerOff();
    status_message = ok ? String("Displayed slot ") + String(slot + 1) : "Display refresh failed";
    if (ok) {
        carousel_index = slot;
        saveLastDisplayedSlot();
        last_carousel_ms = millis();
    }
    return ok;
}

static bool displaySlot(int slot, bool clean_first) {
    return displaySlotWithCleanCycles(slot, clean_first ? 1 : 0);
}

static int nextFilledSlot() {
    for (int i = 0; i < SLOT_COUNT; i++) {
        int candidate = (carousel_index + 1 + i) % SLOT_COUNT;
        if (slotExists(candidate)) {
            return candidate;
        }
    }
    return -1;
}

static int startupDisplaySlot() {
    if (validSlot(startup_slot) && slotExists(startup_slot)) {
        return startup_slot;
    }
    return nextFilledSlot();
}

static void drawThickRect(int x, int y, int w, int h, uint8_t color, int thickness) {
    for (int i = 0; i < thickness; i++) {
        Epd.drawRect(x - i, y - i, w + i * 2, h + i * 2, color);
    }
}

static int scaledCoord(int origin, int value, int scale) {
    return origin + (value * scale) / 100;
}

static int cubicPoint(int p0, int p1, int p2, int p3, int t) {
    int u = 32 - t;
    int64_t value = (int64_t)u * u * u * p0;
    value += (int64_t)3 * u * u * t * p1;
    value += (int64_t)3 * u * t * t * p2;
    value += (int64_t)t * t * t * p3;
    return (int)(value / (32 * 32 * 32));
}

static void drawRoundedStroke(int x0, int y0, int x1, int y1, uint8_t color, int thickness) {
    int radius = thickness / 2;
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            if (dx * dx + dy * dy <= radius * radius) {
                Epd.drawLine(x0 + dx, y0 + dy, x1 + dx, y1 + dy, color);
            }
        }
    }
    Epd.fillCircle(x0, y0, radius, color);
    Epd.fillCircle(x1, y1, radius, color);
}

static void drawStrokeScaled(int ox, int oy, int scale, int x0, int y0, int x1, int y1, uint8_t color, int thickness) {
    drawRoundedStroke(
        scaledCoord(ox, x0, scale),
        scaledCoord(oy, y0, scale),
        scaledCoord(ox, x1, scale),
        scaledCoord(oy, y1, scale),
        color,
        thickness);
}

static void drawCubicScaled(int ox, int oy, int scale, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, uint8_t color, int thickness) {
    int prev_x = scaledCoord(ox, x0, scale);
    int prev_y = scaledCoord(oy, y0, scale);
    for (int t = 1; t <= 32; t++) {
        int bx = cubicPoint(x0, x1, x2, x3, t);
        int by = cubicPoint(y0, y1, y2, y3, t);
        int next_x = scaledCoord(ox, bx, scale);
        int next_y = scaledCoord(oy, by, scale);
        drawRoundedStroke(prev_x, prev_y, next_x, next_y, color, thickness);
        prev_x = next_x;
        prev_y = next_y;
    }
}

static void drawCursiveImbread(int ox, int oy, int scale) {
    const uint8_t ink = 0x00;
    const int thick = (8 * scale) / 100;

    drawCubicScaled(ox, oy, scale, 25, 155, -45, 60, 75, 30, 92, 118, ink, thick);
    drawStrokeScaled(ox, oy, scale, 90, 58, 72, 220, ink, thick);
    drawCubicScaled(ox, oy, scale, 72, 220, 0, 265, 125, 260, 110, 198, ink, thick);
    drawCubicScaled(ox, oy, scale, 110, 198, 135, 190, 143, 182, 155, 182, ink, thick);

    drawCubicScaled(ox, oy, scale, 155, 208, 165, 130, 205, 130, 210, 205, ink, thick);
    drawCubicScaled(ox, oy, scale, 210, 205, 220, 130, 260, 130, 266, 205, ink, thick);
    drawCubicScaled(ox, oy, scale, 266, 205, 280, 150, 320, 150, 330, 202, ink, thick);

    drawCubicScaled(ox, oy, scale, 345, 205, 338, 140, 345, 80, 360, 45, ink, thick);
    drawCubicScaled(ox, oy, scale, 360, 45, 386, 15, 420, 40, 390, 66, ink, thick);
    drawCubicScaled(ox, oy, scale, 352, 118, 455, 95, 454, 178, 367, 202, ink, thick);
    drawCubicScaled(ox, oy, scale, 367, 202, 420, 232, 480, 198, 485, 184, ink, thick);

    drawCubicScaled(ox, oy, scale, 492, 208, 502, 142, 548, 142, 530, 183, ink, thick);
    drawCubicScaled(ox, oy, scale, 530, 183, 555, 143, 605, 155, 586, 205, ink, thick);

    drawCubicScaled(ox, oy, scale, 600, 194, 665, 142, 686, 206, 611, 215, ink, thick);
    drawCubicScaled(ox, oy, scale, 611, 215, 570, 200, 585, 162, 633, 170, ink, thick);

    drawCubicScaled(ox, oy, scale, 700, 203, 715, 132, 780, 143, 765, 204, ink, thick);
    drawCubicScaled(ox, oy, scale, 765, 204, 698, 235, 683, 172, 735, 163, ink, thick);
    drawCubicScaled(ox, oy, scale, 765, 204, 790, 194, 795, 185, 805, 176, ink, thick);

    drawCubicScaled(ox, oy, scale, 822, 202, 832, 132, 900, 143, 885, 205, ink, thick);
    drawCubicScaled(ox, oy, scale, 885, 205, 820, 235, 805, 172, 857, 163, ink, thick);
    drawCubicScaled(ox, oy, scale, 890, 205, 878, 128, 890, 75, 922, 42, ink, thick);
    drawCubicScaled(ox, oy, scale, 922, 42, 950, 28, 968, 60, 930, 75, ink, thick);
}

static void drawLogoFlourish() {
    int prev_x = 320;
    int prev_y = 430;
    for (int x = 328; x <= 1128; x += 8) {
        int dx = x - 724;
        int y = 410 + (dx * dx) / 19000;
        Epd.drawLine(prev_x, prev_y, x, y, 0x00);
        prev_x = x;
        prev_y = y;
    }
    Epd.drawCircle(290, 416, 30, 0x00);
    Epd.drawCircle(1160, 416, 30, 0x00);
    Epd.drawLine(320, 408, 1128, 408, 0x88);
}

static bool drawPacked4bppToFrame(const uint8_t* packed, size_t length) {
    uint32_t w = displayWidth();
    uint32_t h = displayHeight();
    if (packed == nullptr || length != displayFrameBytes()) {
        return false;
    }

    uint8_t* fb = Epd.framebuffer();
    for (uint32_t y = 0; y < h; y++) {
        uint32_t row_base = y * w;
        for (uint32_t x = 0; x < w; x++) {
            int idx = row_base + x;
            uint8_t packed_byte = pgm_read_byte(packed + (idx >> 1));
            uint8_t level = (idx & 1) ? (packed_byte >> 4) : (packed_byte & 0x0F);
            epd_draw_pixel((int)x, (int)y, Epd.calibratedLevelColor(level, x, y), fb);
        }
        if ((y & 0x3F) == 0) {
            yield();
        }
    }
    return true;
}

static void showStartupSplash() {
    if (!Epd.ready()) {
        return;
    }

    display_busy = true;
    Epd.blackWhiteCleanCycle();
    if (displayFrameBytes() == LOGO_IMAGE_BYTES && drawPacked4bppToFrame(LOGO_IMAGE_4BPP, LOGO_IMAGE_BYTES)) {
        Epd.updateScreen(MODE_GC16);
    } else {
        Epd.clearWhiteBuffer();
        Epd.drawText("IMBREAD", displayWidth() / 8, displayHeight() / 3, 8, 0x00);
        Epd.drawText("WIFI ESP DISPLAY", displayWidth() / 8, displayHeight() / 2, 4, 0x44);
        Epd.updateScreen(MODE_GC16);
    }
    Epd.panelPowerOff();
    display_busy = false;
    delay(3000);
}

static void slotCardRect(int slot, int& x, int& y, int& w, int& h) {
    int dw = (int)displayWidth();
    int dh = (int)displayHeight();
    int panel_y = max(96, (dh * 15) / 100);
    int gap_x = max(8, dw / 105);
    int gap_y = max(6, dh / 120);
    int right_margin = max(28, dw / 36);
    int bottom_margin = max(28, dh / 36);
    int min_card_w = max(64, min(92, dw / 8));
    int required_w = min_card_w * 2 + gap_x;
    int panel_x = max(360, (dw * 68) / 100);
    if (panel_x + required_w + right_margin > dw) {
        panel_x = max(right_margin, dw - right_margin - required_w);
    }
    int card_w = max(min_card_w, (dw - panel_x - right_margin - gap_x) / 2);
    int card_h = max(58, (dh - panel_y - bottom_margin - gap_y * 5) / 6);
    int col = slot % 2;
    int row = slot / 2;
    x = panel_x + col * (card_w + gap_x);
    y = panel_y + row * (card_h + gap_y);
    w = card_w;
    h = card_h;
}

static void drawSlotThumbnail(int slot, int x, int y, int w, int h) {
    Epd.drawRect(x, y, w, h, 0x44);
    Epd.drawRect(x + 1, y + 1, w - 2, h - 2, 0x66);
    if (!slotExists(slot) || !ensureFrameBuffer() || !loadSlotToFrame(slot)) {
        Epd.drawText("EMPTY", x + 12, y + h / 2 - 8, 2, 0x66);
        return;
    }

    for (int ty = 0; ty < h - 2; ty++) {
        int sy = (ty * (int)displayHeight()) / (h - 2);
        int row_base = sy * (int)displayWidth();
        for (int tx = 0; tx < w - 2; tx++) {
            int sx = (tx * (int)displayWidth()) / (w - 2);
            int idx = row_base + sx;
            uint8_t packed_byte = frame_buffer[idx >> 1];
            uint8_t level = (idx & 1) ? (packed_byte >> 4) : (packed_byte & 0x0F);
            Epd.drawPixel(x + 1 + tx, y + 1 + ty, Epd.calibratedLevelColor(level, x + tx, y + ty));
        }
        if ((ty & 0x0F) == 0) {
            yield();
        }
    }
}

static void drawSlotCard(int slot, int next_slot, bool highlight) {
    int x;
    int y;
    int w;
    int h;
    slotCardRect(slot, x, y, w, h);
    bool exists = slotExists(slot);

    Epd.fillRect(x, y, w, h, 0xFF);
    uint8_t border = exists ? 0x00 : 0x66;
    Epd.drawRect(x, y, w, h, border);
    Epd.drawRect(x + 1, y + 1, w - 2, h - 2, border);

    char label[8];
    snprintf(label, sizeof(label), "S%02d", slot + 1);
    int label_scale = w > 150 ? 3 : 2;
    Epd.drawText(label, x + 10, y + 10, label_scale, 0x00);
    Epd.drawText(exists ? "READY" : "EMPTY", x + w / 2 + 12, y + 18, 2, exists ? 0x00 : 0x88);

    drawSlotThumbnail(slot, x + 10, y + h / 2, min(100, w - 20), max(28, h / 2 - 10));
    if (slot == next_slot && highlight) {
        drawThickRect(x - 4, y - 4, w + 8, h + 8, 0x00, 3);
    }
}

static void drawSlotHighlight(int slot, bool on) {
    if (!validSlot(slot)) {
        return;
    }
    int x;
    int y;
    int w;
    int h;
    slotCardRect(slot, x, y, w, h);
    drawThickRect(x - 6, y - 6, w + 12, h + 12, 0xFF, 5);
    Epd.drawRect(x, y, w, h, slotExists(slot) ? 0x00 : 0x66);
    Epd.drawRect(x + 1, y + 1, w - 2, h - 2, slotExists(slot) ? 0x00 : 0x66);
    if (on) {
        drawThickRect(x - 4, y - 4, w + 8, h + 8, 0x00, 3);
    }
}

static void drawCountdown(uint32_t seconds_left) {
    Epd.fillRect(84, 918, 92, 48, 0xFF);
    char countdown[16];
    snprintf(countdown, sizeof(countdown), "%02uS", (unsigned)seconds_left);
    Epd.drawText(countdown, 96, 930, 3, 0x00);
}

static void drawStartupDashboard(int next_slot, uint32_t seconds_left, bool highlight) {
    (void)seconds_left;
    int w = (int)displayWidth();
    int h = (int)displayHeight();
    int margin = max(24, min(w, h) / 26);
    int left_x = margin + 60;
    int title_y = margin + 70;
    int title_scale = max(3, min(6, w / 240));
    int brand_scale = max(4, min(8, w / 180));
    int body_scale = max(2, min(4, w / 360));
    int slot_title_x;
    int slot_title_y;
    int slot_card_w;
    int slot_card_h;
    slotCardRect(0, slot_title_x, slot_title_y, slot_card_w, slot_card_h);

    Epd.clearWhiteBuffer();
    Epd.drawRect(margin, margin, w - margin * 2, h - margin * 2, 0x00);
    Epd.drawText("ESP32 ED060KD1 WIFI", left_x, title_y, title_scale, 0x00);
    Epd.drawText("IMBREAD", left_x, title_y + 120, brand_scale, 0x00);

    String sta = staIpString();
    if (!sta.isEmpty()) {
        String sta_line = String("STA IP ") + sta;
        Epd.drawText("STA CONNECTED", left_x, title_y + 270, body_scale, 0x00);
        Epd.drawText(sta_line.c_str(), left_x, title_y + 350, body_scale, 0x00);
    } else if (sta_connecting) {
        Epd.drawText("STA CONNECTING", left_x, title_y + 270, body_scale, 0x00);
        Epd.drawText("WAITING", left_x, title_y + 350, body_scale, 0x88);
    } else {
        Epd.drawText("STA NOT CONNECTED", left_x, title_y + 270, body_scale, 0x00);
        Epd.drawText("AP 192.168.4.1", left_x, title_y + 350, body_scale, 0x00);
    }

    char size_line[40];
    snprintf(size_line, sizeof(size_line), "UPLOAD %dX%d 16 GRAY", w, h);
    Epd.drawText(size_line, left_x, h - margin - 170, max(2, body_scale - 1), 0x00);
    Epd.drawText("WAIT 5S FOR INIT", left_x, h - margin - 90, max(2, body_scale - 1), 0x00);

    if (slot_title_x > left_x + 440) {
        Epd.drawLine(slot_title_x - 24, margin + 40, slot_title_x - 24, h - margin - 40, 0x88);
    }
    Epd.drawText("SLOTS", slot_title_x, margin + 70, body_scale, 0x00);
    if (next_slot >= 0) {
        char next_line[16];
        snprintf(next_line, sizeof(next_line), "NEXT S%02d", next_slot + 1);
        Epd.drawText(next_line, slot_title_x + 160, margin + 76, max(2, body_scale - 1), 0x00);
    } else {
        Epd.drawText("NO SLOT", slot_title_x + 160, margin + 76, max(2, body_scale - 1), 0x88);
    }

    for (int slot = 0; slot < SLOT_COUNT; slot++) {
        drawSlotCard(slot, next_slot, highlight);
        yield();
    }
}

static void runStartupSequence() {
    if (!Epd.ready()) {
        return;
    }

    startup_sequence_active = true;
    startup_auto_cancelled = false;

    showStartupSplash();

    int next_slot = startupDisplaySlot();
    Serial.printf("Startup display target: startupSlot=%d lastSlot=%d selected=%d\n", startup_slot, carousel_index, next_slot);
    display_busy = true;
    drawStartupDashboard(next_slot, STARTUP_AUTOPLAY_SECONDS, false);
    Epd.updateScreen(MODE_GC16);
    Epd.panelPowerOff();
    display_busy = false;

    uint32_t wait_start = millis();
    while (millis() - wait_start < STARTUP_AUTOPLAY_SECONDS * 1000UL && !startup_auto_cancelled) {
        server.handleClient();
        serviceWiFiStatus();
        delay(20);
    }

    startup_sequence_active = false;
    pending_wifi_status_screen = false;

    if (!startup_auto_cancelled && next_slot >= 0) {
        display_busy = true;
        displaySlotWithCleanCycles(next_slot, 1);
        display_busy = false;
    } else if (startup_auto_cancelled) {
        status_message = "Startup autoplay cancelled";
    } else {
        status_message = "Startup has no images";
    }
}

static void sendJsonStatus() {
    String json = "{\"busy\":";
    json += (display_busy || pending_show || upload_in_progress) ? "true" : "false";
    json += ",\"message\":\"";
    json += jsonEscape(status_message);
    json += "\",\"ap\":\"";
    json += WiFi.softAPIP().toString();
    json += "\",\"sta\":\"";
    json += staIpString();
    json += "\",\"staConnected\":";
    json += WiFi.status() == WL_CONNECTED ? "true" : "false";
    json += ",\"ssid\":\"";
    json += jsonEscape(stored_ssid);
    json += "\",\"currentSlot\":";
    json += carousel_index;
    json += ",\"startupSlot\":";
    json += startup_slot;
    json += ",\"display\":{\"width\":";
    json += displayWidth();
    json += ",\"height\":";
    json += displayHeight();
    json += ",\"frameBytes\":";
    json += displayFrameBytes();
    json += "}";
    json += "}";
    server.send(200, "application/json", json);
}

static void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
    sendJsonStatus();
}

static void handleSlots() {
    String json = "{\"slots\":[";
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (i > 0) {
            json += ',';
        }
        String path = slotPath(i);
        File f = LittleFS.exists(path) ? LittleFS.open(path, "r") : File();
        size_t size = f ? f.size() : 0;
        bool exists = f && size == displayFrameBytes();
        if (f) {
            f.close();
        }
        json += "{\"id\":";
        json += i;
        json += ",\"exists\":";
        json += exists ? "true" : "false";
        json += ",\"size\":";
        json += (uint32_t)size;
        json += "}";
    }
    json += "],\"currentSlot\":";
    json += carousel_index;
    json += ",\"startupSlot\":";
    json += startup_slot;
    json += ",\"carousel\":{\"enabled\":";
    json += carousel_enabled ? "true" : "false";
    json += ",\"interval\":";
    json += carousel_interval_s;
    json += "},\"fs\":{\"ready\":";
    json += fs_ready ? "true" : "false";
    json += ",\"total\":";
    json += fs_ready ? LittleFS.totalBytes() : 0;
    json += ",\"used\":";
    json += fs_ready ? LittleFS.usedBytes() : 0;
    json += ",\"slotSize\":";
    json += displayFrameBytes();
    json += ",\"maxSlots\":";
    json += theoreticalSlotCapacity();
    json += ",\"additionalSlots\":";
    json += additionalSlotCapacity();
    json += "},\"display\":{\"width\":";
    json += displayWidth();
    json += ",\"height\":";
    json += displayHeight();
    json += ",\"frameBytes\":";
    json += displayFrameBytes();
    json += "}}";
    server.send(200, "application/json", json);
}

static void handleSlotData() {
    int slot = server.arg("slot").toInt();
    if (!validSlot(slot) || !slotExists(slot)) {
        server.send(404, "text/plain", "Slot empty");
        return;
    }

    File f = LittleFS.open(slotPath(slot), "r");
    if (!f) {
        server.send(500, "text/plain", "Slot open failed");
        return;
    }
    server.streamFile(f, "application/octet-stream");
    f.close();
}

static bool beginImmediateDisplay(const char* message) {
    markUserOperation();
    if (!Epd.ready()) {
        server.send(500, "text/plain", "Display not ready");
        return false;
    }
    if (display_busy || pending_show || upload_in_progress) {
        server.send(409, "text/plain", "Display busy");
        return false;
    }
    display_busy = true;
    status_message = message;
    server.send(200, "text/plain", status_message);
    return true;
}

static void endImmediateDisplay(bool ok, const char* done_message) {
    Epd.panelPowerOff();
    status_message = ok ? String(done_message) : "Display refresh failed";
    display_busy = false;
}

static void fillCalibratedRect(int x, int y, int w, int h, uint8_t level) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > (int)displayWidth()) {
        x1 = (int)displayWidth();
    }
    if (y1 > (int)displayHeight()) {
        y1 = (int)displayHeight();
    }
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            Epd.drawPixel(px, py, Epd.calibratedLevelColor(level, px, py));
        }
        if ((py & 0x1F) == 0) {
            yield();
        }
    }
}

static void drawDeveloperGrayscale() {
    cleanBeforeImageDisplay(1);
    Epd.clearWhiteBuffer();
    int dw = (int)displayWidth();
    int dh = (int)displayHeight();
    int margin_x = max(24, dw / 40);
    int top = max(58, dh / 14);
    int label_scale = max(2, min(4, dw / 360));
    int bar_h = dh - top - max(90, dh / 9);
    int bar_w = (dw - margin_x * 2) / 16;
    Epd.drawText("ED060KD1 16 GRAY", margin_x + 12, max(16, top - 52), label_scale, 0x00);
    for (int level = 0; level < 16; level++) {
        int x = margin_x + level * bar_w;
        int w = level == 15 ? dw - margin_x - x : bar_w;
        fillCalibratedRect(x, top, w, bar_h, (uint8_t)level);
        Epd.drawLine(x, top, x, top + bar_h, 0x88);
        char label[4];
        snprintf(label, sizeof(label), "%02d", level);
        Epd.drawText(label, x + max(4, bar_w / 7), dh - max(64, dh / 15), label_scale, 0x00);
    }
    Epd.drawLine(dw - margin_x, top, dw - margin_x, top + bar_h, 0x88);
}

static void drawDeveloperCheckerboard() {
    cleanBeforeImageDisplay(1);
    Epd.clearWhiteBuffer();
    const int cell = 8;
    int dw = (int)displayWidth();
    int dh = (int)displayHeight();
    for (int y = 0; y < dh; y += cell) {
        int h = y + cell > dh ? dh - y : cell;
        for (int x = 0; x < dw; x += cell) {
            int w = x + cell > dw ? dw - x : cell;
            uint8_t color = (((x / cell) + (y / cell)) & 1) ? 0xFF : 0x00;
            Epd.fillRect(x, y, w, h, color);
        }
        yield();
    }
}

static void drawDeveloperResolution() {
    cleanBeforeImageDisplay(1);
    Epd.clearWhiteBuffer();
    int dw = (int)displayWidth();
    int dh = (int)displayHeight();
    int text_scale = max(2, min(4, dw / 360));
    Epd.drawRect(0, 0, dw, dh, 0x00);
    Epd.drawRect(2, 2, dw - 4, dh - 4, 0x00);
    char title[40];
    snprintf(title, sizeof(title), "RESOLUTION TEST %dX%d", dw, dh);
    Epd.drawText(title, 48, 32, text_scale, 0x00);

    for (int x = 0; x < dw; x += 100) {
        Epd.drawLine(x, 0, x, dh - 1, (x % 500 == 0) ? 0x00 : 0xAA);
    }
    for (int y = 0; y < dh; y += 100) {
        Epd.drawLine(0, y, dw - 1, y, (y % 500 == 0) ? 0x00 : 0xAA);
    }
    Epd.drawLine(dw / 2, 0, dw / 2, dh - 1, 0x00);
    Epd.drawLine(0, dh / 2, dw - 1, dh / 2, 0x00);

    int left = max(36, dw / 30);
    int top = max(100, dh / 9);
    int block_w = max(120, dw / 4);
    int block_h = max(120, dh / 5);
    Epd.drawText("1PX V", left, top, max(2, text_scale - 1), 0x00);
    for (int x = left; x < min(dw - 20, left + block_w); x += 2) {
        Epd.drawLine(x, top + 58, x, min(dh - 20, top + 58 + block_h), 0x00);
    }
    Epd.drawText("1PX H", left, top + block_h + 110, max(2, text_scale - 1), 0x00);
    for (int y = top + block_h + 168; y < min(dh - 20, top + block_h * 2 + 168); y += 2) {
        Epd.drawLine(left, y, min(dw - 20, left + block_w), y, 0x00);
    }

    int mid = max(left + block_w + 100, dw / 3);
    Epd.drawText("1PX DOT MATRIX", mid, top, max(2, text_scale - 1), 0x00);
    for (int y = top + 58; y < min(dh - 20, top + 58 + block_h); y += 4) {
        for (int x = mid; x < min(dw - 20, mid + block_w); x += 4) {
            Epd.drawPixel(x, y, 0x00);
        }
        yield();
    }

    Epd.drawText("BOXES", mid, top + block_h + 110, max(2, text_scale - 1), 0x00);
    for (int i = 0; i < 8; i++) {
        int d = 20 + i * max(16, min(dw, dh) / 38);
        Epd.drawRect(mid + i * 14, top + block_h + 168 + i * 10, d, d, 0x00);
    }

    int right = max(mid + block_w + 80, dw * 68 / 100);
    Epd.drawText("TEXT SCALE 2", right, top + 30, 2, 0x00);
    Epd.drawText("TEXT SCALE 3", right, top + 92, 3, 0x00);
    Epd.drawText("TEXT SCALE 4", right, top + 182, 4, 0x00);
    Epd.drawText("TEXT SCALE 5", right, top + 302, 5, 0x00);
    Epd.drawText("PIXEL CHECK", right, top + 482, text_scale, 0x00);
    for (int y = top + 560; y < min(dh - 20, top + 760); y++) {
        for (int x = right; x < min(dw - 20, right + 200); x++) {
            if (((x ^ y) & 1) == 0) {
                Epd.drawPixel(x, y, 0x00);
            }
        }
        yield();
    }
}

static int monthFromBuildDate(const char* mon) {
    static const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strncmp(mon, names[i], 3) == 0) {
            return i + 1;
        }
    }
    return 1;
}

static void currentCalendarDate(int& year, int& month, int& day) {
    time_t now = time(nullptr);
    if (now > 1600000000) {
        struct tm local_time;
        localtime_r(&now, &local_time);
        year = local_time.tm_year + 1900;
        month = local_time.tm_mon + 1;
        day = local_time.tm_mday;
        return;
    }

    char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0};
    month = monthFromBuildDate(mon);
    day = atoi(__DATE__ + 4);
    year = atoi(__DATE__ + 7);
}

static bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int daysInMonth(int year, int month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days[month - 1];
}

static int weekdayMondayFirst(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    int k = year % 100;
    int j = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 5) % 7;
}

static void drawDeveloperCalendar() {
    cleanBeforeImageDisplay(1);
    Epd.clearWhiteBuffer();

    int dw = (int)displayWidth();
    int dh = (int)displayHeight();
    int year;
    int month;
    int today;
    currentCalendarDate(year, month, today);

    static const char* month_names[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    static const char* weekdays[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

    int margin = max(28, min(dw, dh) / 30);
    int title_scale = max(4, min(8, dw / 190));
    int text_scale = max(2, min(4, dw / 380));
    int small_scale = max(1, min(3, dw / 520));
    int side_w = max(260, dw / 4);
    int grid_x = margin;
    int grid_y = margin + 210;
    int grid_w = dw - margin * 3 - side_w;
    int grid_h = dh - grid_y - margin;
    int cell_w = grid_w / 7;
    int cell_h = grid_h / 7;

    if (cell_w < 54 || cell_h < 42) {
        grid_w = dw - margin * 2;
        side_w = 0;
        cell_w = grid_w / 7;
        cell_h = max(36, (dh - grid_y - margin) / 7);
    }

    Epd.drawText("CALENDAR", margin, margin + 22, title_scale, 0x00);
    char date_title[32];
    snprintf(date_title, sizeof(date_title), "%s %04d", month_names[month - 1], year);
    Epd.drawText(date_title, margin, margin + 120, text_scale + 1, 0x00);

    for (int i = 0; i < 7; i++) {
        int x = grid_x + i * cell_w;
        Epd.drawText(weekdays[i], x + 10, grid_y - 46, small_scale + 1, i >= 5 ? 0x44 : 0x00);
    }

    int first = weekdayMondayFirst(year, month, 1);
    int max_day = daysInMonth(year, month);
    int day = 1;
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            int x = grid_x + col * cell_w;
            int y = grid_y + row * cell_h;
            Epd.drawRect(x, y, cell_w, cell_h, 0x88);
            if ((row + col) & 1) {
                Epd.fillRect(x + 1, y + 1, cell_w - 2, cell_h - 2, 0xEE);
            }
            int index = row * 7 + col;
            if (index < first || day > max_day) {
                continue;
            }
            char label[4];
            snprintf(label, sizeof(label), "%02d", day);
            if (day == today) {
                Epd.fillRect(x + 5, y + 5, cell_w - 10, cell_h - 10, 0x00);
                Epd.drawText(label, x + 18, y + 18, text_scale, 0xFF);
            } else {
                Epd.drawText(label, x + 18, y + 18, text_scale, col >= 5 ? 0x44 : 0x00);
            }
            day++;
        }
        yield();
    }

    if (side_w > 0) {
        int sx = dw - margin - side_w;
        int sy = grid_y;
        Epd.drawRect(sx, sy, side_w, grid_h, 0x00);
        Epd.drawText("TODAY", sx + 24, sy + 34, text_scale, 0x00);
        char day_text[8];
        snprintf(day_text, sizeof(day_text), "%02d", today);
        Epd.drawText(day_text, sx + 28, sy + 116, title_scale + 2, 0x00);
        Epd.drawLine(sx + 24, sy + 250, sx + side_w - 24, sy + 250, 0x88);
        Epd.drawText("EPD WIFI FRAME", sx + 24, sy + 300, small_scale + 1, 0x00);
        Epd.drawText("16 GRAY READY", sx + 24, sy + 360, small_scale + 1, 0x44);
        Epd.drawText("SLOT CAROUSEL", sx + 24, sy + 420, small_scale + 1, 0x44);
        Epd.drawText("WEB UPLOADER", sx + 24, sy + 480, small_scale + 1, 0x00);
    }
}

static void handleDevGrayscale() {
    if (!beginImmediateDisplay("Drawing 16 gray pattern")) {
        return;
    }
    drawDeveloperGrayscale();
    endImmediateDisplay(Epd.updateScreen(MODE_GC16), "16 gray pattern shown");
}

static void handleDevChecker() {
    if (!beginImmediateDisplay("Drawing checkerboard")) {
        return;
    }
    drawDeveloperCheckerboard();
    endImmediateDisplay(Epd.updateScreen(MODE_GC16), "Checkerboard shown");
}

static void handleDevResolution() {
    if (!beginImmediateDisplay("Drawing resolution test")) {
        return;
    }
    drawDeveloperResolution();
    endImmediateDisplay(Epd.updateScreen(MODE_GC16), "Resolution test shown");
}

static void handleDevCalendar() {
    if (!beginImmediateDisplay("Drawing calendar demo")) {
        return;
    }
    drawDeveloperCalendar();
    endImmediateDisplay(Epd.updateScreen(MODE_GC16), "Calendar demo shown");
}

static void handleDevRepair() {
    if (!beginImmediateDisplay("Repair mode: 10 black white cycles")) {
        return;
    }
    for (int i = 0; i < 10; i++) {
        Serial.printf("Repair refresh cycle %d/10\n", i + 1);
        Epd.blackWhiteCleanCycle();
        delay(150);
        yield();
    }
    endImmediateDisplay(true, "Repair mode done");
}

static void handleClear() {
    markUserOperation();
    pending_show = false;
    display_busy = true;
    status_message = "Clearing screen";
    server.send(200, "text/plain", status_message);
    Epd.whiteCleanCycle();
    Epd.showStatus("SCREEN CLEAR", "AP OPEN 192.168.4.1", staIpString().isEmpty() ? "STA NOT CONNECTED" : staIpString().c_str());
    Epd.panelPowerOff();
    status_message = "Screen cleared";
    display_busy = false;
}

static void handleReset() {
    markUserOperation();
    server.send(200, "text/plain", "Restarting");
    delay(300);
    ESP.restart();
}

static const char* wifiAuthName(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN:
            return "open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3";
        default:
            return "unknown";
    }
}

static void handleWifiScan() {
    markUserOperation();
    if (display_busy || pending_show || upload_in_progress) {
        server.send(409, "application/json", "{\"error\":\"display busy\"}");
        return;
    }

    status_message = "Scanning WiFi";
    int count = WiFi.scanNetworks(false, true);
    String json = "{\"networks\":[";
    bool first = true;
    for (int i = 0; i < count; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) {
            continue;
        }
        if (!first) {
            json += ',';
        }
        first = false;
        json += "{\"ssid\":\"";
        json += jsonEscape(ssid);
        json += "\",\"rssi\":";
        json += WiFi.RSSI(i);
        json += ",\"auth\":\"";
        json += wifiAuthName((wifi_auth_mode_t)WiFi.encryptionType(i));
        json += "\"}";
    }
    json += "],\"count\":";
    json += count < 0 ? 0 : count;
    json += "}";
    WiFi.scanDelete();
    status_message = "WiFi scan done";
    server.send(200, "application/json", json);
}

static void handleWifiConfig() {
    if (server.method() == HTTP_GET) {
        sendJsonStatus();
        return;
    }
    markUserOperation();
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    saveWifiConfig(ssid, pass);
    connectSTA(false);
    queueWifiStatusScreen();
    server.send(200, "text/plain", ssid.isEmpty() ? "STA config cleared" : "STA connecting");
}

static void handleWifiClear() {
    markUserOperation();
    saveWifiConfig("", "");
    WiFi.disconnect(false, true);
    sta_connecting = false;
    last_sta_ip = "";
    last_sta_status = (wl_status_t)WiFi.status();
    queueWifiStatusScreen();
    status_message = "STA config cleared";
    server.send(200, "text/plain", status_message);
}

static void handleDisplayConfig() {
    markUserOperation();
    if (display_busy || pending_show || upload_in_progress) {
        server.send(409, "text/plain", "Display busy");
        return;
    }

    uint32_t width = (uint32_t)server.arg("width").toInt();
    uint32_t height = (uint32_t)server.arg("height").toInt();
    if (!validDisplayConfig(width, height)) {
        server.send(400, "text/plain", "Invalid resolution. Width must be a multiple of 8, range 320x240 to 1872x1404.");
        return;
    }

    saveDisplayConfig(width, height);
    status_message = "Resolution saved, restarting";
    server.send(200, "text/plain", status_message);
    delay(400);
    ESP.restart();
}

static void handleCarousel() {
    markUserOperation();
    if (server.hasArg("enabled")) {
        carousel_enabled = server.arg("enabled") == "1";
    }
    if (server.hasArg("interval")) {
        uint32_t interval = (uint32_t)server.arg("interval").toInt();
        carousel_interval_s = clampCarouselInterval(interval);
    }
    last_carousel_ms = millis();
    saveCarouselConfig();
    status_message = carousel_enabled ? "Carousel enabled" : "Carousel disabled";
    server.send(200, "text/plain", status_message);
}

static void handleStartupSlot() {
    markUserOperation();
    int slot = server.arg("slot").toInt();
    if (slot != -1 && !validSlot(slot)) {
        server.send(400, "text/plain", "Invalid slot");
        return;
    }
    startup_slot = slot;
    saveStartupSlot();
    char msg[48];
    if (slot >= 0) {
        snprintf(msg, sizeof(msg), "Startup slot set to %02d", slot + 1);
    } else {
        snprintf(msg, sizeof(msg), "Startup slot auto");
    }
    status_message = msg;
    server.send(200, "text/plain", status_message);
}

static void handleSlotShow() {
    markUserOperation();
    int slot = server.arg("slot").toInt();
    if (!validSlot(slot) || !slotExists(slot)) {
        server.send(404, "text/plain", "Slot empty");
        return;
    }
    startup_slot = slot;
    saveStartupSlot();
    pending_slot = slot;
    pending_show = true;
    clean_before_show = server.arg("clean") != "0";
    status_message = "Slot refresh queued";
    server.send(200, "text/plain", status_message);
}

static void handleSlotDelete() {
    markUserOperation();
    int slot = server.arg("slot").toInt();
    if (!validSlot(slot)) {
        server.send(400, "text/plain", "Invalid slot");
        return;
    }
    bool removed = LittleFS.remove(slotPath(slot));
    if (carousel_index == slot) {
        carousel_index = -1;
        saveLastDisplayedSlot();
    }
    if (startup_slot == slot) {
        startup_slot = -1;
        saveStartupSlot();
    }
    status_message = removed ? "Slot deleted" : "Slot already empty";
    server.send(200, "text/plain", status_message);
}

static void handleUploadDone() {
    upload_in_progress = false;
    if (upload_file) {
        upload_file.close();
    }

    if (upload_ok && upload_offset == displayFrameBytes() && validSlot(upload_slot)) {
        startup_slot = upload_slot;
        saveStartupSlot();
        pending_slot = upload_slot;
        pending_show = true;
        char msg[64];
        snprintf(msg, sizeof(msg), "Slot %02d saved, refresh queued", upload_slot + 1);
        status_message = msg;
        server.send(200, "text/plain", status_message);
        return;
    }

    if (validSlot(upload_slot)) {
        LittleFS.remove(slotPath(upload_slot));
    }
    status_message = "Upload size error";
    server.send(400, "text/plain", status_message);
}

static void handleUploadStream() {
    HTTPUpload& up = server.upload();

    if (up.status == UPLOAD_FILE_START) {
        markUserOperation();
        upload_slot = server.hasArg("slot") ? server.arg("slot").toInt() : 0;
        upload_offset = 0;
        upload_ok = fs_ready && validSlot(upload_slot);
        upload_in_progress = true;
        clean_before_show = server.arg("clean") != "0";

        if (upload_ok) {
            upload_file = LittleFS.open(slotPath(upload_slot), "w");
            upload_ok = (bool)upload_file;
        }
        status_message = upload_ok ? "Receiving image" : "Upload start failed";
        Serial.printf("Upload start: slot=%d, clean=%d, file=%d\n", upload_slot, clean_before_show, upload_ok);
        return;
    }

    if (up.status == UPLOAD_FILE_WRITE) {
        if (!upload_ok || !upload_file) {
            return;
        }
        if (upload_offset + up.currentSize > displayFrameBytes()) {
            upload_ok = false;
            status_message = "Upload too large";
            return;
        }
        size_t written = upload_file.write(up.buf, up.currentSize);
        if (written != up.currentSize) {
            upload_ok = false;
            status_message = "Flash write failed";
            return;
        }
        upload_offset += up.currentSize;
        return;
    }

    if (up.status == UPLOAD_FILE_END) {
        Serial.printf("Upload end: slot=%d, bytes=%u\n", upload_slot, (unsigned)upload_offset);
        if (upload_file) {
            upload_file.close();
        }
        if (upload_offset != displayFrameBytes()) {
            upload_ok = false;
        }
        status_message = upload_ok ? "Upload received" : "Upload incomplete";
        return;
    }

    if (up.status == UPLOAD_FILE_ABORTED) {
        if (upload_file) {
            upload_file.close();
        }
        if (validSlot(upload_slot)) {
            LittleFS.remove(slotPath(upload_slot));
        }
        upload_ok = false;
        upload_in_progress = false;
        status_message = "Upload aborted";
    }
}

static void startWiFi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);
    Serial.printf("Open AP: %s, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    loadWifiConfig();
    connectSTA(true);
    configTime(8 * 3600, 0, "pool.ntp.org", "ntp.aliyun.com", "time.windows.com");
    last_sta_status = (wl_status_t)WiFi.status();
    last_sta_ip = staIpString();
}

static void startServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/slots", HTTP_GET, handleSlots);
    server.on("/clear", HTTP_POST, handleClear);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/wifi", HTTP_GET, handleWifiConfig);
    server.on("/wifi", HTTP_POST, handleWifiConfig);
    server.on("/wifi/scan", HTTP_GET, handleWifiScan);
    server.on("/wifi/clear", HTTP_POST, handleWifiClear);
    server.on("/display-config", HTTP_POST, handleDisplayConfig);
    server.on("/carousel", HTTP_POST, handleCarousel);
    server.on("/startup-slot", HTTP_POST, handleStartupSlot);
    server.on("/dev/grayscale", HTTP_POST, handleDevGrayscale);
    server.on("/dev/checker", HTTP_POST, handleDevChecker);
    server.on("/dev/resolution", HTTP_POST, handleDevResolution);
    server.on("/dev/calendar", HTTP_POST, handleDevCalendar);
    server.on("/dev/repair", HTTP_POST, handleDevRepair);
    server.on("/slot/show", HTTP_POST, handleSlotShow);
    server.on("/slot/delete", HTTP_POST, handleSlotDelete);
    server.on("/slot/data", HTTP_GET, handleSlotData);
    server.on("/upload", HTTP_POST, handleUploadDone, handleUploadStream);
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
    });
    server.begin();
    Serial.println("HTTP server started");
}

static void startFilesystem() {
    fs_ready = LittleFS.begin(true);
    Serial.printf("LittleFS: ready=%d total=%u used=%u\n", fs_ready, fs_ready ? LittleFS.totalBytes() : 0, fs_ready ? LittleFS.usedBytes() : 0);
    if (!fs_ready) {
        status_message = "LittleFS mount failed";
    }
}

static void servicePendingDisplay() {
    if (!pending_show || display_busy || upload_in_progress) {
        return;
    }
    if (!validSlot(pending_slot)) {
        pending_show = false;
        status_message = "Invalid display slot";
        return;
    }

    pending_show = false;
    display_busy = true;
    displaySlot(pending_slot, clean_before_show);
    display_busy = false;
}

static void serviceWiFiStatus() {
    uint32_t now = millis();
    if (now - last_wifi_status_check_ms < 1000) {
        return;
    }
    last_wifi_status_check_ms = now;

    wl_status_t current_status = (wl_status_t)WiFi.status();
    String current_ip = staIpString();

    if (current_status == WL_CONNECTED) {
        if (sta_connecting || current_ip != last_sta_ip || current_status != last_sta_status) {
            sta_connecting = false;
            last_sta_status = current_status;
            last_sta_ip = current_ip;
            status_message = String("STA connected ") + current_ip;
            queueWifiStatusScreen();
        }
        return;
    }

    if (sta_connecting && now - sta_connect_start_ms > STA_CONNECT_TIMEOUT_MS) {
        sta_connecting = false;
        status_message = "STA connect timeout";
        queueWifiStatusScreen();
    } else if (!sta_connecting && current_status != last_sta_status && !stored_ssid.isEmpty()) {
        status_message = "STA not connected";
        queueWifiStatusScreen();
    }

    if (current_status != last_sta_status) {
        last_sta_status = current_status;
    }
    if (!current_ip.isEmpty()) {
        last_sta_ip = current_ip;
    } else if (!last_sta_ip.isEmpty()) {
        last_sta_ip = "";
    }
}

static void serviceWiFiStatusScreen() {
    if (startup_sequence_active || !pending_wifi_status_screen || display_busy || pending_show || upload_in_progress) {
        return;
    }

    pending_wifi_status_screen = false;
    display_busy = true;
    showWifiStatusScreen();
    display_busy = false;
}

static void serviceCarousel() {
    if (!carousel_enabled || display_busy || pending_show || upload_in_progress) {
        return;
    }
    uint64_t elapsed_ms = (uint32_t)(millis() - last_carousel_ms);
    uint64_t interval_ms = (uint64_t)carousel_interval_s * 1000ULL;
    if (elapsed_ms < interval_ms) {
        return;
    }

    int slot = nextFilledSlot();
    if (slot < 0) {
        status_message = "Carousel has no images";
        last_carousel_ms = millis();
        return;
    }

    display_busy = true;
    displaySlot(slot, true);
    display_busy = false;
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("ESP32 ED060KD1 WiFi uploader");

    if (psramInit()) {
        Serial.println("PSRAM initialized");
    } else {
        Serial.println("PSRAM init failed");
    }

    loadDisplayConfig();
    loadCarouselConfig();
    startFilesystem();

    if (!Epd.begin(configured_width, configured_height)) {
        Serial.println("ED060KD1 init failed");
    }

    startWiFi();
    startServer();

    status_message = "Ready";
    runStartupSequence();
    String sta = staIpString();
    pending_wifi_status_screen = false;
    last_sta_status = (wl_status_t)WiFi.status();
    last_sta_ip = sta;
    last_carousel_ms = millis();
}

void loop() {
    serviceWiFiStatus();
    server.handleClient();
    servicePendingDisplay();
    serviceWiFiStatusScreen();
    serviceCarousel();
    delay(2);
}
