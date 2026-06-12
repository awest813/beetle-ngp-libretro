#ifndef BEETLENGP_MENU_H
#define BEETLENGP_MENU_H

#include <stdbool.h>

typedef enum menu_action
{
   MENU_ACTION_NONE = 0,
   MENU_ACTION_LOAD,
   MENU_ACTION_QUIT
} menu_action_t;

typedef enum menu_pause_action
{
   MENU_PAUSE_RESUME = 0,
   MENU_PAUSE_SAVE,
   MENU_PAUSE_LOAD,
   MENU_PAUSE_SETTINGS,
   MENU_PAUSE_QUIT
} menu_pause_action_t;

/* Brief boot splash (skippable). Call after dc_ui_init / video init. */
void menu_splash(void);

/* One-frame loading indicator before core loads a ROM. */
void menu_loading_screen(const char *rom_path);

/* Main launcher: Continue (if available), Load Game, Settings, or Exit. */
menu_action_t menu_main(char **rom_path_out);

/* ROM browser (used from main menu). */
char *menu_pick_rom(void);

/* Settings editor; saves to beetlengp.cfg on exit. */
void menu_settings(void);

/* Settings with optional ROM path for save-file status (in-game). */
void menu_settings_for_rom(const char *rom_path);

/* In-game pause overlay (Resume, Save, Load, Settings, Quit). */
menu_pause_action_t menu_pause(const char *rom_path);

#endif
