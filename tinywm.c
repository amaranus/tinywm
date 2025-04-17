#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <X11/Xatom.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MODKEY Mod1Mask
#define NUM_WORKSPACES 5

// Dizilim modları için enum'u güncelle
typedef enum {
    LAYOUT_FLOAT,    // Serbest dizilim (varsayılan)
    LAYOUT_TILE_V,   // Dikey dizilim
    LAYOUT_TILE_H    // Yatay dizilim
} LayoutMode;

// Gaps ayarları için yapı
typedef struct {
    int inner;  // Pencereler arası boşluk
    int outer;  // Ekran kenarlarından boşluk
} Gaps;

// Workspace yapısını güncelle
typedef struct {
    Window *windows;      // Workspace'deki pencereler
    int num_windows;      // Pencere sayısı
    int capacity;        // Dizinin kapasitesi
    LayoutMode layout;   // Dizilim modu
    Gaps gaps;          // Boşluk ayarları
} Workspace;

// Varsayılan değerler
#define DEFAULT_INNER_GAP 10
#define DEFAULT_OUTER_GAP 20

Workspace workspaces[NUM_WORKSPACES];
unsigned long current_workspace = 0;

// init_workspaces fonksiyonunu güncelle
void init_workspaces(void) {
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        workspaces[i].windows = malloc(sizeof(Window) * 10);
        workspaces[i].num_windows = 0;
        workspaces[i].capacity = 10;
        workspaces[i].layout = LAYOUT_FLOAT;  // Varsayılan float mod
        workspaces[i].gaps.inner = DEFAULT_INNER_GAP;
        workspaces[i].gaps.outer = DEFAULT_OUTER_GAP;
    }
}

void add_window_to_workspace(Window win, int workspace) {
    if (workspace < 0 || workspace >= NUM_WORKSPACES) return;
    
    Workspace *ws = &workspaces[workspace];
    
    // Pencere zaten bu workspace'de mi kontrol et
    for (int i = 0; i < ws->num_windows; i++) {
        if (ws->windows[i] == win) return;
    }
    
    // Kapasite kontrolü
    if (ws->num_windows >= ws->capacity) {
        ws->capacity *= 2;
        Window *new_windows = realloc(ws->windows, sizeof(Window) * ws->capacity);
        if (new_windows) {
            ws->windows = new_windows;
        } else {
            return; // Bellek hatası
        }
    }
    
    ws->windows[ws->num_windows++] = win;
}

void remove_window_from_workspace(Window win, int workspace) {
    Workspace *ws = &workspaces[workspace];
    
    for (int i = 0; i < ws->num_windows; i++) {
        if (ws->windows[i] == win) {
            // Pencereyi listeden kaldır
            for (int j = i; j < ws->num_windows - 1; j++) {
                ws->windows[j] = ws->windows[j + 1];
            }
            ws->num_windows--;
            break;
        }
    }
}

void switch_workspace(Display *dpy, int new_workspace) {
    if (new_workspace < 0 || new_workspace >= NUM_WORKSPACES) return;
    
    Window root = DefaultRootWindow(dpy);
    Atom net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    
    // Mevcut workspace'deki pencereleri gizle
    for (int i = 0; i < workspaces[current_workspace].num_windows; i++) {
        Window win = workspaces[current_workspace].windows[i];
        if (win != None) {
            // Pencerenin tipini kontrol et
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            Atom *data = NULL;
            
            // Pencere dock tipi mi kontrol et (Polybar gibi)
            if (XGetWindowProperty(dpy, win, net_wm_window_type, 0, 1,
                False, XA_ATOM, &actual_type, &actual_format,
                &nitems, &bytes_after, (unsigned char **)&data) == Success) {
                
                if (data) {
                    // Eğer pencere dock tipinde değilse gizle
                    if (*data != net_wm_window_type_dock) {
                        XUnmapWindow(dpy, win);
                    }
                    XFree(data);
                } else {
                    // Pencere tipi belirtilmemişse gizle
                    XUnmapWindow(dpy, win);
                }
            }
        }
    }
    
    // Workspace'i değiştir
    current_workspace = new_workspace;
    
    // Yeni workspace'deki pencereleri göster
    for (int i = 0; i < workspaces[new_workspace].num_windows; i++) {
        Window win = workspaces[new_workspace].windows[i];
        if (win != None) {
            // Pencerenin tipini kontrol et
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            Atom *data = NULL;
            
            // Dock tipi olmayan pencereleri göster
            if (XGetWindowProperty(dpy, win, net_wm_window_type, 0, 1,
                False, XA_ATOM, &actual_type, &actual_format,
                &nitems, &bytes_after, (unsigned char **)&data) == Success) {
                
                if (data) {
                    // Eğer pencere dock tipinde değilse göster
                    if (*data != net_wm_window_type_dock) {
                        XMapWindow(dpy, win);
                    }
                    XFree(data);
                } else {
                    // Pencere tipi belirtilmemişse göster
                    XMapWindow(dpy, win);
                }
            }
        }
    }
    
    // EWMH özelliğini güncelle
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&new_workspace, 1);
    
    XSync(dpy, False);
}

