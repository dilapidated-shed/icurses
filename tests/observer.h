#ifndef IC_OBSERVER_H
#define IC_OBSERVER_H
#include <stddef.h>
#include <stdint.h>
/* Deliberately independent representation and parser. */
typedef struct { uint32_t point, combining; int fg, bg, bold, tail; } ObservedCell;
typedef struct { ObservedCell cells[64][200]; int row, col, fg, bg, bold; } ObservedTerminal;
int observe_output(ObservedTerminal *terminal, const unsigned char *bytes, size_t count);
#endif
