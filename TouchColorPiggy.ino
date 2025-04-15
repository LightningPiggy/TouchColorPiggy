#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "bsp_cst816.h"

// Display configuration
#define TFT_HOR_RES   320
#define TFT_VER_RES   240
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

// UI layout constants
#define NOTIFICATION_BAR_HEIGHT  24
#define QR_CODE_SIZE             80
#define BUTTON_WIDTH             100
#define BUTTON_HEIGHT            40
#define PADDING_TINY             5
#define PADDING_SMALL            10
#define PADDING_MEDIUM           20
#define PADDING_LARGE            40
#define OFFSET_WIFI_ICON         -70
#define OFFSET_BATTERY_ICON      -40
#define DRAWER_BUTTON_Y_OFFSET   60

// Touch interaction
#define TOUCH_SWIPE_THRESHOLD    20
#define TOUCH_BAR_Y_THRESHOLD    40

// Animation and timing
#define DRAWER_ANIM_DURATION     300
#define TIME_UPDATE_INTERVAL     1000

// Slider settings
#define SLIDER_MIN_VALUE         1
#define SLIDER_MAX_VALUE         100
#define SLIDER_DEFAULT_VALUE     80

// Miscellaneous
#define KEYBOARD_CHILD_INDEX     4
#define BACKLIGHT_INTENSITY      0.8

uint32_t draw_buf[DRAW_BUF_SIZE / 4];

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

// Display and touch pins
#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_BL   1
#define TP_SDA   48
#define TP_SCL   47

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, -1, 1, true, TFT_VER_RES, TFT_HOR_RES);

lv_obj_t *notification_bar = NULL;
lv_obj_t *drawer = NULL;
lv_obj_t *wifi_screen = NULL;

bool drawer_open = false;
static lv_point_t touch_start;
static bool touch_moved = false;

// Color palette
static lv_color_t DARKPINK = lv_color_hex(0xEC048C);
static lv_color_t MEDIUMPINK = lv_color_hex(0xF480C5);
static lv_color_t LIGHTPINK = lv_color_hex(0xF9E9F2);
static lv_color_t DARKYELLOW = lv_color_hex(0xFBDC05);
static lv_color_t LIGHTYELLOW = lv_color_hex(0xFBE499);
static lv_color_t PUREBLACK = lv_color_hex(0x000000);

static lv_color_t COLOR_NOTIF_BAR_BG = DARKPINK;
static lv_color_t COLOR_SAVEBUTTON = DARKPINK;
static lv_color_t COLOR_CANCELBUTTON = MEDIUMPINK;
static lv_color_t COLOR_TEXT_WHITE = LIGHTPINK;
static lv_color_t COLOR_DRAWER_BG = MEDIUMPINK;
static lv_color_t COLOR_DRAWER_BUTTON_BG = DARKYELLOW;
static lv_color_t COLOR_DRAWER_BUTTONTEXT = PUREBLACK;
static lv_color_t COLOR_SLIDER_BG = LIGHTYELLOW;
static lv_color_t COLOR_SLIDER_KNOB = DARKYELLOW;
static lv_color_t COLOR_SLIDER_INDICATOR = LIGHTYELLOW;
static lv_color_t COLOR_MAIN_SCREEN_BG = LIGHTPINK;
static lv_color_t COLOR_SATS_BG = MEDIUMPINK;
static lv_color_t COLOR_QR_FG = PUREBLACK;
static lv_color_t COLOR_QR_BG = LIGHTPINK;
static lv_color_t COLOR_WIFI_SCREEN_BG = LIGHTPINK;

void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1, h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_display_flush_ready(display);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    static bool touch_pressed = false;
    static lv_point_t touch_current;

    bsp_touch_read();
    if (bsp_touch_get_coordinates(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;

        if (!touch_pressed) {
            touch_start.x = x;
            touch_start.y = y;
            touch_pressed = true;
        }
        touch_current.x = x;
        touch_current.y = y;
    } else {
        if (touch_pressed) {
            int16_t delta_y = touch_current.y - touch_start.y;
            if (abs(delta_y) > TOUCH_SWIPE_THRESHOLD) {
                if (delta_y > 0 && !drawer_open && touch_start.y < TOUCH_BAR_Y_THRESHOLD) {
                    open_drawer();
                } else if (delta_y < 0 && drawer_open) {
                    close_drawer();
                }
            }
            touch_pressed = false;
        }
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void open_drawer() {
    if (!drawer_open) {
        lv_obj_clear_flag(drawer, LV_OBJ_FLAG_HIDDEN);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, drawer);
        lv_anim_set_values(&a, -TFT_VER_RES + NOTIFICATION_BAR_HEIGHT, NOTIFICATION_BAR_HEIGHT);
        lv_anim_set_time(&a, DRAWER_ANIM_DURATION);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&a, [](void *var, int32_t value) {
            lv_obj_set_y((lv_obj_t *)var, value);
        });
        lv_anim_start(&a);
        drawer_open = true;
    }
}

