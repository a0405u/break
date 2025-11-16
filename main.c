#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/keysym.h>
#include <ao/ao.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

/*
    TODO:
    - Sound
    - Thread for sound
    - Time output
    - Warning
*/


typedef struct config 
{
    char title[128];
    char message[256];
    char end_title[128];
    char end_message[256];
    int timer_duration;
    int break_duration;
    char font_color[16];
    char background_color[16];
    char font_name[128];
    int title_font_size;
    char title_font_style[64];
    int message_font_size;
    char message_font_style[64];
    int margin;
    bool repeat;
} Config;


typedef struct context
{
    Config *config;
    Display *display;
    double dpi;
    uint width;
    uint height;
    XftDraw *xftDraw;
    XftFont *title_font;
    XftFont *message_font;
    XftColor *font_color;
} Context;


#pragma pack(push, 1)
typedef struct {
    char     riff[4];
    uint32_t size;
    char     wave[4];
    char     fmt[4];
    uint32_t fmt_len;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} WavHeader;
#pragma pack(pop)


static void load_defaults(Config *config)
{
    strcpy(config->title, "Take a break!");
    strcpy(config->message, "Rest your eyes. Stretch your legs. Breathe. Relax.");
    strcpy(config->end_title, "Break has ended!");
    strcpy(config->end_message, "Press any button to continue...");
    config->timer_duration = 1;
    config->break_duration = 3;
    strcpy(config->font_color, "#ffffff");
    strcpy(config->background_color, "#000000");
    strcpy(config->font_name, "monospace");
    config->title_font_size = 14;
    strcpy(config->title_font_style, "bold");
    config->message_font_size = 12;
    strcpy(config->message_font_style, "regular");
    config->margin = 12;
    config->repeat = false;
}


static void get_config_path(char *buffer, size_t length)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg)
        snprintf(buffer, length, "%s/xbreak/config", xdg);
    else if (home)
        snprintf(buffer, length, "%s/.config/xbreak/config", home);
    else
        snprintf(buffer, length, "config");
}


void trim(char* str) {
    char* start = str;
    while (isspace((unsigned char)*start)) start++;
    memmove(str, start, strlen(start) + 1);

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
}


