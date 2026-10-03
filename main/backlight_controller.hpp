#pragma once

#include <functional>
#include <lvgl.h>

class BacklightController {
 public:
  using BrightnessCallback = std::function<void(int)>;

  explicit BacklightController(BrightnessCallback cb, int initialLevel = 50)
      : onBrightnessChanged{std::move(cb)} {
    // 1. Setup the Overlay Layer (Hidden by default)
    // Placed on lv_layer_top() so it sits above any other LVGL screens
    uiLayer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(uiLayer);  // Transparent background
    lv_obj_set_size(uiLayer, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(uiLayer, LV_OBJ_FLAG_HIDDEN);

    // 2. Create the Arc
    arc = lv_arc_create(uiLayer);
    lv_obj_set_size(arc, 260, 260);
    lv_obj_center(arc);
    lv_arc_set_range(arc, 5, 100);  // 5% to 100%
    lv_arc_set_value(arc, initialLevel);

    // 3. Event Routing
    // Make the base screen catch taps to wake up the UI
    lv_obj_add_flag(lv_scr_act(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lv_scr_act(), onScreenTapped, LV_EVENT_CLICKED, this);

    // Arc interaction events
    lv_obj_add_event_cb(arc, onArcValueChanged, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(arc, onArcInteraction, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(arc, onArcInteraction, LV_EVENT_RELEASED, this);

    // 4. Fade Out Animation
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, uiLayer);
    lv_anim_set_time(&fadeAnim, 400);    // 400ms fade duration
    lv_anim_set_delay(&fadeAnim, 2500);  // 2.5s delay before fading begins
    lv_anim_set_values(&fadeAnim, LV_OPA_COVER, LV_OPA_TRANSP);

    // Apply the fading value to the layer's opacity
    lv_anim_set_exec_cb(&fadeAnim, [](void* var, int32_t v) {
      lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(v), 0);
    });

    // Hide the layer completely once the animation finishes to save render cycles
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
  lv_obj_t* uiLayer;
  lv_obj_t* arc;
  lv_anim_t fadeAnim;
  BrightnessCallback onBrightnessChanged;

  static void onScreenTapped(lv_event_t* e) {
    auto* self = static_cast<BacklightController*>(lv_event_get_user_data(e));
    self->wakeUp();
  }

  static void onArcValueChanged(lv_event_t* e) {
    auto* self = static_cast<BacklightController*>(lv_event_get_user_data(e));
    if (self->onBrightnessChanged) {
      self->onBrightnessChanged(lv_arc_get_value(self->arc));
    }
    self->wakeUp();  // Reset the fade timer while actively dragging
  }

  static void onArcInteraction(lv_event_t* e) {
    auto* self = static_cast<BacklightController*>(lv_event_get_user_data(e));
    self->wakeUp();  // Reset timer on hold/release
  }
};