void close_drawer() {
    if (drawer_open) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, drawer);
        lv_anim_set_values(&a, NOTIFICATION_BAR_HEIGHT, -TFT_VER_RES + NOTIFICATION_BAR_HEIGHT);
        lv_anim_set_time(&a, DRAWER_ANIM_DURATION);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&a, [](void *var, int32_t value) {
            lv_obj_set_y((lv_obj_t *)var, value);
        });
        lv_anim_set_completed_cb(&a, [](lv_anim_t *a) {
            lv_obj_add_flag(drawer, LV_OBJ_FLAG_HIDDEN);
        });
        lv_anim_start(&a);
        drawer_open = false;
    }
}
static uint32_t my_tick(void) {
    return millis();
}

void create_notification_bar() {
    notification_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(notification_bar, COLOR_NOTIF_BAR_BG, 0);
    lv_obj_set_size(notification_bar, TFT_HOR_RES, NOTIFICATION_BAR_HEIGHT);
    lv_obj_set_pos(notification_bar, 0, 0);
    lv_obj_set_scrollbar_mode(notification_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(notification_bar, LV_DIR_VER);
    lv_obj_set_style_border_width(notification_bar, 0, 0);
    lv_obj_set_style_radius(notification_bar, 0, 0);

    // Time label
    lv_obj_t *time_label = lv_label_create(notification_bar);
    lv_label_set_text(time_label, "12:00");
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, PADDING_TINY, 0);
    lv_obj_set_style_text_color(time_label, COLOR_TEXT_WHITE, 0);

    // Notification icons (placeholder)
    lv_obj_t *notif_icon = lv_label_create(notification_bar);
    lv_label_set_text(notif_icon, LV_SYMBOL_BELL);
    lv_obj_align_to(notif_icon, time_label, LV_ALIGN_OUT_RIGHT_MID, PADDING_SMALL, 0);
    lv_obj_set_style_text_color(notif_icon, COLOR_TEXT_WHITE, 0);

    // WiFi icon
    lv_obj_t *wifi_icon = lv_label_create(notification_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, OFFSET_WIFI_ICON, 0);
    lv_obj_set_style_text_color(wifi_icon, COLOR_TEXT_WHITE, 0);

    // Battery icon
    lv_obj_t *battery_icon = lv_label_create(notification_bar);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(battery_icon, LV_ALIGN_RIGHT_MID, OFFSET_BATTERY_ICON, 0);
    lv_obj_set_style_text_color(battery_icon, COLOR_TEXT_WHITE, 0);

    // Battery percentage
    lv_obj_t *battery_label = lv_label_create(notification_bar);
    lv_label_set_text(battery_label, "100%");
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(battery_label, COLOR_TEXT_WHITE, 0);

    // Update time every second
    lv_timer_create([](lv_timer_t *timer) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", (millis() / 3600000) % 24, (millis() / 60000) % 60);
        lv_label_set_text(lv_obj_get_child(notification_bar, 0), buf);
    }, TIME_UPDATE_INTERVAL, NULL);
}


