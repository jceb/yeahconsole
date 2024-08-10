/*************************************************************************/
/*  _____________                                                        */
/* < yeahconsole >                                                       */
/*  -------------                                                        */
/*         \   ^__^                                                      */
/*          \  (oo)\_______                                              */
/*             (__)\       )\/\                                          */
/*                 ||----w |                                             */
/*                 ||     ||                                             */
/*                                                                       */
/*  Copyright (C) knorke                                                 */
/*                                                                       */
/*  This program is free software; you can redistribute it and/or modify */
/*  it under the terms of the GNU General Public License as published by */
/*  the Free Software Foundation; either version 2 of the License, or    */
/*  (at your option) any later version.                                  */
/*                                                                       */
/*  This program is distributed in the hope that it will be useful,      */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/*  GNU General Public License for more details.                         */
/*                                                                       */
/*  You should have received a copy of the GNU General Public License    */
/*  along with this program; if not, write to the Free Software          */
/*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            */
/*************************************************************************/

/* Indentation: indent -brf -br -ut -ts4 -i4 -nsaf -nsai -nsaw -npcs *.c */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UP 0
#define DOWN -height - opt_bw

Display *dpy;
Window root;
Window win;
Window termwin;
char *progname, command[256];
int revert_to;
int screen;
int opt_x, opt_x_orig, opt_y, opt_y_orig, opt_width, opt_height, opt_delay,
    opt_bw, opt_step, opt_xrandr, height, opt_restart, opt_restart_hidden,
    opt_height_inc;
char *opt_color;
char *opt_term;
KeySym opt_key;
KeySym opt_key_smaller;
KeySym opt_key_bigger;
KeySym opt_key_full;
Cursor cursor;
int resize_inc;
void get_defaults(void);
int get_display_height();
int get_height_inc();
int get_optimal_height(int h);
int get_screen_geom(Window last_focused, int *x, int *y, int *w, int *h);
Window get_toplevel_parent(Window window);
KeySym grab_that_key(char *opt, unsigned int numlockmask);
int handle_xerror(Display *display, XErrorEvent *e);
void init_command(int argc, char *argv[]);
void init_win(void);
void init_xterm(int move);
void resize(void);
void resize_term(int w, int h);
void roll(int i);
void update_geom(Window last_focused);

