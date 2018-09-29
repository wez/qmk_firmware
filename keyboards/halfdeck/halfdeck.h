#pragma once

#include "quantum.h"

// This maps the human perceived logical keycap layout to the less intuitive
// minimized hardware key matrix wiring.  This mapping is coupled with the
// row and column pin bindings in matrix.c
// lsw == "left switch", a switch position on the left half
// rsw == "right switch", a switch position on the right half
// LSW00 is the switch position labeled SW00 in the top left corner
// of the left half of the keyboard.  RSW00 is the switch position
// labeled SW00 on the top right corner of the right half
#define KEYMAP(\
               lsw00, lsw10, lsw50, lsw02, lsw52, lsw04, lsw54, \
               lsw30, lsw20, lsw11, lsw12, lsw22, lsw14, lsw44, \
               lsw40, lsw51, lsw01, lsw21, lsw32, lsw24, lsw34, \
               lsw03, lsw13, lsw41, lsw31, lsw42, lsw05, \
                                                         lsw55, \
                      lsw23,                      lsw15, lsw25, \
               lsw53, lsw33, lsw43,               lsw45, lsw35, \
               rsw54, rsw04, rsw52, rsw02, rsw50, rsw10, rsw00, \
               rsw44, rsw14, rsw22, rsw12, rsw11, rsw20, rsw30, \
               rsw34, rsw24, rsw32, rsw21, rsw01, rsw51, rsw40, \
                      rsw05, rsw42, rsw31, rsw41, rsw13, rsw03, \
               rsw55,                                           \
               rsw25, rsw15,                      rsw23,        \
               rsw35, rsw45,               rsw43, rsw33, rsw53) \
      {\
        {    lsw00, lsw01, lsw02, lsw03, lsw04, lsw05, rsw00, rsw01, rsw02, rsw03, rsw04, rsw05 }, \
        {    lsw10, lsw11, lsw12, lsw13, lsw14, lsw15, rsw10, rsw11, rsw12, rsw13, rsw14, rsw15 }, \
        {    lsw20, lsw21, lsw22, lsw23, lsw24, lsw25, rsw20, rsw21, rsw22, rsw23, rsw24, rsw25 }, \
        {    lsw30, lsw31, lsw32, lsw33, lsw34, lsw35, rsw30, rsw31, rsw32, rsw33, rsw34, rsw35 }, \
        {    lsw40, lsw41, lsw42, lsw43, lsw44, lsw45, rsw40, rsw41, rsw42, rsw43, rsw44, rsw45 }, \
        {    lsw50, lsw51, lsw52, lsw53, lsw54, lsw55, rsw50, rsw51, rsw52, rsw53, rsw54, rsw55 }}


bool sx1509_init(void);
uint16_t sx1509_read(void);
bool sx1509_make_ready(void);
bool sx1509_select_row(uint8_t row);
bool sx1509_unselect_row(uint8_t row);
bool sx1509_unselect_rows(void);
uint8_t sx1509_read_b(uint8_t current_row);

void halfdeck_led_enable(bool on);
void halfdeck_blink_led(int times);