void create_drawer() {
    drawer = lv_obj_create(lv_screen_active());
    lv_obj_set_size(drawer, TFT_HOR_RES, TFT_VER_RES - NOTIFICATION_BAR_HEIGHT);
    lv_obj_set_pos(drawer, 0, NOTIFICATION_BAR_HEIGHT);
    lv_obj_set_style_bg_color(drawer, COLOR_DRAWER_BG, 0);
    lv_obj_clear_flag(drawer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(drawer, LV_OBJ_FLAG_HIDDEN);

    // Brightness slider
    lv_obj_t *slider = lv_slider_create(drawer);
    lv_slider_set_range(slider, SLIDER_MIN_VALUE, SLIDER_MAX_VALUE);
    lv_slider_set_value(slider, SLIDER_DEFAULT_VALUE, LV_ANIM_OFF);
    lv_obj_set_width(slider, TFT_HOR_RES - PADDING_MEDIUM);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, PADDING_SMALL);
    lv_obj_set_style_bg_color(slider, COLOR_SLIDER_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, COLOR_SLIDER_INDICATOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, COLOR_SLIDER_KNOB, LV_PART_KNOB);
    lv_obj_t *slider_label = lv_label_create(drawer);
    lv_label_set_text(slider_label, "80%");
    lv_obj_set_style_text_color(slider_label, COLOR_TEXT_WHITE, 0);
    lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_TOP_MID, 0, -5);

    lv_obj_add_event_cb(slider, [](lv_event_t *e) {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
        int value = lv_slider_get_value(slider);
        char buf[5];
        snprintf(buf, sizeof(buf), "%d%%", value);
        lv_label_set_text(label, buf);
        #ifdef LCD_BL
        ledcWrite(LCD_BL, (1 << 10) * (value / 100.0)); // Adjust backlight
        #endif
    }, LV_EVENT_VALUE_CHANGED, slider_label);

    // WiFi button
    lv_obj_t *wifi_btn = lv_btn_create(drawer);
    lv_obj_set_size(wifi_btn, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_LEFT, PADDING_SMALL, DRAWER_BUTTON_Y_OFFSET);
    lv_obj_set_style_bg_color(wifi_btn, COLOR_DRAWER_BUTTON_BG, 0);
    lv_obj_t *wifi_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI " WiFi");
    lv_obj_center(wifi_label);
    lv_obj_set_style_text_color(wifi_label, COLOR_DRAWER_BUTTONTEXT, 0);
    lv_obj_add_event_cb(wifi_btn, [](lv_event_t *e) {
        lv_obj_clear_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(drawer, LV_OBJ_FLAG_HIDDEN);
        drawer_open = false;
    }, LV_EVENT_CLICKED, NULL);

    // Settings button
    lv_obj_t *settings_btn = lv_btn_create(drawer);
    lv_obj_set_size(settings_btn, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_align(settings_btn, LV_ALIGN_TOP_RIGHT, -PADDING_SMALL, DRAWER_BUTTON_Y_OFFSET);
    lv_obj_set_style_bg_color(settings_btn, COLOR_DRAWER_BUTTON_BG, 0);
    lv_obj_t *settings_label = lv_label_create(settings_btn);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_center(settings_label);
    lv_obj_set_style_text_color(settings_label, COLOR_DRAWER_BUTTONTEXT, 0);
    lv_obj_add_event_cb(settings_btn, [](lv_event_t *e) {
        lv_obj_add_flag(drawer, LV_OBJ_FLAG_HIDDEN);
        drawer_open = false;
    }, LV_EVENT_CLICKED, NULL);   
}

