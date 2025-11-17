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

/*
    TODO:
    - Warning before break
    - Snooze
    - Time output
    - Disable warning
    - Disable end screen
    - End colors
    - Return focus
*/


typedef struct config 
{
    char title[128]; // Break Message Title
    char message[256]; // Break Message
    char warning[256]; // Warning Message
    char end_title[128]; // End Screen Title
    char end_message[256]; // End Screen Message
    int timer_duration; // Time before/between Breaks
    int break_duration; // Duration of Breaks
    int warning_duration; // Duration of Warning Screen if enabled
    char font_color[16]; // Main foreground color
    char background_color[16]; // Backgroung color
    char font_name[128]; // 
    int title_font_size;
    char title_font_style[64];
    int message_font_size;
    char message_font_style[64];
    int margin;
    bool repeat;
    char start_sound_path[512];
    char end_sound_path[512];
    float volume;
    uint warning_width;
    uint warning_height;
} Config;


typedef struct context
{
    Config *config;
    Display *display;
    double dpi;
    uint window_width;
    uint window_height;
    XftDraw *draw_context;
    XftFont *title_font;
    XftFont *message_font;
    XftFont *warning_font;
    XftColor *font_color;
} Context;


#pragma pack(push, 1)
typedef struct 
{
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
    strcpy(config->warning, "You need a break...");
    strcpy(config->end_title, "Break has ended!");
    strcpy(config->end_message, "Press any button to continue...");
    config->timer_duration = 1;
    config->break_duration = 3;
    config->warning_duration = 3;
    strcpy(config->font_color, "#ffffff");
    strcpy(config->background_color, "#000000");
    strcpy(config->font_name, "monospace");
    config->title_font_size = 14;
    strcpy(config->title_font_style, "bold");
    config->message_font_size = 12;
    strcpy(config->message_font_style, "regular");
    config->margin = 12;
    config->repeat = false;
    strcpy(config->start_sound_path, "start.wav");
    strcpy(config->end_sound_path, "end.wav");
    config->volume = 0.4;
    config->warning_width = 256; // pt
    config->warning_height = 128; // pt
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


static void apply_volume(char *buf, size_t bytes, int bits, float volume)
{
    if (volume == 1.0f) return;

    if (bits == 8) {
        // 8-bit PCM is unsigned
        for (size_t i = 0; i < bytes; i++) {
            int s = (unsigned char)buf[i] - 128;
            s = (int)(s * volume);
            if (s > 127) s = 127;
            if (s < -128) s = -128;
            buf[i] = (char)(s + 128);
        }
    }
    else if (bits == 16) {
        // 16-bit PCM is signed little-endian
        int16_t *p = (int16_t*)buf;
        size_t samples = bytes / 2;
        for (size_t i = 0; i < samples; i++) {
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
    if (fread(&h, sizeof(h), 1, f) != 1) {
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


static void draw_warning(char *warning_text, uint time, Context *context)
{
    Display *display = context->display;
    double dpi = context->dpi;
    uint window_width = context->window_width;
    uint window_height = context->window_height;
    XftDraw *draw_context = context->draw_context;
    XftFont *warning_font = context->warning_font;
    XftColor *font_color = context->font_color;

    // Calculate text extents
    XGlyphInfo warning_extents;
    XftTextExtentsUtf8(display, warning_font, (XftChar8 *)warning_text, strlen(warning_text), &warning_extents);

    // Draw Warning Text
    int warning_text_x = (window_width - warning_extents.width) / 2;
    int warning_text_y = (window_height - warning_extents.height) / 2 + warning_extents.height - (warning_extents.height - warning_extents.y);

    XftDrawStringUtf8(draw_context, font_color, warning_font, warning_text_x, warning_text_y, (XftChar8 *)warning_text, strlen(warning_text));
    
    // Update display
    XFlush(display);
}


static void draw_message(char *title_text, char *message_text, Context *context)
{
    Display *display = context->display;
    double dpi = context->dpi;
    uint window_width = context->window_width;
    uint window_height = context->window_height;
    XftDraw *draw_context = context->draw_context;
    XftFont *title_font = context->title_font;
    XftFont *message_font = context->message_font;
    XftColor *font_color = context->font_color;
    uint margin = context->config->margin;

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

    int pixel_margin = pt_to_px(margin, dpi);
    int message_heigth = message_lines_count * message_extents.height + (message_lines_count - 1) * message_extents.height;

    // Draw Title
    int title_text_x = (window_width - title_extents.width) / 2;
    int title_text_y = (window_height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height - title_extents.y;

    XftDrawStringUtf8(draw_context, font_color, title_font, title_text_x, title_text_y, (XftChar8 *)title_text, strlen(title_text));
    
    // Draw Message line by line
    const char *message_line = strtok(message_text, "\n");
    for (int i = 0; message_line; i++)
    {
        XGlyphInfo message_line_extents;
        XftTextExtentsUtf8(display, message_font, (XftChar8 *)message_line, strlen(message_line), &message_line_extents);

        int message_line_x = (window_width - message_line_extents.width) / 2;
        int message_start_y = (window_height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height + pixel_margin;
        int message_line_y = message_start_y + message_extents.height + message_extents.height * i * 1.5 - message_extents.y;

        XftDrawStringUtf8(draw_context, font_color, message_font, message_line_x, message_line_y, (XftChar8 *)message_line, strlen(message_line));
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

        unsigned int screen_width  = DisplayWidth(display, screen);
        unsigned int screen_height = DisplayHeight(display, screen);

        // Initialize dpi
        double dpi = 96.0;
        char *xft_dpi = XGetDefault(display, "Xft", "dpi");

        if (xft_dpi)
            dpi = atof(xft_dpi);

        // Load font_color
        XftColor font_color;
        XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), config.font_color, &font_color);

        // Load title font
        char *title_font_string = get_font_string(config.font_name, config.title_font_size, config.title_font_style);
        XftFont *title_font = XftFontOpenName(display, screen, title_font_string);
        assert(title_font);

        // Load message font
        char *message_font_string = get_font_string(config.font_name, config.message_font_size, config.message_font_style);
        XftFont *message_font = XftFontOpenName(display, screen, message_font_string);
        assert(message_font);

        // Load background color
        Colormap colormap = DefaultColormap(display, screen);
        XColor background_color;

        // Remember current focus
        Window previous_focus;
        int revert_to;

        XGetInputFocus(display, &previous_focus, &revert_to);

        // Draw Warning
        uint warning_width = pt_to_px(config.warning_width, dpi);
        uint warning_height = pt_to_px(config.warning_height, dpi);

        int x = (screen_width  - warning_width) / 2;
        int y = (screen_height - warning_height) / 2;

        XSetWindowAttributes warning_attrs;
        warning_attrs.override_redirect = false;
        warning_attrs.background_pixel  = BlackPixel(display, screen);

        if (XParseColor(display, colormap, config.background_color, &background_color)) 
            if (XAllocColor(display, colormap, &background_color)) 
                warning_attrs.background_pixel = background_color.pixel;

        // Create Warning window
        Window warning_window = XCreateWindow(
            display, RootWindow(display, screen),
            x, y, warning_width, warning_height, 0,
            DefaultDepth(display, screen),
            InputOutput,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel,
            &warning_attrs
        );

        XMapWindow(display, warning_window);
        XFlush(display);

        // Create draw context
        XftDraw *warning_draw_context = XftDrawCreate(display, warning_window,
                                        DefaultVisual(display, screen),
                                        DefaultColormap(display, screen));

        context.config = &config;
        context.display = display;
        context.dpi = dpi;
        context.font_color = &font_color;

        context.draw_context = warning_draw_context;
        context.warning_font = message_font;
        context.window_width = warning_width;
        context.window_height = warning_height;
        
        // Draw Warning message
        draw_warning(config.warning, 3, &context);

        XRaiseWindow(display, warning_window);
        XSetInputFocus(display, warning_window, RevertToNone, CurrentTime);        

        sleep(config.warning_duration);

        // Create fullscreen override-redirect window
        XSetWindowAttributes attrs;
        attrs.override_redirect = true;
        attrs.background_pixel = BlackPixel(display, screen);

        if (XParseColor(display, colormap, config.background_color, &background_color)) 
            if (XAllocColor(display, colormap, &background_color)) 
                attrs.background_pixel = background_color.pixel;

        // Create window
        Window window = XCreateWindow(
            display,
            root,
            0, 0, screen_width, screen_height,
            0,
            DefaultDepth(display, screen),
            InputOutput,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel,
            &attrs
        );

        XMapWindow(display, window);
        XDestroyWindow(display, warning_window);
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
        XftDraw *message_draw_context = XftDrawCreate(display, window,
                                        DefaultVisual(display, screen),
                                        DefaultColormap(display, screen));

        context.window_width = screen_width;
        context.window_height = screen_height;
        context.draw_context = message_draw_context;
        context.message_font = message_font;
        context.title_font = title_font;
        
        // Draw break message
        draw_message(config.title, config.message, &context);

        // Play sound
        play_wav_async(config.start_sound_path, config.volume);

        // Hold for break duration
        sleep(config.break_duration);

        GC graphics_context = XCreateGC(display, window, 0, NULL);
        XFillRectangle(display, window, graphics_context, 0, 0, screen_width, screen_height);

        // Draw end message
        draw_message(config.end_title, config.end_message, &context);

        // Play sound
        play_wav_async(config.end_sound_path, config.volume);

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
        // Restore focus
        XSetInputFocus(display, previous_focus, RevertToNone, CurrentTime);

        XCloseDisplay(display);

    } while (config.repeat);

    return 0;
}