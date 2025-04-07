#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>

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

    /* 
    Fare imlecini değiştirmek için bir imleç oluşturuyoruz
    XCreateFontCursor fonksiyonu ile fare imlecini sol ok imleci olarak ayarlıyoruz. */
    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);

    Window root = DefaultRootWindow(dpy);

    /* 
    XDefineCursor fonksiyonu ile fare imlecini değiştirdik.
    Fare imleci, XC_left_ptr ile sol ok imleci olarak ayarlandı.
    Bu, fare imlecinin görünümünü değiştirmek için kullanılır. */
    XDefineCursor(dpy, root, cursor);

    /*  
    Klavye ve fare olaylarını yakala
    XGrabKey ve XGrabButton fonksiyonları ile tuş ve fare olaylarını yakalıyoruz.
    Bu olayları yakalayarak, belirli tuşlara veya fare butonlarına basıldığında
    belirli işlemleri gerçekleştirebiliriz. */
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("F1")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("q")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("p")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);

    /*  
    Ses kontrolleri için tuşları yakala
    XF86AudioRaiseVolume, XF86AudioLowerVolume ve XF86AudioMute tuşları
    genellikle klavye üzerinde bulunur ve ses seviyesini artırmak, azaltmak
    ve sessize almak için kullanılır. */
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioRaiseVolume")), 0, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioLowerVolume")), 0, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioMute")), 0, root, True, GrabModeAsync, GrabModeAsync);

    XGrabButton(dpy, 1, MODKEY, root, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 3, MODKEY, root, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    // Focus follow pointer için root'a da EnterWindowMask ekle
    XSelectInput(dpy, root, SubstructureNotifyMask | EnterWindowMask);

    start.subwindow = None;

    for (;;)
    {
        /*  
        XNextEvent fonksiyonu ile olayları dinliyoruz.
        Olaylar geldiğinde, olay türüne göre işlemler yapıyoruz.
        Örneğin, bir tuşa basıldığında veya fare butonuna tıklandığında
        belirli işlemler gerçekleştiriyoruz. */
        XNextEvent(dpy, &ev);

        // Olay türüne göre işlemler yapıyoruz
        if (ev.type == KeyPress)
        {
            KeyCode f1 = XKeysymToKeycode(dpy, XStringToKeysym("F1"));
            KeyCode q = XKeysymToKeycode(dpy, XStringToKeysym("q"));
            KeyCode p = XKeysymToKeycode(dpy, XStringToKeysym("p"));
            KeyCode vol_up = XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioRaiseVolume"));
            KeyCode vol_down = XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioLowerVolume"));
            KeyCode mute = XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioMute"));

            if (ev.xkey.keycode == f1 && ev.xkey.subwindow != None)
            {
                XRaiseWindow(dpy, ev.xkey.subwindow);
            }
            else if (ev.xkey.keycode == q && ev.xkey.subwindow != None)
            {
                Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
                Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

                Atom *protocols;
                int n, deleted = 0;
                if (XGetWMProtocols(dpy, ev.xkey.subwindow, &protocols, &n))
                {
                    for (int i = 0; i < n; ++i)
                    {
                        if (protocols[i] == wm_delete)
                        {
                            XEvent msg;
                            memset(&msg, 0, sizeof(msg));
                            msg.xclient.type = ClientMessage;
                            msg.xclient.window = ev.xkey.subwindow;
                            msg.xclient.message_type = wm_protocols;
                            msg.xclient.format = 32;
                            msg.xclient.data.l[0] = wm_delete;
                            msg.xclient.data.l[1] = CurrentTime;
                            XSendEvent(dpy, ev.xkey.subwindow, False, NoEventMask, &msg);
                            deleted = 1;
                            break;
                        }
                    }
                    XFree(protocols);
                }

                if (!deleted)
                    XDestroyWindow(dpy, ev.xkey.subwindow);
            }
            else if (ev.xkey.keycode == p)
            {
                system("dmenu_run -l 10 -p 'Uygulama seç:' -fn 'Terminus-13' -nb '#242933' -sb '#1b1f26'");
            }
            else if (ev.xkey.keycode == vol_up)
            {
                system("pactl set-sink-volume @DEFAULT_SINK@ +5%");
                system("notify-send 'Ses Seviyesi' \"$(pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]\\+%' | head -n1)\" -t 1000");
            }
            else if (ev.xkey.keycode == vol_down)
            {
                system("pactl set-sink-volume @DEFAULT_SINK@ -5%");
                system("notify-send 'Ses Seviyesi' \"$(pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]\\+%' | head -n1)\" -t 1000");
            }
            else if (ev.xkey.keycode == mute)
            {
                system("pactl set-sink-mute @DEFAULT_SINK@ toggle");
                system("notify-send 'Ses Seviyesi' \"$(pactl get-sink-mute @DEFAULT_SINK@ | grep -q 'yes' && echo 'Sessiz' || echo 'Ses Açık')\" -t 1000");
            }
        }

        // Fare olayları için işlemler
        else if (ev.type == ButtonPress && ev.xbutton.subwindow == None)
        {
            start = ev.xbutton;
            XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
            XRaiseWindow(dpy, root);
        }
        else if (ev.type == ButtonPress && ev.xbutton.subwindow != None)
        {
            XRaiseWindow(dpy, ev.xbutton.subwindow);
            XSetInputFocus(dpy, ev.xbutton.subwindow, RevertToPointerRoot, CurrentTime);

            XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
            start = ev.xbutton;

            // Focus follow pointer'ı korumak için pencereye EnterWindowMask bağla
            XSelectInput(dpy, start.subwindow, EnterWindowMask);
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
        {
            start.subwindow = None;
        }
        else if (ev.type == MapNotify && ev.xmap.event == root)
        {
            XWindowAttributes wattr;
            XGetWindowAttributes(dpy, ev.xmap.window, &wattr);

            if (!wattr.override_redirect)
            {
                XSelectInput(dpy, ev.xmap.window, EnterWindowMask);
                XSetInputFocus(dpy, ev.xmap.window, RevertToPointerRoot, CurrentTime);
            }
        }
        else if (ev.type == EnterNotify && ev.xcrossing.window != root)
        {
            XWindowAttributes wattr;
            XGetWindowAttributes(dpy, ev.xcrossing.window, &wattr);

            if (!wattr.override_redirect)
            {
                XSetInputFocus(dpy, ev.xcrossing.window, RevertToPointerRoot, CurrentTime);
            }
        }
    }
}
