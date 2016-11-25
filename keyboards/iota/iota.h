#ifndef IOTA_H
#define IOTA_H

#include "quantum.h"
#include <stddef.h>
#include <avr/io.h>
#include <avr/interrupt.h>

extern void iota_gfx_init(void);
extern void iota_gfx_off(void);

bool iota_mcp23017_init(void);
bool iota_mcp23017_make_ready(void);
uint16_t iota_mcp23017_read(void);
bool iota_mcp23017_enable_interrupts(void);

#endif