void cleanup_workspaces(void) {
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        free(workspaces[i].windows);
    }
}

void move_window_to_workspace(Display *dpy, Window win, int new_workspace) {
    if (new_workspace < 0 || new_workspace >= NUM_WORKSPACES) return;
    if (win == None) return;

    // Önce pencereyi eski workspace'den kaldır
    remove_window_from_workspace(win, current_workspace);

    // Pencereyi yeni workspace'e ekle
    add_window_to_workspace(win, new_workspace);

    // Eğer pencere farklı bir workspace'e taşınıyorsa gizle
    if (new_workspace != current_workspace) {
        XUnmapWindow(dpy, win);
    }
}

void arrange_windows_tiled(Display *dpy, Workspace *ws, int horizontal) {
    if (ws->num_windows == 0) return;

    Window root = DefaultRootWindow(dpy);
    XWindowAttributes root_attr;
    XGetWindowAttributes(dpy, root, &root_attr);

    int x = ws->gaps.outer;
    int y = ws->gaps.outer;
    int available_width = root_attr.width - (2 * ws->gaps.outer);
    int available_height = root_attr.height - (2 * ws->gaps.outer);

    if (horizontal) {
        // Yatay dizilim
        int width = available_width;
        int height = (available_height - ((ws->num_windows - 1) * ws->gaps.inner)) / ws->num_windows;

        for (int i = 0; i < ws->num_windows; i++) {
            XMoveResizeWindow(dpy, ws->windows[i], x, y, width, height);
            y += height + ws->gaps.inner;
        }
    } else {
        // Dikey dizilim
        int width = (available_width - ((ws->num_windows - 1) * ws->gaps.inner)) / ws->num_windows;
        int height = available_height;

        for (int i = 0; i < ws->num_windows; i++) {
            XMoveResizeWindow(dpy, ws->windows[i], x, y, width, height);
            x += width + ws->gaps.inner;
        }
    }
}

// Pencereyi merkeze yerleştirme fonksiyonunu ekle
void center_window(Display *dpy, Window win) {
    Window root = DefaultRootWindow(dpy);
    XWindowAttributes root_attr, win_attr;
    
    XGetWindowAttributes(dpy, root, &root_attr);
    XGetWindowAttributes(dpy, win, &win_attr);
    
    int x = (root_attr.width - win_attr.width) / 2;
    int y = (root_attr.height - win_attr.height) / 2;
    
    XMoveWindow(dpy, win, x, y);
}

// toggle_layout fonksiyonunu güncelle
void toggle_layout(Display *dpy) {
    Workspace *ws = &workspaces[current_workspace];
    
    if (ws->layout == LAYOUT_FLOAT) {
        ws->layout = LAYOUT_TILE_V;  // Float'tan tile'a geçişte dikey mod
        arrange_windows_tiled(dpy, ws, 0);  // 0 = dikey dizilim
    } else {
        ws->layout = LAYOUT_FLOAT;
    }
}

void set_layout(Display *dpy, LayoutMode mode) {
    Workspace *ws = &workspaces[current_workspace];
    ws->layout = mode;
    
    if (mode != LAYOUT_FLOAT) {
        arrange_windows_tiled(dpy, ws, mode == LAYOUT_TILE_H);
    }
}

