typedef struct 
{
    char title[128]; // Break Message Title
    char message[256]; // Break Message
    char warning[256]; // Warning Message
    char warning_hint[256]; // Warning Hint
    char end_title[128]; // End Screen Title
    char end_message[256]; // End Screen Message
    time_t timer_duration; // Time before/between Breaks
    time_t break_duration; // Duration of Breaks
    time_t warning_duration; // Duration of Warning Screen if enabled
    time_t snooze_duration; // Duration between Warnings
    char font_color[16]; // Main foreground color
    char background_color[16]; // Backgroung color
    char font_name[128]; // Main font name
    int title_font_size;
    int title_font_slant;
    int title_font_weight;
    char title_font_style[64];
    int message_font_size;
    int message_font_slant;
    int message_font_weight;
    char message_font_style[64];
    int hint_font_size;
    int hint_font_slant;
    int hint_font_weight;
    char hint_font_style[64];
    int margin;
    bool repeat;
    char progress_color[16];
    int progress_weight;
    int fps;
    char start_sound_path[512];
    char end_sound_path[512];
    float volume;
    uint warning_width;
    uint warning_height;
    uint border_width;
    char border_color[16];
} Config;


typedef struct wctx
{
    Window window;
    uint width;
    uint height;
    Pixmap draw_buffer;
    XftDraw *draw_context;
    GC graphics_context;
} WindowContext;


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
