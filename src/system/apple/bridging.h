#pragma once

#define __APPLE__ 1
#define BUILD_EDITORS 1
#define BUILD_SURF 1

#include "studio/system.h"
#include "api.h"
#include "tic.h"

static inline void tic_input_set_mouse(tic80_input* input, s32 x, s32 y, s32 rx, s32 ry, bool left, bool middle, bool right, s32 scrollx, s32 scrolly, bool relative)
{
    if (relative)
    {
        input->mouse.rx = rx;
        input->mouse.ry = ry;
    }
    else
    {
        input->mouse.x = x;
        input->mouse.y = y;
    }
    input->mouse.left = left;
    input->mouse.middle = middle;
    input->mouse.right = right;
    input->mouse.scrollx = scrollx;
    input->mouse.scrolly = scrolly;
    input->mouse.relative = relative;
}

static inline int tic_keys_count_val(void) {
    return tic_keys_count;
}

static inline u8 tic_map_mac_key(u16 keyCode) {
    switch (keyCode) {
        case 0:   return tic_key_a;
        case 11:  return tic_key_b;
        case 8:   return tic_key_c;
        case 2:   return tic_key_d;
        case 14:  return tic_key_e;
        case 3:   return tic_key_f;
        case 5:   return tic_key_g;
        case 4:   return tic_key_h;
        case 34:  return tic_key_i;
        case 38:  return tic_key_j;
        case 40:  return tic_key_k;
        case 37:  return tic_key_l;
        case 46:  return tic_key_m;
        case 45:  return tic_key_n;
        case 31:  return tic_key_o;
        case 35:  return tic_key_p;
        case 12:  return tic_key_q;
        case 15:  return tic_key_r;
        case 1:   return tic_key_s;
        case 17:  return tic_key_t;
        case 32:  return tic_key_u;
        case 9:   return tic_key_v;
        case 13:  return tic_key_w;
        case 7:   return tic_key_x;
        case 16:  return tic_key_y;
        case 6:   return tic_key_z;
        case 29:  return tic_key_0;
        case 18:  return tic_key_1;
        case 19:  return tic_key_2;
        case 20:  return tic_key_3;
        case 21:  return tic_key_4;
        case 23:  return tic_key_5;
        case 22:  return tic_key_6;
        case 26:  return tic_key_7;
        case 28:  return tic_key_8;
        case 25:  return tic_key_9;
        case 27:  return tic_key_minus;
        case 24:  return tic_key_equals;
        case 33:  return tic_key_leftbracket;
        case 30:  return tic_key_rightbracket;
        case 42:  return tic_key_backslash;
        case 41:  return tic_key_semicolon;
        case 39:  return tic_key_apostrophe;
        case 50:  return tic_key_grave;
        case 43:  return tic_key_comma;
        case 47:  return tic_key_period;
        case 44:  return tic_key_slash;
        case 49:  return tic_key_space;
        case 48:  return tic_key_tab;
        case 36:  return tic_key_return;
        case 51:  return tic_key_backspace;
        case 117: return tic_key_delete;
        case 114: return tic_key_insert;
        case 116: return tic_key_pageup;
        case 121: return tic_key_pagedown;
        case 115: return tic_key_home;
        case 119: return tic_key_end;
        case 126: return tic_key_up;
        case 125: return tic_key_down;
        case 123: return tic_key_left;
        case 124: return tic_key_right;
        case 57:  return tic_key_capslock;
        case 55:
        case 59:  return tic_key_ctrl;
        case 56:
        case 60:  return tic_key_shift;
        case 58:
        case 61:  return tic_key_alt;
        case 53:  return tic_key_escape;
        case 122: return tic_key_f1;
        case 120: return tic_key_f2;
        case 99:  return tic_key_f3;
        case 118: return tic_key_f4;
        case 96:  return tic_key_f5;
        case 97:  return tic_key_f6;
        case 98:  return tic_key_f7;
        case 100: return tic_key_f8;
        case 101: return tic_key_f9;
        case 109: return tic_key_f10;
        case 103: return tic_key_f11;
        case 111: return tic_key_f12;
        case 82:  return tic_key_numpad0;
        case 83:  return tic_key_numpad1;
        case 84:  return tic_key_numpad2;
        case 85:  return tic_key_numpad3;
        case 86:  return tic_key_numpad4;
        case 87:  return tic_key_numpad5;
        case 88:  return tic_key_numpad6;
        case 89:  return tic_key_numpad7;
        case 91:  return tic_key_numpad8;
        case 92:  return tic_key_numpad9;
        case 69:  return tic_key_numpadplus;
        case 78:  return tic_key_numpadminus;
        case 67:  return tic_key_numpadmultiply;
        case 75:  return tic_key_numpaddivide;
        case 76:  return tic_key_numpadenter;
        case 65:  return tic_key_numpadperiod;
        default:  return tic_key_unknown;
    }
}

static inline void tic_sys_srand(unsigned int seed) {
    srand(seed);
}
