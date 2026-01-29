# xrest

Simple configurable break timer for Xserver that reminds you to rest.

![screenshot](https://0x405.ru/xrest/screenshot_a.png) ![screenshot](https://0x405.ru/xrest/screenshot_b.png)

## Build

Install dependencies (example for Ubuntu/Debian):

```bash
sudo apt update
sudo apt install libx11-dev libxft-dev libxss-dev
```

Build application:

```bash
gcc main.c timer.c -o xrest -lX11 -lXft -lXss -I/usr/include/freetype2 -lm -lao
chmod +x xrest
```

## Install

Create application folder and move everything there:

```bash
sudo mkdir -p /opt/xrest
sudo cp -r xrest sounds /opt/xrest/
```

Create a symlink to be able to run the app:

```bash
sudo ln -s /opt/xrest/xrest /usr/local/bin/xrest
```

## Configure

Put your config in `$XDG_CONFIG_HOME/xrest/config.ini`

Example config with defaults:

```ini
# Text on the break screen
break_title_text = "Break time!"
break_message_text = "Rest your eyes. Stretch your legs. Breathe. Relax."
break_hint_text = "s - stop, q - quit"

# Text on the warning screen
warning_message_text = "Please, take a break!"
warning_hint_text = "space - start, w - snooze, s - skip, q - quit"

# Text on the end screen
end_title_text = "Break has ended!"
end_message_text = "Work fruitfully. Concentrate on important. Don't get distracted."
end_hint_text = "press any key to continue..."

# Enable warning before break
warning_enabled = true
# Allow break skip
skip_enabled = true
# Allow break snooze
snooze_enabled = true
# Allow break interruption
stop_enabled = true
# Enable break end screen
end_enabled = true
# Enable hints
hints_enabled = true
# Enable time output
time_enabled = true
# Enable sound
sound_enabled = true
# Block all input on break (excluding break application)
block_input = false

# Time is specified in such maner: XXh YYm ZZs
# Time between breaks
timer_duration = 28m
# Duration of a break
break_duration = 5m
# Duration before break starts
warning_duration = 1m
# Snooze duration
snooze_duration = 3m

# Restart timer on end
repeat = true

# Skip break on idle
detect_idle = true
# Idle time limit to skip break
idle_limit = 5m

# Color specifications in #rrggbb format
font_color = #ffffff
hint_font_color = #aaaaaa
background_font_color = #222222
background_color = #000000
progress_color = #161616
border_color = #333333

# Font name, example: JetBrainsMono Nerd Font
font_name = "monospace"

# Title font specification
title_font_size = 14
title_font_weight = 300
title_font_slant = 0
title_font_style = "regular"

# Message and Warning font specification
message_font_size = 12
message_font_weight = 200
message_font_slant = 0
message_font_style = "regular"

# Hint font specification
hint_font_size = 10
hint_font_weight = 100
hint_font_slant = 100
hint_font_style = "regular"

# Time font specification
time_font_size = 128
time_font_weight = 300
time_font_slant = 0
time_font_style = "regular"

# Warning window width in pt
warning_width = 320
# Warning window height in pt
warning_height = 96
# Warning window border width
border_width = 0
# Unused
progress_weight = 16
# Margin between Title and Message
margin = 12

# Screen update limit
fps = 60

# Only WAV is currently supported
# Break start sound
start_sound_path = "/opt/xrest/sounds/start.wav"
# Break end sound
end_sound_path = "/opt/xrest/sounds/end.wav"
# Sound volume from 0.0 to 1.0
volume = 0.8
```

Legally, this project is licensed under the MIT License – see LICENSE file for details.

Morally, the Author opposes the intentional use of the provided code or software to harm any person, whether to their body or soul. See the STATEMENT file for details.
