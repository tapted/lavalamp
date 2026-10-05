#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <lvgl.h>

class SettingsController {
 public:
  struct Config {
    // Initial values
    int backlight = 50;
    int targetFps = 8;
    float blobSpeed = 1.0f;
    float colorSpeed = 1.0f;
    int numBlobs = 5;
    int blobRadius = 70;

    // Callbacks
    std::function<void(int)> onBacklight;
    std::function<void(int)> onTargetFps;
    std::function<void(float)> onBlobSpeed;
    std::function<void(float)> onColorSpeed;
    std::function<void(int)> onNumBlobs;
    std::function<void(int)> onBlobRadius;
  };

  explicit SettingsController(Config config) {
    // 1. Setup UI Layer
    uiLayer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(uiLayer);
    lv_obj_set_size(uiLayer, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(uiLayer, LV_OBJ_FLAG_HIDDEN);

    // 2. Create Arc
    arc = lv_arc_create(uiLayer);
    lv_obj_set_size(arc, 260, 260);
    lv_obj_center(arc);

    // 3. Create Roller (Centered inside the arc)
    roller = lv_roller_create(uiLayer);
    lv_roller_set_options(roller,
                          "Backlight\n"
                          "Target FPS\n"
                          "Blob Speed\n"
                          "Color Speed\n"
                          "Blob Count\n"
                          "Blob Radius",
                          LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(roller, 160);
    lv_obj_center(roller);

    // 4. Create Value Label (Positioned just below the roller)
    valueLabel = lv_label_create(uiLayer);
    lv_obj_align_to(valueLabel, roller, LV_ALIGN_OUT_BOTTOM_MID, 0, 40);
    lv_obj_set_style_text_color(valueLabel, lv_color_hex(0xFFFFFF), 0);
    // Optional: make the label pop a bit more
    // lv_obj_set_style_text_font(valueLabel, &lv_font_montserrat_20, 0);

    // 5. Initialize internal state mapping with custom label formatters
    settings[0] = {
        5,
        100,
        config.backlight,
        [cb = config.onBacklight](int v) {
          if (cb) cb(v);
        },
        [](lv_obj_t* lbl, int v) { lv_label_set_text_fmt(lbl, "%d%%", v); },
    };

    settings[1] = {
        1,
        120,
        config.targetFps,
        [cb = config.onTargetFps](int v) {
          if (cb) cb(v);
        },
        [](lv_obj_t* lbl, int v) { lv_label_set_text_fmt(lbl, "%d fps", v); },
    };

    settings[2] = {
        1,
        100,
        static_cast<int>(std::round(config.blobSpeed * 10.0f)),
        [cb = config.onBlobSpeed](int v) {
          if (cb) cb(v / 10.0f);
        },
        [](lv_obj_t* lbl, int v) { lv_label_set_text_fmt(lbl, "%.1fx", v / 10.0f); },
    };

    settings[3] = {
        0,
        100,
        static_cast<int>(std::round(config.colorSpeed * 10.0f)),
        [cb = config.onColorSpeed](int v) {
          if (cb) cb(v / 10.0f);
        },
        [](lv_obj_t* lbl, int v) { lv_label_set_text_fmt(lbl, "%.1fx", v / 10.0f); },
    };

    settings[4] = {
        1,
        20,
        config.numBlobs,
        [cb = config.onNumBlobs](int v) {
          if (cb) cb(v);
        },
        [](lv_obj_t* lbl, int v) { lv_label_set_text_fmt(lbl, "%d blobs", v); },
    };

    settings[5] = {
        10,
        150,
        config.blobRadius,
        [cb = config.onBlobRadius](int v) {
          if (cb) cb(v);
        },
        [](lv_obj_t* lbl, int v) { lv_label_set_text_fmt(lbl, "%d px", v); },
    };

    // Sync arc and label to default roller selection
    syncArcToRoller();

    // 6. Event Routing
    lv_obj_add_flag(lv_scr_act(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lv_scr_act(), onScreenTapped, LV_EVENT_CLICKED, this);

    // Arc events
    lv_obj_add_event_cb(arc, onArcValueChanged, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(arc, onInteraction, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(arc, onInteraction, LV_EVENT_RELEASED, this);

    // Roller events
    lv_obj_add_event_cb(roller, onRollerValueChanged, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(roller, onInteraction, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(roller, onInteraction, LV_EVENT_RELEASED, this);

    // 7. Fade Out Animation
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, uiLayer);
    lv_anim_set_time(&fadeAnim, 400);
    lv_anim_set_delay(&fadeAnim, 2500);
    lv_anim_set_values(&fadeAnim, LV_OPA_COVER, LV_OPA_TRANSP);

    lv_anim_set_exec_cb(&fadeAnim, [](void* var, int32_t v) {
      lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(v), 0);
    });

    lv_anim_set_ready_cb(&fadeAnim, [](lv_anim_t* a) {
      lv_obj_add_flag(static_cast<lv_obj_t*>(a->var), LV_OBJ_FLAG_HIDDEN);
    });
  }

  void wakeUp() {
    lv_obj_remove_flag(uiLayer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(uiLayer, LV_OPA_COVER, 0);
    lv_anim_start(&fadeAnim);
  }

  bool isVisible() const { return !lv_obj_has_flag(uiLayer, LV_OBJ_FLAG_HIDDEN); }

 private:
  struct SettingItem {
    int arcMin;
    int arcMax;
    int currentVal;
    std::function<void(int)> apply;
    std::function<void(lv_obj_t*, int)> updateLabel;
  };

  std::array<SettingItem, 6> settings;
  lv_obj_t* uiLayer;
  lv_obj_t* arc;
  lv_obj_t* roller;
  lv_obj_t* valueLabel;
  lv_anim_t fadeAnim;

  void syncArcToRoller() {
    uint16_t activeId = lv_roller_get_selected(roller);
    if (activeId < settings.size()) {
      lv_arc_set_range(arc, settings[activeId].arcMin, settings[activeId].arcMax);
      lv_arc_set_value(arc, settings[activeId].currentVal);
      // Update the label instantly when the roller changes
      settings[activeId].updateLabel(valueLabel, settings[activeId].currentVal);
    }
  }

  static void onScreenTapped(lv_event_t* e) {
    auto* self = static_cast<SettingsController*>(lv_event_get_user_data(e));
    self->wakeUp();
  }

  static void onRollerValueChanged(lv_event_t* e) {
    auto* self = static_cast<SettingsController*>(lv_event_get_user_data(e));
    self->syncArcToRoller();
    self->wakeUp();
  }

  static void onArcValueChanged(lv_event_t* e) {
    auto* self = static_cast<SettingsController*>(lv_event_get_user_data(e));
    uint16_t activeId = lv_roller_get_selected(self->roller);

    if (activeId < self->settings.size()) {
      int newVal = lv_arc_get_value(self->arc);
      self->settings[activeId].currentVal = newVal;
      self->settings[activeId].apply(newVal);
      // Update the label dynamically as the arc is dragged
      self->settings[activeId].updateLabel(self->valueLabel, newVal);
    }
    self->wakeUp();
  }

  static void onInteraction(lv_event_t* e) {
    auto* self = static_cast<SettingsController*>(lv_event_get_user_data(e));
    self->wakeUp();  // Reset fade timer on press/release
  }
};