int main(int argc, char *argv[]) {
  KeySym key;
  XEvent event;
  int hidden = 1;
  int fullscreen = 0;
  int i;
  int old_height = 0;
  Window last_focused, current_focused;

  /* strip the path from argv[0] if there is one */
  progname = strrchr(argv[0], '/');
  if (!progname)
    progname = argv[0];
  else
    progname++;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-h")) {
      printf("%s:\n"
             "-e: program to execute\n"
             "you can configure me via xresources:\n"
             "%s*foo:value\n"
             "foo can be any standard xterm/urxvt/st xresource or:\n"
             "resource               default value\n\n"
             "term:                  xterm\n"
             "restart:               0\n"
             "xOffset:               0\n"
             "yOffset:               0\n"
             "xrandrSupport:         0\n"
             "screenWidth:           Display width\n"
             "consoleHeight:         10\n"
             "aniDelay:              40\n"
             "stepSize;              1\n"
             "toggleKey:             Control+Alt+y\n"
             "keySmaller:            Control+KP_Subtract\n"
             "keyBigger:             Control+KP_Add\n"
             "keyFull:               Alt+F11\n",
             progname, progname);
      exit(0);
    }
  }

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, " can not open dpy %s", XDisplayName(NULL));
  }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  XSetErrorHandler(handle_xerror);
  cursor = XCreateFontCursor(dpy, XC_double_arrow);
  get_defaults();
  init_win();
  init_command(argc, argv);
  init_xterm(1);
  while (1) {
    XNextEvent(dpy, &event);
    switch (event.type) {
    case FocusIn:
      XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime);
      XSync(dpy, False);
    case FocusOut:
      if (!hidden) {
        /* Always keep window on top */
        XRaiseWindow(dpy, win);
      }
      break;
    case EnterNotify:
      XRaiseWindow(dpy, win);
      XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime);
      XSync(dpy, False);
      break;
    case KeyPress:
      key = XKeycodeToKeysym(dpy, event.xkey.keycode, 0);
      if (key == opt_key) {
        if (!hidden) {
          XGetInputFocus(dpy, &current_focused, &revert_to);
          if (last_focused && current_focused == termwin)
            XSetInputFocus(dpy, last_focused, RevertToPointerRoot, CurrentTime);
          /* else
             XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
           */
          if (opt_step && !fullscreen)
            roll(UP);
          XUnmapWindow(dpy, win);
          hidden = 1;
          XSync(dpy, False);
        } else {
          XGetInputFocus(dpy, &last_focused, &revert_to);
          last_focused = get_toplevel_parent(last_focused);

          if (opt_step && !fullscreen) {
            XGrabServer(dpy);
            roll(DOWN);
            XUngrabServer(dpy);
          } else if (opt_xrandr) {
            update_geom(last_focused);
          }
          XMoveWindow(dpy, win, opt_x, opt_y);
          XMapWindow(dpy, win);
          XRaiseWindow(dpy, win);
          XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime);
          XSync(dpy, False);
          hidden = 0;
        }
        break;
      }
      if (!hidden) {
        if (key == opt_key_full) {
          if (!fullscreen) {
            old_height = height;
            height = get_optimal_height(get_display_height());
            fullscreen = 1;
          } else {
            height = old_height;
            fullscreen = 0;
          }
        }

        /* update height inc just in case something changed for the
         * terminal, i.e. font size */
        resize_inc = get_height_inc();
        if (key == opt_key_bigger)
          height += resize_inc;
        if (key == opt_key_smaller)
          height -= resize_inc;
        if (height < resize_inc)
          height = resize_inc;
        height = get_optimal_height(height);
        resize_term(opt_width, height);
        /* XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime); */
        XSync(dpy, False);
      }
      break;
    case ButtonPress:
      resize();
      XSync(dpy, False);
      break;
    case UnmapNotify:
      if (event.xunmap.window == termwin) {
        if (opt_restart) {
          if (opt_restart_hidden) {
            roll(UP);
            hidden = 1;
          }
          init_xterm(0);
          XSync(dpy, False);
          if (opt_restart_hidden && last_focused)
            XSetInputFocus(dpy, last_focused, RevertToPointerRoot, CurrentTime);
          else
            XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime);
        } else {
          if (last_focused)
            XSetInputFocus(dpy, last_focused, RevertToPointerRoot, CurrentTime);
          XSync(dpy, False);
          exit(0);
        }
      }
      break;
    }
  }
  return 0;
}

