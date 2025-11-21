#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/keysym.h>
#include <pthread.h>
#include <ao/ao.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include "main.h"

/*
    TODO:
    - Disable warning
    - Disable end screen
    - End colors
    - Global commands with breakc
    - Managed / unmanaged?
    - Notification?
    - Enable autostart
    - Enable autoend
    - Tray icon
*/

bool debug = false;

Config config;

Display *display;
int screen;
double dpi;
int depth;
Window root;
XVisualInfo vinfo;
Visual *visual;
Colormap colormap;

uint screen_width;
uint screen_height;

XftFont *title_font;
XftFont *message_font;
XftFont *warning_font;
XftFont *hint_font;

XftColor font_color;
XColor background_color;
XColor border_color;

Window last_focus;
int revert_to;

static void load_defaults(Config *config)
{
    strcpy(config->title, "Take a break!");
    strcpy(config->message, "Rest your eyes. Stretch your legs. Breathe. Relax.");
    strcpy(config->warning, "Break starts in %ds.");
    strcpy(config->warning_hint, "space - start, w - snooze, s - skip, q - quit");
    strcpy(config->end_title, "Break has ended!");
    strcpy(config->end_message, "Press any key to continue...");

    config->timer_duration = 28 * 60;
    config->break_duration = 5 * 60;
    config->warning_duration = 60;
    config->snooze_duration = 60;
    config->repeat = true;

    strcpy(config->font_color, "#ffffff");
    strcpy(config->background_color, "#000000");
    config->warning_width = 320; // pt
    config->warning_height = 96; // pt
    config->border_width = 0; // px
    strcpy(config->border_color, "#333333");
    config->margin = 12;
    strcpy(config->font_name, "monospace");

    config->title_font_size = 14;
    config->title_font_weight = 300;
    config->title_font_slant = 0;
    strcpy(config->title_font_style, "regular");

    config->message_font_size = 12;
    config->message_font_weight = 200;
    config->message_font_slant = 0;
    strcpy(config->message_font_style, "regular");

    config->hint_font_size = 10;
    config->hint_font_weight = 100;
    config->hint_font_slant = 100;
    strcpy(config->hint_font_style, "regular");

    strcpy(config->start_sound_path, "start.wav");
    strcpy(config->end_sound_path, "end.wav");
    config->volume = 0.8;
}


static void load_dev(Config *config)
{
    config->timer_duration = 1;
    config->break_duration = 3;
    config->warning_duration = 3;
    config->snooze_duration = 3;
}


static void get_config_path(char *buffer, size_t length)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg)
        snprintf(buffer, length, "%s/break/config", xdg);
    else if (home)
        snprintf(buffer, length, "%s/.config/break/config", home);
    else
        snprintf(buffer, length, "config");
}


void trim(char* str) 
{
    char* start = str;
    while (isspace((unsigned char)*start)) start++;
    memmove(str, start, strlen(start) + 1);

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
}


int parse_duration(const char *str) 
{
    int total = 0;
    int value = 0;

    while (*str) {
        if (isdigit((unsigned char)*str)) 
        {
            value = value * 10 + (*str - '0');
        } 
        else 
        {
            if (*str == 'h') 
            {
                total += value * 3600;
                value = 0;
            } else if (*str == 'm') 
            {
                total += value * 60;
                value = 0;
            } else if (*str == 's') 
            {
                total += value;
                value = 0;
            }
        }
        str++;
    }

    return total;
}


