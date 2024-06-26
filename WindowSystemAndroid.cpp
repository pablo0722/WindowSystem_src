#include "WindowSystem/EglWindowSystem.hpp"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

bool EglWindowSystem::getDisplay() {
    display = XOpenDisplay(NULL);
    if ( display == NULL )
    {
        return false;
    }

    return true;
}

bool EglWindowSystem::createWindow(const char *title, int posx, int posy, int width, int height) {
    Window defaultRootWindow;

    defaultRootWindow = DefaultRootWindow(display);

    {
        XSetWindowAttributes swa;

        swa.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask;
        window = XCreateWindow(
                display, defaultRootWindow,
                posx, posy, width, height, 0,
                CopyFromParent, InputOutput,
                CopyFromParent, CWEventMask,
                &swa);
    }

    deleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &deleteMessage, 1);

    {
        XSetWindowAttributes xattr;
        xattr.override_redirect = false;
        XChangeWindowAttributes ( display, window, CWOverrideRedirect, &xattr );
    }

    {
        XWMHints xWMHints;
        xWMHints.input = true;
        xWMHints.flags = InputHint;
        XSetWMHints(display, window, &xWMHints);
    }
    
    {
        XSizeHints xSizeHints = {0};
        xSizeHints.flags  = PPosition | PSize;     /* I want to specify position and size */
        xSizeHints.x      = posx;       /* The origin and size coords I want */
        xSizeHints.y      = posy;
        xSizeHints.width  = width;
        xSizeHints.height = height;
        XSetNormalHints(display, window, &xSizeHints);  /* Where new_window is the new window */
    }

    // make the window visible on the screen
    XMapWindow (display, window);
    XStoreName (display, window, title);

    {
        Atom wm_state;

        // get identifiers for the provided atom name strings
        wm_state = XInternAtom (display, "_NET_WM_STATE", false);

        {
            XEvent xev;

            memset ( &xev, 0, sizeof(xev) );
            xev.type                 = ClientMessage;
            xev.xclient.window       = window;
            xev.xclient.message_type = wm_state;
            xev.xclient.format       = 32;
            xev.xclient.data.l[0]    = 1;
            xev.xclient.data.l[1]    = false;

            XSendEvent (
            display,
            defaultRootWindow,
            false,
            SubstructureNotifyMask,
            &xev );
        }
    }

    return true;
}

///
//  winSystem_Create()
//
//      This function initialized the native X11 display and window for EGL
//
bool EglWindowSystem::create(const char *title, int posx, int posy, int width, int height)
{
    printf("%s: creating native window\n", __func__);

    /*
     * X11 native display initialization
     */

    if(!getDisplay()) return false;
    if(!createWindow(title, posx, posy, width, height)) return false;

    printf("%s: end creating native window\n", __func__);

    return true;
}

void EglWindowSystem::registerKeyFunc(void (*keyFunc)(void *ctx, unsigned char keyChar, int x, int y)) {
    this->keyFunc = keyFunc;
}

Display *EglWindowSystem::getNativeDisplay() const {
    return display;
}

Window EglWindowSystem::getNativeWindow() const {
    return window;
}

EglWindowSystem::Event EglWindowSystem::getEvent(void *ctx) const
{
    Event ret = Event::Empty;
    XEvent xev;
    KeySym keySymbol;
    char keyChar;

    // Pump all messages from X server. Keypresses are directed to keyfunc (if defined)
    while(XPending(display))
    {
        XNextEvent(display, &xev);
        if(KeyPress == xev.type)
        {
            if(1 == XLookupString(&xev.xkey, &keyChar, 1, &keySymbol, 0))
            {
                printf("%s: key received: '%c' (%d)\n", __func__, keyChar, keyChar);
                if (keyFunc != NULL) {
                    keyFunc(ctx, keyChar, 0, 0);
                }
            }
        }
        if(xev.type == ClientMessage) {
            if(xev.xclient.data.l[0] == deleteMessage) {
                // Received when alt+F4 or "x" icon is clicked
                printf("%s: Delete Message\n", __func__);
                ret = Event::Delete;
            }
        }
        if(xev.type == DestroyNotify) {
            // Received when calling XDestroyWindow() or XDestroySubwindows()
            printf("%s: Destroy notify\n", __func__);
            ret = Event::Delete;
        }
    }

    return ret;
}