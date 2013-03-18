 /* config.h for minime.c */

#ifndef CONFIG_H
#define CONFIG_H

#define Mod                 Mod1Mask
#define Mod4                Mod4Mask
#define BORDER_WIDTH        2
#define DESKS               4
#define RESIZEMOVEKEY       Mod1Mask

/* Colors */
#define FOCUS           "#664422" // dkorange
#define UNFOCUS         "#004050" // blueish

#define DESKTOPCHANGE(K,N)\
  {  Mod, K, change_desktop, {.a = N}},\
   {  Mod4|ShiftMask,  K, send_to_desktop, {.a = N}},\
    {  Mod|ShiftMask,  K, follow_to_desktop, {.a = N}},

const char* dmenucmd[] = {"dmenu_run","-i","-nb","#666622","-nf","white",NULL};
const char* termcmd[]  = {"Terminal",NULL};

static key keys[] = {
    // MOD               KEY         FUNCTION            ARGS
    {  Mod,             XK_c,      kill_client,       {NULL}},
    {  Mod,             XK_Tab,    next_win,          {NULL}},
    {  Mod,             XK_v,      spawn,             {.com = dmenucmd}},
    {  Mod,             XK_j,      window_x,          {.a = -10}},  
    {  Mod,             XK_k,      window_x,          {.a = 10}},
    {  Mod,             XK_h,      window_y,          {.a = -10}},
    {  Mod,             XK_l,      window_y,          {.a = 10}},
    {  Mod,             XK_w,      resize_width,      {.a = -10}},
    {  Mod,             XK_e,      resize_width,      {.a = 10}},
    {  Mod,             XK_r,      resize_height,     {.a = -10}},
    {  Mod,             XK_t,      resize_height,     {.a = 10}},
    {  Mod4,            XK_t,      spawn,             {.com = termcmd}},
    {  Mod|ShiftMask,   XK_f,      fullscreen,        {NULL}},
    {  Mod|ControlMask, XK_q,      quit,              {NULL}},
       DESKTOPCHANGE(   XK_1,                              0)
       DESKTOPCHANGE(   XK_2,                              1)
       DESKTOPCHANGE(   XK_3,                              2)
       DESKTOPCHANGE(   XK_4,                              3)
};

#endif

