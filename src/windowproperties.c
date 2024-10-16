#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include<X11/extensions/shape.h>
#include<X11/extensions/Xfixes.h>

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "../include/windowproperties.h"

#define _NET_WM_STATE_ADD           1    /* add/set property */

static Bool add_window_property(Display *dpy, Window wdw, char *atomName);  // add property to window
 Bool WP_keep_above(Display *dpy, Window wdw);    // Keep window above
 Bool WP_full_screen(Display *dpy, Window wdw);   // Full screen window
 Bool WP_skip_taskbar(Display *dpy, Window wdw);  // Window not appear in taskbar
 Bool WP_nonresizable(Display *dpy, Window wdw, int width, int height); // Window can't be resize
void WP_pass_input(Display *dpy, Window wdw);     // Pass all input through the window


// Name         : add_window_property
// Parameters   : dpy       > Display structure serves as the connection to the X server
//                wdw       > Window ID
//                atomName  > property atom to add
// Call         : XInternAtom(), fprintf(), memset(), XSendEvent(),
//                XDefaultRootWindow(), XFlush()
// Called by    : WP_skip_taskbar(), WP_full_screen(), WP_keep_above()
// Description  : add property to window
Bool add_window_property(Display *dpy, Window wdw, char *atomName)
{

    Atom propertyAtom = XInternAtom(dpy, atomName, True);
    if(propertyAtom == None)
    {
        fprintf(stderr, "ERROR: %s not found!\n", atomName );
        return False;
    }

    Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", True);
    if(wmState == None)
    {
        fprintf(stderr, "ERROR:  _NET_WM_STATE not found!\n" );
        return False;
    }

    XClientMessageEvent mEv;
    memset(&mEv, 0, sizeof(mEv));
    mEv.type = ClientMessage;
    mEv.window = wdw;
    mEv.message_type = wmState;
    mEv.format = 32;
    mEv.data.l[0] = _NET_WM_STATE_ADD;
    mEv.data.l[1] = propertyAtom;
    mEv.data.l[2] = 0;
    mEv.data.l[3] = 0;
    mEv.data.l[4] = 0;
    XSendEvent(dpy, XDefaultRootWindow(dpy), False,
          SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&mEv );
    XFlush(dpy);
    return True;
}

// Name         : WP_skip_taskbar
// Parameters   : dpy   > Display structure serves as the connection to the X server
//                wdw   > Window ID
// Call         : add_window_property()
// Called by    : main()
// Description  : Window not appear in taskbar
Bool WP_skip_taskbar(Display *dpy, Window wdw)
{
    return add_window_property(dpy, wdw, "_NET_WM_STATE_SKIP_TASKBAR");
}

// Name         : WP_keep_above
// Parameters   : dpy   > Display structure serves as the connection to the X server
//                wdw   > Window ID
// Call         : add_window_property()
// Called by    : main(), CP_color_picker()
// Description  : Keep window above other windows

Bool WP_keep_above(Display* dpy, Window wdw)
{
    return add_window_property(dpy, wdw, "_NET_WM_STATE_ABOVE");
}

// Name         : WP_full_screen
// Parameters   : dpy   > Display structure serves as the connection to the X server
//                wdw   > Window ID
// Call         : add_window_property()
// Called by    : main()
// Description  : Full screen window
Bool WP_full_screen(Display* dpy, Window wdw)
{
    return add_window_property(dpy, wdw, "_NET_WM_STATE_FULLSCREEN");
}

// Name         : WP_pass_input
// Parameters   : dpy   > Display structure serves as the connection to the X server
//                wdw   > Window ID
// Call         : XFixesCreateRegion(), XFixesSetWindowShapeRegion()
// Called by    : main()
// Description  : Pass all input through the window like it doesn't exist.
void WP_pass_input(Display *dpy, Window wdw)
{
    XRectangle rect;
    XserverRegion region;

    region = XFixesCreateRegion(dpy, &rect, 1);
    XFixesSetWindowShapeRegion(dpy, wdw, ShapeInput, 0, 0, region);
}

// Name         : WP_nonresizable
// Parameters   : dpy   > Display structure serves as the connection to the X server
//                wdw   > Window ID
//                w     > window width
//                h     > window height
// Call         : XAllocSizeHints(), fprintf(), XSetWMSizeHints(), XFree()
// Called by    : CP_color_picker()
// Description  : Window width and height is non-resizeable
Bool WP_nonresizable(Display *dpy, Window wdw, int w, int h)
{
    XSizeHints *xsh;

    xsh = XAllocSizeHints();

    if(!xsh)
    {
        fprintf(stderr, "Error: Unable to allocate memory using XAllocSizeHints().\n");
        return False;
    }

    xsh->flags = PMinSize | PMaxSize;
    xsh->min_width = xsh->max_width = w;
    xsh->min_height = xsh->max_height = h;
    XSetWMSizeHints(dpy, wdw, xsh, XA_WM_NORMAL_HINTS);

    XFree(xsh);
    return True;
}
