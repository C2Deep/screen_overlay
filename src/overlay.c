// NEEDED Dependencies :
//              sudo apt-get install libx11-dev xserver-xorg-dev xorg-dev

#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include<X11/extensions/shape.h>
#include<X11/extensions/Xfixes.h>

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<ctype.h>
#include<pthread.h>
#include<termios.h>
#include<time.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#include "../include/colorpicker.h"
#include "../include/windowproperties.h"

int create_CP_pipe(void);               // Create color picker to screenOverlay pipe
Window create_window(Display *dpy);     // Create screen overlay window
void *Tcolor_picker(void *);            // [Thread function] create color picker window

int main(int argc, char *argv[])
{
    XEvent ev;

    unsigned long px;

    int pipeFD = 0, exitF = 0;
    size_t count;
    pthread_t threadID;

    Display *dpy =  XOpenDisplay(NULL);       // Default display
    Window wdw =  create_window(dpy);         // Create window

    XMapWindow(dpy, wdw);

    WP_skip_taskbar(dpy, wdw);       // Window not appear in taskbar
    WP_pass_input(dpy, wdw);         // All the input pass through the window (Transparent window)
    WP_full_screen(dpy, wdw);        // Full screen window
    WP_keep_above(dpy, wdw);         // keep above all windows

    if(create_CP_pipe() < 0)
    {
        perror("Erorr:");
        exit(-1);
    }

    L_ColorPicker:
    if(pthread_create(&threadID, NULL, Tcolor_picker, NULL) != 0)
    {
        fprintf(stderr, "Unable to create a thread for Tcolor_picker().\n");
        exit(-1);
    }

    pipeFD = open(CP_PIPE_FILE, O_RDONLY);

    if(pipeFD < 0)
    {
        perror("Error:");
        exit(-1);
    }

    while( (count = read(pipeFD, &px, sizeof(unsigned long))) == sizeof(unsigned long) )
    {
        XSetWindowBackground(dpy, wdw, px);
        XClearWindow(dpy, wdw);
        XFlush(dpy);
    }

    close(pipeFD);
    pthread_join(threadID, NULL);

    while(!exitF)
    {
        char c;
        printf("\n\t\tSCREEN OVERLAY\n\t\t-`-`-`-`-`-`-`\n\n- Enter 'p' to pick color.\n- Enter 'q' to exit.\n> ");
        c = getchar();
        getchar();
        switch(tolower(c))
        {
            case 'p':
                system("clear");
                goto L_ColorPicker;
                break;
            case 'q':
                exitF = 1;
                break;
        }
        system("clear");
    }

    unlink(CP_PIPE_FILE);
    XDestroyWindow(dpy, wdw);
    XCloseDisplay(dpy);
    return 0;
}

// Name         : create_CP_pipe
// Parameters   : void
// Call         : mkfifo()
// Called by    : main()
// Description  : Create pipe connecting main() and CP_color_picker()
int create_CP_pipe(void)
{
    if(mkfifo (CP_PIPE_FILE, S_IRUSR | S_IWUSR) < 0)
    {
      if(errno != EEXIST)
      {
        return -1;
      }
    }

    return 0;
}

// Name         : create_window
// Parameters   : dpy   > Display structure serves as the connection to the X server
// Call         : XDefaultRootWindow(), XMatchVisualInfo(), XDefaultScreen(), fprintf(),
//                exit(), XCreateColormap(), XCreateWindow()
// Called by    : main()
// Description  : Create the overlay window
Window create_window(Display* dpy)
{

    XVisualInfo vInfo;
    unsigned long xAttrMask = CWBackPixel | CWBackPixmap | CWBorderPixel | CWColormap;
    XSetWindowAttributes xAttr;
    Window wdw, root;

    root = XDefaultRootWindow(dpy);
    if(!XMatchVisualInfo(dpy, XDefaultScreen(dpy), 32, TrueColor, &vInfo))
    {
        fprintf(stderr, "Error: Visual not found !\n");
        exit(-1);
    }

    // Window attributes
    xAttr.background_pixel  = 0x80000000;
    xAttr.background_pixmap = None;
    xAttr.border_pixel      = 0;
    xAttr.colormap          = XCreateColormap(dpy, root, vInfo.visual, AllocNone);

    // Create window
    wdw = XCreateWindow(dpy, root, 0, 0, 500, 500, 0, 32, InputOutput,
                                vInfo.visual, xAttrMask, &xAttr);

    return wdw;

}

// Name         : Tcolor_picker
// Parameters   : void*
// Call         : CP_color_picker()
// Called by    : main()
// Description  : Wrapper function to CP_color_picker() to run as a thread function
void *Tcolor_picker(void*)
{
    CP_color_picker();
}

