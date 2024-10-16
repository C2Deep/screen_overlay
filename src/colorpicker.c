#include<X11/Xlib.h>
#include <X11/Xutil.h>
#include<X11/Xatom.h>
#include<X11/extensions/shape.h>
#include<X11/extensions/Xfixes.h>

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#include "../include/colorpicker.h"
#include "../include/windowproperties.h"

#define BG_COLOR   0xc0c0c0c0     // Color picker window background color
#define CUR_COLOR  0x50505050     // Cursors color

#define CONFIG_FILE     "config.cfg"         // Configuration file to save pixel information

// [Local functions]
// Create color spectrum image
static XImage *create_color_spectrum(int width, int height);
// Create color value slider image
static XImage *create_value_slider(int width, int height, float h, float s);
// Color spectrum cursor
static void color_cursor(int x, int y);
// Color value slider cursor
static void value_cursor(int y);
// Opacity slider cursor
static int opacity_slider(int x);
// Open colorpicker to screenoverlay pipe
static int open_CP_pipe(void);
// Create and set colorpicker icon
static void set_CP_icon(Display *display, Window window, int size);
// Get saved configuration from CONFIG_FILE and set it
static void get_config(unsigned long *pixel, int *csX, int *csY, int *osX, int *cvY);
// Save configuration
static void save_config(unsigned long pixel, int csX, int csY, int osX, int cvY);
// HSV to RGB color converter
static void HSV_2_RGB(float hue, float saturation, float value,
                        unsigned char *red, unsigned char *green, unsigned char *blue);

// [Global function]
// Create color picker window
int CP_color_picker(void);

static Display *Gdpy;
static Visual *Gvisual;
static Window Gwdw;
static GC Ggc;

// Color Spectrum structure
struct cs
{
    int w;      // Color spectrum width
    int h;      // Color spectrum height
    int mw;     // Color spectrum margin width
    int mh;     // Color spectrum margin height
    int cs;     // Cursor size
    XImage* img;    // Color spectrum image
    unsigned char* data;    // Image data
}Gcs;

// Color Value slider structure
struct cv
{
    int w;           // Color value slider width
    int h;           // Color value slider height
    int mw;          // Color value slider margin width
    int mh;          // Color value slider margin height
    int cs;          // Cursor size
    XImage* img;     // Color value image
    unsigned char* data;    // Image data
}Gcv;

// Output Color structure
struct oc
{
    int w;      // Output color width
    int h;      // Output color height
    int mw;     // Output color margin width
    int mh;     // Output color margin height
}Goc;

// Opacity Slider structure
struct os
{
    int w;      // Opacity slider width
    int h;      // Opacity slider height
    int mw;     // Opacity slider margin width
    int mh;     // Opacity slider margin height
    int owp;    // Opacity string width in pixel
    int cs;     // Cursor size
    char *opacityStr;   // Opacity character string
}Gos;

