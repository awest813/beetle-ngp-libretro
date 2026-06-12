#include "dc_input.h"

#include <kos.h>
#include <dc/maple/controller.h>

static maple_device_t *dc_input_device(unsigned port)
{
   if (port != 0)
      return NULL;

   return maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
}

static uint32_t filter_system_buttons(uint32_t buttons)
{
   if (buttons & CONT_START)
      buttons &= ~(CONT_START | CONT_A | CONT_B | CONT_X | CONT_Y);

   return buttons;
}

void dc_input_poll(dc_input_t *input)
{
   cont_state_t *state;
   uint32_t buttons = 0;

   if (!input)
      return;

   input->maple   = 0;
   input->gamepad = 0;
   input->ltrig   = 0;
   input->rtrig   = 0;

   state = (cont_state_t *)maple_dev_status(dc_input_device(input->port));
   if (!state)
      return;

   input->maple = state->buttons;
   input->ltrig = (uint8_t)state->ltrig;
   input->rtrig = (uint8_t)state->rtrig;
   buttons      = filter_system_buttons(state->buttons);

   if (buttons & CONT_DPAD_UP)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
   if (buttons & CONT_DPAD_DOWN)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   if (buttons & CONT_DPAD_LEFT)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
   if (buttons & CONT_DPAD_RIGHT)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   if (buttons & CONT_B)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_B);
   if (buttons & CONT_A)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_A);
   if (buttons & CONT_START)
      input->gamepad |= (1 << RETRO_DEVICE_ID_JOYPAD_START);
}

int16_t dc_input_state(const dc_input_t *input, unsigned device, unsigned id)
{
   if (!input || device != RETRO_DEVICE_JOYPAD)
      return 0;

   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)(input->gamepad & 0xffff);

   if (id >= 16)
      return 0;

   return (input->gamepad & (1u << id)) ? 1 : 0;
}

uint32_t dc_input_maple_buttons(const dc_input_t *input)
{
   if (!input)
      return 0;

   return input->maple;
}

static bool trigger_pressed(uint8_t value, uint8_t *prev)
{
   bool pressed = false;

   if (!prev)
      return false;

   if (value >= DC_INPUT_TRIGGER_THRESHOLD && *prev < DC_INPUT_TRIGGER_THRESHOLD)
      pressed = true;

   *prev = value;
   return pressed;
}

bool dc_input_ltrigger_pressed(const dc_input_t *input, uint8_t *prev_ltrig)
{
   if (!input)
      return false;

   return trigger_pressed(input->ltrig, prev_ltrig);
}

bool dc_input_rtrigger_pressed(const dc_input_t *input, uint8_t *prev_rtrig)
{
   if (!input)
      return false;

   return trigger_pressed(input->rtrig, prev_rtrig);
}

void dc_menu_input_reset(dc_menu_input_t *input)
{
   if (!input)
      return;

   input->buttons    = 0;
   input->previous   = 0;
   input->ltrig      = 0;
   input->rtrig      = 0;
   input->prev_ltrig = 0;
   input->prev_rtrig = 0;
}

bool dc_menu_input_poll(dc_menu_input_t *input, unsigned port, uint32_t *pressed)
{
   maple_device_t *device;
   cont_state_t *state;
   uint32_t newly;

   if (!input)
      return false;

   if (pressed)
      *pressed = 0;

   if (port != 0)
      return false;

   device = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
   if (!device)
      return false;

   state = (cont_state_t *)maple_dev_status(device);
   if (!state)
      return false;

   input->buttons = state->buttons;
   input->ltrig   = (uint8_t)state->ltrig;
   input->rtrig   = (uint8_t)state->rtrig;

   newly = input->buttons & ~input->previous;
   input->previous = input->buttons;

   if (pressed)
      *pressed = newly;

   return true;
}

bool dc_menu_input_ltrigger_edge(dc_menu_input_t *input)
{
   if (!input)
      return false;

   return trigger_pressed(input->ltrig, &input->prev_ltrig);
}

bool dc_menu_input_rtrigger_edge(dc_menu_input_t *input)
{
   if (!input)
      return false;

   return trigger_pressed(input->rtrig, &input->prev_rtrig);
}
