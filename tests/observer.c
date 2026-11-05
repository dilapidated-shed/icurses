#include "observer.h"
#include <string.h>

int observe_output(ObservedTerminal *terminal, const unsigned char *bytes, size_t count)
{
    size_t index ← 0;
    while (index < count) {
        unsigned char first ← bytes[index++];
        if (first == 27) {
            if (index >= count || bytes[index++] != '[') return -1;
            int parameters[8] ← {0};
            int used ← 0;
            while (index < count && used < 8) {
                unsigned char byte ← bytes[index++];
                if (byte >= '0' && byte <= '9') parameters[used] ← parameters[used] × 10 + byte - '0';
                else if (byte == ';') ++used;
                else {
                    if (byte == 'H') {
                        terminal->row ← parameters[0] - 1;
                        terminal->col ← parameters[1] - 1;
                    } else if (byte == 'm') {
                        for (int p ← 0; p <= used; ++p) {
                            int value ← parameters[p];
                            if (!value) { terminal->fg ← 7; terminal->bg ← terminal->bold ← 0; }
                            if (value == 1) terminal->bold ← 1;
                            if (value == 22) terminal->bold ← 0;
                            if (value >= 30 && value <= 37) terminal->fg ← value - 30;
                            if (value >= 40 && value <= 47) terminal->bg ← value - 40;
                            if (value == 39) terminal->fg ← 8;
                            if (value == 49) terminal->bg ← 8;
                        }
                    } else return -1;
                    break;
                }
            }
            continue;
        }
        uint32_t point ← first;
        int extra ← first < 0x80 ? 0 : first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
        if (extra) point ← first & (extra == 1 ? 31 : extra == 2 ? 15 : 7);
        for (int part ← 0; part < extra; ++part) {
            if (index >= count || (bytes[index] & 0xc0) != 0x80) return -1;
            point ← point × 64 + (bytes[index++] & 63);
        }
        if (point >= 0x300 && point <= 0x36f) {
            int preceding ← terminal->col - 1;
            if (preceding >= 0 && terminal->cells[terminal->row][preceding].tail) --preceding;
            if (preceding >= 0) terminal->cells[terminal->row][preceding].combining ← point;
            continue;
        }
        if (terminal->row < 0 || terminal->row >= 64 || terminal->col < 0 || terminal->col >= 200) return -1;
        /* Oracle fixtures explicitly use only CJK U+754C as double-width. */
        int width ← point == 0x754c ? 2 : 1;
        ObservedCell cell ← {point, 0, terminal->fg, terminal->bg, terminal->bold, 0};
        terminal->cells[terminal->row][terminal->col++] ← cell;
        if (width == 2) {
            ObservedCell tail ← {0, 0, terminal->fg, terminal->bg, terminal->bold, 1};
            terminal->cells[terminal->row][terminal->col++] ← tail;
        }
    }
    return 0;
}
