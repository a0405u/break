#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define TIMER_DURATION 3
#define BREAK_DURATION 3

#define TITLE "Take a break!"
#define MESSAGE "Rest your eyes. Stretch your legs. Breathe. Relax."

#define BACKGROUND_COLOR "#000000"
#define FONT_COLOR "#e5e1cf"
#define FONT_NAME "JetBrains Mono"
#define TITLE_FONT_SIZE 14
#define TITLE_FONT_STYLE "bold"
#define MESSAGE_FONT_SIZE 12
#define MESSAGE_FONT_STYLE "regular"

#define MARGIN 12

#define REPEAT false

#define DEFAULT_TITLE_FONT_STRING "Arial:bold:size=14"
#define DEFAULT_MESSAGE_FONT_STRING "Arial:regular:size=12"


char *get_font_string(const char *font_name, uint font_size, const char *font_style)
{
    char *fstring = "%s:style=%s:size=%u";
    int size = snprintf(NULL, 0, fstring, font_name, font_style, font_size);
    char *string = malloc(size + 1);
    assert(string);
    snprintf(string, size + 1, fstring, font_name, font_style, font_size);
    return string;
}


int main(int argc, char **argv) {

    const char *title = TITLE;
    const char *message = MESSAGE;
    int timer_duration = TIMER_DURATION;
    int break_duration = BREAK_DURATION;
    const char *font_color = FONT_COLOR;
    const char *font_name = FONT_NAME;
    int title_font_size = TITLE_FONT_SIZE;
    const char *title_font_style = TITLE_FONT_STYLE;
    int message_font_size = MESSAGE_FONT_SIZE;
    const char *message_font_style = MESSAGE_FONT_STYLE;
    int margin = MARGIN;
    bool repeat = REPEAT;

    while (true)
    {
        sleep(timer_duration);

        Display *display = XOpenDisplay(NULL);
        if (!display) {
            fprintf(stderr, "Can't open display\n");
            return 1;
        }

        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);

        unsigned int width  = DisplayWidth(display, screen);
        unsigned int height = DisplayHeight(display, screen);

        // Initialize dpi
        char *xft_dpi = XGetDefault(display, "Xft", "dpi");
        double dpi;

        if (xft_dpi) {
            dpi = atof(xft_dpi);
        } else {
            dpi = 96.0;
        }

        // Create fullscreen override-redirect window
        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = BlackPixel(display, screen);

        Window window = XCreateWindow(
            display,
            root,
            0, 0, width, height,
            0,
            DefaultDepth(display, screen),
            InputOutput,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel,
            &attrs
        );

        XMapWindow(display, window);
        XFlush(display);

        // Try to grab pointer and keyboard
        XGrabPointer(display, window, True,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync,
                    None, None, CurrentTime);

        XGrabKeyboard(display, window,
                    True,
                    GrabModeAsync, GrabModeAsync,
                    CurrentTime);

        /*
        // Alternative bitmap title_font title

        // Create GC for drawing text
        GC gc = XCreateGC(display, window, 0, NULL);
        XSetForeground(display, gc, WhitePixel(display, screen));

        // Load a basic title_font
        XFontStruct *title_font = XLoadQueryFont(display, "fixed");
        if (title_font) {
            XSetFont(display, gc, title_font->fid);
        }

        // Draw text centered-ish
        int text_x = width / 2 - (XTextWidth(title_font, title, strlen(title)) / 2);
        int text_y = height / 2;

        XDrawString(display, window, gc, text_x, text_y, title, strlen(title));
        XFlush(display);

        */

        // Create draw context
        XftDraw *xftDraw = XftDrawCreate(display, window,
                                        DefaultVisual(display, screen),
                                        DefaultColormap(display, screen));

        // Load font_color
        XftColor xftColor;
        // XRenderColor xrColor = {.red = 65535, .green = 0, .blue = 0, .alpha = 65535};
        // XftColorAllocValue(display, DefaultVisual(display, screen), DefaultColormap(display, screen), &xrColor, &xftColor);
        XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), font_color, &xftColor);

        // Load title font
        char *title_font_string = get_font_string(font_name, title_font_size, title_font_style);
        XftFont *title_font = XftFontOpenName(display, screen, title_font_string);
        assert(title_font);

        // Load message font
        char *message_font_string = get_font_string(font_name, message_font_size, message_font_style);
        XftFont *message_font = XftFontOpenName(display, screen, message_font_string);
        assert(message_font);

        // Print title
        const char *title_text = title;
        const char *message_text = message;

        XGlyphInfo title_extents;
        XftTextExtentsUtf8(display, title_font, (XftChar8 *)title_text, strlen(title_text), &title_extents);
        XGlyphInfo message_extents;
        XftTextExtentsUtf8(display, message_font, (XftChar8 *)message_text, strlen(message_text), &message_extents);

        int pixel_margin = margin * dpi / 72.0;

        int title_text_x = (width - title_extents.width) / 2;
        int title_text_y = (height - title_extents.height - message_extents.height - pixel_margin) / 2 + title_extents.height - title_extents.y;
        int message_text_x = (width - message_extents.width) / 2;
        int message_text_y = (height - title_extents.height - message_extents.height - pixel_margin) / 2 + title_extents.height + message_extents.height + pixel_margin - message_extents.y;

        XftDrawStringUtf8(xftDraw, &xftColor, title_font, title_text_x, title_text_y, (XftChar8 *)title_text, strlen(title_text));
        XftDrawStringUtf8(xftDraw, &xftColor, message_font, message_text_x, message_text_y, (XftChar8 *)message_text, strlen(message_text));

        // Update display
        XFlush(display);

        // Hold for break duration
        sleep(break_duration);

        // Cleanup + release
        XUngrabKeyboard(display, CurrentTime);
        XUngrabPointer(display, CurrentTime);

        XDestroyWindow(display, window);
        XCloseDisplay(display);

        if (!repeat)
         return 0;
    }

    return 0;
}