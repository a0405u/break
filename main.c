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
#include <poll.h>
#include <stdbool.h>

#include "main.h"
#include "timer.h"

/*
    Release:
    X Bug: Wrong last focus, needs update on each warning / break
    X Bug: Break without warning
    X Bug: Exit without end
    X Bug: Repeat
    - README: Custom license
    - README: Build requirements
    - README: List of Features

    To Do:
    - Bug: No end sound without end screen
    - Config: Separate font for time
    - Config: Disable time output
    - Config: End colors
    - Config: Disable idle detection
    - Config: Disable sound
    - Feature: Detect Idle time
    - Feature: Time left on warning
    - Feature: Hard stop by solving puzzle / pressing long combination / writing a sentence
    - Feature: Break time output
    - Feature: Global commands with breakc
    - Feature: System notification instead of warning?
    - Feature: Tray icon
    - Feature: Quit from end screen
    - Refactor: Separate audio.c, audio.h
    - Refactor: Function names, 
    - Refactor: Classes?
    - Managed / unmanaged?
*/

static void load_defaults(Config *config)
{
    strcpy(config->break_title_text, "Break time!");
    strcpy(config->break_message_text, "Rest your eyes. Stretch your legs. Breathe. Relax.");
    strcpy(config->break_hint_text, "s - stop, q - quit");

    strcpy(config->warning_message_text, "Please, take a break!");
    strcpy(config->warning_hint_text, "space - start, w - snooze, s - skip, q - quit");

    strcpy(config->end_title_text, "Break has ended!");
    strcpy(config->end_message_text, "Work fruitfully. Concentrate on important. Don't get distracted.");
    strcpy(config->end_hint_text, "press any key to continue...");

    config->warning_enabled = true;
    config->skip_enabled = true;
    config->snooze_enabled = true;
    config->stop_enabled = true;
    config->end_enabled = true;
    config->hints_enabled = true;
    config->time_enabled = true;
    config->sound_enabled = true;
    config->block_input = false;

    config->timer_duration = 28 * 60;
    config->break_duration = 5 * 60;
    config->warning_duration = 60;
    config->snooze_duration = 60;

    config->repeat = true;

    strcpy(config->font_color, "#ffffff");
    strcpy(config->hint_font_color, "#aaaaaa");
    strcpy(config->background_font_color, "#222222");
    strcpy(config->background_color, "#000000");
    strcpy(config->progress_color, "#161616");
    strcpy(config->border_color, "#333333");

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

    config->time_font_size = 128;
    config->time_font_weight = 300;
    config->time_font_slant = 0;
    strcpy(config->time_font_style, "regular");

    config->warning_width = 320; // pt
    config->warning_height = 96; // pt
    config->border_width = 0; // px
    config->progress_weight = 16;
    config->margin = 12;

    config->fps = 60;

    strcpy(config->start_sound_path, "sounds/start.wav");
    strcpy(config->end_sound_path, "sounds/end.wav");
    config->volume = 0.8;
}


static void load_dev(Config *config)
{
    config->timer_duration = 1;
    // config->break_duration = 3;
    // config->warning_duration = 3;
    config->snooze_duration = 3;
    // config->warning_enabled = false;
    // config->end_enabled = false;
    // config->repeat = false;
}


static void get_config_path(char *buffer, size_t length)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg)
        snprintf(buffer, length, "%s/break/config.ini", xdg);
    else if (home)
        snprintf(buffer, length, "%s/.config/break/config.ini", home);
    else
        snprintf(buffer, length, "config.ini");
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


