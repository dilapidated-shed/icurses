#ifndef ICURSES_H
#define ICURSES_H
#include <stddef.h>
#include <stdint.h>

/* Semantic values contain no native waitable or platform ABI types. */
#define IC_ROWS 64
#define IC_COLS 200
#define IC_TEXT_BYTES 20
#define IC_OUTPUT_BYTES 1048576
enum { IC_KEY_TEXT, IC_KEY_ESCAPE, IC_KEY_UP, IC_KEY_DOWN, IC_KEY_LEFT,
       IC_KEY_RIGHT, IC_KEY_HOME, IC_KEY_END, IC_KEY_PAGE_UP, IC_KEY_PAGE_DOWN };
typedef struct { unsigned kind; uint32_t codepoint; } IcEvent;
typedef struct { unsigned char bytes[256]; size_t count; } IcReceivedBytes;
typedef struct {
    IcReceivedBytes remaining;
    IcEvent event;
    int available;
    int needs_escape_deadline;
} IcDecodedEvent;
size_t ic_receive_bytes(IcReceivedBytes *buffer, const void *bytes, size_t count);
IcDecodedEvent ic_received_bytes_to_event(IcReceivedBytes received, int escape_expired);

typedef struct {
    unsigned char text[IC_TEXT_BYTES];
    unsigned char length, columns, foreground, background, bold;
} IcCell;
typedef struct {
    int rows, columns, cursor_row, cursor_column, cursor_visible;
    IcCell cells[IC_ROWS][IC_COLS];
} IcScreen;
typedef struct { unsigned char foreground, background, bold; } IcStyle;
typedef struct { int row, first, past; } IcChangedRegion;
typedef struct { IcChangedRegion rows[IC_ROWS]; size_t count; int cursor_changed; } IcChangedRegions;
typedef struct { unsigned colors; } IcTerminalCapabilities;
typedef struct {
    unsigned char bytes[IC_OUTPUT_BYTES];
    size_t count, acknowledged;
} IcOutputBytes;
int ic_empty_screen(int rows, int columns, IcScreen *desired);
int ic_codepoint_columns(uint32_t codepoint);
int ic_text_and_placement(const void *utf8, size_t length, int row, int column,
                          IcStyle style, IcScreen *desired);
IcChangedRegions ic_previous_and_desired_to_regions(const IcScreen *previous,
                                                    const IcScreen *desired, int invalidated);
int ic_regions_and_capabilities_to_output(const IcChangedRegions *regions,
        const IcScreen *desired, IcTerminalCapabilities capabilities, IcOutputBytes *output);
/* Acknowledgement is a separate transformation; a short write cannot commit. */
int ic_acknowledged_output_to_previous(const IcOutputBytes *output,
                                      const IcScreen *target, IcScreen *previous);
#endif