// Name         : CP_color_picker
// Parameters   : void
// Call         : XOpenDisplay(), XDefaultVisual(), XDefaultScreen(), XDisplayWidth(),
//                XDisplayHeight(), XLoadQueryFont(), fprintf(), exit(), XCreateSimpleWindow(),
//                XStoreName(), set_CP_icon(), WP_nonresizable(), XCreateGC(), XMapWindow(),
//                WP_keep_above(), create_color_spectrum(), XSelectInput(), XInternAtom(),
//                XSetWMProtocols(), open_CP_pipe(), get_config(), write(), XNextEvent(),
//                XPutImage(), color_cursor(), create_value_slider(), XDestroyImage(),
//                value_cursor(), HSV_2_RGB(), XSetForeground(), XFillRectangle(), opacity_slider(),
//                save_config(), close(), XFreeFont(), XDestroyImage(), XFreeGC(), XDestroyWindow(),
//                XCloseDisplay().
// Called by    : Tcolor_picker()
// Description  : Create the color picker window
int CP_color_picker(void)
{
    float hue = 1.0f/3.0f;     // hue
    float sat = 1.0f/3.0f;     // saturation
    float val = 1.0f/3.0f;     // Value

    Gdpy = XOpenDisplay(NULL);
    Gvisual = XDefaultVisual(Gdpy, 0);

    int scrn =  XDefaultScreen(Gdpy);
    int scrnWidth = XDisplayWidth(Gdpy, scrn);
    int scrnHeight = XDisplayHeight(Gdpy, scrn);

    Atom wmDeleteMessage;
    XFontStruct *fontStruct;

    fontStruct = XLoadQueryFont(Gdpy, "fixed");
    if (!fontStruct)
    {
        fprintf(stderr, "Error: Couldn't loading \"fixed\" font.\n");
        exit(-1);
    }

    Gcs.w = (1.0/3.0) * scrnHeight;
    Gcs.h = (1.0/3.0) * scrnHeight;
    Gcs.mw = (1.0/36.0) * scrnHeight;
    Gcs.mh = (1.0/36.0) * scrnHeight;
    Gcs.cs = (1.0/153.6) * scrnHeight;

    Gcv.w = (1.0/30.0) * scrnHeight;
    Gcv.h = Gcs.h;
    Gcv.mw = Gcs.mw + Gcs.w + (1.0/30.0)*scrnHeight;
    Gcv.mh = Gcs.mh;
    Gcv.cs = (1.0/109.0) * scrnHeight;

    Goc.w = Gcs.w;
    Goc.h = (1.0/15.0) * scrnHeight;
    Goc.mw = Gcs.mw;
    Goc.mh = Gcs.mh + Gcs.h + (1.0/75.0) * scrnHeight;

    Gos.w = Gcs.w;
    Gos.mw = Gcs.mw;
    Gos.mh = Goc.mh + Goc.h + (1.0/30.0)*scrnHeight;
    Gos.opacityStr = "OPACITY";
    Gos.owp = XTextWidth(fontStruct, Gos.opacityStr, strlen(Gos.opacityStr));
    Gos.cs = (1.0/109.0) * scrnHeight;

    int wdwWidth = (int)( (15.0/32.0) * scrnHeight), wdwHeight = (int)((25.0/48.0) * scrnHeight),      // Window width and height
        csX = Gcs.mw + Gcs.w * hue, csY = Gcs.mh + Gcs.h*sat,             // Colour spectrum cursor position
        cvX = Gcv.mw, cvY = Gcv.mh + (Gcv.h - 1)*val,                     // Color value slider cursor position
        osX = Gos.mw + Gos.owp + (Gos.owp/2);                             // Opacity slider cursor position

    int posX = 0, posY = 0,                 // Temprory variables holding mouse position
        exitF = 0,                          // Exit flag
        pipeFD = 0;                         // Fifo file descriptor

    int positions[4] = {0};                 // Store cursors positions

    unsigned char o, r, g, b;               // Opacity, Red, Green, Blue
    unsigned long pixel = 0;                // Hold ARGB values
    char inF[3] = {0};                      // Input Flags (to detect mouse clicks on certain areas)

    XEvent ev;

    o = r = g = b = 0;

    Gwdw = XCreateSimpleWindow(Gdpy, RootWindow(Gdpy, 0), 0, 0, wdwWidth, wdwHeight, 1, 0, BG_COLOR);

    XStoreName(Gdpy, Gwdw, "Color picker");

    set_CP_icon(Gdpy, Gwdw, 128);
    WP_nonresizable(Gdpy, Gwdw, wdwWidth, wdwHeight);

    Ggc = XCreateGC(Gdpy, Gwdw, 0, NULL);

    if(Gvisual->class != TrueColor)
    {
        fprintf(stderr,  "Can't handle non true color visual ...\n");
        exit(-1);
    }

    XMapWindow(Gdpy, Gwdw);

    if(WP_keep_above(Gdpy, Gwdw) == False)
    {
        fprintf(stderr, "Couldn't keep window above others.\n");
        exit(-1);
    }

    Gcs.img = create_color_spectrum(Gcs.w, Gcs.h);
    XSelectInput(Gdpy, Gwdw, ExposureMask | ButtonPressMask |
                                ButtonReleaseMask | ButtonMotionMask);

    wmDeleteMessage = XInternAtom(Gdpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(Gdpy, Gwdw, &wmDeleteMessage, 1);

    pipeFD = open_CP_pipe();       // open a pipe for writing to screenOverlay window

    get_config(&pixel, &csX, &csY, &osX, &cvY);

    write(pipeFD, &pixel, sizeof(unsigned long));

    while(!exitF)
    {
        XNextEvent(Gdpy, &ev);

        if(inF[0] || inF[1] || inF[2])
        {
            pixel = (o << 24) | (r << 16) | (g << 8) | b;
            write(pipeFD, &pixel, sizeof(unsigned long));
        }
        if(ev.type == Expose)
        {
            XPutImage(Gdpy, Gwdw, Ggc, Gcs.img, 0, 0, Gcs.mw, Gcs.mh, Gcs.w, Gcs.h);
            color_cursor(csX, csY);

            Gcv.img  = create_value_slider(Gcv.w, Gcv.h, hue, sat);
            XPutImage(Gdpy, Gwdw, Ggc, Gcv.img, 0, 0, Gcv.mw, Gcv.mh, Gcv.w, Gcv.h);
            XDestroyImage(Gcv.img);
            value_cursor(cvY);

            hue = (float)(csX - Gcs.mw) / (Gcs.w - 1);
            sat = 1.0f - ((float)(csY - Gcs.mh) / (Gcs.h - 1));
            val = 1.0f - ((float)(cvY - Gcv.mh) / (Gcv.h - 1));

            HSV_2_RGB(hue, sat, val, &r, &g, &b);
            XSetForeground(Gdpy, Ggc, (r << 16) | (g << 8) | b);
            XFillRectangle(Gdpy, Gwdw, Ggc, Goc.mw, Goc.mh, Goc.w, Goc.h);

            o = opacity_slider(osX);
            continue;
        }

        if(ev.type == ButtonRelease)
        {
            inF[0] = inF[1] = inF[2] = 0;
            continue;
        }
        if(ev.type == ButtonPress)
        {
            posX = ev.xmotion.x;
            posY = ev.xmotion.y;

            // Button pressed inside color spectrum area
            if(
                posX > Gcs.mw && posX < (Gcs.w + Gcs.mw) &&
                posY > Gcs.mh && posY < (Gcs.h + Gcs.mh)
              )
                inF[0] = 1;

            // Button pressed inside color value slider area
            else if(
                    posX >= (Gcv.mw - Gcv.cs) && posX <= (Gcv.w + Gcv.mw + Gcv.cs) &&
                    posY >= Gcv.mh && posY <= (Gcv.h + Gcv.mh)
                   )
                inF[1] = 1;

            // Button pressed inside opacity slider area
            else if(
                    posX >= (Gos.mw + Gos.owp + (Gos.owp/2) - Gos.cs) && posX <= (Gos.w + Gos.mw + Gos.cs) &&
                    posY >= (Gos.mh - Gos.cs) && posY <= (Gos.mh + Gos.cs)
                   )
                inF[2] = 1;

            else
                continue;
        }

        if( ( (ev.type == MotionNotify) && inF[0]) || inF[0])
        {
            csX = ev.xmotion.x;
            csY = ev.xmotion.y;

            csX = csX < Gcs.mw ? Gcs.mw : csX;
            csX = csX > (Gcs.w + Gcs.mw - 1) ? (Gcs.w + Gcs.mw - 1) : csX;
            csY = csY < Gcs.mh ? Gcs.mh : csY;
            csY = csY > (Gcs.h + Gcs.mh - 1)? (Gcs.h + Gcs.mh - 1) : csY;

            hue = (float)(csX - Gcs.mw) / (Gcs.w - 1);
            sat = 1.0f - ((float)(csY - Gcs.mh) / (Gcs.h - 1));

            XPutImage(Gdpy, Gwdw, Ggc, Gcs.img, 0, 0, Gcs.mw, Gcs.mh, Gcs.w, Gcs.h);
            color_cursor(csX, csY);

            Gcv.img  = create_value_slider(Gcv.w, Gcv.h, hue, sat);
            XPutImage(Gdpy, Gwdw, Ggc, Gcv.img, 0, 0, Gcv.mw, Gcv.mh, Gcv.w, Gcv.h);
            XDestroyImage(Gcv.img);

            HSV_2_RGB(hue, sat, val, &r, &g, &b);

            XSetForeground(Gdpy, Ggc, (r << 16) | (g << 8) | b);
            XFillRectangle(Gdpy, Gwdw, Ggc, Goc.mw, Goc.mh, Goc.w, Goc.h);

            continue;
        }

        if( ( (ev.type == MotionNotify) && inF[1]) || inF[1])
        {
            cvY = ev.xmotion.y;

            if(cvY < Gcv.mh)
                cvY = Gcv.mh;

            if(cvY > (Gcv.h + Gcv.mh - 1))
                cvY  = (Gcv.h + Gcv.mh - 1);

            value_cursor(cvY);

            val = 1.0f - ((float)(cvY - Gcv.mh) / (Gcv.h - 1));

            HSV_2_RGB(hue, sat, val, &r, &g, &b);

            XSetForeground(Gdpy, Ggc, (r << 16) | (g << 8) | b);
            XFillRectangle(Gdpy, Gwdw, Ggc, Goc.mw, Goc.mh, Goc.w, Goc.h);
            continue;
        }

        if( ((ev.type == MotionNotify) && inF[2] ) || inF[2])
        {
            osX = ev.xmotion.x;

            if(osX < Gos.mw + Gos.owp + (Gos.owp/2))
                osX = Gos.mw + Gos.owp + (Gos.owp/2);
            if(osX > Gos.w + Gos.mw)
                osX = Gos.w + Gos.mw;

            o = opacity_slider(osX);
            continue;
        }

        if(ev.type == ClientMessage && (ev.xclient.data.l[0] == wmDeleteMessage))
            exitF = 1;
    }

    // Save to configuration file
    save_config(pixel, csX, csY, osX, cvY);

    // Clean up
    close(pipeFD);
    XFreeFont(Gdpy, fontStruct);
    XDestroyImage(Gcs.img);
    XFreeGC(Gdpy, Ggc);
    XDestroyWindow(Gdpy, Gwdw);
    XCloseDisplay(Gdpy);

    return 0;
}

// Name         : create_color_spectrum
// Parameters   : width     > Color spectrum width
//                height    > Color spectrum height
// Call         : malloc(), fprintf(), exit(), HSV_2_RGB(), XCreateImage().
// Called by    : CP_color_picker()
// Description  : Create color spectrum image
XImage *create_color_spectrum(int width, int height)
{
    Gcs.data = (unsigned char *)malloc(width * height * 4);
    if(!Gcs.data){fprintf(stderr, "Error: Unable to allocate memeory.\n"); exit(-1);}

    unsigned char *pImg = Gcs.data;
    unsigned char r, g, b;
    int i, j;
    for(i = height - 1 ; i >= 0; --i)
    {
        for(j = 0 ; j < width; ++j)
        {
            HSV_2_RGB((float)j/(width - 1), (float)i/(height - 1), 1.0f, &r, &g, &b);

            *pImg++ = b;   // Blue
            *pImg++ = g;   // Green
            *pImg++ = r;   // Red
             pImg++;
        }

    }

    return XCreateImage(Gdpy, Gvisual, 24, ZPixmap, 0, Gcs.data, width, height, 32, 0);
}

// Name         : create_value_slider
// Parameters   : width     > Width of color value slider
//                height    > Height of color value slider
//                h         > Hue value
//                s         > Saturation value
// Call         : malloc(), fprintf(), exit(), HSV_2_RGB(), XCreateImage().
// Called by    : CP_color_picker()
// Description  : create color value slider image
XImage *create_value_slider(int width, int height, float h, float s)
{
    Gcv.data = (unsigned char *)malloc(width * height * 4);
    if(!Gcv.data){fprintf(stderr, "Error: Unable to allocate memory.\n"); exit(-1);}

    unsigned char *pImg = Gcv.data;
    unsigned char r = 0, g = 0, b = 0;

    int i, j;

    for(i = height - 1 ; i >= 0 ; --i)
    {
        for(j = 0 ; j < width ; ++j)
        {
            HSV_2_RGB(h, s, (float) i / (height - 1), &r, &g, &b);

            *pImg++ = b;
            *pImg++ = g;
            *pImg++ = r;
             pImg++;
        }
    }
    return XCreateImage(Gdpy, Gvisual, 24, ZPixmap, 0, Gcv.data, width, height, 32, 0);
}

// Name         : HSV_2_RGB
// Parameters   : h     > Hue value (input)
//                s     > Saturation value  (input)
//                v     > Value value   (input)
//                r     > Red value (output)
//                g     > Green value (output)
//                b     > Blue value (output)
// Call         : void
// Called by    : CP_color_picker(), create_value_slider(),
// Description  : Convert from HSV color space to RGB color space
void HSV_2_RGB(float h, float s, float v, unsigned char *r, unsigned char *g, unsigned char *b)
{

    if((int) h == 1)
        h = 0.0f;

    char i = (char)(h * 6.0f);                      // Intger part (0 - 6)
    float f = (h * 6.0f) - i;                       // Fraction part (0.0 - 1.0)
    float p = v * (1.0f - s);                       // Minimum value (0.0 - 1.0)
    float q = v * (1.0f - (s * f));                 // Positive liner gradient (0.0 - 1.0)
    float t = v * (1.0f - (s * (1.0f - f)));        // Negative liner gradient (1.0 - 0.0)

    unsigned char max = v * 255;        // Max value    (0 - 255)
    unsigned char min = p * 255;        // Minimum value (0 - 255)
    unsigned char plg = t * 255;        // Positive liner gradient (0 - 255)
    unsigned char nlg = q * 255;        // Negative liner gradient (0 - 255)

    switch (i) {
        case 0: *r = max; *g = plg; *b = min; break;
        case 1: *r = nlg; *g = max; *b = min; break;
        case 2: *r = min; *g = max; *b = plg; break;
        case 3: *r = min; *g = nlg; *b = max; break;
        case 4: *r = plg; *g = min; *b = max; break;
        case 5: *r = max; *g = min; *b = nlg; break;
    }
}

// Name         : color_cursor
// Parameters   : x     > Color spectrum cursor X-coordinate position
//                y     > Color spectrum cursor Y-coordinate position
// Call         : XSetForeground(), XSetLineAttributes(), XDrawLine().
// Called by    : CP_color_picker()
// Description  : Position color spectrum cursor in (x, y) (relative to root window)
void color_cursor(int x, int y)
{
    int x1[4], x2[4], y1[4], y2[4];
    int i, j;

    x1[0] = x1[3] = x2[0] = x2[3] = x;
    y1[1] = y1[2] = y2[1] = y2[2] = y;

    x1[1] = x - (2 * Gcs.cs);  x1[2] = x + Gcs.cs;
    y1[0] = y - Gcs.cs;        y1[3] = y + Gcs.cs;

    x2[1] = x - Gcs.cs;        x2[2] = x + (2 * Gcs.cs);
    y2[0] = y - (2 * Gcs.cs);  y2[3] = y + (2 * Gcs.cs);

    XSetForeground(Gdpy, Ggc, 0);
    XSetLineAttributes(Gdpy, Ggc, 1, LineSolid, CapButt, JoinBevel);

    if(y1[0] > Gcs.mh && y2[0] > Gcs.mh)
        XDrawLine(Gdpy, Gwdw, Ggc, x1[0], y1[0], x2[0], y2[0]);
    else
    {
        for( i = 0 ; (y2[0] < Gcs.mh) && (i < Gcs.cs) ; ++i)
            ++y2[0];
        if(i < Gcs.cs)
            XDrawLine(Gdpy, Gwdw, Ggc, x1[0], y1[0], x2[0], y2[0]);
    }

    if(x1[1] > Gcs.mw && x2[1] > Gcs.mw)
        XDrawLine(Gdpy, Gwdw, Ggc, x1[1], y1[1], x2[1], y2[1]);
    else
    {
        for(i = 0 ; (x1[1] < Gcs.mw) && (i < Gcs.cs) ; ++i)
            ++x1[1];
        if(i < Gcs.cs)
           XDrawLine(Gdpy, Gwdw, Ggc, x1[1], y1[1], x2[1], y2[1]);
    }

    if((x1[2] < (Gcs.w + Gcs.mw)) && (x2[2] < (Gcs.w + Gcs.mw)))
        XDrawLine(Gdpy, Gwdw, Ggc, x1[2], y1[2], x2[2], y2[2]);
    else
    {
        for( i = 0 ; (x2[2] > (Gcs.w + Gcs.mw - 1)) && (i < Gcs.cs) ; ++i)
            --x2[2];
        if(i < Gcs.cs)
             XDrawLine(Gdpy, Gwdw, Ggc, x1[2], y1[2], x2[2], y2[2]);
    }

    if((y1[3] < (Gcs.h + Gcs.mh)) && (y2[3] < (Gcs.h + Gcs.mh)))
        XDrawLine(Gdpy, Gwdw, Ggc, x1[3], y1[3], x2[3], y2[3]);
    else
    {
        for( i = 0 ; (y2[3] > (Gcs.h + Gcs.mh - 1)) && (i < Gcs.cs) ; ++i)
            --y2[3];
        if(i < Gcs.cs)
           XDrawLine(Gdpy, Gwdw, Ggc, x1[3], y1[3], x2[3], y2[3]);
    }

}

// Name         : value_cursor
// Parameters   : y     > Color value slider Y-coordinate position
// Call         : XSetForeground(), XFillPolygon().
// Called by    : CP_color_picker()
// Description  : Position color value slider cursor to Y-coordinate (relative to root window)
void value_cursor(int y)
{
    static XPoint points[2][4];
    static int owF = 0;     // Overwrite Flag

    if(owF)
    {
        XSetForeground(Gdpy, Ggc, BG_COLOR);
        XFillPolygon(Gdpy, Gwdw, Ggc, points[0], 4, Convex, CoordModeOrigin);
        XFillPolygon(Gdpy, Gwdw, Ggc, points[1], 4, Convex, CoordModeOrigin);
    }
    else
        owF = 1;

    // Left Triangle cursor
    points[0][0] = (XPoint) {Gcv.mw - Gcv.cs, y - Gcv.cs};
    points[0][1] = (XPoint) {Gcv.mw, y};
    points[0][2] = (XPoint) {Gcv.mw - Gcv.cs, y + Gcv.cs};
    points[0][3] = (XPoint) {Gcv.mw - Gcv.cs, y - Gcv.cs};

    // Right Triangle cursor
    points[1][0] = (XPoint) {Gcv.mw + Gcv.w + Gcv.cs, y - Gcv.cs};
    points[1][1] = (XPoint) {Gcv.mw + Gcv.w, y};
    points[1][2] = (XPoint) {Gcv.mw + Gcv.w + Gcv.cs, y + Gcv.cs};
    points[1][3] = (XPoint) {Gcv.mw + Gcv.w + Gcv.cs, y - Gcv.cs};

    XSetForeground(Gdpy, Ggc, CUR_COLOR);
    XFillPolygon(Gdpy, Gwdw, Ggc, points[0], 4, Convex, CoordModeOrigin);
    XFillPolygon(Gdpy, Gwdw, Ggc, points[1], 4, Convex, CoordModeOrigin);
}

// Name         : opacity_slider
// Parameters   : x     > Opacity slider X-coordinate position
// Call         : XLoadQueryFont(), fprintf(), exit(), XSetForeground(), XDrawString(),
//                XFillPolygon(), XSetLineAttributes(), XDrawLine(), sprintf(), XTextWidth(),
//                XFreeFont().
// Called by    : CP_color_picker()
// Description  : Create opacity slider and position it's cursor to X-coordinate (relative to root window)
int opacity_slider(int x)
{
    XFontStruct *fontStruct;

    static char prcntStr[5];    // 100%'\0'
    static XPoint points[5];

    static int owF = 0;                        // Overwrite Flag
    static int pwp = 0;                        // Percent string width in pixel

    int  shp = 0,                              // Character string height in pixel
         oss = Gos.mw + Gos.owp + (Gos.owp/2), // Opacity Slider Start
         ose = Gos.w + Gos.mw,                 // Opacity Slider End
         cntrTxt = 0;                          // Center text relative to slider line

    float opacity = 0;

    fontStruct = XLoadQueryFont(Gdpy, "fixed");
    if (!fontStruct) {
        fprintf(stderr, "Error: Couldn't loading \"fixed\" font.\n");
        exit(-1);
    }

    shp = fontStruct->ascent + fontStruct->descent;
    cntrTxt = Gos.mh + (shp/2) - 1;

    XSetForeground(Gdpy, Ggc, CUR_COLOR);
    XDrawString(Gdpy, Gwdw, Ggc, Gos.mw, cntrTxt, Gos.opacityStr, strlen(Gos.opacityStr));

    if(owF)
    {
        XSetForeground(Gdpy, Ggc, BG_COLOR);
        XFillPolygon(Gdpy, Gwdw, Ggc, points, 5, Convex, CoordModeOrigin);

        XSetLineAttributes(Gdpy, Ggc, 2, LineSolid, CapButt, JoinBevel);
        XDrawLine(Gdpy, Gwdw, Ggc, oss, Gos.mh, ose, Gos.mh);

        XDrawString(Gdpy, Gwdw, Ggc, ose + pwp, cntrTxt, prcntStr, strlen(prcntStr));

        XSetForeground(Gdpy, Ggc, CUR_COLOR);
        XSetLineAttributes(Gdpy, Ggc, 1, LineSolid, CapButt, JoinBevel);
        XDrawLine(Gdpy, Gwdw, Ggc, oss, Gos.mh, ose, Gos.mh);
    }
    else
    {
        owF = 1;
        XSetLineAttributes(Gdpy, Ggc, 1, LineSolid, CapButt, JoinBevel);
        XDrawLine(Gdpy, Gwdw, Ggc, oss, Gos.mh, ose, Gos.mh);
    }

    points[0] = (XPoint) {x - Gos.cs, Gos.mh};
    points[1] = (XPoint) {x, Gos.mh - Gos.cs};
    points[2] = (XPoint) {x + Gos.cs, Gos.mh};
    points[3] = (XPoint) {x, Gos.mh + Gos.cs};
    points[4] = (XPoint) {x - Gos.cs, Gos.mh};

    XSetForeground(Gdpy, Ggc, CUR_COLOR);
    XFillPolygon(Gdpy, Gwdw, Ggc, points, 5, Convex, CoordModeOrigin);

    XSetLineAttributes(Gdpy, Ggc, 2, LineSolid, CapButt, JoinBevel);
    XDrawLine(Gdpy, Gwdw, Ggc, oss, Gos.mh, x, Gos.mh);

    opacity = ((float)(x - oss) / (ose - oss));
    sprintf(prcntStr, "%3d%%", (int)(100.0f * opacity ));
    pwp = XTextWidth(fontStruct, prcntStr, strlen(prcntStr));
    XDrawString(Gdpy, Gwdw, Ggc, ose + pwp, cntrTxt, prcntStr, strlen(prcntStr));
    XFreeFont(Gdpy, fontStruct);

    return (int)(opacity * 255);
}

// Name         : open_CP_pipe
// Parameters   : void
// Call         : open(), perror(), unlink(), exit().
// Called by    : CP_color_picker()
// Description  : open color picker pipe for writing
int open_CP_pipe(void)
{
    int fd = 0;

    fd = open (CP_PIPE_FILE, O_WRONLY);

    if(fd < 0)
    {
        perror("Error");
        unlink(CP_PIPE_FILE);
        exit(-1);
    }

    return fd;

}

// Name         : set_CP_icon
// Parameters   : dpy   > Display structure serves as the connection to the X server
//                wdw   > Window ID
//                size  > Icon Size
// Call         : XInternAtom(), malloc(), sizeof(), HSV_2_RGB(), XChangeProperty(), free().
// Called by    : CP_color_picker()
// Description  : Create an Icon for color picker window
void set_CP_icon(Display *dpy, Window wdw, int size) {

    Atom wmIcon = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom cardinal = XInternAtom(dpy, "CARDINAL", False);

    float h, s, v;      // Hue, Saturation, Value
    int fullSize = size * size + 2,
    i = 0, j = 0,  k = 0;

    unsigned char r, g, b;
    unsigned long pixel = 0;
    unsigned long *icon = malloc(fullSize * sizeof(unsigned long));
    if(!icon) return;

    icon[0] = size;  // width
    icon[1] = size;  // height

    for(i = size - 1 ; i >= 0; --i)
    {
        for(j = 0 ; j < size; ++j, ++k)
        {
            h = (float)j/(size - 1);
            s = (float)i/(size - 1);
            v = 0.5f + (s/2.0f);

            HSV_2_RGB(h, s, v, &r, &g, &b);
            icon[2 + k] = (0xff << 24) | (r << 16) | (g << 8) | b;
        }

    }
    // Set the property
    XChangeProperty(dpy, wdw, wmIcon, cardinal, 32, PropModeReplace, (unsigned char *)icon, fullSize);

    free(icon);
}

// Name         : get_config
// Parameters   : pixel > hold pixel value (output)
//                csX   > hold csX value (output)
//                csY   > hold csY value (output)
//                osX   > hold osX value (output)
//                cvY   > hold cvY value (output)
// Call         : open(), read(), sizeof(), close().
// Called by    : CP_color_picker()
// Description  : Configure the parameters with the values retrived from the configuration file.
void get_config(unsigned long *pixel, int *csX, int *csY, int *osX, int *cvY)
{
    int cfgFD = open(CONFIG_FILE, O_RDONLY);
    int positions[4] = {0};

    if(cfgFD > 0)
    {
        read(cfgFD, pixel, sizeof(unsigned long));
        read(cfgFD, positions, sizeof(positions));

        *csX = positions[0];    // color picker cursor X-coordinate position
        *csY = positions[1];    // color picker cursor Y-coordinate position
        *osX = positions[2];    // opacity slider X-coordinate position
        *cvY = positions[3];    // color value slider Y-coordinate position
    }

    close(cfgFD);
}

// Name         : save_config
// Parameters   : pixel > hold pixel value (input)
//                csX   > hold csX value (input)
//                csY   > hold csY value (input)
//                osX   > hold osX value (input)
//                cvY   > hold cvY value (input)
// Call         : open(), write(), sizeof(), close()
// Called by    : CP_color_picker()
// Description  : Save the parameters to the configuration file
void save_config(unsigned long pixel, int csX, int csY, int osX, int cvY)
{
    int cfgFD = open(CONFIG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    int positions[4] = {csX, csY, osX, cvY};

    if(cfgFD > 0)
    {
        write(cfgFD, &pixel, sizeof(unsigned long));
        write(cfgFD, positions, sizeof(positions));
    }

    close(cfgFD);
}
