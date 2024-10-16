#ifndef WINDOWPROPERTIES_H_
#define WINDOWPROPERTIES_H_

#include<X11/Xlib.h>

Bool WP_keep_above(Display *dpy, Window wdw);
Bool WP_full_screen(Display *dpy, Window wdw);
Bool WP_skip_taskbar(Display *dpy, Window wdw);
void WP_pass_input(Display *dpy, Window wdw);
Bool WP_nonresizable(Display *dpy, Window wdw, int width, int height);
#endif
