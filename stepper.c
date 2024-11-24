#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <gui/view_port.h>

#define TEXT_STORE_SIZE  64U

typedef struct {
    Gui *gui;
    ViewPort *view_port;
    FuriMessageQueue *event_queue;
    const GpioPin *dirPin;
    const GpioPin *stepPin;
    bool state;
} StepperContext;

static void stepper_draw_callback(Canvas* canvas, void* ctx) {
    StepperContext* context = ctx;

    char text_store[TEXT_STORE_SIZE];
    snprintf(text_store, TEXT_STORE_SIZE, "State: %d", context->state);

    const size_t middle_x = canvas_width(canvas) / 2U;
    canvas_draw_str_aligned(canvas, middle_x, 58, AlignCenter, AlignBottom, text_store);
}

/* This function is called from the GUI thread. All it does is put the event
   into the application's queue so it can be processed later. */
static void stepper_input_callback(InputEvent* event, void* ctx) {
    StepperContext* context = ctx;
    furi_message_queue_put(context->event_queue, event, FuriWaitForever);
}

static StepperContext* stepper_context_alloc(uint8_t dirPin, uint8_t stepPin) {
  StepperContext* context = malloc(sizeof(StepperContext));

  context->view_port = view_port_alloc();
  view_port_draw_callback_set(context->view_port, stepper_draw_callback, context);
  view_port_input_callback_set(context->view_port, stepper_input_callback, context);
  context->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
  context->dirPin = furi_hal_resources_pin_by_number(dirPin)->pin;
  context->stepPin = furi_hal_resources_pin_by_number(stepPin)->pin;

  context->gui = furi_record_open(RECORD_GUI);
  gui_add_view_port(context->gui, context->view_port, GuiLayerFullscreen);
  context->state = false;

  return context;
}

static void stepper_context_free(StepperContext* context) {
    view_port_enabled_set(context->view_port, false);
    gui_remove_view_port(context->gui, context->view_port);

    furi_message_queue_free(context->event_queue);
    view_port_free(context->view_port);

    furi_record_close(RECORD_GUI);
}

int32_t stepper_app(void* p) {
  UNUSED(p);
  StepperContext *context = stepper_context_alloc(6, 7);
  furi_hal_gpio_write(context->stepPin, false);
  furi_hal_gpio_init(context->stepPin,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
  furi_hal_gpio_write(context->dirPin, false);
  furi_hal_gpio_init(context->dirPin,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);

  for (bool is_running = true; is_running; ) {
    // Step before checking anything else
    if (context->state) {
      furi_hal_gpio_write(context->stepPin, true);
      furi_delay_us(3);
      furi_hal_gpio_write(context->stepPin, false);
      furi_delay_us(3000);
    }

    InputEvent event;
    // If we're stepping, don't wait
    uint32_t timeout = context->state ? 0 : FuriWaitForever;
    const FuriStatus status = furi_message_queue_get(context->event_queue, &event, timeout);

    if ((status != FuriStatusOk) || (event.type != InputTypeShort)) {
      continue;
    }

    switch (event.key) {
      case InputKeyBack: is_running = false; break;
      case InputKeyOk:
        context->state = !context->state;
        view_port_update(context->view_port);
        break;
      default: break;
    }

  }

  furi_hal_gpio_init(context->stepPin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
  furi_hal_gpio_init(context->dirPin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
  stepper_context_free(context);

  return 0;
}