void get_defaults() {
  char *opt;
  XModifierKeymap *modmap;
  unsigned int numlockmask = 0;
  int i, j;
  char def_key[] = "ControlAlt+y";
  char def_key_bigger[] = "Control+KP_Add";
  char def_key_smaller[] = "Control+KP_Subtract";
  char def_key_full[] = "Alt+F11";

  /* modifier stuff taken from evilwm */
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++) {
    for (j = 0; j < modmap->max_keypermod; j++) {
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(dpy, XK_Num_Lock)) {
        numlockmask = (1 << i);
      }
    }
  }
  XFreeModifiermap(modmap);
  opt = XGetDefault(dpy, progname, "screenWidth");
  opt_width = opt ? atoi(opt) : DisplayWidth(dpy, screen);
  opt = XGetDefault(dpy, progname, "xrandrSupport");
  opt_xrandr = opt ? atoi(opt) : 0;
  opt = XGetDefault(dpy, progname, "handleColor");
  opt_color = opt ? opt : "grey70";
  opt = XGetDefault(dpy, progname, "handleWidth");
  opt_bw = opt ? atoi(opt) : 3;
  opt = XGetDefault(dpy, progname, "consoleHeight");
  opt_height = opt ? atoi(opt) : 10;
  opt = XGetDefault(dpy, progname, "xOffset");
  opt_x = opt_x_orig = opt ? atoi(opt) : 0;
  opt = XGetDefault(dpy, progname, "yOffset");
  opt_y = opt_y_orig = opt ? atoi(opt) : 0;
  opt = XGetDefault(dpy, progname, "aniDelay");
  opt_delay = opt ? atoi(opt) : 40;
  opt = XGetDefault(dpy, progname, "stepSize");
  opt_step = opt_xrandr ? 0 : opt ? atoi(opt) : 1;
  opt = XGetDefault(dpy, progname, "restart");
  opt_restart = opt ? atoi(opt) : 0;
  opt = XGetDefault(dpy, progname, "restartHidden");
  opt_restart_hidden = opt ? atoi(opt) : 0;
  opt = XGetDefault(dpy, progname, "term");
  opt_term = opt ? opt : "xterm";
  opt = XGetDefault(dpy, progname, "heightIncrease");
  opt_height_inc = opt ? atoi(opt) : 0;
  opt = XGetDefault(dpy, progname, "toggleKey");
  opt_key = opt ? grab_that_key(opt, numlockmask)
                : grab_that_key(def_key, numlockmask);
  opt = XGetDefault(dpy, progname, "keySmaller");
  opt_key_smaller = opt ? grab_that_key(opt, numlockmask)
                        : grab_that_key(def_key_smaller, numlockmask);
  opt = XGetDefault(dpy, progname, "keyBigger");
  opt_key_bigger = opt ? grab_that_key(opt, numlockmask)
                       : grab_that_key(def_key_bigger, numlockmask);
  opt = XGetDefault(dpy, progname, "keyFull");
  opt_key_full = opt ? grab_that_key(opt, numlockmask)
                     : grab_that_key(def_key_full, numlockmask);
}

int get_display_height() {
  int height, tmp, tmp2;
  height = DisplayHeight(dpy, screen);
  if (opt_xrandr && get_screen_geom(win, &tmp, &tmp, &tmp, &tmp2))
    height = tmp2;
  return height;
}

int get_height_inc() {
  int height_inc = 0;
  XSizeHints *size;
  long dummy;
  size = XAllocSizeHints();
  /* wait for terminal to initialize */
  if (!size->height_inc && opt_height_inc)
    height_inc = opt_height_inc;
  else {
    while (!size->height_inc)
      XGetWMNormalHints(dpy, termwin, size, &dummy);
    height_inc = size->height_inc;
  }
  XFree(size);
  return height_inc;
}

int get_optimal_height(int h) {
  int height, dh;

  resize_inc = get_height_inc();
  /* readjust height to new resize_inc, +5 prevents the window
   * from getting too small */
  height = (h / resize_inc) * resize_inc + 5;
  if (!height)
    height = resize_inc;
  dh = get_display_height() - (opt_y_orig + opt_bw);
  if (height > dh)
    height = (dh / resize_inc) * resize_inc + 5;
  return height;
}

int get_screen_geom(Window last_focused, int *x, int *y, int *w, int *h) {
  int i;
  XWindowAttributes wa;
  XRRScreenResources *resources;
  XRRCrtcInfo *monitor_info = NULL;

  XGetWindowAttributes(dpy, last_focused, &wa);
  /* TODO externalize resources */
  resources = XRRGetScreenResourcesCurrent(dpy, last_focused);

  for (i = 0; i < resources->ncrtc; i++) {
    monitor_info = XRRGetCrtcInfo(dpy, resources, resources->crtcs[i]);
    if (wa.x >= monitor_info->x &&
        wa.x <= monitor_info->x + monitor_info->width - 1 &&
        wa.y >= monitor_info->y &&
        wa.y <= monitor_info->y + monitor_info->height - 1) {
      *x = monitor_info->x;
      *y = monitor_info->y;
      *w = monitor_info->width;
      *h = monitor_info->height;
      XRRFreeCrtcInfo(monitor_info);
      XRRFreeScreenResources(resources);
      return 1;
    }
  }
  if (monitor_info) {
    XRRFreeCrtcInfo(monitor_info);
  }
  XRRFreeScreenResources(resources);
  return 0;
}

