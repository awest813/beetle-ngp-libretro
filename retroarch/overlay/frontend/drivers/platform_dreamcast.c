/* RetroArch Dreamcast platform frontend (KallistiOS) */

#include <kos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <boolean.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

#ifndef IS_SALAMANDER
#include "../../defaults.h"
#include "../../paths.h"
#include "../../verbosity.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../frontend_driver.h"

static void dc_fill_user_paths(void)
{
#ifndef IS_SALAMANDER
   const char *root = "/sd/retroarch";

   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT], root,
         sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], root, "cores",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], root, "info",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], root, "autoconfig",
         sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], root, "assets",
         sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], root, "filters/video",
         sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], root, "filters/audio",
         sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], root, "cheats",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], root, "overlays",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY], root, "overlays/keyboards",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], root, "playlists",
         sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], root, "config",
         sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
         g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "remaps",
         sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], root, "savefiles",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], root, "savestates",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], root, "system",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], root, "logs",
         sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   fill_pathname_join(g_defaults.path_config, root, "retroarch.cfg",
         sizeof(g_defaults.path_config));
#endif
}

static void frontend_dc_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
   struct rarch_main_wrap *params = (struct rarch_main_wrap *)params_data;
#endif

   (void)argc;
   (void)argv;
   (void)args;

   dc_fill_user_paths();

#ifndef IS_SALAMANDER
   if (params && *argc > 1 && argv && argv[1] && argv[1][0])
   {
      params->content_path = argv[1];
      params->sram_path    = NULL;
      params->state_path   = NULL;
      params->config_path  = NULL;
      params->libretro_path = NULL;
      params->flags &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
            | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
      params->flags |= RARCH_MAIN_WRAP_FLAG_TOUCHED;
   }

   dir_check_defaults("custom.ini");
#endif
}

static void frontend_dc_init(void *data)
{
   (void)data;
}

static void frontend_dc_deinit(void *data)
{
   (void)data;
}

static void frontend_dc_shutdown(bool unused)
{
   (void)unused;
#ifndef IS_SALAMANDER
   arch_exit();
#endif
}

static enum frontend_architecture frontend_dc_get_arch(void)
{
   return FRONTEND_ARCH_NONE;
}

static int frontend_dc_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t *)data;
   enum msg_hash_enums enum_idx = load_content
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

   menu_entries_append(list, "/sd/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx, FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list, "/ide/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx, FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list, "/pc/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx, FILE_TYPE_DIRECTORY, 0, 0, NULL);
#endif

   (void)load_content;
   return 0;
}

static uint64_t frontend_dc_get_total_mem(void)
{
   return 0;
}

static uint64_t frontend_dc_get_free_mem(void)
{
   return 0;
}

frontend_ctx_driver_t frontend_ctx_dreamcast = {
   frontend_dc_get_env,
   frontend_dc_init,
   frontend_dc_deinit,
   NULL,
   NULL,
   NULL,
   NULL,
   frontend_dc_shutdown,
   NULL,
   NULL,
   NULL,
   frontend_dc_get_arch,
   NULL,
   frontend_dc_parse_drive_list,
   frontend_dc_get_total_mem,
   frontend_dc_get_free_mem,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "dreamcast",
   NULL
};
