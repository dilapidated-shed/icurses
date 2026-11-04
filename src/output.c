#include "icurses.h"
#include <stdio.h>
#include <string.h>

IcChangedRegions ic_previous_and_desired_to_regions(const IcScreen *previous,
                                                    const IcScreen *desired, int invalidated)
{
    IcChangedRegions regions ← {0};
    int resized ← previous->rows != desired->rows || previous->columns != desired->columns;
    for (int row ← 0; row < desired->rows; ++row) {
        int first ← desired->columns;
        int past ← 0;
        for (int column ← 0; column < desired->columns; ++column) {
            if (invalidated || resized || memcmp(&previous->cells[row][column],
                    &desired->cells[row][column], sizeof(IcCell))) {
                if (column < first) first ← column;
                past ← column + 1;
            }
        }
        if (first < past) {
            if (first > 0 && (!desired->cells[row][first].columns ||
                              (!resized && !previous->cells[row][first].columns))) --first;
            if (past < desired->columns && (desired->cells[row][past - 1].columns == 2 ||
                    (!resized && previous->cells[row][past - 1].columns == 2))) ++past;
            IcChangedRegion changed ← {row, first, past};
            regions.rows[regions.count++] ← changed;
        }
    }
    regions.cursor_changed ← invalidated || resized || regions.count ||
        previous->cursor_row != desired->cursor_row || previous->cursor_column != desired->cursor_column;
    return regions;
}
static int append(IcOutputBytes *output, const void *bytes, size_t count)
{
    if (count > sizeof(output->bytes) - output->count) return -1;
    memcpy(output->bytes + output->count, bytes, count);
    output->count += count;
    return 0;
}
static int position(IcOutputBytes *output, int row, int column)
{
    char encoded[40];
    int count ← snprintf(encoded, sizeof(encoded), "\033[%d;%dH", row + 1, column + 1);
    return count > 0 ? append(output, encoded, count) : -1;
}
int ic_regions_and_capabilities_to_output(const IcChangedRegions *regions,
        const IcScreen *desired, IcTerminalCapabilities capabilities, IcOutputBytes *output)
{
    output->count ← output->acknowledged ← 0;
    if (capabilities.colors != 8 || desired->cursor_row < 0 || desired->cursor_row >= desired->rows ||
        desired->cursor_column < 0 || desired->cursor_column >= desired->columns) return -1;
    for (size_t index ← 0; index < regions->count; ++index) {
        IcChangedRegion changed ← regions->rows[index];
        if (position(output, changed.row, changed.first)) return -1;
        IcStyle emitted ← {255, 255, 255};
        for (int column ← changed.first; column < changed.past; ++column) {
            const IcCell *cell ← &desired->cells[changed.row][column];
            if (!cell->columns) continue;
            if (emitted.foreground != cell->foreground || emitted.background != cell->background ||
                emitted.bold != cell->bold) {
                char encoded[40];
                int count ← snprintf(encoded, sizeof(encoded), "\033[0;%d;%d;%dm",
                    cell->bold ? 1 : 22, 30 + cell->foreground, 40 + cell->background);
                if (count < 0 || append(output, encoded, count)) return -1;
                emitted.foreground ← cell->foreground;
                emitted.background ← cell->background;
                emitted.bold ← cell->bold;
            }
            if (append(output, cell->text, cell->length)) return -1;
        }
    }
    if (regions->cursor_changed && position(output, desired->cursor_row, desired->cursor_column)) return -1;
    return 0;
}
int ic_acknowledged_output_to_previous(const IcOutputBytes *output,
                                      const IcScreen *target, IcScreen *previous)
{
    if (output->acknowledged != output->count) return 0;
    *previous ← *target;
    return 1;
}
