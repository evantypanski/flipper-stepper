#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

typedef enum {
  ViewIndexSubmenu,
  ViewIndexStepping,
  ViewIndexStepPin,
  ViewIndexDirPin,
} ViewIndex;

// Enumeration of submenu items.
typedef enum {
  SubmenuIndexStep,
  SubmenuIndexStepPin,
  SubmenuIndexDirPin,
} SubmenuIndex;

// Each pin that can be used
typedef enum {
  PinA7,
  PinA6,
  PinA4,
  PinB3,
  PinB2,
  PinC3,
  PinC1,
  PinC0,
} UsablePins;

typedef struct {
  Gui *gui;
  ViewDispatcher *view_dispatcher;
  Submenu *submenu;
  Submenu *stepmenu;
  Submenu *dirmenu;
  Widget *stepping_widget;
  const GpioPin *dir_pin;
  const GpioPin *step_pin;
  bool state;
} StepperContext;

// This function is called when the user has pressed the Back key.
static bool navigation_callback(void *ctx) {
  furi_assert(ctx);
  StepperContext *context = ctx;

  // Back means exit the application, which can be done by stopping the
  // ViewDispatcher.
  view_dispatcher_stop(context->view_dispatcher);

  return true;
}

static void stepper_tick_callback(void *ctx) {
  FURI_LOG_D("stepper", "TICK!");
  furi_assert(ctx);
  StepperContext *context = ctx;
  furi_hal_gpio_write(context->step_pin, true);
  furi_delay_us(10);
  furi_hal_gpio_write(context->step_pin, false);
}

static bool stepper_event_callback(void *ctx, uint32_t event) {
  furi_assert(ctx);
  StepperContext *context = ctx;

  if (event == ViewIndexStepping) {
    FURI_LOG_D("stepper", "Changing to stepping");
    view_dispatcher_set_tick_event_callback(context->view_dispatcher,
                                            stepper_tick_callback, 3000);

    furi_event_loop_tick_set(
        view_dispatcher_get_event_loop(context->view_dispatcher), 3,
        stepper_tick_callback, context);
  } else {
    furi_event_loop_tick_set(
        view_dispatcher_get_event_loop(context->view_dispatcher), 0, NULL,
        context);
  }
  // Switch to the requested view.
  view_dispatcher_switch_to_view(context->view_dispatcher, event);

  return true;
}

// This function is called when the user activates the "Switch View" submenu
// item.
static void stepper_dispatcher_app_submenu_callback(void *ctx, uint32_t index) {
  furi_assert(ctx);
  StepperContext *context = ctx;
  if (index == SubmenuIndexStep) {
    view_dispatcher_send_custom_event(context->view_dispatcher,
                                      ViewIndexStepping);
  } else if (index == SubmenuIndexStepPin) {
    view_dispatcher_send_custom_event(context->view_dispatcher,
                                      ViewIndexStepPin);
  } else if (index == SubmenuIndexDirPin) {
    view_dispatcher_send_custom_event(context->view_dispatcher,
                                      ViewIndexDirPin);
  }
}

