#ifndef DC_INPUT_H
#define DC_INPUT_H

#include <libretro.h>
#include <stdint.h>

/* Dreamcast controller -> libretro joypad (Beetle NGP mapping). */
typedef struct dc_input
{
   unsigned port;
   uint32_t maple;
   uint32_t gamepad;
} dc_input_t;

void dc_input_poll(dc_input_t *input);
int16_t dc_input_state(const dc_input_t *input, unsigned device, unsigned id);
uint32_t dc_input_maple_buttons(const dc_input_t *input);

#endif
