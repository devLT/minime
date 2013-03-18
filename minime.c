/* minime.c */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
#define TABLENGTH(X)    (sizeof(X)/sizeof(*X))

typedef union {
    const char** com;
    const int a;
} Arg;

// Structs
typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
} key;

typedef struct client client;
struct client {
    Window win;
    unsigned int x, y, w, h, fscreen;
};

typedef struct {
    client *cl;
} record;

typedef struct {
    record drec[10]; // Limit of ten windows per desktop
    unsigned int current, numwins;
} Desktop;

static void add_window(Window win, unsigned int da, client *cl);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void change_desktop(const Arg arg);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void follow_to_desktop(const Arg arg);
static void fullscreen();
static void grabkeys();
static void keypress(XEvent *e);
static void kill_client();
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void next_win();
static void quit();
static void remove_client(unsigned int e, unsigned int dr);
static void resize_width(const Arg arg);
static void resize_height(const Arg arg);
static void save_desktop(unsigned int a);
static void select_desktop(unsigned int a);
static void send_to_desktop(const Arg arg);
static void send_kill_signal(Window w);
static void setup();
static void sigchld();
static void spawn(const Arg arg);
static void startup();
static void update_current();
static void window_x(const Arg arg);
static void window_y(const Arg arg);
static unsigned long getcolor(const char* color);

#include "config.h"

static Display *dis;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *ee);
static Window root;
static XWindowAttributes attr;
static XButtonEvent start;

static unsigned int stop_running, numwins, current, numlockmask, i, cd, doresize;
static unsigned int sw, sh, win_focus, win_unfocus;

static record rec[10]; // Limit of ten windows per desktop
static Desktop desk[DESKS];

/************************* Window Management *************/
void add_window(Window win, unsigned int da, client *cl) {
    client *c;
    if(da == 0) {
        if(!(c = (client *)calloc(1,sizeof(client)))) {
            puts("CALLOC ERROR");
            exit (1);
        }
        c->win = win;
        rec[numwins].cl = c;
        XGetWindowAttributes(dis, rec[numwins].cl->win, &attr);
        if(attr.x < 20 || attr.y < 20) {
            XMoveWindow(dis, rec[numwins].cl->win, numwins*10+100, numwins*10+100);
            XGetWindowAttributes(dis, rec[numwins].cl->win, &attr);
        }
        rec[numwins].cl->x = attr.x;
        rec[numwins].cl->y = attr.y;
        rec[numwins].cl->w = attr.width;
        rec[numwins].cl->h = attr.height;
    } else rec[numwins].cl = cl;
    current = numwins;
    XMapWindow(dis, rec[current].cl->win);
    ++numwins;
    update_current();
    return;
}

void remove_client(unsigned int e, unsigned int dr) {
    XUngrabButton(dis, AnyButton, AnyModifier, rec[e].cl->win);
    XUnmapWindow(dis, rec[e].cl->win);
    if(dr == 0) free(rec[e].cl);
    for(i=e;i<numwins;++i)
        rec[i].cl = rec[i+1].cl;
    rec[numwins-1].cl = NULL;
    --numwins;
    update_current();
    return;
}

