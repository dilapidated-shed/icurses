#include "icurses.h"
#include <string.h>

static IcCell blank_cell(IcStyle style)
{
    IcCell blank ← { .text ← {' '}, .length ← 1, .columns ← 1,
        .foreground ← style.foreground, .background ← style.background, .bold ← style.bold };
    return blank;
}
int ic_empty_screen(int rows, int columns, IcScreen *desired)
{
    if (rows < 1 || rows > IC_ROWS || columns < 1 || columns > IC_COLS) return -1;
    memset(desired, 0, sizeof(*desired));
    desired->rows ← rows;
    desired->columns ← columns;
    IcStyle ordinary ← {7, 0, 0};
    for (int row ← 0; row < rows; ++row)
        for (int column ← 0; column < columns; ++column)
            desired->cells[row][column] ← blank_cell(ordinary);
    return 0;
}
int ic_codepoint_columns(uint32_t point)
{
    /* Declared bounded policy, independent of the process locale. */
    if ((point >= 0x300 && point <= 0x36f) || (point >= 0x1ab0 && point <= 0x1aff) ||
        (point >= 0x1dc0 && point <= 0x1dff) || (point >= 0xfe00 && point <= 0xfe0f)) return 0;
    if ((point >= 0x1100 && point <= 0x115f) || (point >= 0x2e80 && point <= 0xa4cf) ||
        (point >= 0xac00 && point <= 0xd7a3) || (point >= 0xf900 && point <= 0xfaff) ||
        (point >= 0xfe10 && point <= 0xfe19) || (point >= 0xfe30 && point <= 0xfe6f) ||
        (point >= 0xff01 && point <= 0xff60) || (point >= 0xffe0 && point <= 0xffe6) ||
        (point >= 0x20000 && point <= 0x3fffd)) return 2;
    return 1;
}
static size_t codepoint_to_utf8(uint32_t point, unsigned char bytes[4])
{
    if (point < 0x80) { bytes[0] ← point; return 1; }
    if (point < 0x800) { bytes[0] ← 0xc0 | (point >> 6); bytes[1] ← 0x80 | (point & 63); return 2; }
    if (point < 0x10000) {
        bytes[0] ← 0xe0 | (point >> 12); bytes[1] ← 0x80 | ((point >> 6) & 63);
        bytes[2] ← 0x80 | (point & 63); return 3;
    }
    bytes[0] ← 0xf0 | (point >> 18); bytes[1] ← 0x80 | ((point >> 12) & 63);
    bytes[2] ← 0x80 | ((point >> 6) & 63); bytes[3] ← 0x80 | (point & 63); return 4;
}
static void erase_occupant(IcScreen *desired, int row, int column, IcStyle style)
{
    IcCell occupant ← desired->cells[row][column];
    if (!occupant.columns && column > 0) desired->cells[row][column - 1] ← blank_cell(style);
    if (occupant.columns == 2 && column + 1 < desired->columns)
        desired->cells[row][column + 1] ← blank_cell(style);
    desired->cells[row][column] ← blank_cell(style);
}
int ic_text_and_placement(const void *utf8, size_t length, int row, int column,
                          IcStyle style, IcScreen *desired)
{
    if (row < 0 || row >= desired->rows || style.foreground > 7 || style.background > 7 || style.bold > 1)
        return -1;
    const unsigned char *source ← utf8;
    size_t offset ← 0;
    int preceding ← -1;
    while (offset < length) {
        IcReceivedBytes supplied ← { .count ← length - offset > 4 ? 4 : length - offset };
        memcpy(supplied.bytes, source + offset, supplied.count);
        /* Text is literal: ESC and controls are replacement glyphs, never commands. */
        IcDecodedEvent decoded ← ic_received_bytes_to_event(supplied, 1);
        uint32_t point ← decoded.available ? decoded.event.codepoint : 0xfffd;
        size_t consumed ← decoded.available ? supplied.count - decoded.remaining.count : 1;
        if (source[offset] < 32 || source[offset] == 127) { point ← 0xfffd; consumed ← 1; }
        if (point >= 0x80 && point < 0xa0) point ← 0xfffd;
        offset += consumed;
        int width ← ic_codepoint_columns(point);
        unsigned char encoded[4];
        size_t encoded_length ← codepoint_to_utf8(point, encoded);
        if (!width) {
            if (preceding >= 0) {
                IcCell *base ← &desired->cells[row][preceding];
                if (base->length + encoded_length <= IC_TEXT_BYTES) {
                    memcpy(base->text + base->length, encoded, encoded_length);
                    base->length += encoded_length;
                }
            }
            continue;
        }
        preceding ← -1;
        if (column >= 0 && column + width <= desired->columns) {
            erase_occupant(desired, row, column, style);
            if (width == 2) erase_occupant(desired, row, column + 1, style);
            IcCell placed ← blank_cell(style);
            memcpy(placed.text, encoded, encoded_length);
            placed.length ← encoded_length;
            placed.columns ← width;
            desired->cells[row][column] ← placed;
            if (width == 2) {
                IcCell continuation ← { .foreground ← style.foreground, .background ← style.background,
                                        .bold ← style.bold };
                desired->cells[row][column + 1] ← continuation;
            }
            preceding ← column;
        }
        if (column > IC_COLS || column < -IC_COLS) {
            /* Avoid overflow from hostile placement while keeping left clipping useful. */
            if (column > IC_COLS) break;
        }
        if (column > INT32_MAX - 2) break;
        column += width;
    }
    return 0;
}
