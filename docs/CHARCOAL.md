# Default terminal colors

The library's empty screen and the iLua table use the terminal owner's configured
foreground and background. IC_DEFAULT_COLOR (8) emits SGR39/49; indexed ANSI
colors 0–7 remain available to explicit consumers. The demo distinguishes the
selected row with bold text, the existing selected-label footer and cursor.
It no longer paints every cell black or fills selection with cyan.

This preserves a configured charcoal terminal without changing its palette,
forcing RGB support, repainting the shell globally or assuming index0 is black
on every terminal. C owns encoding and bounds; iLua still owns presentation.
Core tests independently decode SGR39/49 and refuse colors outside the declared
range. Policy tests require terminal-default foreground/background on all rows.

Exact-source hosted ICK/iLua and Android runtime qualification remains pending.
This is terminal palette inheritance, not evidence of Android APK theming or
physical MIRO C67 execution.
