# Break

Use the following command to build:
```
gcc main.c -o main -lX11 -lXft -I/usr/include/freetype2 -lm -lao
```

Put your config in `$XDG_CONFIG_HOME/break/config`
Example and defaults:
```
title = Take a break!
message = Rest your eyes. Stretch your legs. Breathe. Relax.
warning = Break starts in %ds.
warning_hint = space - start, w - snooze, s - skip, q - quit
end_title = Break has ended!
end_message = Press any key to continue...

timer_duration = 28m
break_duration = 5m
warning_duration = 1m
snooze_duration = 1m
repeat = true

font_color = #ffffff
background_color = #000000
border_width = 0
unsigned long border_color = #333333
warning_width = 96
warning_height = 0
margin = 12
font_name = monospace

title_font_size = 14
title_font_slant = 300
title_font_weight = 0
title_font_style = regular

message_font_size = 12
message_font_slant = 200
message_font_weight = 0
message_font_style = regular

hint_font_size = 10
hint_font_slant = 100
hint_font_weight = 100
hint_font_style = regular

start_sound_path = ~/.config/break/start.wav
end_sound_path = ~/.config/break/end.wav
volume = 0.8
```