void update_current() {
    if(numwins < 1) return;

    rec[numwins].cl = rec[current].cl;
    for(i=numwins-1;i>0;--i) {
        if(i <= current)
            rec[i].cl = rec[i-1].cl;
        XGrabButton(dis, AnyButton, AnyModifier, rec[i].cl->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
        XSetWindowBorderWidth(dis,rec[i].cl->win,BORDER_WIDTH);
        XSetWindowBorder(dis,rec[i].cl->win,win_unfocus);
    }

    current = 0;
    rec[current].cl = rec[numwins].cl;
    rec[numwins].cl = NULL;
    XUngrabButton(dis, AnyButton, AnyModifier, rec[current].cl->win);
    XSetWindowBorderWidth(dis,rec[current].cl->win,BORDER_WIDTH);
    XSetWindowBorder(dis,rec[current].cl->win,win_focus);
    XSetInputFocus(dis,rec[current].cl->win,RevertToParent,CurrentTime);
    XRaiseWindow(dis, rec[current].cl->win);
    save_desktop(cd);
}

void next_win() {
    current = numwins-1;
    update_current();
}

void fullscreen() {
    if(rec[0].cl->fscreen == 0) {
        XMoveResizeWindow(dis,rec[0].cl->win,0,0,sw,sh);
        rec[0].cl->fscreen = 1;
    } else {
        XMoveResizeWindow(dis,rec[0].cl->win,rec[0].cl->x,rec[0].cl->y,rec[0].cl->w,rec[0].cl->h);
        rec[0].cl->fscreen = 0;
    }
}

void window_x(const Arg arg) {
    if(numwins < 1) return;
    rec[0].cl->x += arg.a;
    XMoveResizeWindow(dis,rec[0].cl->win,rec[0].cl->x,rec[0].cl->y,rec[0].cl->w,rec[0].cl->h);
}

void window_y(const Arg arg) {
    if(numwins < 1) return;
    rec[0].cl->y += arg.a;
    XMoveResizeWindow(dis,rec[0].cl->win,rec[0].cl->x,rec[0].cl->y,rec[0].cl->w,rec[0].cl->h);
}

void resize_width(const Arg arg) {
    if(numwins < 1) return;
    rec[0].cl->w += arg.a;
    XMoveResizeWindow(dis,rec[0].cl->win,rec[0].cl->x,rec[0].cl->y,rec[0].cl->w,rec[0].cl->h);
}

void resize_height(const Arg arg) {
    if(numwins < 1) return;
    rec[0].cl->h += arg.a;
    XMoveResizeWindow(dis,rec[0].cl->win,rec[0].cl->x,rec[0].cl->y,rec[0].cl->w,rec[0].cl->h);
}

/************************* DESKTOP *********************/
void change_desktop(const Arg arg) {
    if(arg.a == cd) return;
    save_desktop(cd);
    if(numwins > 0)
        for(i=0;i<numwins;++i)
            XUnmapWindow(dis, rec[i].cl->win);

    select_desktop(arg.a);
    if(numwins > 0)
        for(i=0;i<numwins;++i)
            XMapWindow(dis, rec[i].cl->win);
    update_current();
}

void send_to_desktop(const Arg arg) {
    if(arg.a == cd || numwins == 0) return;

    unsigned int tmp = cd;
    client *sve = rec[current].cl;

    remove_client(current, 1);
    select_desktop(arg.a);
    add_window(sve->win, 1, sve);
    select_desktop(tmp);
    update_current();
}

void follow_to_desktop(const Arg arg) {
    if(arg.a == cd || numwins == 0) return;

    send_to_desktop(arg);
    change_desktop(arg);
}

void save_desktop(unsigned int a) {
    for(i=0;i<TABLENGTH(rec);++i)
        desk[a].drec[i] = rec[i];
    desk[a].current = current;
    desk[a].numwins = numwins;
}

void select_desktop(unsigned int a) {
    for(i=0;i<TABLENGTH(desk[a].drec);++i)
        rec[i] = desk[a].drec[i];
    current = desk[a].current;
    numwins = desk[a].numwins;
    cd = a;
}

/************************* KEYBOARD *********************/
void grabkeys() {
    int j;
    XModifierKeymap *modmap;
    KeyCode code;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    // numlock workaround
    numlockmask = 0;
    modmap = XGetModifierMapping(dis);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < modmap->max_keypermod; ++j) {
            if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dis, XStringToKeysym("Num_Lock")))
                numlockmask = (1 << i);
        }
    }
    XFreeModifiermap(modmap);

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    // For each shortcuts
    for(i=0;i<TABLENGTH(keys);++i) {
        code = XKeysymToKeycode(dis, keys[i].keysym);
        XGrabKey(dis, code, keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    }
    for(i=1;i<4;i+=2) {
        XGrabButton(dis, i, RESIZEMOVEKEY, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, RESIZEMOVEKEY | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, RESIZEMOVEKEY | numlockmask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, RESIZEMOVEKEY | numlockmask | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

/************************ EVENTS ***********************/
void keypress(XEvent *e) {
    KeySym keysym;
    XKeyEvent *ev = &e->xkey;

    keysym = XkbKeycodeToKeysym(dis, (KeyCode)ev->keycode, 0, 0);
    for(i = 0; i < TABLENGTH(keys); ++i) {
        if(keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) {
            if(keys[i].function) {
                keys[i].function(keys[i].arg);
                return;
            }
        }
    }
}

void buttonpress(XEvent *e) {
    if(e->xbutton.button == Button1)
        for(i=0;i<numwins;++i)
            if(i != current && rec[i].cl->win == e->xbutton.window) {
                current = i;
                update_current();
                XSendEvent(dis, PointerWindow, False, 0xfff, e);
                XFlush(dis);
                return;
            }
    if(e->xbutton.subwindow != None) {
        XGrabPointer(dis, e->xbutton.subwindow, True,
                PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
                GrabModeAsync, None, None, CurrentTime);
        XGetWindowAttributes(dis, e->xbutton.subwindow, &attr);
        start = e->xbutton; doresize = 1;
    }
}

void buttonrelease(XEvent *e) {
    XUngrabPointer(dis, CurrentTime);
    for(i=0;i<numwins;++i)
        if(rec[i].cl->win == e->xbutton.subwindow) {
            XGetWindowAttributes(dis, rec[numwins].cl->win, &attr);
            rec[i].cl->x = attr.x;
            rec[i].cl->y = attr.y;
            rec[i].cl->w = attr.width;
            rec[i].cl->h = attr.height;
        }
    doresize = 0;
}

void configurerequest(XEvent *e) {
    XWindowChanges wc;

    wc.x = e->xconfigurerequest.x;
    wc.y = e->xconfigurerequest.y;
    wc.width = e->xconfigurerequest.width;
    wc.height = e->xconfigurerequest.height;
    wc.border_width = 0;
    wc.sibling = e->xconfigurerequest.above;
    wc.stack_mode = e->xconfigurerequest.detail;
    XConfigureWindow(dis, e->xconfigurerequest.window, e->xconfigurerequest.value_mask, &wc);
    XSync(dis, False);
}

void destroynotify(XEvent *e) {
    unsigned int j, tmp = cd;
    for(j=tmp;j<(tmp+DESKS);++j) {
        select_desktop((j<DESKS ? j: j-DESKS));
        for(i=0;i<numwins;++i) {
            if(rec[i].cl->win == e->xdestroywindow.window) {
                remove_client(i, 0);
                select_desktop(tmp);
                return;
            }
        }
    }
    select_desktop(tmp);
}

unsigned long getcolor(const char* color) {
    XColor c;
    Colormap map = DefaultColormap(dis,DefaultScreen(dis));

    if(!XAllocNamedColor(dis,map,color,&c,&c))
        puts("\033[0;31mMINIME : Error parsing color!");

    return c.pixel;
}

void kill_client() {
    send_kill_signal(rec[current].cl->win);
}

void maprequest(XEvent *e) {
    if(attr.override_redirect == True) return;
    add_window(e->xmaprequest.window,0,NULL);
}

void motionnotify(XEvent *e) {
    if(doresize == 0) return;
    int xdiff, ydiff;
    while(XCheckTypedEvent(dis, MotionNotify, e));
    xdiff = e->xbutton.x_root - start.x_root;
    ydiff = e->xbutton.y_root - start.y_root;
    XMoveResizeWindow(dis, e->xmotion.window,
          attr.x + (start.button==1 ? xdiff : 0),
          attr.y + (start.button==1 ? ydiff : 0),
          MAX(1, attr.width + (start.button==3 ? xdiff : 0)),
          MAX(1, attr.height + (start.button==3 ? ydiff : 0)));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(fork() == 0) {
            if(dis) close(ConnectionNumber(dis));
            setsid();
            execvp((char*)arg.com[0],(char**)arg.com);
        }
        exit(0);
    }
}

/*********************** SETUP ********************/
int xerror(Display *dis, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    if(ee->error_code == BadAccess) {
        puts("\033[0;31mIs Another Window Manager Running? Exiting!");
        exit(1);
    } else puts("\033[0;31mMINIME : Bad Window Error!\033[0m");
    return xerrorxlib(dis, ee); /* may call exit */
}

void sigchld() {
	if(signal(SIGCHLD, sigchld) == SIG_ERR)
		puts("Can't install SIGCHLD handler");
	while(0 < waitpid(-1, NULL, WNOHANG));
}

static void setup() {
    sigchld();
    root = DefaultRootWindow(dis);
    sw = XDisplayWidth(dis,DefaultScreen(dis));
    sh = XDisplayHeight(dis,DefaultScreen(dis));
    // Colors
    win_focus = getcolor(FOCUS);
    win_unfocus = getcolor(UNFOCUS);
    grabkeys();
    doresize = numwins = 0;
    rec[0].cl = NULL;
    for(i=0;i<DESKS;++i) {
        desk[i].drec[0] = rec[0];
        desk[i].current = 0;
        desk[i].numwins = 0;
    }
    select_desktop(0);
    XSelectInput(dis,root,SubstructureNotifyMask|SubstructureRedirectMask);
    XSetErrorHandler(xerror);
    stop_running = 0;
}

void startup() {
    XEvent ev;
    while (!stop_running && !XNextEvent(dis, &ev)) {
        switch(ev.type) {
            case ButtonPress: buttonpress(&ev); break;
            case ButtonRelease: buttonrelease(&ev); break;
            case ConfigureRequest: configurerequest(&ev); break;
            case DestroyNotify: destroynotify(&ev); break;
            case KeyPress: keypress(&ev); break;
            case MapRequest: maprequest(&ev); break;
            case MotionNotify: motionnotify(&ev); break;
        }
    }
}

void quit() {
    Window root_return, parent, *children;
    unsigned int nchildren, i;

    XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
    for(i = 0; i < nchildren; i++)
        send_kill_signal(children[i]);

    XClearWindow(dis, root);
    XUngrabKey(dis, AnyKey, AnyModifier, root);
    XSync(dis, False);
    XSetInputFocus(dis, root, RevertToPointerRoot, CurrentTime);
    fputs("MINIME : you quit, bye !\n", stderr);
    stop_running = 1;
}

void send_kill_signal(Window w) {
    XEvent ke;
    ke.type = ClientMessage;
    ke.xclient.window = w;
    ke.xclient.message_type = XInternAtom(dis, "WM_PROTOCOLS", True);
    ke.xclient.format = 32;
    ke.xclient.data.l[0] = XInternAtom(dis, "WM_DELETE_WINDOW", True);
    ke.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ke);
}

int main() {
    if(!(dis = XOpenDisplay(NULL))) return 1;
    setup();
    puts("MINIME : Starting up");
    startup();
    XCloseDisplay(dis);
    return 0;
}