void create_main_screen() {
    lv_obj_t *main_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(main_screen, COLOR_MAIN_SCREEN_BG, 0);
    lv_obj_set_size(main_screen, TFT_HOR_RES, TFT_VER_RES - NOTIFICATION_BAR_HEIGHT);
    lv_obj_set_pos(main_screen, 0, NOTIFICATION_BAR_HEIGHT);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(main_screen, 0, 0);
    lv_obj_set_style_radius(main_screen, 0, 0);
    lv_obj_set_style_pad_all(main_screen, PADDING_TINY, 0); // Default padding is a bit too much

    // Sats label container
    lv_obj_t *sats_container = lv_obj_create(main_screen);
    lv_obj_set_size(sats_container, TFT_HOR_RES - QR_CODE_SIZE - PADDING_TINY * 3, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(sats_container, COLOR_SATS_BG, 0); // Pink background
    lv_obj_set_style_bg_opa(sats_container, LV_OPA_COVER, 0); // Fully opaque
    lv_obj_set_style_pad_all(sats_container, PADDING_TINY, 0); // Default padding is a bit too much
    lv_obj_align(sats_container, LV_ALIGN_TOP_LEFT, 0, 0); // Reduced position padding
    lv_obj_clear_flag(sats_container, LV_OBJ_FLAG_SCROLLABLE); // Disable scrolling
    lv_obj_set_style_border_width(sats_container, 0, 0);

    // Sats label
    lv_obj_t *sats_label = lv_label_create(sats_container); // Parent to container
    lv_label_set_text(sats_label, "12345 sats");
    lv_obj_set_style_text_font(sats_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(sats_label, COLOR_TEXT_WHITE, 0);
    lv_obj_align(sats_label, LV_ALIGN_LEFT_MID, 0, 0); // Center label in container

    // QR code
    lv_obj_t *qr = lv_qrcode_create(main_screen);
    lv_qrcode_set_size(qr, QR_CODE_SIZE);
    lv_qrcode_set_dark_color(qr, COLOR_QR_FG);
    lv_qrcode_set_light_color(qr, COLOR_QR_BG);
    lv_qrcode_update(qr, "tfar@getalby.com", strlen("tfar@getalby.com"));
    lv_obj_align(qr, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Payment comments
    lv_obj_t *text_label = lv_label_create(main_screen);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(text_label, "100 sats: Thank you!\n200 sats: Great job!\n5000 sats: Amazingly well done, I love it so much. Wish I could contribute more!");
    lv_obj_set_width(text_label, TFT_HOR_RES - QR_CODE_SIZE - PADDING_TINY * 3);
    lv_obj_align_to(text_label, sats_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, PADDING_SMALL); // a bit of margin below the sats label
    lv_obj_set_style_text_color(text_label, PUREBLACK, 0);
}

void kb_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *kb = lv_obj_get_child(lv_obj_get_parent(ta), KEYBOARD_CHILD_INDEX);
    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

void kb_confirm_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        lv_obj_add_flag((lv_obj_t *)lv_event_get_target(e), LV_OBJ_FLAG_HIDDEN);
    }
}

void save_btn_handler(lv_event_t *e) {
    lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
}

void cancel_btn_handler(lv_event_t *e) {
    lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
}

void create_wifi_screen() {
    wifi_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(wifi_screen, TFT_HOR_RES, TFT_VER_RES);
    lv_obj_set_pos(wifi_screen, 0, 0);
    lv_obj_set_style_bg_color(wifi_screen, COLOR_WIFI_SCREEN_BG, 0);
    lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(wifi_screen, PADDING_SMALL, 0);

    lv_obj_t *ssid_ta = lv_textarea_create(wifi_screen);
    lv_textarea_set_one_line(ssid_ta, true);
    lv_textarea_set_placeholder_text(ssid_ta, "Enter SSID");
    lv_obj_set_width(ssid_ta, TFT_HOR_RES - PADDING_LARGE);
    lv_obj_align(ssid_ta, LV_ALIGN_TOP_MID, 0, PADDING_SMALL);
    lv_obj_add_event_cb(ssid_ta, kb_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ssid_ta, kb_handler, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *pass_ta = lv_textarea_create(wifi_screen);
    lv_textarea_set_one_line(pass_ta, true);
    lv_textarea_set_placeholder_text(pass_ta, "Enter Password");
    lv_obj_set_width(pass_ta, TFT_HOR_RES - PADDING_LARGE);
    lv_obj_align_to(pass_ta, ssid_ta, LV_ALIGN_OUT_BOTTOM_MID, 0, PADDING_SMALL);
    lv_obj_add_event_cb(pass_ta, kb_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(pass_ta, kb_handler, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *save_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(save_btn, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_align_to(save_btn, pass_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, PADDING_SMALL);
    lv_obj_set_style_bg_color(save_btn, COLOR_SAVEBUTTON, 0);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);
    lv_obj_add_event_cb(save_btn, save_btn_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(cancel_btn, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_align_to(cancel_btn, pass_ta, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, PADDING_SMALL);
    lv_obj_set_style_bg_color(cancel_btn, COLOR_CANCELBUTTON, 0);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn, cancel_btn_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *kb = lv_keyboard_create(wifi_screen);
    lv_obj_set_size(kb, TFT_HOR_RES, TFT_VER_RES / 2);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(kb, ssid_ta);
    lv_obj_add_event_cb(kb, kb_confirm_handler, LV_EVENT_READY, NULL);
}

void setup() {
    Serial.begin(115200);
    gfx->begin();

#ifdef LCD_BL
    ledcAttach(LCD_BL, 5000, 10);
    ledcWrite(LCD_BL, (1 << 10) * BACKLIGHT_INTENSITY);
#endif

    Wire.begin(TP_SDA, TP_SCL);
    bsp_touch_init(&Wire, gfx->getRotation(), gfx->width(), gfx->height());
    lv_init();

    lv_tick_set_cb(my_tick);

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    lv_display_t *disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    create_notification_bar();
    create_main_screen();
    create_drawer();
    create_wifi_screen();
}

void loop() {
    lv_timer_handler();
    delay(5);
}