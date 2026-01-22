# Break

Simple break timer for XServer that reminds you to rest.

Use the following command to build:

```
gcc main.c timer.c -o break -lX11 -lXft -I/usr/include/freetype2 -lm -lao
```

Put your config in `$XDG_CONFIG_HOME/break/config`
Example and defaults:

```
break_title_text = Break time!
break_message_text = Rest your eyes. Stretch your legs. Breathe. Relax.
break_hint_text = s - stop, q - quit

warning_message_text = Please, take a break!
warning_hint_text = space - start, w - snooze, s - skip, q - quit

end_title_text = Break has ended!
end_message_text = Work fruitfully. Concentrate on important. Don't get distracted.
end_hint_text = press any key to continue...

warning_enabled = true # Enable warning before break
skip_enabled = true # Allow break skip
snooze_enabled = true # Allow break snooze
stop_enabled = true # Allow break interruption
end_enabled = true # Enable break end screen
hints_enabled = true # Enable hints
time_enabled = true # Enable time output

# Time is specified in such maner: XXh YYm ZZs
timer_duration = 28m # Time between breaks
break_duration = 5m # Duration of a break
warning_duration = 1m # Duration before break starts
snooze_duration = 3m # Snooze duration

repeat = true # Restart timer on end

# Color specifications in #rrggbb format
font_color = #ffffff
hint_font_color = #aaaaaa
background_font_color = #222222
background_color = #000000
progress_color = #161616
border_color = #333333

# Font name, example: JetBrainsMono Nerd Font
font_name = monospace

# Title font specification
title_font_size = 14
title_font_weight = 300
title_font_slant = 0
title_font_style = regular

# Message and Warning font specification
message_font_size = 12
message_font_weight = 200
message_font_slant = 0
message_font_style = regular

# Hint font specification
hint_font_size = 10
hint_font_weight = 100
hint_font_slant = 100
hint_font_style = regular

# Time font specification
time_font_size = 128
time_font_weight = 300
time_font_slant = 0
time_font_style = regular

warning_width = 320 # Warning window width in pt
warning_height = 96 # Warning window height in pt
border_width = 0 # Warning window border width
progress_weight = 16 # Unused
margin = 12 # Margin between Title and Message

fps = 60 # Screen update limit

start_sound_path = start.wav # Break start sound, only WAV is supported
end_sound_path = end.wav # Break end sound, only WAV is supported
volume = 0.8 # 0.0 - 1.0
```