Window get_toplevel_parent(Window window) {
  Window parent;
  Window root;
  Window *children;
  unsigned int num_children;

  while (1) {
    if (0 ==
        XQueryTree(dpy, window, &root, &parent, &children, &num_children)) {
      return (Window)NULL;
    }
    if (children) {
      XFree(children);
    }
    if (window == root || parent == root) {
      return window;
    } else {
      window = parent;
    }
  }

  return (Window)NULL;
}

KeySym grab_that_key(char *opt, unsigned int numlockmask) {
  unsigned int modmask = 0;
  KeySym keysym;

  if (strstr(opt, "Control"))
    modmask = modmask | ControlMask;
  if (strstr(opt, "Alt"))
    modmask = modmask | Mod1Mask;
  if (strstr(opt, "Win"))
    modmask = modmask | Mod4Mask;
  if (strstr(opt, "None"))
    modmask = 0;

  opt = strrchr(opt, '+');
  keysym = XStringToKeysym(++opt);

  XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), modmask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), LockMask | modmask, root, True,
           GrabModeAsync, GrabModeAsync);
  if (numlockmask) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), numlockmask | modmask, root,
             True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, keysym),
             numlockmask | LockMask | modmask, root, True, GrabModeAsync,
             GrabModeAsync);
  }
  return keysym;
}

int handle_xerror(Display *display, XErrorEvent *e) {
  /* this function does nothing, we receive xerrors
     when the last_focused window gets lost.. */
  /* fprintf(stderr, "XError caught\n"); */
  fprintf(stderr, "%d  XError caught\n", e->error_code);
  return 0;
}

void init_command(int argc, char *argv[]) {
  int i;
  char *pos;
  pos = command;
  if (strstr(opt_term, "urxvt"))
    pos += sprintf(pos, "%s -b 0 -embed %d -name %s ", opt_term, (int)win,
                   progname);
  else if (strstr(opt_term, "st"))
    pos += sprintf(pos, "%s -w %d -t %s ", opt_term, (int)win, progname);
  else if (strstr(opt_term, "alacritty"))
    pos +=
        sprintf(pos, "%s --embed %d --title %s ", opt_term, (int)win, progname);
  else
    pos += sprintf(pos, "%s -b 0 -into %d -name %s ", opt_term, (int)win,
                   progname);
  for (i = 1; i < argc; i++) {
    pos += sprintf(pos, "%s ", argv[i]);
  }
  sprintf(pos, "&");
}

