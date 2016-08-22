#ifndef CONFIG_USER_H
#define CONFIG_USER_H

#include "../../config.h"

/* We only use CTL_T style mod-tap keys, so we want the built-in
 * mod-tap-interrupt behavior to allow quickly typing CTRL-W arrow
 * sequences with a CTL_T(ESC) key */
#undef IGNORE_MOD_TAP_INTERRUPT

#endif
