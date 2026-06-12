#ifndef DC_INPUT_H
#define DC_INPUT_H

#include <libretro.h>
#include <stdint.h>

/* Dreamcast controller -> libretro joypad (Beetle NGP mapping). */
#define DC_INPUT_TRIGGER_THRESHOLD 160

typedef struct dc_input
{
   unsigned port;
   uint32_t maple;
   uint32_t gamepad;
   uint8_t ltrig;
   uint8_t rtrig;
} dc_input_t;

void dc_input_poll(dc_input_t *input);
int16_t dc_input_state(const dc_input_t *input, unsigned device, unsigned id);
uint32_t dc_input_maple_buttons(const dc_input_t *input);
bool dc_input_ltrigger_pressed(const dc_input_t *input, uint8_t *prev_ltrig);
bool dc_input_rtrigger_pressed(const dc_input_t *input, uint8_t *prev_rtrig);

/* Raw Maple buttons for launcher / pause menus (no Start filtering). */
typedef struct dc_menu_input
{
   uint32_t buttons;
   uint32_t previous;
   uint8_t ltrig;
   uint8_t rtrig;
   uint8_t prev_ltrig;
   uint8_t prev_rtrig;
} dc_menu_input_t;

void dc_menu_input_reset(dc_menu_input_t *input);
bool dc_menu_input_poll(dc_menu_input_t *input, unsigned port, uint32_t *pressed);
bool dc_menu_input_ltrigger_edge(dc_menu_input_t *input);
bool dc_menu_input_rtrigger_edge(dc_menu_input_t *input);

#endif
