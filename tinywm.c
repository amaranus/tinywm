#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <X11/Xatom.h>

// EWMH atomları için global tanımlamalar
Atom net_supported;
Atom net_current_desktop;
Atom net_number_of_desktops;
Atom net_client_list;
Atom net_active_window;
Atom net_wm_window_type;
Atom net_wm_state;
Atom net_wm_name;
Atom net_wm_window_type_dock;
Atom net_wm_window_type_menu;
Atom net_wm_window_type_dialog;
Atom wm_delete;
Atom wm_protocols;

// Bar pozisyonu için enum
typedef enum {
    BAR_TOP,    // Bar üstte
    BAR_BOTTOM  // Bar altta
} BarPosition;

// Varsayılan değerler
#define BAR_HEIGHT 30
#define DEFAULT_INNER_GAP 10
#define DEFAULT_OUTER_GAP 20
#define LAUNCHER "dmenu_run -l 10 -p 'Uygulama seç:' -fn 'Terminus-13' -nb '#242933' -sb '#1b1f26'"

// Global değişkenler
BarPosition bar_position = BAR_TOP; // Varsayılan olarak üstte

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MODKEY Mod1Mask
#define NUM_WORKSPACES 5

// Dizilim modları için enum
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

// Pencere pozisyonunu saklamak için yeni yapı
typedef struct {
    Window id;
    int x;
    int y;
    int width;
    int height;
} WindowState;

// Workspace yapısını güncelle
typedef struct {
    WindowState *windows;  // Pencere durumlarını tutan dizi
    int num_windows;
    int capacity;
    LayoutMode layout;
    Gaps gaps;
} Workspace;

Workspace workspaces[NUM_WORKSPACES];
unsigned long current_workspace = 0;

// Aktif pencereyi tutan global değişken
Window active_window = None;

// Xlib hata işleyici
int x_error_handler(Display *dpy, XErrorEvent *error) {
    char error_msg[256];
    XGetErrorText(dpy, error->error_code, error_msg, sizeof(error_msg));
    fprintf(stderr, "X Error: %s (error code: %d, request code: %d, minor code: %d)\n",
            error_msg, error->error_code, error->request_code, error->minor_code);
    return 0; // Devam et, çökme
}

// Fonksiyon prototipleri
void init_workspaces(void);
void save_window_state(Display *dpy, Window win, WindowState *state);
Bool is_movable_window(Display *dpy, Window win);
void add_window_to_workspace(Display *dpy, Window win, int workspace);
void remove_window_from_workspace(Display *dpy, Window win, int workspace);
void switch_workspace(Display *dpy, int new_workspace);
void cleanup_workspaces(void);
void move_window_to_workspace(Display *dpy, Window win, int new_workspace);
Bool has_dock_window(Display *dpy);
void arrange_windows_tiled(Display *dpy, Workspace *ws, int horizontal);
void toggle_layout(Display *dpy);
void set_layout(Display *dpy, LayoutMode mode);
void toggle_bar_position(Display *dpy);
void focus_next_window(Display *dpy);
void init_atoms(Display *dpy);
int x_error_handler(Display *dpy, XErrorEvent *error);

// init_workspaces fonksiyonunu güncelle
void init_workspaces(void) {
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        workspaces[i].windows = malloc(sizeof(WindowState) * 10);
        workspaces[i].num_windows = 0;
        workspaces[i].capacity = 10;
        workspaces[i].layout = LAYOUT_FLOAT;  // Varsayılan float mod
        workspaces[i].gaps.inner = DEFAULT_INNER_GAP;
        workspaces[i].gaps.outer = DEFAULT_OUTER_GAP;
    }
}

// Pencere konumunu kaydet
void save_window_state(Display *dpy, Window win, WindowState *state) {
    XWindowAttributes attr;
    if (XGetWindowAttributes(dpy, win, &attr)) {
        state->id = win;
        state->x = attr.x;
        state->y = attr.y;
        state->width = attr.width;
        state->height = attr.height;
    }
}

