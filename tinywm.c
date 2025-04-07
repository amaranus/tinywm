/* TinyWM is written by Nick Welch <nick@incise.org> in 2005 & 2011.
 *
 * This software is in the public domain
 * and is provided AS IS, with NO WARRANTY. 
 * Modified by amaranus <amaranus@hotmail.com> in 2025 */

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MODKEY Mod1Mask

int main(void)
{
    Display *dpy;
    XWindowAttributes attr;
    XButtonEvent start;
    XEvent ev;

    if (!(dpy = XOpenDisplay(0x0)))
        return 1;

    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
    XDefineCursor(dpy, DefaultRootWindow(dpy), cursor);

    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("F1")), MODKEY,
             DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("q")), MODKEY,
             DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("p")), MODKEY,
             DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync);
    XGrabButton(dpy, 1, MODKEY, DefaultRootWindow(dpy), True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 3, MODKEY, DefaultRootWindow(dpy), True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    start.subwindow = None;
    for (;;)
    {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress && ev.xkey.subwindow != None && ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("F1")))
            XRaiseWindow(dpy, ev.xkey.subwindow);
        else if (ev.type == KeyPress && ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("q")))
            XDestroyWindow(dpy, ev.xkey.subwindow);
        else if (ev.type == KeyPress && ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("p")))
            system("dmenu_run -l 10 -p 'Uygulama seç:' -fn 'Terminus-13' -nb '#242933' -sb '#1b1f26'");
        else if (ev.type == ButtonPress && ev.xbutton.subwindow != None)
        {
            XRaiseWindow(dpy, ev.xbutton.subwindow);
            XSetInputFocus(dpy, ev.xbutton.subwindow, RevertToPointerRoot, CurrentTime); // Odaklamayı ayarla

            XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
            start = ev.xbutton;
        }  
        else if (ev.type == MotionNotify && start.subwindow != None)
        {
            int xdiff = ev.xbutton.x_root - start.x_root;
            int ydiff = ev.xbutton.y_root - start.y_root;
            XMoveResizeWindow(dpy, start.subwindow,
                              attr.x + (start.button == 1 ? xdiff : 0),
                              attr.y + (start.button == 1 ? ydiff : 0),
                              MAX(1, attr.width + (start.button == 3 ? xdiff : 0)),
                              MAX(1, attr.height + (start.button == 3 ? ydiff : 0)));
        }
        else if (ev.type == ButtonRelease)
            start.subwindow = None;
    }
}
