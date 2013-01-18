 /* config.h for minime.c */

#ifndef CONFIG_H
#define CONFIG_H

#define Mod                 Mod1Mask
#define Mod4                Mod4Mask
#define Shft                ShiftMask
#define Ctrl                ControlMask
// #define BORDER_WIDTH     2
#define DESKS               4

#define DESKTOPCHANGE(K,N)\
  {  Mod, K, change_desktop, {.a = N}},\
   {  Mod4|ShiftMask,  K, send_to_desktop, {.a = N}},

const char* dmenucmd[]      = {"dmenu_run","-i","-nb","#666622","-nf","white",NULL};

static key keys[] = {
    // MOD               KEY             FUNCTION            ARGS
//    {  Mod,             XK_c,          kill_client,       {NULL}},
    {  Mod,             XK_Tab,        next_win,          {NULL}},
    {  Mod,             XK_v,          spawn,             {.com = dmenucmd}},
    {  Mod|Ctrl,        XK_q,          quit,              {NULL}},
       DESKTOPCHANGE(   XK_1,                              0)
       DESKTOPCHANGE(   XK_2,                              1)
       DESKTOPCHANGE(   XK_3,                              2)
       DESKTOPCHANGE(   XK_4,                              3)
};

#endif