static void load_config(Config *config)
{
    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f)
        return;

    char line[512];
    while (fgets(line, sizeof(line), f)) 
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
            continue;

        char* delimeter = strchr(line, '=');
        if (!delimeter) continue;

        *delimeter = '\0';
        char* key = line;
        char* value = delimeter + 1;
        trim(key); trim(value);

        #define SET_INT(field) \
            if (strcmp(key, #field) == 0) \
                config->field = atoi(value)

        #define SET_STRING(field) \
            if (strcmp(key, #field) == 0) \
            { \
                strncpy(config->field, value, sizeof(config->field) - 1); \
                config->field[sizeof(config->field) - 1] = '\0'; \
            }
        
        #define SET_BOOL(field) \
            if (strcmp(key, #field) == 0) \
                config->field = strcmp(value, "true") == 0;

        SET_STRING(title);
        SET_STRING(message);
        SET_STRING(end_title);
        SET_STRING(end_message);
        SET_INT(timer_duration);
        SET_INT(break_duration);
        SET_STRING(font_color);
        SET_STRING(background_color);
        SET_STRING(font_name);
        SET_INT(title_font_size);
        SET_STRING(title_font_style);
        SET_INT(message_font_size);
        SET_STRING(message_font_style);
        SET_INT(margin);
        SET_BOOL(repeat);
    }
    fclose(f);
}


char *get_font_string(const char *font_name, uint font_size, const char *font_style)
{
    char *fstring = "%s:style=%s:size=%u";
    int size = snprintf(NULL, 0, fstring, font_name, font_style, font_size);
    char *string = malloc(size + 1);
    assert(string);
    snprintf(string, size + 1, fstring, font_name, font_style, font_size);
    return string;
}


int play_wav(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    WavHeader h;
    if (fread(&h, sizeof(h), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    if (memcmp(h.riff, "RIFF", 4) ||
        memcmp(h.wave, "WAVE", 4) ||
        memcmp(h.fmt,  "fmt ", 4) ||
        h.audio_format != 1)            // must be PCM
    {
        fclose(f);
        return -1;
    }

    // Skip extra fmt bytes if present
    if (h.fmt_len > 16) {
        fseek(f, h.fmt_len - 16, SEEK_CUR);
    }

    // Locate the "data" chunk
    char tag[4];
    uint32_t chunk_size = 0;

    while (fread(tag, 1, 4, f) == 4) {
        fread(&chunk_size, 4, 1, f);

        if (!memcmp(tag, "data", 4)) {
            break;
        }
        // Skip unknown chunks
        fseek(f, chunk_size, SEEK_CUR);
    }

    uint32_t data_size = chunk_size;
    uint32_t remaining = data_size;

    // Initialize libao
    ao_initialize();
    int driver = ao_default_driver_id();

    ao_sample_format fmt = {
        .bits        = h.bits_per_sample,
        .channels    = h.num_channels,
        .rate        = h.sample_rate,
        .byte_format = AO_FMT_LITTLE
    };

    ao_device *dev = ao_open_live(driver, &fmt, NULL);
    if (!dev) {
        ao_shutdown();
        fclose(f);
        return -1;
    }

    // Streaming loop
    const size_t buf_size = 4096;
    char *buffer = malloc(buf_size);
    if (!buffer) {
        ao_close(dev);
        ao_shutdown();
        fclose(f);
        return -1;
    }

    while (remaining > 0) {
        size_t to_read = remaining < buf_size ? remaining : buf_size;
        size_t read = fread(buffer, 1, to_read, f);
        if (read == 0) break;

        // Trim to full frames (block-aligned)
        read -= read % h.block_align;
        if (read == 0) break;

        ao_play(dev, buffer, read);
        remaining -= read;
    }

    // Clean up
    free(buffer);
    ao_close(dev);
    ao_shutdown();
    fclose(f);

    return 0;
}



static void play_sine()
{
    ao_initialize();

    int driver = ao_default_driver_id();

    ao_sample_format format = {
        .bits = 16,
        .channels = 1,
        .rate = 44100,
        .byte_format = AO_FMT_LITTLE
    };

    ao_device *device = ao_open_live(driver, &format, NULL);
    if (!device) return;

    short buf[44100];
    for (int i = 0; i < 44100; i++) {
        buf[i] = (short)(sin(2 * M_PI * 440 * i / 44100.0) * 30000);
    }

    ao_play(device, (char*)buf, sizeof(buf));

    ao_close(device);
    ao_shutdown();
}


static void draw_message(char *title_text, char *message_text, Context *context)
{
    Config *config = context->config;
    Display *display = context->display;
    double dpi = context->dpi;
    uint width = context->width;
    uint height = context->height;
    XftDraw *xftDraw = context->xftDraw;
    XftFont *title_font = context->title_font;
    XftFont *message_font = context->message_font;
    XftColor *font_color = context->font_color;
    uint margin = config->margin;

    // Calculate text extents
    XGlyphInfo title_extents;
    XftTextExtentsUtf8(display, title_font, (XftChar8 *)title_text, strlen(title_text), &title_extents);
    XGlyphInfo message_extents;
    XftTextExtentsUtf8(display, message_font, (XftChar8 *)message_text, strlen(message_text), &message_extents);

    // Count Message lines and calculate multiline heigth
    int message_lines_count = 0;

    const char *c = message_text;
    while (c = strchr(c, '\n')) 
    {
        message_lines_count++;
        c++; // Move past the found newline
    }

    int pixel_margin = margin * dpi / 72.0;
    int message_heigth = message_lines_count * message_extents.height + (message_lines_count - 1) * message_extents.height;

    // Draw Title
    int title_text_x = (width - title_extents.width) / 2;
    int title_text_y = (height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height - title_extents.y;

    XftDrawStringUtf8(xftDraw, font_color, title_font, title_text_x, title_text_y, (XftChar8 *)title_text, strlen(title_text));
    
    // Draw Message line by line
    const char *message_line = strtok(message_text, "\n");
    for (int i = 0; message_line; i++)
    {
        XGlyphInfo message_line_extents;
        XftTextExtentsUtf8(display, message_font, (XftChar8 *)message_line, strlen(message_line), &message_line_extents);

        int message_line_x = (width - message_line_extents.width) / 2;
        int message_start_y = (height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height + pixel_margin;
        int message_line_y = message_start_y + message_extents.height + message_extents.height * i * 1.5 - message_extents.y;

        XftDrawStringUtf8(xftDraw, font_color, message_font, message_line_x, message_line_y, (XftChar8 *)message_line, strlen(message_line));
        message_line = strtok(NULL, "\n");
    }
    // Update display
    XFlush(display);
}


int main(int argc, char **argv) {

    Config config;
    Context context;

    load_defaults(&config);
    load_config(&config);

    do {
        sleep(config.timer_duration);

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
        double dpi = 96.0;
        char *xft_dpi = XGetDefault(display, "Xft", "dpi");

        if (xft_dpi)
            dpi = atof(xft_dpi);

        char *sound_path = "duck.wav";

        // Create fullscreen override-redirect window
        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = BlackPixel(display, screen);

        // Load background color
        Colormap colormap = DefaultColormap(display, screen);
        XColor color;

        if (XParseColor(display, colormap, config.background_color, &color)) 
            if (XAllocColor(display, colormap, &color)) 
                attrs.background_pixel = color.pixel;

        // Create window
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

        // Create draw context
        XftDraw *xftDraw = XftDrawCreate(display, window,
                                        DefaultVisual(display, screen),
                                        DefaultColormap(display, screen));

        // Load font_color
        XftColor xftColor;
        XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), config.font_color, &xftColor);

        // Load title font
        char *title_font_string = get_font_string(config.font_name, config.title_font_size, config.title_font_style);
        XftFont *title_font = XftFontOpenName(display, screen, title_font_string);
        assert(title_font);

        // Load message font
        char *message_font_string = get_font_string(config.font_name, config.message_font_size, config.message_font_style);
        XftFont *message_font = XftFontOpenName(display, screen, message_font_string);
        assert(message_font);

        context.config = &config;
        context.display = display;
        context.dpi = dpi;
        context.width = width;
        context.height = height;
        context.xftDraw = xftDraw;
        context.font_color = &xftColor;
        context.message_font = message_font;
        context.title_font = title_font;
        
        // Draw break message
        draw_message(config.title, config.message, &context);

        // Play sound
        play_wav(sound_path);

        // Hold for break duration
        sleep(config.break_duration);

        GC graphics_context = XCreateGC(display, window, 0, NULL);
        XFillRectangle(display, window, graphics_context, 0, 0, width, height);

        // Draw end message
        draw_message(config.end_title, config.end_message, &context);

        // Play sound
        play_wav(sound_path);

        // Listen for keypresses
        XSelectInput(display, window, KeyPressMask | ExposureMask);

        XEvent event;
        int running = 1;

        while (running) 
        {
            XNextEvent(display, &event);

            switch (event.type) 
            {
                case KeyPress: {
                    KeySym key = XLookupKeysym(&event.xkey, 0);
                    running = 0;
                    break;
                }
            }
        }

        // Cleanup + release
        XUngrabKeyboard(display, CurrentTime);
        XUngrabPointer(display, CurrentTime);

        XDestroyWindow(display, window);
        XCloseDisplay(display);

    } while (config.repeat);

    return 0;
}