// Pencerenin taşınabilir ve tile modda düzenlenebilir olup olmadığını kontrol et
Bool is_movable_window(Display *dpy, Window win) {
    XWindowAttributes attr;
    if (!XGetWindowAttributes(dpy, win, &attr) || attr.override_redirect) {
        return False; // override_redirect pencereler taşınamaz
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom *data = NULL;

    if (XGetWindowProperty(dpy, win, net_wm_window_type, 0, 1,
                           False, XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, (unsigned char **)&data) == Success) {
        if (data) {
            Bool is_movable = (*data != net_wm_window_type_dock &&
                              *data != net_wm_window_type_menu &&
                              *data != net_wm_window_type_dialog);
            XFree(data);
            return is_movable;
        }
        XFree(data);
    }
    return True; // Varsayılan olarak taşınabilir
}

// Pencereyi workspace'e ekle
void add_window_to_workspace(Display *dpy, Window win, int workspace) {
    if (workspace < 0 || workspace >= NUM_WORKSPACES) return;
    
    Workspace *ws = &workspaces[workspace];
    
    // Pencere zaten bu workspace'de mi kontrol et
    for (int i = 0; i < ws->num_windows; i++) {
        if (ws->windows[i].id == win) return;
    }
    
    // Kapasite kontrolü
    if (ws->num_windows >= ws->capacity) {
        ws->capacity *= 2;
        WindowState *new_windows = realloc(ws->windows, sizeof(WindowState) * ws->capacity);
        if (new_windows) {
            ws->windows = new_windows;
        } else {
            return; // Bellek hatası
        }
    }
    
    // Yeni pencere durumunu kaydet
    save_window_state(dpy, win, &ws->windows[ws->num_windows]);
    ws->num_windows++;
    
    // Mesaj göster
    char command[100];
    snprintf(command, sizeof(command), "notify-send 'Taşıma' 'Taşınan alan: %d' -t 1000", workspace + 1);
    system(command);

    // Tile moddaysak ve pencere taşınabilir ise düzeni güncelle
    if (ws->layout != LAYOUT_FLOAT && is_movable_window(dpy, win)) {
        arrange_windows_tiled(dpy, ws, ws->layout == LAYOUT_TILE_H);
    }
}

// Pencereyi workspace'den kaldır
void remove_window_from_workspace(Display *dpy, Window win, int workspace) {
    Workspace *ws = &workspaces[workspace];
    
    for (int i = 0; i < ws->num_windows; i++) {
        if (ws->windows[i].id == win) {
            // Pencereyi listeden kaldır
            for (int j = i; j < ws->num_windows - 1; j++) {
                ws->windows[j] = ws->windows[j + 1];
            }
            ws->num_windows--;
            // Tile moddaysak ve bu workspace aktifse düzeni güncelle
            if (workspace == current_workspace && ws->layout != LAYOUT_FLOAT) {
                arrange_windows_tiled(dpy, ws, ws->layout == LAYOUT_TILE_H);
            }
            break;
        }
    }
}

// Workspace'e geçiş yaparken pencereleri göster
void switch_workspace(Display *dpy, int new_workspace) {
    if (new_workspace < 0 || new_workspace >= NUM_WORKSPACES || new_workspace == current_workspace) 
        return;
    
    Window root = DefaultRootWindow(dpy);
    
    // Mevcut workspace'deki pencereleri gizle
    for (int i = 0; i < workspaces[current_workspace].num_windows; i++) {
        Window win = workspaces[current_workspace].windows[i].id;
        if (win != None) {
            save_window_state(dpy, win, &workspaces[current_workspace].windows[i]);
            // Pencerenin tipini kontrol et
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            Atom *data = NULL;
            
            if (XGetWindowProperty(dpy, win, net_wm_window_type, 0, 1,
                False, XA_ATOM, &actual_type, &actual_format,
                &nitems, &bytes_after, (unsigned char **)&data) == Success) {
                
                if (data) {
                    // Eğer pencere dock, menu veya dialog tipinde değilse gizle
                    if (*data != net_wm_window_type_dock &&
                        *data != net_wm_window_type_menu &&
                        *data != net_wm_window_type_dialog) {
                        XUnmapWindow(dpy, win);
                    }
                    XFree(data);
                } else {
                    XUnmapWindow(dpy, win);
                }
            }
        }
    }
    
    // Workspace'i değiştir
    current_workspace = new_workspace;

    // Mesaj göster
    char command[100];
    snprintf(command, sizeof(command), "notify-send 'Geçiş' 'Geçiş yapılan alan: %d' -t 1000", new_workspace + 1);
    system(command);

    // Yeni workspace'deki pencereleri göster
    for (int i = 0; i < workspaces[new_workspace].num_windows; i++) {
        WindowState *state = &workspaces[new_workspace].windows[i];
        if (state->id != None) {
            if (workspaces[new_workspace].layout == LAYOUT_FLOAT) {
                // Float modda kaydedilen konuma geri getir
                XMoveResizeWindow(dpy, state->id, state->x, state->y, 
                                state->width, state->height);
            }
            // Pencerenin tipini kontrol et
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            Atom *data = NULL;
            
            if (XGetWindowProperty(dpy, state->id, net_wm_window_type, 0, 1,
                False, XA_ATOM, &actual_type, &actual_format,
                &nitems, &bytes_after, (unsigned char **)&data) == Success) {
                
                if (data) {
                    // Eğer pencere dock, menu veya dialog tipinde değilse göster
                    if (*data != net_wm_window_type_dock &&
                        *data != net_wm_window_type_menu &&
                        *data != net_wm_window_type_dialog) {
                        XMapWindow(dpy, state->id);
                    }
                    XFree(data);
                } else {
                    XMapWindow(dpy, state->id);
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
    if (win == None || !is_movable_window(dpy, win)) return;

    // Önce pencereyi eski workspace'den kaldır
    remove_window_from_workspace(dpy, win, current_workspace);

    // Pencereyi yeni workspace'e ekle
    add_window_to_workspace(dpy, win, new_workspace);

    // Eğer pencere farklı bir workspace'e taşınıyorsa gizle
    if (new_workspace != current_workspace) {
        XUnmapWindow(dpy, win);
    }
}

// Dock penceresinin varlığını kontrol eden fonksiyon
Bool has_dock_window(Display *dpy) {
    Window *children, dummy;
    unsigned int nchildren;
    XQueryTree(dpy, DefaultRootWindow(dpy), &dummy, &dummy, &children, &nchildren);
    
    for (unsigned int i = 0; i < nchildren; i++) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        Atom *data = NULL;
        
        if (XGetWindowProperty(dpy, children[i], net_wm_window_type, 0, 1,
            False, XA_ATOM, &actual_type, &actual_format,
            &nitems, &bytes_after, (unsigned char **)&data) == Success) {
            
            if (data) {
                if (*data == net_wm_window_type_dock) {
                    XFree(data);
                    XFree(children);
                    return True;
                }
                XFree(data);
            }
        }
    }
    
    XFree(children);
    return False;
}

// arrange_windows_tiled fonksiyonunu güncelle
void arrange_windows_tiled(Display *dpy, Workspace *ws, int horizontal) {
    if (ws->num_windows == 0) return;

    Window root = DefaultRootWindow(dpy);
    XWindowAttributes root_attr;
    XGetWindowAttributes(dpy, root, &root_attr);

    int x = ws->gaps.outer;
    int y = ws->gaps.outer;
    int available_width = root_attr.width - (2 * ws->gaps.outer);
    int available_height = root_attr.height - (2 * ws->gaps.outer);

    // Dock penceresi varsa boşluk bırak
    if (has_dock_window(dpy)) {
        if (bar_position == BAR_TOP) {
            y += BAR_HEIGHT;
            available_height -= BAR_HEIGHT;
        } else {
            available_height -= BAR_HEIGHT;
        }
    }

    // Tile modda düzenlenecek pencere sayısını hesapla
    int tileable_windows = 0;
    for (int i = 0; i < ws->num_windows; i++) {
        if (is_movable_window(dpy, ws->windows[i].id)) {
            tileable_windows++;
        }
    }
    if (tileable_windows == 0) return;

    if (horizontal) {
        // Yatay dizilim
        int width = available_width;
        int height = (available_height - ((tileable_windows - 1) * ws->gaps.inner)) / tileable_windows;

        for (int i = 0; i < ws->num_windows; i++) {
            if (!is_movable_window(dpy, ws->windows[i].id)) continue;
            XMoveResizeWindow(dpy, ws->windows[i].id, x, y, width, height);
            y += height + ws->gaps.inner;
        }
    } else {
        // Dikey dizilim
        int width = (available_width - ((tileable_windows - 1) * ws->gaps.inner)) / tileable_windows;
        int height = available_height;

        for (int i = 0; i < ws->num_windows; i++) {
            if (!is_movable_window(dpy, ws->windows[i].id)) continue;
            XMoveResizeWindow(dpy, ws->windows[i].id, x, y, width, height);
            x += width + ws->gaps.inner;
        }
    }
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

// Bar pozisyonunu değiştiren fonksiyon
void toggle_bar_position(Display *dpy) {
    bar_position = (bar_position == BAR_TOP) ? BAR_BOTTOM : BAR_TOP;
    
    // Mevcut workspace'deki pencereleri yeniden düzenle
    Workspace *ws = &workspaces[current_workspace];
    if (ws->layout != LAYOUT_FLOAT) {
        arrange_windows_tiled(dpy, ws, ws->layout == LAYOUT_TILE_H);
    }
}

// Pencere değiştirme fonksiyonu
void focus_next_window(Display *dpy) {
    Workspace *ws = &workspaces[current_workspace];
    
    if (ws->num_windows <= 1) return;  // Tek pencere veya pencere yoksa işlem yapma
    
    // Mevcut aktif pencerenin indeksini bul
    int current_index = -1;
    for (int i = 0; i < ws->num_windows; i++) {
        if (ws->windows[i].id == active_window) {
            current_index = i;
            break;
        }
    }
    
    // Sonraki pencereyi bul
    int next_index = (current_index + 1) % ws->num_windows;
    Window next_window = ws->windows[next_index].id;
    
    // Dock, menu veya dialog penceresi ise sonrakine geç
    while (!is_movable_window(dpy, next_window)) {
        next_index = (next_index + 1) % ws->num_windows;
        next_window = ws->windows[next_index].id;
        if (next_index == current_index) break; // Döngüyü kır
    }
    
    // Pencereyi aktif et ve öne getir
    XRaiseWindow(dpy, next_window);
    XSetInputFocus(dpy, next_window, RevertToPointerRoot, CurrentTime);
    active_window = next_window;
}

// Atomları başlatma fonksiyonu
void init_atoms(Display *dpy) {
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    net_wm_window_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    net_wm_window_type_menu = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
    net_wm_window_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
}

int main(void) {
    Display *dpy;
    XWindowAttributes attr;
    XButtonEvent start;
    XEvent ev;

    if (!(dpy = XOpenDisplay(0x0))) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    // X hata işleyiciyi ayarla
    XSetErrorHandler(x_error_handler);

    Window root = DefaultRootWindow(dpy);

    // Atomları başlat
    init_atoms(dpy);

    // Workspace sayısını ayarla
    unsigned long desktop_num = NUM_WORKSPACES;
    XChangeProperty(dpy, root, net_number_of_desktops, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&desktop_num, 1);

    // Desteklenen özellikleri bildir
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

    // Mevcut workspace'i 0 olarak ayarla
    unsigned long current_desktop = 0;
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&current_desktop, 1);

    // Workspace değiştirme ve taşıma tuşlarını tanımla
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        char key[2];
        snprintf(key, sizeof(key), "%d", i + 1);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(key)), 
                 MODKEY, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(key)), 
                 MODKEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    }

    // Diğer tuş tanımlamaları
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("space")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("h")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("v")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("b")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("Tab")), 
             MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("F1")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("q")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("p")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);

    // Ses kontrolleri için tuşlar
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioRaiseVolume")), 0, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioLowerVolume")), 0, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioMute")), 0, root, True, GrabModeAsync, GrabModeAsync);

    // Fare olayları
    XGrabButton(dpy, 1, MODKEY, root, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 3, MODKEY, root, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    // Fare imleci
    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
    XDefineCursor(dpy, root, cursor);

    // Root olayları
    XSelectInput(dpy, root, SubstructureNotifyMask | EnterWindowMask | PropertyChangeMask);

    // Workspace'leri başlat
    init_workspaces();

    start.subwindow = None;

    for (;;) {
        XNextEvent(dpy, &ev);

        if (ev.type == KeyPress) {
            KeyCode f1 = XKeysymToKeycode(dpy, XStringToKeysym("F1"));
            KeyCode q = XKeysymToKeycode(dpy, XStringToKeysym("q"));
            KeyCode p = XKeysymToKeycode(dpy, XStringToKeysym("p"));
            KeyCode vol_up = XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioRaiseVolume"));
            KeyCode vol_down = XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioLowerVolume"));
            KeyCode mute = XKeysymToKeycode(dpy, XStringToKeysym("XF86AudioMute"));

            if (ev.xkey.keycode == f1 && ev.xkey.subwindow != None && is_movable_window(dpy, ev.xkey.subwindow)) {
                XRaiseWindow(dpy, ev.xkey.subwindow);
            }
            else if (ev.xkey.keycode == q && active_window != None && is_movable_window(dpy, active_window)) {
                // Pencereyi kapatmadan önce logla
                fprintf(stderr, "Attempting to close window: %lu\n", active_window);

                XWindowAttributes attr;
                if (!XGetWindowAttributes(dpy, active_window, &attr)) {
                    fprintf(stderr, "Failed to get attributes for window: %lu\n", active_window);
                    active_window = None;
                    continue;
                }

                // Root penceresi olmadığından emin ol
                if (active_window == root) {
                    fprintf(stderr, "Attempt to close root window blocked\n");
                    active_window = None;
                    continue;
                }

                Atom *protocols = NULL;
                int n;
                Bool deleted = False;

                if (XGetWMProtocols(dpy, active_window, &protocols, &n)) {
                    for (int i = 0; i < n; ++i) {
                        if (protocols[i] == wm_delete) {
                            XEvent msg;
                            memset(&msg, 0, sizeof(msg));
                            msg.xclient.type = ClientMessage;
                            msg.xclient.window = active_window;
                            msg.xclient.message_type = wm_protocols;
                            msg.xclient.format = 32;
                            msg.xclient.data.l[0] = wm_delete;
                            msg.xclient.data.l[1] = CurrentTime;
                            if (XSendEvent(dpy, active_window, False, NoEventMask, &msg)) {
                                deleted = True;
                            } else {
                                fprintf(stderr, "Failed to send WM_DELETE_WINDOW to window: %lu\n", active_window);
                            }
                            break;
                        }
                    }
                    XFree(protocols);
                }

                if (!deleted) {
                    fprintf(stderr, "No WM_DELETE_WINDOW protocol, destroying window: %lu\n", active_window);
                    XDestroyWindow(dpy, active_window);
                }
                XSync(dpy, False); // Hataları hemen işle
                active_window = None; // Pencereyi sıfırla
            }
            else if (ev.xkey.keycode == p) {
                system(LAUNCHER);
            }
            else if (ev.xkey.keycode == vol_up) {
                system("pactl set-sink-volume @DEFAULT_SINK@ +5%");
                system("notify-send 'Ses Seviyesi' \"$(pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]\\+%' | head -n1)\" -t 1000");
            }
            else if (ev.xkey.keycode == vol_down) {
                system("pactl set-sink-volume @DEFAULT_SINK@ -5%");
                system("notify-send 'Ses Seviyesi' \"$(pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]\\+%' | head -n1)\" -t 1000");
            }
            else if (ev.xkey.keycode == mute) {
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
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("b"))) {
                toggle_bar_position(dpy);
            }
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym("Tab"))) {
                focus_next_window(dpy);
            }
            for (int i = 0; i < NUM_WORKSPACES; i++) {
                char key[2];
                snprintf(key, sizeof(key), "%d", i + 1);
                if (ev.xkey.keycode == XKeysymToKeycode(dpy, XStringToKeysym(key))) {
                    if (ev.xkey.state & ShiftMask && ev.xkey.subwindow != None && is_movable_window(dpy, ev.xkey.subwindow)) {
                        move_window_to_workspace(dpy, ev.xkey.subwindow, i);
                    } else {
                        switch_workspace(dpy, i);
                    }
                    break;
                }
            }
        }
        else if (ev.type == ButtonPress && ev.xbutton.subwindow != None && is_movable_window(dpy, ev.xbutton.subwindow)) {
            XRaiseWindow(dpy, ev.xbutton.subwindow);
            XSetInputFocus(dpy, ev.xbutton.subwindow, RevertToPointerRoot, CurrentTime);
            XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
            start = ev.xbutton;
            XSelectInput(dpy, start.subwindow, EnterWindowMask);
        }
        else if (ev.type == MotionNotify && start.subwindow != None && is_movable_window(dpy, start.subwindow)) {
            int xdiff = ev.xbutton.x_root - start.x_root;
            int ydiff = ev.xbutton.y_root - start.y_root;
            XMoveResizeWindow(dpy, start.subwindow,
                              attr.x + (start.button == 1 ? xdiff : 0),
                              attr.y + (start.button == 1 ? ydiff : 0),
                              MAX(1, attr.width + (start.button == 3 ? xdiff : 0)),
                              MAX(1, attr.height + (start.button == 3 ? ydiff : 0)));
        }
        else if (ev.type == ButtonRelease) {
            start.subwindow = None;
        }
        else if (ev.type == MapNotify) {
            if (!ev.xmap.override_redirect) {
                if (is_movable_window(dpy, ev.xmap.window)) {
                    add_window_to_workspace(dpy, ev.xmap.window, current_workspace);
                    active_window = ev.xmap.window;
                } else {
                    // Dialog pencereleri için sadece workspace'e ekle, tile düzenlemesi yapma
                    add_window_to_workspace(dpy, ev.xmap.window, current_workspace);
                }
                XSelectInput(dpy, ev.xmap.window, EnterWindowMask);
            }
        }
        else if (ev.type == EnterNotify && ev.xcrossing.window != root) {
            XWindowAttributes wattr;
            if (XGetWindowAttributes(dpy, ev.xcrossing.window, &wattr) && !wattr.override_redirect && is_movable_window(dpy, ev.xcrossing.window)) {
                XSetInputFocus(dpy, ev.xcrossing.window, RevertToPointerRoot, CurrentTime);
                active_window = ev.xcrossing.window;
            }
        }
        else if (ev.type == UnmapNotify) {
            remove_window_from_workspace(dpy, ev.xunmap.window, current_workspace);
            if (ev.xunmap.window == active_window) {
                active_window = None;
            }
        }
        else if (ev.type == ClientMessage) {
            if (ev.xclient.message_type == net_current_desktop) {
                unsigned int new_workspace = (unsigned int)ev.xclient.data.l[0];
                if (new_workspace < NUM_WORKSPACES) {
                    switch_workspace(dpy, new_workspace);
                }
            }
        }
    }

    // Cleanup (never reached in this loop, but for completeness)
    cleanup_workspaces();
    XCloseDisplay(dpy);
    return 0;
}