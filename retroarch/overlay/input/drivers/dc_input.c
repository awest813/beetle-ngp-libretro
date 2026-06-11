/* RetroArch Dreamcast input driver (joypad-only) */

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <boolean.h>
#include <libretro.h>

#include "../input_driver.h"

static void *dc_input_init(const char *joypad_driver) { (void)joypad_driver; return (void *)-1; }
static void dc_input_poll(void *data) { (void)data; }
static int16_t dc_input_state(void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data;
   (void)sec_joypad;
   (void)joypad_info;
   (void)binds;
   (void)keyboard_mapping_blocked;
   (void)port;
   (void)device;
   (void)idx;
   (void)id;
   (void)joypad;
   return 0;
}
static void dc_input_free(void *data) { (void)data; }

static uint64_t dc_input_get_capabilities(void *data)
{
   (void)data;
   return (1 << RETRO_DEVICE_JOYPAD);
}

input_driver_t input_dc = {
   dc_input_init,
   dc_input_poll,
   dc_input_state,
   dc_input_free,
   NULL,
   NULL,
   dc_input_get_capabilities,
   "dc",
   NULL,
   NULL,
   NULL
};
