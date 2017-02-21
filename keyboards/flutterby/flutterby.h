#ifndef FLUTTERBY_H
#define FLUTTERBY_H

#include "quantum.h"

#define KEYMAP(                        \
    /* left hand */                    \
         k01, k02, k03, k04, k05, k06, \
    k10, k11, k12, k13, k14, k15, k16, \
    k20, k21, k22, k23, k24, k25, k26, \
    k30, k31, k32, k33, k34, k35,      \
                                  k07, \
                             k36, k17, \
                             k37, k27, \
    /* right hand */                   \
    k09, k0a, k0b, k0c, k0d, k0e,      \
    k19, k1a, k1b, k1c, k1d, k1e, k1f, \
    k29, k2a, k2b, k2c, k2d, k2e, k2f, \
         k3a, k3b, k3c, k3d, k3e, k3f, \
    k08,                               \
    k18, k39,                          \
    k28, k38) {                        \
  { KC_NO, k01, k02, k03, k04, k05, k06, k07, k08, k09, k0a, k0b, k0c, k0d, k0e, KC_NO }, \
  {   k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k1a, k1b, k1c, k1d, k1e, k1f }, \
  {   k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k2a, k2b, k2c, k2d, k2e, k2f }, \
  {   k30, k31, k32, k33, k34, k35, k36, k37, k38, k39, k3a, k3b, k3c, k3d, k3e, k3f }, \
}

bool sx1509_init(void);
uint16_t sx1509_read(void);
bool sx1509_make_ready(void);

#endif