void init_win() {
  XSetWindowAttributes attrib;
  XColor color;
  XColor dummy_color;

  attrib.override_redirect = True;
  attrib.background_pixel = BlackPixel(dpy, screen);
  win = XCreateWindow(dpy, root, opt_x, -200, opt_width, 200, 0, CopyFromParent,
                      InputOutput, CopyFromParent,
                      CWOverrideRedirect | CWBackPixel, &attrib);
  XSelectInput(dpy, win,
               SubstructureNotifyMask | EnterWindowMask | LeaveWindowMask |
                   KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                   FocusChangeMask);
  XAllocNamedColor(dpy, DefaultColormap(dpy, screen), opt_color, &color,
                   &dummy_color);
  XSetWindowBackground(dpy, win, color.pixel);
  XDefineCursor(dpy, win, cursor);
  char *name = "yeahconsole";
  Atom wmNetWmName = XInternAtom(dpy, "_NET_WM_NAME", 0);
  XChangeProperty(dpy, win, wmNetWmName, XInternAtom(dpy, "UTF8_STRING", 0), 8,
                  PropModeReplace, (unsigned char *)name, strlen(name));

  Atom property[4];
  property[0] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", 0);
  property[1] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", 0);
  XChangeProperty(dpy, win, property[1], XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)property, 1);
  property[0] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", 0);
  property[1] = XInternAtom(dpy, "_NET_WM_STATE_MODAL", 0);
  property[2] = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", 0);
  property[3] = XInternAtom(dpy, "_NET_WM_STATE", 0);
  XChangeProperty(dpy, win, property[3], XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)property, 3);
  XClassHint class;
  class.res_name = "Yeahconsole";
  class.res_class = "Yeahconsole";
  XSetClassHint(dpy, win, &class);
  XTextProperty _name;
  _name.value = (unsigned char *)name;
  _name.encoding = XInternAtom(dpy, "UTF8_STRING", 0);
  _name.format = 8;
  _name.nitems = strlen(name);
  XSetWMName(dpy, win, &_name);

  /* start unmaped */
  XUnmapWindow(dpy, win);
}

void init_xterm(int move) {
  XEvent ev;

  int ret = system(command);
  if (ret != 0) {
    fprintf(stderr, "terminal failed to start with exit code: %d\n", ret);
    exit(ret);
  }
  while (1) {
    XMaskEvent(dpy, SubstructureNotifyMask, &ev);
    if (ev.type == CreateNotify || ev.type == MapNotify) {
      termwin = ev.xcreatewindow.window;
      break;
    }
  }

  XSetWindowBorderWidth(dpy, termwin, 0);
  if (move) {
    resize_inc = get_height_inc();
    height = get_optimal_height(resize_inc * opt_height);
    resize_term(opt_width, height);
    XMoveWindow(dpy, win, opt_x, -(height + opt_bw));
    XSync(dpy, False);
  }
  XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime);
  XSync(dpy, False);
}

void resize() {
  XEvent ev;
  if (!XGrabPointer(dpy, root, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, cursor,
                    CurrentTime) == GrabSuccess)
    return;
  resize_inc = get_height_inc();
  while (1) {
    XMaskEvent(dpy, ButtonPressMask | ButtonReleaseMask | PointerMotionHintMask,
               &ev);
    switch (ev.type) {
    case MotionNotify:
      if (ev.xmotion.y >= resize_inc) {
        height = ev.xmotion.y - ev.xmotion.y % resize_inc;
        height = get_optimal_height(height);
        resize_term(opt_width, height);
        break;
      case ButtonRelease:
        XUngrabPointer(dpy, CurrentTime);
        return;
      }
    }
  }
}

void resize_term(int w, int h) {
  XResizeWindow(dpy, termwin, w, h);
  XResizeWindow(dpy, win, w, h + opt_bw);
  XSync(dpy, False);
}

void update_geom(
    Window last_focused) { /* Determine the position of the currently selected
                              window and open console on that screen */
  int x, y, w, h;
  x = y = w = h = -1;
  XWindowAttributes wa;

  if (!get_screen_geom(last_focused, &x, &y, &w, &h))
    return;

  XGetWindowAttributes(dpy, win, &wa);
  if (wa.width != w) {
    opt_width = w;
    resize_term(opt_width, height);
  }
  opt_x = x + opt_x_orig;
  opt_y = y + opt_y_orig;
}

void roll(int i) {
  int x = height / 100 + 1 + opt_step;

  if (i == 0)
    x = -x;

  while (1) {
    i += x;
    XMoveWindow(dpy, win, opt_x, i);
    XSync(dpy, False);
    usleep(opt_delay * 100);
    if (i / x == 0 || i < -(height + opt_bw))
      break;
  }
}
