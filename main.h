typedef struct cfg
{
    char break_title_text[128]; // Break Message Title
    char break_message_text[256]; // Break Message
    char break_hint_text[256]; // Break Hint
    char warning_message_text[256]; // Warning Message
    char warning_hint_text[256]; // Warning Hint
    char end_title_text[128]; // End Screen Title
    char end_message_text[256]; // End Screen Message

    bool warning_enabled;
    bool skip_enabled;
    bool snooze_enabled;
    bool stop_enabled;
    bool end_enabled;

    time_t timer_duration; // Time before/between Breaks
    time_t break_duration; // Duration of Breaks
    time_t warning_duration; // Duration of Warning Screen if enabled
    time_t snooze_duration; // Duration between Warnings

    char font_color[16]; // Main foreground color
    char background_color[16]; // Backgroung color
    char progress_color[16];

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


typedef struct gctx
{
    Config config;
    bool debug;
    WindowContext wctx;

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
    XColor progress_color;

    Window last_focus;
    int revert_to;

    double frame_time;
    double progress;
} GlobalContext;


typedef enum {
    STATE_NONE,
    STATE_WAIT,
    STATE_WARNING,
    STATE_BREAK,
    STATE_END,
    STATE_EXIT,
    STATE_TIMEOUT
} GlobalState;


typedef struct {
    void (*on_frame)(GlobalContext *gctx, double elapsed, double duration, void *userdata);
    GlobalState (*on_event)(GlobalContext *gctx, XEvent *event, void *userdata);
    GlobalState (*on_exit)(GlobalContext *gctx, GlobalState state, void *userdata);
    double duration;   // <= 0 means infinite
} FrameEventLoop;


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