static void parse_string(char *dst, size_t dst_size, const char *src)
{
    // Skip leading whitespace
    while (isspace((unsigned char)*src))
        src++;

    size_t i = 0;

    if (*src == '"') {
        // Quoted string
        src++; // skip opening quote
        while (*src && *src != '"' && i + 1 < dst_size) {
            dst[i++] = *src++;
        }
    } else {
        // Single-word string
        while (*src && !isspace((unsigned char)*src) && i + 1 < dst_size) {
            dst[i++] = *src++;
        }
    }

    dst[i] = '\0';
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
                config->field = atoi(value);
        
        #define SET_FLOAT(field) \
            if (strcmp(key, #field) == 0) \
                config->field = atof(value);

        #define SET_STRING(field) \
            if (strcmp(key, #field) == 0) \
                parse_string(config->field, sizeof(config->field), value);
        
        #define SET_BOOL(field) \
            if (strcmp(key, #field) == 0) \
                config->field = strcmp(value, "true") == 0;
        
        #define SET_DURATION(field) \
            if (strcmp(key, #field) == 0) \
                config->field = parse_duration(value);
        
        #define SET_COLOR(field) \
            if (strcmp(key, #field) == 0) \
                config->field = parse_color(value);

        SET_STRING(break_title_text);
        SET_STRING(break_message_text);
        SET_STRING(break_hint_text);

        SET_STRING(warning_message_text);
        SET_STRING(warning_hint_text);

        SET_STRING(end_title_text);
        SET_STRING(end_message_text);
        SET_STRING(end_hint_text);

        SET_BOOL(warning_enabled);
        SET_BOOL(skip_enabled);
        SET_BOOL(snooze_enabled);
        SET_BOOL(stop_enabled);
        SET_BOOL(end_enabled);
        SET_BOOL(hints_enabled);
        SET_BOOL(time_enabled);
        SET_BOOL(sound_enabled);
        SET_BOOL(block_input);

        SET_DURATION(timer_duration);
        SET_DURATION(break_duration);
        SET_DURATION(warning_duration);
        SET_DURATION(snooze_duration);

        SET_BOOL(repeat);

        SET_STRING(font_color);
        SET_STRING(hint_font_color);
        SET_STRING(background_font_color);
        SET_STRING(background_color);
        SET_STRING(progress_color);
        SET_STRING(border_color);

        SET_STRING(font_name);

        SET_INT(title_font_size);
        SET_INT(title_font_weight);
        SET_INT(title_font_slant);
        SET_STRING(title_font_style);

        SET_INT(message_font_size);
        SET_INT(message_font_weight);
        SET_INT(message_font_slant);
        SET_STRING(message_font_style);

        SET_INT(hint_font_size);
        SET_INT(hint_font_weight);
        SET_INT(hint_font_slant);
        SET_STRING(hint_font_style);

        SET_INT(time_font_size);
        SET_INT(time_font_weight);
        SET_INT(time_font_slant);
        SET_STRING(time_font_style);

        SET_INT(warning_width);
        SET_INT(warning_height);
        SET_INT(border_width);
        SET_INT(progress_weight);
        SET_INT(margin);

        SET_INT(fps);

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


static void die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}


static void load_color(GlobalContext *gctx, const char *name, XColor *out)
{
    if (!XParseColor(gctx->display, gctx->colormap, name, out) ||
        !XAllocColor(gctx->display, gctx->colormap, out))
    {
        die("Failed to load color!\n");
    }
}


static void load_xft_color(GlobalContext *gctx, const char *name, XftColor *out)
{
    if (!XftColorAllocName(gctx->display, gctx->visual, gctx->colormap, name, out))
    {
        die("Failed to load xft color!\n");
    }
}


static XftFont *load_xft_font(GlobalContext *gctx, const char *family, int size, char *style, int weight, int slant)
{
    char *font_str = get_font_string(family, size, style, weight, slant);
    XftFont *font = XftFontOpenName(gctx->display, gctx->screen, font_str);

    if (!font)
        die("Failed to load xft font!");

    return font;
}


static void init(GlobalContext *gctx)
{
    gctx->display = XOpenDisplay(NULL);
    if (!gctx->display)
        die("Can't open display\n");

    gctx->screen = DefaultScreen(gctx->display);
    gctx->root   = RootWindow(gctx->display, gctx->screen);
    gctx->depth  = DefaultDepth(gctx->display, gctx->screen);
    gctx->visual = DefaultVisual(gctx->display, gctx->screen);
    gctx->colormap = DefaultColormap(gctx->display, gctx->screen);

    gctx->screen_width  = DisplayWidth(gctx->display, gctx->screen);
    gctx->screen_height = DisplayHeight(gctx->display, gctx->screen);

    gctx->frame_time = 1.0d / gctx->config.fps;

    /*
    // Possible transparency
    if (XMatchVisualInfo(gctx->display, gctx->screen, 32, TrueColor, &gctx->vinfo)) 
    {
        gctx->depth = 32;
        gctx->visual = gctx->vinfo.visual;
        gctx->colormap = XCreateColormap(
            gctx->display, gctx->root, gctx->visual, AllocNone
        );
    }
    */

    /* --- DPI --- */
    const char *xft_dpi = XGetDefault(gctx->display, "Xft", "dpi");
    gctx->dpi = xft_dpi ? atof(xft_dpi) : 96.0;

    /* --- COLORS --- */
    load_xft_color(gctx, gctx->config.font_color, &gctx->font_color);
    load_xft_color(gctx, gctx->config.hint_font_color, &gctx->hint_font_color);
    load_xft_color(gctx, gctx->config.background_font_color, &gctx->background_font_color);
    load_color(gctx, gctx->config.background_color, &gctx->background_color);
    load_color(gctx, gctx->config.border_color, &gctx->border_color);
    load_color(gctx, gctx->config.progress_color, &gctx->progress_color); 

    /* ---- FONTS ---- */
    gctx->title_font = load_xft_font(gctx, gctx->config.font_name, gctx->config.title_font_size, gctx->config.title_font_style, gctx->config.title_font_weight, gctx->config.title_font_slant);

    gctx->message_font = load_xft_font(gctx, gctx->config.font_name, gctx->config.message_font_size, gctx->config.message_font_style, gctx->config.message_font_weight, gctx->config.message_font_slant);

    gctx->warning_font = gctx->message_font;

    gctx->hint_font = load_xft_font(gctx, gctx->config.font_name, gctx->config.hint_font_size, gctx->config.hint_font_style, gctx->config.hint_font_weight, gctx->config.hint_font_slant);

    gctx->time_font = load_xft_font(gctx, gctx->config.font_name, gctx->config.time_font_size, gctx->config.time_font_style, gctx->config.time_font_weight, gctx->config.time_font_slant);

    /* ---- FOCUS ---- */
    XGetInputFocus(gctx->display, &gctx->last_focus, &gctx->revert_to);
}


static WindowContext spawn_window(GlobalContext *gctx, uint width, uint height, int x, int y, int border, XColor *background_color, bool override_redirect)
{
    WindowContext wctx;

    XSetWindowAttributes attrs;
    attrs.override_redirect = override_redirect;
    attrs.background_pixel = background_color->pixel;
    // attrs.background_pixel = 0;
    attrs.colormap = gctx->colormap;
    
    // Create Warning window
    wctx.window = XCreateWindow(gctx->display, gctx->root, x, y, width, height, border, gctx->depth, InputOutput, gctx->visual, CWColormap | CWOverrideRedirect | CWBackPixel, &attrs);

    XMapWindow(gctx->display, wctx.window);
    XSync(gctx->display, False);

    Pixmap draw_buffer = XCreatePixmap(gctx->display, wctx.window, width, height, gctx->depth);
    XftDraw *draw_context = XftDrawCreate(gctx->display, draw_buffer, gctx->visual, gctx->colormap);
    GC graphics_context = XCreateGC(gctx->display, wctx.window, 0, NULL);

    wctx.width = width;
    wctx.height = height;
    wctx.draw_buffer = draw_buffer;
    wctx.draw_context = draw_context;
    wctx.graphics_context = graphics_context;

    return wctx;
}


static void resize_window(GlobalContext *gctx, WindowContext *wctx, uint width, uint height, int x, int y)
{
    XMoveResizeWindow(gctx->display, wctx->window, x, y, width, height);

    Pixmap draw_buffer = XCreatePixmap(gctx->display, wctx->window, width, height, gctx->depth);
    XftDraw *draw_context = XftDrawCreate(gctx->display, draw_buffer, gctx->visual, gctx->colormap);
    GC graphics_context = XCreateGC(gctx->display, wctx->window, 0, NULL);

    wctx->width = width;
    wctx->height = height;
    wctx->draw_buffer = draw_buffer;
    wctx->draw_context = draw_context;
    wctx->graphics_context = graphics_context;
}


static void draw_warning(GlobalContext *gctx, char *warning_text, char *hint_text, uint time, WindowContext *wctx)
{
    char text[256];
    sprintf(text, warning_text, time);

    // Calculate text extents

    XGlyphInfo warning_extents;
    XftTextExtentsUtf8(gctx->display, gctx->warning_font, (XftChar8 *)text, strlen(text), &warning_extents);

    // Draw Warning Text

    int warning_text_x = (wctx->width - warning_extents.width) / 2;
    int warning_text_y = (wctx->height - warning_extents.height) / 2 + warning_extents.height - (warning_extents.height - warning_extents.y);
    XftDrawStringUtf8(wctx->draw_context, &gctx->font_color, gctx->warning_font, warning_text_x, warning_text_y, (XftChar8 *)text, strlen(text));

    // Draw Hint

    if (hint_text && gctx->config.hints_enabled)
    {
        XGlyphInfo hint_extents;
        XftTextExtentsUtf8(gctx->display, gctx->hint_font, (XftChar8 *)hint_text, strlen(hint_text), &hint_extents);

        int hint_text_x = (wctx->width - hint_extents.width) / 2;
        int hint_text_y = wctx->height - hint_extents.height;
        XftDrawStringUtf8(wctx->draw_context, &gctx->hint_font_color, gctx->hint_font, hint_text_x, hint_text_y, (XftChar8 *)hint_text, strlen(hint_text));
    }
    
    // Update display
    XCopyArea(gctx->display, wctx->draw_buffer, wctx->window, wctx->graphics_context, 0, 0, wctx->width, wctx->height, 0, 0);
    XFlush(gctx->display);
}


static void clear_window(GlobalContext *gctx, WindowContext *wctx, XColor color)
{
    XSetForeground(gctx->display, wctx->graphics_context, color.pixel);
    XFillRectangle(gctx->display, wctx->draw_buffer, wctx->graphics_context, 0, 0, wctx->width, wctx->height);
}


static void draw_progress(GlobalContext *gctx, WindowContext *wctx, double progress)
{
    XSetForeground(gctx->display, wctx->graphics_context, gctx->progress_color.pixel);

    // int progress_max_width = 840;
    int progress_max_width = wctx->width;
    int progress_width = progress_max_width * progress;
    // int progress_height = pt_to_px(config.progress_weight, dpi);
    int progress_height = wctx->height;
    int progress_x = (wctx->width - progress_max_width) / 2;
    // int progress_y = title_text_y + title_extents.y;
    // int progress_y = wctx->height - progress_height;
    int progress_y = 0;
    XFillRectangle(gctx->display, wctx->draw_buffer, wctx->graphics_context, progress_x, progress_y, progress_width, progress_height);

}


void format_time(uint32_t seconds, char *out, size_t out_size)
{
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;

    if (h > 0) {
        // hh:mm:ss
        snprintf(out, out_size, "%02u:%02u:%02u", h, m, s);
    } else {
        // mm:ss
        snprintf(out, out_size, "%02u:%02u", m, s);
    }
}


static void draw_message(GlobalContext *gctx, char *title_text, char *message_text, char *hint_text, uint time, WindowContext *wctx)
{
    // Draw time left
    
    if (gctx->config.time_enabled)
    {
        char time_text[256];
        format_time(time, time_text, 256);

        XGlyphInfo time_extents;
        XftTextExtentsUtf8(gctx->display, gctx->time_font, (XftChar8 *)time_text, strlen(time_text), &time_extents);

        int time_text_x = (wctx->width - time_extents.xOff) / 2;
        int time_text_y = (wctx->height - time_extents.yOff) / 3;
        XftDrawStringUtf8(wctx->draw_context, &gctx->background_font_color, gctx->time_font, time_text_x, time_text_y, (XftChar8 *)time_text, strlen(time_text));
    }

    // Calculate text extents

    XGlyphInfo title_extents;
    XftTextExtentsUtf8(gctx->display, gctx->title_font, (XftChar8 *)title_text, strlen(title_text), &title_extents);
    XGlyphInfo message_extents;
    XftTextExtentsUtf8(gctx->display, gctx->message_font, (XftChar8 *)message_text, strlen(message_text), &message_extents);

    // Count Message lines and calculate multiline heigth

    int message_lines_count = 0;

    const char *c = message_text;
    while (c = strchr(c, '\n')) 
    {
        message_lines_count++;
        c++; // Move past the found newline
    }

    int pixel_margin = pt_to_px(gctx->config.margin, gctx->dpi);
    int message_heigth = message_lines_count * message_extents.height + (message_lines_count - 1) * message_extents.height;

    // Draw Title

    int title_text_x = (wctx->width - title_extents.width) / 2;
    int title_text_y = (wctx->height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height - title_extents.y;

    XftDrawStringUtf8(wctx->draw_context, &gctx->font_color, gctx->title_font, title_text_x, title_text_y, (XftChar8 *)title_text, strlen(title_text));

    // Draw Message line by line

    const char *message_line = strtok(message_text, "\n");
    for (int i = 0; message_line; i++)
    {
        XGlyphInfo message_line_extents;
        XftTextExtentsUtf8(gctx->display, gctx->message_font, (XftChar8 *)message_line, strlen(message_line), &message_line_extents);

        int message_line_x = (wctx->width - message_line_extents.width) / 2;
        int message_start_y = (wctx->height - title_extents.height - message_heigth - pixel_margin) / 2 + title_extents.height + pixel_margin;
        int message_line_y = message_start_y + message_extents.height + message_extents.height * i * 1.5 - message_extents.y;

        XftDrawStringUtf8(wctx->draw_context, &gctx->font_color, gctx->message_font, message_line_x, message_line_y, (XftChar8 *)message_line, strlen(message_line));
        message_line = strtok(NULL, "\n");
    }

    // Draw Hint

    if (hint_text && gctx->config.hints_enabled)
    {
        XGlyphInfo hint_extents;
        XftTextExtentsUtf8(gctx->display, gctx->hint_font, (XftChar8 *)hint_text, strlen(hint_text), &hint_extents);

        int hint_text_x = (wctx->width - hint_extents.width) / 2;
        int hint_text_y = wctx->height - hint_extents.height;
        XftDrawStringUtf8(wctx->draw_context, &gctx->hint_font_color, gctx->hint_font, hint_text_x, hint_text_y, (XftChar8 *)hint_text, strlen(hint_text));
    }
    
    // Update display
    XCopyArea(gctx->display, wctx->draw_buffer, wctx->window, wctx->graphics_context, 0, 0, wctx->width, wctx->height, 0, 0);
    XFlush(gctx->display);
}

int event_wait(Display *display, XEvent *event, double timeout_sec)
{
    /* If events are already queued, return immediately */
    if (XPending(display)) 
    {
        XNextEvent(display, event);
        return 1;
    }

    int fd = ConnectionNumber(display);

    struct pollfd pfd = {.fd = fd, .events = POLLIN};

    int timeout_ms;

    if (timeout_sec < 0.0)
        timeout_ms = -1;            /* wait forever */
    else
        timeout_ms = (int)(timeout_sec * 1000);

    int ret = poll(&pfd, 1, timeout_ms);

    if (ret > 0) 
    {
        if (pfd.revents & POLLIN) 
        {
            XNextEvent(display, event);
            return 1;
        }
        return -1; /* unexpected event */
    } 
    else if (ret == 0) 
    {
        return 0;  /* timeout */
    } 
    else 
    {
        perror("poll");
        return -1;
    }
}


static void print_usage(const char *prog)
{
    printf(
        "Usage: %s [options]\n"
        "\nOptions:\n"
        "  -d, --debug        Enable debug mode\n"
        "  -h, --help         Show this help and exit\n",
        prog
    );
}


static void parse_args(int argc, char **argv, GlobalContext *gctx)
{
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0)
        {
            gctx->debug = true;
            load_dev(&gctx->config);
            printf("Debug mode enabled!\n");
            continue;
        }

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            exit(0);
        }

        fprintf(stderr, "Unknown option: %s\n", argv[i]);
        print_usage(argv[0]);
        exit(1);
    }
}


static void set_input_focus(GlobalContext *gctx, Window window)
{
    Window last_focus;
    XGetInputFocus(gctx->display, &last_focus, &gctx->revert_to);
    if (last_focus != gctx->wctx.window)
        gctx->last_focus = last_focus;
    XSetInputFocus(gctx->display, window, RevertToNone, CurrentTime);      
}


GlobalState run_frame_event_loop(GlobalContext *gctx, FrameEventLoop *loop, void *userdata)
{
    XEvent event;
    Timer timer = {0};
    timer_start(&timer);
    GlobalState state = STATE_NONE;

    double next_frame = 0;

    while (loop->duration <= 0 || timer_elapsed(&timer) < loop->duration) {
        double elapsed = timer_elapsed(&timer);
        double wait_time = next_frame - elapsed;
        if (wait_time < 0) wait_time = 0;

        int r = event_wait(gctx->display, &event, wait_time);
        if (r == -1)
            die("Failed input!\n");

        if (r == 0) {
            loop->on_frame(gctx, elapsed, loop->duration, userdata);
            next_frame += gctx->frame_time;
            continue;
        }

        state = loop->on_event(gctx, &event, userdata);
        if (state != STATE_NONE) {
            if (loop->on_exit) 
                return loop->on_exit(gctx, state, userdata);
            return state;
        }
    }
    
    state = STATE_TIMEOUT;
    if (loop->on_exit) 
        return loop->on_exit(gctx, state, userdata);
    return state;
}


static GlobalState process_wait(GlobalContext *gctx)
{
    printf("Waiting...\n");

    sleep(gctx->config.timer_duration);
    
    if (gctx->config.warning_enabled)
        return STATE_WARNING;

    return STATE_BREAK;
}


static void warning_on_frame(GlobalContext *gctx, double elapsed, double duration, void *ud)
{
    double time_left = duration - elapsed;
    double progress = time_left / duration;

    clear_window(gctx, &gctx->wctx, gctx->background_color);
    draw_progress(gctx, &gctx->wctx, progress);
    draw_warning(gctx, gctx->config.warning_message_text, gctx->config.warning_hint_text, (int)time_left, &gctx->wctx);
}


static GlobalState warning_on_event(GlobalContext *gctx, XEvent *event, void *ud)
{
    (void)ud;

    if (event->type == ButtonPress)
    {
        set_input_focus(gctx, gctx->wctx.window);
    }
    else if (event->type == KeyPress)
    {
        KeySym key = XLookupKeysym(&event->xkey, 0);
        switch (key)
        {
            case XK_space: 
                return STATE_BREAK;
            case XK_w: // Snooze
                if (gctx->config.snooze_enabled)
                    return STATE_SNOOZE;
                break;
            case XK_s: // Skip
                if (gctx->config.skip_enabled)
                    return STATE_RESTART;
                break;
            case XK_q: // Quit
                return STATE_EXIT;
        }
    }
    return STATE_NONE;
}


static GlobalState warning_on_exit(GlobalContext *gctx, GlobalState state, void *ud)
{
    switch (state)
    {
        case STATE_BREAK: 
        case STATE_TIMEOUT:
            return STATE_BREAK;
        case STATE_SNOOZE: // Snooze
            return state;
        case STATE_RESTART: // Skip
            return state;
    }
    return STATE_EXIT;
}


static GlobalState process_warning(GlobalContext *gctx)
{
    uint warning_width = pt_to_px(gctx->config.warning_width, gctx->dpi);
    uint warning_height = pt_to_px(gctx->config.warning_height, gctx->dpi);

    int warning_x = (gctx->screen_width  - warning_width) / 2;
    int warning_y = (gctx->screen_height - warning_height) / 2;

    gctx->wctx = spawn_window(gctx, warning_width, warning_height, warning_x, warning_y, gctx->config.border_width, &gctx->background_color, true);

    // Manage window focus
    XRaiseWindow(gctx->display, gctx->wctx.window);
    set_input_focus(gctx, gctx->wctx.window);
    XSetWindowBorder(gctx->display, gctx->wctx.window, gctx->border_color.pixel);
    XFlush(gctx->display);  

    // Listen for keypresses
    // XGrabKeyboard(gctx->display, gctx->wctx.window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XSelectInput(gctx->display, gctx->wctx.window, KeyPressMask | ButtonPressMask);

    FrameEventLoop loop = {
        .on_frame = warning_on_frame,
        .on_event = warning_on_event,
        .on_exit  = warning_on_exit,
        .duration = gctx->config.warning_duration
    };

    return run_frame_event_loop(gctx, &loop, NULL);
}

static void break_on_frame(GlobalContext *gctx, double elapsed, double duration, void *ud)
{
    double time_left = duration - elapsed;
    double progress = elapsed / duration;

    clear_window(gctx, &gctx->wctx, gctx->background_color);
    draw_progress(gctx, &gctx->wctx, progress);
    draw_message(gctx, gctx->config.break_title_text, gctx->config.break_message_text, gctx->config.break_hint_text, time_left, &gctx->wctx);
}


static GlobalState break_on_event(GlobalContext *gctx, XEvent *event, void *ud)
{
    (void)ud;

    if (event->type == ButtonPress)
    {
        set_input_focus(gctx, gctx->wctx.window);
    }
    else if (event->type == KeyPress)
    {
        KeySym key = XLookupKeysym(&event->xkey, 0);
        switch (key)
        {
            case XK_s: // Skip
                if (gctx->config.stop_enabled)
                    return STATE_RESTART;
                break;
            case XK_q: // Quit
                if (gctx->config.stop_enabled)
                    return STATE_EXIT;
                break;
        }
    }
    return STATE_NONE;
}


static GlobalState break_on_exit(GlobalContext *gctx, GlobalState state, void *ud)
{
    (void)ud;

    switch (state)
    {
        case STATE_END:
        case STATE_TIMEOUT:
        {
            // Break ended, go to End Screen
            if (gctx->config.end_enabled)
                return STATE_END;
            // Break ended, restart without End Screen
            if (gctx->config.repeat)
                return STATE_RESTART;
            return STATE_EXIT;
        }
        // Skip Break and restart
        case STATE_RESTART:
        {
            return state;
        }
    }
    return STATE_EXIT;
}


static GlobalState process_break(GlobalContext *gctx)
{
    printf("Starting break...\n");

    if (gctx->config.warning_enabled)
        resize_window(gctx, &gctx->wctx, gctx->screen_width, gctx->screen_height, 0, 0);
    else
        gctx->wctx = spawn_window(gctx, gctx->screen_width, gctx->screen_height, 0, 0, 0, &gctx->background_color, true);
    set_input_focus(gctx, gctx->wctx.window);
    XRaiseWindow(gctx->display, gctx->wctx.window);
    XFlush(gctx->display);

    if (gctx->config.block_input)
    {
        // Try to grab pointer and keyboard
        XGrabPointer(gctx->display, gctx->wctx.window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        XGrabKeyboard(gctx->display, gctx->wctx.window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    }
    
    double progress = 0;

    // Draw break message
    clear_window(gctx, &gctx->wctx, gctx->background_color);
    draw_progress(gctx, &gctx->wctx, progress);
    draw_message(gctx, gctx->config.break_title_text, gctx->config.break_message_text, gctx->config.break_hint_text, gctx->config.break_duration, &gctx->wctx);

    // Play sound
    play_wav_async(gctx->config.start_sound_path, gctx->config.volume);


    // Listen for keypresses
    // XGrabKeyboard(gctx->display, gctx->wctx.window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XSelectInput(gctx->display, gctx->wctx.window, KeyPressMask | ButtonPressMask | ExposureMask | StructureNotifyMask);

    FrameEventLoop loop = {
        .on_frame = break_on_frame,
        .on_event = break_on_event,
        .on_exit  = break_on_exit,
        .duration = gctx->config.break_duration
    };

    return run_frame_event_loop(gctx, &loop, NULL);
}


static GlobalState process_end(GlobalContext *gctx)
{
    printf("Ending break...\n");

    // Draw end message
    clear_window(gctx, &gctx->wctx, gctx->background_color);
    draw_progress(gctx, &gctx->wctx, 1.0);
    draw_message(gctx, gctx->config.end_title_text, gctx->config.end_message_text, gctx->config.end_hint_text, 0, &gctx->wctx);

    // Play sound
    play_wav_async(gctx->config.end_sound_path, gctx->config.volume);

    // Listen for keypresses
    XSelectInput(gctx->display, gctx->wctx.window, KeyPressMask | ExposureMask);

    XEvent event;

    while (true) 
    {
        XNextEvent(gctx->display, &event);
        if (event.type == KeyPress) 
        {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            break;
        }
    }

    if(gctx->config.repeat)
        return STATE_RESTART;

    return STATE_EXIT;
}


static GlobalState process_snooze(GlobalContext *gctx)
{
    printf("Snoozing...\n");
    XDestroyWindow(gctx->display, gctx->wctx.window);
    XSetInputFocus(gctx->display, gctx->last_focus, RevertToNone, CurrentTime);
    XFlush(gctx->display);
    sleep(gctx->config.snooze_duration);
        
    if (gctx->config.warning_enabled)
        return STATE_WARNING;

    return STATE_BREAK;
}


static GlobalState process_restart(GlobalContext *gctx)
{
    printf("Restarting break...\n");

    if (gctx->config.block_input)
    {
        XUngrabKeyboard(gctx->display, CurrentTime);
        XUngrabPointer(gctx->display, CurrentTime);
    }
    XDestroyWindow(gctx->display, gctx->wctx.window);

    XSetInputFocus(gctx->display, gctx->last_focus, RevertToNone, CurrentTime);
    XFlush(gctx->display);

    return STATE_WAIT;
}

static GlobalState process_exit(GlobalContext *gctx)
{
    printf("Quitting...\n");

    if (gctx->config.block_input)
    {
        XUngrabKeyboard(gctx->display, CurrentTime);
        XUngrabPointer(gctx->display, CurrentTime);
    }
    XDestroyWindow(gctx->display, gctx->wctx.window);

    XSetInputFocus(gctx->display, gctx->last_focus, RevertToNone, CurrentTime);
    XCloseDisplay(gctx->display);

    return STATE_EXIT;
}


int main(int argc, char **argv) 
{
    GlobalContext gctx = {0};

    load_defaults(&gctx.config);
    load_config(&gctx.config);
    parse_args(argc, argv, &gctx);

    init(&gctx);

    GlobalState state = STATE_WAIT;

    while (state != STATE_EXIT)
    {
        switch (state)
        {
            case STATE_WAIT: 
                state = process_wait(&gctx);
                break;
            case STATE_WARNING:
                state = process_warning(&gctx);
                break;
            case STATE_SNOOZE:
                state = process_snooze(&gctx);
                break;
            case STATE_BREAK:
                state = process_break(&gctx);
                break;
            case STATE_RESTART:
                state = process_restart(&gctx);
                break;
            case STATE_END:
                state = process_end(&gctx);
                break;
        }
    }
    process_exit(&gctx);
    return 0;
}
