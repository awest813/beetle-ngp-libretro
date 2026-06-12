#ifndef BEETLENGP_MENU_H
#define BEETLENGP_MENU_H

#include <stdbool.h>

typedef enum menu_action
{
   MENU_ACTION_NONE = 0,
   MENU_ACTION_LOAD,
   MENU_ACTION_QUIT
} menu_action_t;

/* Main launcher: Load Game, Settings, or Exit. */
menu_action_t menu_main(char **rom_path_out);

/* ROM browser (used from main menu). */
char *menu_pick_rom(void);

/* Settings editor; saves to beetlengp.cfg on exit. */
void menu_settings(void);

/* Settings with optional ROM path for save-file status (in-game). */
void menu_settings_for_rom(const char *rom_path);

#endif
