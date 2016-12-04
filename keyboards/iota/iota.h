#ifndef IOTA_H
#define IOTA_H

#include "quantum.h"
#include <stddef.h>
#ifdef __AVR__
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

extern bool iota_gfx_init(void);
extern void iota_gfx_task(void);
extern bool iota_gfx_off(void);
extern bool iota_gfx_on(void);
extern void iota_gfx_flush(void);
extern void iota_gfx_write_char(uint8_t c);
extern void iota_gfx_write(const char *data);
extern void iota_gfx_write_P(const char *data);
extern void iota_gfx_clear_screen(void);

bool iota_mcp23017_init(void);
bool iota_mcp23017_make_ready(void);
uint16_t iota_mcp23017_read(void);
bool iota_mcp23017_enable_interrupts(void);

#endif
