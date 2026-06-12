/* RetroArch Dreamcast joypad driver (Maple controller) */

#include <kos.h>
#include <dc/maple/controller.h>

#include <stdint.h>
#include <string.h>

#include "../../config.def.h"

#include "../input_driver.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"

enum
{
   DC_BTN_B = 0,
   DC_BTN_A,
   DC_BTN_X,
   DC_BTN_Y,
   DC_BTN_START,
   DC_BTN_DPAD_UP,
   DC_BTN_DPAD_DOWN,
   DC_BTN_DPAD_LEFT,
   DC_BTN_DPAD_RIGHT,
   DC_BTN_MENU
};

extern uint64_t lifecycle_state;

static maple_device_t *dc_controller;
static uint64_t pad_state[DEFAULT_MAX_PADS];

static const char *dc_joypad_name(unsigned pad)
{
   (void)pad;
   return "Dreamcast Controller";
}

static void *dc_joypad_init(void *data)
{
   unsigned i;

   (void)data;

   dc_controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      input_autoconfigure_connect(
            dc_joypad_name(i),
            NULL, NULL,
            dc_joypad.ident,
            i, 0, 0);

   dc_joypad_poll();
   return (void *)-1;
}

static bool dc_joypad_query_pad(unsigned pad)
{
   return (pad == 0 && dc_controller != NULL);
}

static void dc_joypad_destroy(void) { }

static int32_t dc_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;

   return (pad_state[port] & (UINT64_C(1) << joykey)) ? 1 : 0;
}

static void dc_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (port < DEFAULT_MAX_PADS)
      BITS_COPY64_PTR(state, pad_state[port]);
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t dc_joypad_axis(unsigned port, uint32_t joyaxis)
{
   (void)port;
   (void)joyaxis;
   return 0;
}

static int16_t dc_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret = 0;
   uint16_t port_idx = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      const uint64_t joykey = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey : joypad_info->auto_binds[i].joykey;

      if ((uint16_t)joykey != NO_BTN
            && (pad_state[port_idx] & (UINT64_C(1) << joykey)))
         ret |= (1 << i);
   }

   return ret;
}

static void dc_joypad_poll(void)
{
   cont_state_t *state = NULL;
   uint64_t cur        = 0;

   pad_state[0] = 0;

   if (dc_controller)
      state = (cont_state_t *)maple_dev_status(dc_controller);

   uint32_t buttons;

   if (!state)
      return;

   buttons = state->buttons;
   if (buttons & CONT_START)
      buttons &= ~(CONT_START | CONT_A | CONT_B | CONT_X | CONT_Y);

   if (buttons & CONT_B)
      cur |= (UINT64_C(1) << DC_BTN_B);
   if (buttons & CONT_A)
      cur |= (UINT64_C(1) << DC_BTN_A);
   if (buttons & CONT_X)
      cur |= (UINT64_C(1) << DC_BTN_X);
   if (buttons & CONT_Y)
      cur |= (UINT64_C(1) << DC_BTN_Y);
   if (buttons & CONT_START)
      cur |= (UINT64_C(1) << DC_BTN_START);
   if (buttons & CONT_DPAD_UP)
      cur |= (UINT64_C(1) << DC_BTN_DPAD_UP);
   if (buttons & CONT_DPAD_DOWN)
      cur |= (UINT64_C(1) << DC_BTN_DPAD_DOWN);
   if (buttons & CONT_DPAD_LEFT)
      cur |= (UINT64_C(1) << DC_BTN_DPAD_LEFT);
   if (buttons & CONT_DPAD_RIGHT)
      cur |= (UINT64_C(1) << DC_BTN_DPAD_RIGHT);

   if ((state->buttons & CONT_START) && (state->buttons & CONT_A))
      cur |= (UINT64_C(1) << DC_BTN_MENU);

   pad_state[0] = cur;

   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);
   if (cur & (UINT64_C(1) << DC_BTN_MENU))
      BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
}

input_device_driver_t dc_joypad = {
   dc_joypad_init,
   dc_joypad_query_pad,
   dc_joypad_destroy,
   dc_joypad_button,
   dc_joypad_state,
   dc_joypad_get_buttons,
   dc_joypad_axis,
   dc_joypad_poll,
   NULL,
   NULL,
   NULL,
   NULL,
   dc_joypad_name,
   "dc"
};