int main(void)
{
    Display *dpy;
    XWindowAttributes attr;
    XButtonEvent start;
    XEvent ev;

    if (!(dpy = XOpenDisplay(0x0)))
        return 1;

    // EWMH atomlarını tanımla
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    Atom net_number_of_desktops, net_client_list;
    Atom net_active_window, net_wm_window_type, net_wm_state, net_wm_name;
    Atom net_wm_window_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    
    // Atomları başlat
    net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);

    // Root window'u tanımla
    Window root = DefaultRootWindow(dpy);

    // Workspace sayısını ayarla
    unsigned long desktop_num = NUM_WORKSPACES;
    XChangeProperty(dpy, root, net_number_of_desktops, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&desktop_num, 1);

    // Mevcut workspace'i 0 olarak ayarla
    unsigned long current_desktop = 0;
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&current_desktop, 1);

    // Desteklenen EWMH özelliklerini bildir
    Atom supported[] = {
        net_supported,
        net_current_desktop,
        net_number_of_desktops,
        net_client_list,
        net_active_window,
        net_wm_window_type,
        net_wm_state,
        net_wm_name
    };
    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32,
                   PropModeReplace, (unsigned char *)supported,
                   sizeof(supported) / sizeof(supported[0]));

    // Workspace değiştirme tuşlarını tanımla
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        char key[2];
        snprintf(key, sizeof(key), "%d", i + 1);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(key)), 
                 MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    }

    // Workspace tuşlarını tanımla (main fonksiyonunda, diğer XGrabKey çağrılarının yanına)
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        char key[2];
        snprintf(key, sizeof(key), "%d", i + 1);
        // Normal workspace değiştirme için Alt+[1-5]
        XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(key)), 
                 MODKEY, root, True, GrabModeAsync, GrabModeAsync);
        // Pencere taşıma için Alt+Shift+[1-5]
        XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(key)), 
                 MODKEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    }

    // Main fonksiyonunda tuş tanımlamaları
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("space")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("h")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("v")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);

    /* 
    Fare imlecini değiştirmek için bir imleç oluşturuyoruz
    XCreateFontCursor fonksiyonu ile fare imlecini sol ok imleci olarak ayarlıyoruz. */
    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);

    

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
    XSelectInput(dpy, root, SubstructureNotifyMask | EnterWindowMask | PropertyChangeMask);

    // Workspace'leri başlat
    init_workspaces();

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
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("space"))) {
                toggle_layout(dpy);
            }
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("h"))) {
                set_layout(dpy, LAYOUT_TILE_H);
            }
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("v"))) {
                set_layout(dpy, LAYOUT_TILE_V);
            }
            for (int i = 0; i < NUM_WORKSPACES; i++) {
                char key[2];
                snprintf(key, sizeof(key), "%d", i + 1);
                if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym(key))) {
                    if (ev.xkey.state & ShiftMask) {
                        // Alt+Shift+[1-5]: Pencereyi workspace'e taşı
                        if (ev.xkey.subwindow != None) {
                            move_window_to_workspace(dpy, ev.xkey.subwindow, i);
                            // Bildirim göster
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Pencere %d. workspace'e taşındı", i + 1);
                            system("notify-send 'Workspace' \"$msg\" -t 1000");
                        }
                    } else {
                        // Alt+[1-5]: Workspace değiştir
                        switch_workspace(dpy, i);
                    }
                    break;
                }
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
        // MapNotify olayını güncelleyelim
        else if (ev.type == MapNotify) {
            if (!ev.xmap.override_redirect) {
                // Pencere dock tipi mi kontrol et
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                Atom *data = NULL;
                
                if (XGetWindowProperty(dpy, ev.xmap.window, net_wm_window_type, 0, 1,
                    False, XA_ATOM, &actual_type, &actual_format,
                    &nitems, &bytes_after, (unsigned char **)&data) == Success) {
                    
                    if (data) {
                        if (*data != net_wm_window_type_dock) {
                            add_window_to_workspace(ev.xmap.window, current_workspace);
                            if (workspaces[current_workspace].layout == LAYOUT_FLOAT) {
                                center_window(dpy, ev.xmap.window);
                            } else {
                                arrange_windows_tiled(dpy, &workspaces[current_workspace], 
                                    workspaces[current_workspace].layout == LAYOUT_TILE_H);
                            }
                        }
                        XFree(data);
                    } else {
                        add_window_to_workspace(ev.xmap.window, current_workspace);
                        if (workspaces[current_workspace].layout == LAYOUT_FLOAT) {
                            center_window(dpy, ev.xmap.window);
                        } else {
                            arrange_windows_tiled(dpy, &workspaces[current_workspace], 
                                workspaces[current_workspace].layout == LAYOUT_TILE_H);
                        }
                    }
                }
                XSelectInput(dpy, ev.xmap.window, EnterWindowMask);
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
        else if (ev.type == UnmapNotify) {
            // Pencere kapatıldığında workspace'den kaldır
            remove_window_from_workspace(ev.xunmap.window, current_workspace);
        }
        // main fonksiyonundaki olay döngüsüne ekleyin (diğer else if bloklarının yanına)
        else if (ev.type == ClientMessage) {
            if (ev.xclient.message_type == net_current_desktop) {
                // Polybar'dan gelen workspace değiştirme isteği
                unsigned int new_workspace = (unsigned int)ev.xclient.data.l[0];
                if (new_workspace < NUM_WORKSPACES) {
                    switch_workspace(dpy, new_workspace);
                }
            }
        }
    }
}