unsigned int parse_color(const char *str) 
{
    if (*str == '#')
        str++;  // skip '#'

    return (unsigned int)strtoul(str, NULL, 16);
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
        
        #define SET_FLOAT(field) \
            if (strcmp(key, #field) == 0) \
                config->field = atof(value)

        #define SET_STRING(field) \
            if (strcmp(key, #field) == 0) \
            { \
                strncpy(config->field, value, sizeof(config->field) - 1); \
                config->field[sizeof(config->field) - 1] = '\0'; \
            }
        
        #define SET_BOOL(field) \
            if (strcmp(key, #field) == 0) \
                config->field = strcmp(value, "true") == 0;
        
        #define SET_DURATION(field) \
            if (strcmp(key, #field) == 0) \
                config->field = parse_duration(value);
        
        #define SET_COLOR(field) \
            if (strcmp(key, #field) == 0) \
                config->field = parse_color(value);

        SET_STRING(title);
        SET_STRING(message);
        SET_STRING(warning);
        SET_STRING(warning_hint);
        SET_STRING(end_title);
        SET_STRING(end_message);

        SET_DURATION(timer_duration);
        SET_DURATION(break_duration);
        SET_DURATION(warning_duration);
        SET_DURATION(snooze_duration);
        SET_BOOL(repeat);

        SET_STRING(font_color);
        SET_STRING(background_color);
        SET_INT(border_width);
        SET_STRING(border_color);
        SET_INT(warning_width);
        SET_INT(warning_height);
        SET_INT(margin);
        SET_STRING(font_name);

        SET_INT(title_font_size);
        SET_INT(title_font_slant);
        SET_INT(title_font_weight);
        SET_STRING(title_font_style);

        SET_INT(message_font_size);
        SET_INT(message_font_slant);
        SET_INT(message_font_weight);
        SET_STRING(message_font_style);

        SET_INT(hint_font_size);
        SET_INT(hint_font_slant);
        SET_INT(hint_font_weight);
        SET_STRING(hint_font_style);

        SET_STRING(start_sound_path);
        SET_STRING(end_sound_path);
        SET_FLOAT(volume);
    }
    fclose(f);
}


char *get_font_string(const char *font_name, uint font_size, const char *font_style, uint font_weight, uint font_slant)
{
    char *fstring = "%s:style=%s:size=%u:weight=%d:slant=%d";
    int size = snprintf(NULL, 0, fstring, font_name, font_style, font_size, font_weight, font_slant);
    char *string = malloc(size + 1);
    assert(string);
    snprintf(string, size + 1, fstring, font_name, font_style, font_size, font_weight, font_slant);
    return string;
}


static void apply_volume(char *buf, size_t bytes, int bits, float volume)
{
    if (volume == 1.0f) return;

    if (bits == 8) 
    {
        // 8-bit PCM is unsigned
        for (size_t i = 0; i < bytes; i++) 
        {
            int s = (unsigned char)buf[i] - 128;
            s = (int)(s * volume);
            if (s > 127) s = 127;
            if (s < -128) s = -128;
            buf[i] = (char)(s + 128);
        }
    }
    else if (bits == 16) 
    {
        // 16-bit PCM is signed little-endian
        int16_t *p = (int16_t*)buf;
        size_t samples = bytes / 2;
        for (size_t i = 0; i < samples; i++) 
        {
            int v = (int)(p[i] * volume);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            p[i] = (int16_t)v;
        }
    }
}


int play_wav(const char *path, float volume)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    WavHeader h;
    if (fread(&h, sizeof(h), 1, f) != 1) 
    {
        fclose(f);
        return -1;
    }

    if (memcmp(h.riff, "RIFF", 4) ||
        memcmp(h.wave, "WAVE", 4) ||
        memcmp(h.fmt,  "fmt ", 4) ||
        h.audio_format != 1) // must be PCM
    {
        fclose(f);
        return -1;
    }

    // Skip extra fmt bytes if present
    if (h.fmt_len > 16) 
    {
        fseek(f, h.fmt_len - 16, SEEK_CUR);
    }

    // Locate the "data" chunk
    char tag[4];
    uint32_t chunk_size = 0;

    while (fread(tag, 1, 4, f) == 4) 
    {
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
    if (!dev) 
    {
        ao_shutdown();
        fclose(f);
        return -1;
    }

    // Streaming loop
    const size_t buf_size = 4096;
    char *buffer = malloc(buf_size);
    if (!buffer) 
    {
        ao_close(dev);
        ao_shutdown();
        fclose(f);
        return -1;
    }

    while (remaining > 0) 
    {
        size_t to_read = remaining < buf_size ? remaining : buf_size;
        size_t read = fread(buffer, 1, to_read, f);
        if (read == 0) break;

        // Trim to full frames (block-aligned)
        read -= read % h.block_align;
        if (read == 0) break;

        // Apply volume
        apply_volume(buffer, read, h.bits_per_sample, volume);

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


typedef struct 
{
    char *path;
    float volume;
} PlayJob;


static void *play_thread(void *arg)
{
    PlayJob *job = (PlayJob*)arg;
    play_wav(job->path, job->volume);
    free(job->path);
    free(job);
    return NULL;
}


int play_wav_async(const char *path, float volume)
{
    PlayJob *job = malloc(sizeof(PlayJob));
    job->path = strdup(path);
    job->volume = volume;

    pthread_t t;
    pthread_create(&t, NULL, play_thread, job);
    pthread_detach(t);

    return 0;
}


double pt_to_px(double pt, double dpi)
{
    return pt * dpi / 72.0;
}


static void init()
{
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Can't open display\n");
        exit(1);
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    depth = DefaultDepth(display, screen);
    visual = DefaultVisual(display, screen);
    colormap = DefaultColormap(display, screen);

    // if (XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) {
    //     depth = 32;
    //     visual = vinfo.visual;
    //     colormap = XCreateColormap(display, root, visual, AllocNone);
    // }

    screen_width  = DisplayWidth(display, screen);
    screen_height = DisplayHeight(display, screen);

    // Initialize dpi
    char *xft_dpi = XGetDefault(display, "Xft", "dpi");

    if (xft_dpi)
        dpi = atof(xft_dpi);
    else
        dpi = 96.0;

    // Load font_color
    XftColorAllocName(display, visual, colormap, config.font_color, &font_color);

    // Load title font
    char *title_font_string = get_font_string(
        config.font_name, 
        config.title_font_size, 
        config.title_font_style, 
        config.title_font_weight, 
        config.title_font_slant);
    title_font = XftFontOpenName(display, screen, title_font_string);
    assert(title_font);

    // Load message font
    char *message_font_string = get_font_string(
        config.font_name, 
        config.message_font_size, 
        config.message_font_style, 
        config.message_font_weight,
        config.message_font_slant);
    message_font = XftFontOpenName(display, screen, message_font_string);
    assert(message_font);

    warning_font = message_font;

    // Load hint font
    char *hint_font_string = get_font_string(
        config.font_name, 
        config.hint_font_size, 
        config.hint_font_style, 
        config.hint_font_weight,
        config.hint_font_slant);
    hint_font = XftFontOpenName(display, screen, hint_font_string);
    assert(hint_font);

    // Load background color
    if (XParseColor(display, colormap, config.background_color, &background_color)) 
        if (!XAllocColor(display, colormap, &background_color))
            exit(1);

    // Load border color
    if (XParseColor(display, colormap, config.border_color, &border_color)) 
        if (!XAllocColor(display, colormap, &border_color))
            exit(1);

    // Remember current focus
    XGetInputFocus(display, &last_focus, &revert_to);
}


static void spawn_window(uint width, uint height, int x, int y, int border, XColor *background_color, bool override_redirect, WindowContext *wctx)
{
    XSetWindowAttributes attrs;
    attrs.override_redirect = override_redirect;
    attrs.background_pixel = background_color->pixel;
    // attrs.background_pixel = 0;
    attrs.colormap = colormap;

    // Create Warning window
    Window window = XCreateWindow(
        display, root,
        x, y, width, height, border,
        depth,
        InputOutput,
        visual,
        CWColormap | CWOverrideRedirect | CWBackPixel,
        &attrs
    );

    XMapWindow(display, window);
    XFlush(display);

    XftDraw *draw_context = XftDrawCreate(display, window, visual, colormap);

    wctx->window = window;
    wctx->width = width;
    wctx->height = height;
    wctx->draw_context = draw_context;
}


static void draw_warning(char *warning_text, char *hint_text, uint time, WindowContext *wctx)
{
    // Clear window
    GC gctx = XCreateGC(display, wctx->window, 0, NULL);
    XFillRectangle(display, wctx->window, gctx, 0, 0, wctx->width, wctx->height);

    char text[256];
    sprintf(text, warning_text, time);

    // Calculate text extents
    XGlyphInfo warning_extents;
    XftTextExtentsUtf8(display, warning_font, (XftChar8 *)text, strlen(text), &warning_extents);
    XGlyphInfo hint_extents;
    XftTextExtentsUtf8(display, hint_font, (XftChar8 *)hint_text, strlen(hint_text), &hint_extents);

    // Draw Warning Text
    int warning_text_x = (wctx->width - warning_extents.width) / 2;
    int warning_text_y = (wctx->height - warning_extents.height) / 2 + warning_extents.height - (warning_extents.height - warning_extents.y);
    XftDrawStringUtf8(wctx->draw_context, &font_color, warning_font, warning_text_x, warning_text_y, (XftChar8 *)text, strlen(text));

    int hint_text_x = (wctx->width - hint_extents.width) / 2;
    int hint_text_y = wctx->height - hint_extents.height;
    XftDrawStringUtf8(wctx->draw_context, &font_color, hint_font, hint_text_x, hint_text_y, (XftChar8 *)hint_text, strlen(hint_text));
    
    // Update display
    XFlush(display);
}


static void draw_message(char *title_text, char *message_text, WindowContext *wctx)
{
    // Clear window
    GC gctx = XCreateGC(display, wctx->window, 0, NULL);
    XFillRectangle(display, wctx->window, gctx, 0, 0, wctx->width, wctx->height);

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

    int pixel_margin = pt_to_px(config.margin, dpi);
    int message_heigth = message_lines_count * message_extents.height + (message_lines_count - 1) * message_extents.height;

    // Draw Title
    int title_text_x = (wctx->width - title_extents.width) / 2;
    int title_text_y = (wctx->height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height - title_extents.y;

    XftDrawStringUtf8(wctx->draw_context, &font_color, title_font, title_text_x, title_text_y, (XftChar8 *)title_text, strlen(title_text));
    
    // Draw Message line by line
    const char *message_line = strtok(message_text, "\n");
    for (int i = 0; message_line; i++)
    {
        XGlyphInfo message_line_extents;
        XftTextExtentsUtf8(display, message_font, (XftChar8 *)message_line, strlen(message_line), &message_line_extents);

        int message_line_x = (wctx->width - message_line_extents.width) / 2;
        int message_start_y = (wctx->height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height + pixel_margin;
        int message_line_y = message_start_y + message_extents.height + message_extents.height * i * 1.5 - message_extents.y;

        XftDrawStringUtf8(wctx->draw_context, &font_color, message_font, message_line_x, message_line_y, (XftChar8 *)message_line, strlen(message_line));
        message_line = strtok(NULL, "\n");
    }
    // Update display
    XFlush(display);
}


int event_wait(Display *display, XEvent *event, int timeout)
{
    int fd = ConnectionNumber(display);

    fd_set in_fds;
    FD_ZERO(&in_fds);
    FD_SET(fd, &in_fds);

    struct timeval tv;
    tv.tv_sec  = timeout;
    tv.tv_usec = 0;

    // Wait until X data is available or timeout
    int ret = select(fd + 1, &in_fds, NULL, NULL, &tv);

    if (ret > 0) {
        // We have X events pending
        XNextEvent(display, event);
        return 1;
    } else if (ret == 0) {
        // Timeout
        return 0;
    } else {
        // Error
        perror("select");
        return -1;
    }
}


int main(int argc, char **argv) 
{
    load_defaults(&config);
    load_config(&config);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = true;
            load_dev(&config);
            printf("Debug mode enabled!\n");
        }
    }

    do {
        sleep(config.timer_duration);

        warn:

        init();

        // Spawn Warning
        WindowContext warning_wctx;

        uint warning_width = pt_to_px(config.warning_width, dpi);
        uint warning_height = pt_to_px(config.warning_height, dpi);

        int warning_x = (screen_width  - warning_width) / 2;
        int warning_y = (screen_height - warning_height) / 2;

        spawn_window(warning_width, warning_height, warning_x, warning_y, config.border_width, &background_color, true, &warning_wctx);

        // Manage window focus
        XRaiseWindow(display, warning_wctx.window);
        XSetInputFocus(display, warning_wctx.window, RevertToNone, CurrentTime);      
        XSetWindowBorder(display, warning_wctx.window, border_color.pixel);
        XFlush(display);  

        // XGrabKeyboard(display, warning_wctx.window,
        //             True,
        //             GrabModeAsync, GrabModeAsync,
        //             CurrentTime);

        // Listen for keypresses
        XSelectInput(display, warning_wctx.window, KeyPressMask | ButtonPressMask |ExposureMask);

        double warning_time = config.warning_duration;

        // Draw Warning message
        // draw_warning(config.warning, config.warning_hint, warning_time, &warning_wctx);

        XEvent event;
        time_t time_start = time(NULL);
        time_t time_left = warning_time - (time(NULL) - time_start);

        while (true) 
        {
            time_left = warning_time - (time(NULL) - time_start);
            if (time_left < 0)
                goto start_break;
            draw_warning(config.warning, config.warning_hint, time_left, &warning_wctx);

            if (event_wait(display, &event, 1) > 0)
            {
                switch (event.type)
                {
                    case ButtonPress:
                    {
                        XSetInputFocus(display, warning_wctx.window, RevertToPointerRoot, CurrentTime);
                        break;
                    }
                    case KeyPress:
                    {
                        KeySym key = XLookupKeysym(&event.xkey, 0);
                        switch (key)
                        {
                            case XK_space: 
                            {
                                printf("Breaking...\n");
                                goto start_break;
                                break;
                            }
                            case XK_w: // Snooze
                            {
                                printf("Snoozing...\n");
                                // XUngrabKeyboard(display, CurrentTime);
                                // XUngrabPointer(display, CurrentTime);
                                XDestroyWindow(display, warning_wctx.window);
                                XSetInputFocus(display, last_focus, RevertToNone, CurrentTime);
                                XFlush(display);

                                sleep(config.snooze_duration);
                                goto warn;
                                break;
                            }
                            case XK_s: // Skip
                            {
                                printf("Skipping...\n");
                                goto skip_break;
                                break;
                            }
                            case XK_q: // Quit
                            {
                                printf("Quitting...\n");
                                XUngrabKeyboard(display, CurrentTime);
                                XUngrabPointer(display, CurrentTime);
                                XDestroyWindow(display, warning_wctx.window);
                                XSetInputFocus(display, last_focus, RevertToNone, CurrentTime);
                                XCloseDisplay(display);
                                return 0;
                            }
                        }
                        break;
                    }
                }
            }
        }
        start_break:

        // Spawn Break window
        WindowContext break_wctx;
        spawn_window(screen_width, screen_height, 0, 0, 0, &background_color, true, &break_wctx);

        XDestroyWindow(display, warning_wctx.window);
        XFlush(display);

        // Try to grab pointer and keyboard
        XGrabPointer(display, break_wctx.window, True,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync,
                    None, None, CurrentTime);

        XGrabKeyboard(display, break_wctx.window,
                    True,
                    GrabModeAsync, GrabModeAsync,
                    CurrentTime);
        
        // Draw break message
        draw_message(config.title, config.message, &break_wctx);

        // Play sound
        play_wav_async(config.start_sound_path, config.volume);

        // Hold for break duration
        sleep(config.break_duration);

        // Draw end message
        draw_message(config.end_title, config.end_message, &break_wctx);

        // Play sound
        play_wav_async(config.end_sound_path, config.volume);

        // Listen for keypresses
        XSelectInput(display, break_wctx.window, KeyPressMask | ExposureMask);

        while (true) 
        {
            XNextEvent(display, &event);
            if (event.type == KeyPress) 
            {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                break;
            }
        }

        XUngrabKeyboard(display, CurrentTime);
        XUngrabPointer(display, CurrentTime);
        XDestroyWindow(display, break_wctx.window);
        skip_break:
        XSetInputFocus(display, last_focus, RevertToNone, CurrentTime);
        XCloseDisplay(display);

    } while (config.repeat);

    return 0;
}