static void switch_pin(StepperContext *context, const GpioPin **pin,
                       uint32_t index) {
  furi_hal_gpio_init(*pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
  switch (index) {
  case PinA7:
    *pin = furi_hal_resources_pin_by_number(2)->pin;
    break;
  case PinA6:
    *pin = furi_hal_resources_pin_by_number(3)->pin;
    break;
  case PinA4:
    *pin = furi_hal_resources_pin_by_number(4)->pin;
    break;
  case PinB3:
    *pin = furi_hal_resources_pin_by_number(5)->pin;
    break;
  case PinB2:
    *pin = furi_hal_resources_pin_by_number(6)->pin;
    break;
  case PinC3:
    *pin = furi_hal_resources_pin_by_number(7)->pin;
    break;
  case PinC1:
    *pin = furi_hal_resources_pin_by_number(15)->pin;
    break;
  case PinC0:
    *pin = furi_hal_resources_pin_by_number(16)->pin;
    break;
  }
  furi_hal_gpio_write(*pin, false);
  furi_hal_gpio_init(*pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
  view_dispatcher_send_custom_event(context->view_dispatcher, ViewIndexSubmenu);
}

static void stepper_dispatcher_step_pin_callback(void *ctx, uint32_t index) {
  furi_assert(ctx);
  StepperContext *context = ctx;
  switch_pin(context, &context->step_pin, index);
}

static void stepper_dispatcher_dir_pin_callback(void *ctx, uint32_t index) {
  furi_assert(ctx);
  StepperContext *context = ctx;
  switch_pin(context, &context->step_pin, index);
}

static void stepping_button_callback(GuiButtonType button_type,
                                     InputType input_type, void *ctx) {
  furi_assert(ctx);
  StepperContext *context = ctx;
  // Only request the view switch if the user short-presses the Center button.
  if (button_type == GuiButtonTypeCenter && input_type == InputTypeShort) {
    // Request switch to the Submenu view via the custom event queue.
    view_dispatcher_send_custom_event(context->view_dispatcher,
                                      ViewIndexSubmenu);
  }
}

static void add_pins_to_submenu(StepperContext *context, Submenu *submenu,
                                SubmenuItemCallback callback) {
  submenu_add_item(submenu, "A7", PinA7, callback, context);
  submenu_add_item(submenu, "A6", PinA6, callback, context);
  submenu_add_item(submenu, "A4", PinA4, callback, context);
  submenu_add_item(submenu, "B3", PinB3, callback, context);
  submenu_add_item(submenu, "B2", PinB2, callback, context);
  submenu_add_item(submenu, "C3", PinC3, callback, context);
  submenu_add_item(submenu, "C1", PinC1, callback, context);
  submenu_add_item(submenu, "C0", PinC0, callback, context);
}

static StepperContext *stepper_context_alloc(uint8_t dir_pin,
                                             uint8_t step_pin) {
  StepperContext *context = malloc(sizeof(StepperContext));

  context->dir_pin = furi_hal_resources_pin_by_number(dir_pin)->pin;
  context->step_pin = furi_hal_resources_pin_by_number(step_pin)->pin;

  context->gui = furi_record_open(RECORD_GUI);

  // Create and initialize the Submenu view.
  context->submenu = submenu_alloc();
  submenu_add_item(context->submenu, "Go!", SubmenuIndexStep,
                   stepper_dispatcher_app_submenu_callback, context);
  submenu_add_item(context->submenu, "Step Pin", SubmenuIndexStepPin,
                   stepper_dispatcher_app_submenu_callback, context);
  submenu_add_item(context->submenu, "Dir Pin", SubmenuIndexDirPin,
                   stepper_dispatcher_app_submenu_callback, context);

  // Step pin submenu
  context->stepmenu = submenu_alloc();
  add_pins_to_submenu(context, context->stepmenu,
                      stepper_dispatcher_step_pin_callback);

  // Dir pin submenu
  context->dirmenu = submenu_alloc();
  add_pins_to_submenu(context, context->dirmenu,
                      stepper_dispatcher_dir_pin_callback);

  // Create the stepping widget.
  context->stepping_widget = widget_alloc();
  widget_add_string_multiline_element(context->stepping_widget, 64, 32,
                                      AlignCenter, AlignCenter, FontSecondary,
                                      "Press the Button below");
  widget_add_button_element(context->stepping_widget, GuiButtonTypeCenter,
                            "Switch View", stepping_button_callback, context);

  context->view_dispatcher = view_dispatcher_alloc();
  view_dispatcher_attach_to_gui(context->view_dispatcher, context->gui,
                                ViewDispatcherTypeFullscreen);
  view_dispatcher_add_view(context->view_dispatcher, ViewIndexSubmenu,
                           submenu_get_view(context->submenu));
  view_dispatcher_add_view(context->view_dispatcher, ViewIndexStepPin,
                           submenu_get_view(context->stepmenu));
  view_dispatcher_add_view(context->view_dispatcher, ViewIndexDirPin,
                           submenu_get_view(context->dirmenu));
  view_dispatcher_add_view(context->view_dispatcher, ViewIndexStepping,
                           widget_get_view(context->stepping_widget));
  view_dispatcher_set_custom_event_callback(context->view_dispatcher,
                                            stepper_event_callback);

  // Set the navigation, or back button callback. It will be called if the
  // user pressed the Back button and the event was not handled in the
  // currently displayed view.
  view_dispatcher_set_navigation_event_callback(context->view_dispatcher,
                                                navigation_callback);

  // The context will be passed to the callbacks as a parameter, so we have
  // access to our application object.
  view_dispatcher_set_event_callback_context(context->view_dispatcher, context);
  context->state = false;
  return context;
}

static void stepper_context_free(StepperContext *context) {
  view_dispatcher_remove_view(context->view_dispatcher, ViewIndexSubmenu);
  view_dispatcher_remove_view(context->view_dispatcher, ViewIndexStepPin);
  view_dispatcher_remove_view(context->view_dispatcher, ViewIndexDirPin);
  view_dispatcher_remove_view(context->view_dispatcher, ViewIndexStepping);
  view_dispatcher_free(context->view_dispatcher);
  submenu_free(context->stepmenu);
  submenu_free(context->dirmenu);
  submenu_free(context->submenu);
  furi_record_close(RECORD_GUI);
  free(context);
}

int32_t stepper_app(void *p) {
  UNUSED(p);
  StepperContext *context = stepper_context_alloc(6, 7);

  furi_hal_gpio_write(context->step_pin, false);
  furi_hal_gpio_init(context->step_pin, GpioModeOutputPushPull, GpioPullNo,
                     GpioSpeedLow);
  furi_hal_gpio_write(context->dir_pin, false);
  furi_hal_gpio_init(context->dir_pin, GpioModeOutputPushPull, GpioPullNo,
                     GpioSpeedLow);

  view_dispatcher_switch_to_view(context->view_dispatcher, ViewIndexSubmenu);
  view_dispatcher_run(context->view_dispatcher);

  furi_hal_gpio_init(context->step_pin, GpioModeAnalog, GpioPullNo,
                     GpioSpeedLow);
  furi_hal_gpio_init(context->dir_pin, GpioModeAnalog, GpioPullNo,
                     GpioSpeedLow);

  stepper_context_free(context);

  return 0;
}
