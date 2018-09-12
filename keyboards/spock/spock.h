#pragma once

#include "quantum.h"

// This maps the human perceived logical keycap layout to the less intuitive
// minimized hardware key matrix wiring.  This mapping is coupled with the
// row and column pin bindings in keyscanner.h
#define KEYMAP(\
               l00, l01, l02, l03, l04, l05, \
               l10, l11, l12, l13, l14, l15, \
               l20, l21, l22, l23, l24, l25, \
               l30, l31, l32, l33, l34, l35, \
               l40, l41, l42, l43, l44, l45, \
               l50, l51, l52, l53, l54, l55, \
               l60, l61, l62, l63, l64, l65, \
               r00, r01, r02, r03, r04, r05, \
               r10, r11, r12, r13, r14, r15, \
               r20, r21, r22, r23, r24, r25, \
               r30, r31, r32, r33, r34, r35, \
               r40, r41, r42, r43, r44, r45, \
               r50, r51, r52, r53, r54, r55, \
               r60, r61, r62, r63, r64, r65) \
      {\
        {    l10, l21, l03, l41, l04, l35, l61, r15, r24, r02, r44, r01, r30, r64}, \
        {     l00, l31, l13, l42, l14, l45, l62, r05, r34, r12, r43, r11, r40, r63}, \
        {     l01, l32, l23, l51, l15, l54, l52, r04, r33, r22, r54, r10, r51, r53}, \
        {     l11, l22, l33, l50, l25, l55, l44, r14, r23, r32, r55, r20, r50, r41}, \
        {     l12, l20, l43, l60, l24, l65, l53, r13, r25, r42, r65, r21, r60, r52}, \
        {     l02, l30, l34, l40, l05, l64, l63, r03, r35, r31, r45, r00, r61, r62}}


bool sx1509_init(void);
uint16_t sx1509_read(void);
bool sx1509_make_ready(void);
bool sx1509_select_row(uint8_t row);
bool sx1509_unselect_row(uint8_t row);
bool sx1509_unselect_rows(void);
uint8_t sx1509_read_b(uint8_t current_row);

void spock_led_enable(bool on);
void spock_blink_led(int times);
