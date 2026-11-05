#include "icurses.h"
#include "observer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) do { if (!(condition)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#condition); exit(1); } } while (0)
static IcScreen previous, desired, committed;
static IcOutputBytes output;
static ObservedTerminal observed;

static void decoder(void)
{
    IcReceivedBytes bytes ← {0};
    REQUIRE(ic_receive_bytes(&bytes, "\xc4", 1) == 1);
    IcDecodedEvent event ← ic_received_bytes_to_event(bytes, 0);
    REQUIRE(!event.available && event.remaining.count == 1);
    REQUIRE(ic_receive_bytes(&bytes, "\x9f\033[Aq", 5) == 5);
    event ← ic_received_bytes_to_event(bytes, 0);
    REQUIRE(event.available && event.event.codepoint == 0x11f);
    event ← ic_received_bytes_to_event(event.remaining, 0);
    REQUIRE(event.available && event.event.kind == IC_KEY_UP);
    event ← ic_received_bytes_to_event(event.remaining, 0);
    REQUIRE(event.available && event.event.codepoint == 'q' && !event.remaining.count);
    bytes.count ← 0;
    ic_receive_bytes(&bytes, "\033[", 2);
    event ← ic_received_bytes_to_event(bytes, 0);
    REQUIRE(!event.available && event.needs_escape_deadline);
    event ← ic_received_bytes_to_event(bytes, 1);
    REQUIRE(event.available && event.event.kind == IC_KEY_ESCAPE && event.remaining.count == 1);
    bytes.count ← 0;
    ic_receive_bytes(&bytes, "\xf0\x80\x80\x80", 4);
    event ← ic_received_bytes_to_event(bytes, 0);
    REQUIRE(event.available && event.event.codepoint == 0xfffd && event.remaining.count == 3);
    bytes.count ← sizeof(bytes.bytes)+1;
    REQUIRE(ic_receive_bytes(&bytes,"x",1) == 0);
    REQUIRE(ic_received_bytes_to_event(bytes,0).invalid_buffer);
}
static void rendering(void)
{
    REQUIRE(!ic_empty_screen(3, 12, &desired));
    IcStyle highlighted ← {2,4,1};
    const char *text ← "Tuğba é界";
    REQUIRE(!ic_text_and_placement(text, strlen(text), 0, 0, highlighted, &desired));
    REQUIRE(desired.cells[0][2].length == 2 && desired.cells[0][2].text[0] == 0xc4);
    REQUIRE(desired.cells[0][6].length == 3 && desired.cells[0][7].columns == 2 &&
            desired.cells[0][8].columns == 0);
    REQUIRE(!ic_text_and_placement("界A", 4, 1, -1, highlighted, &desired));
    REQUIRE(desired.cells[1][0].text[0] == ' ' && desired.cells[1][1].text[0] == 'A');
    REQUIRE(!ic_text_and_placement("界", 3, 1, 11, highlighted, &desired));
    REQUIRE(desired.cells[1][11].text[0] == ' ');
    REQUIRE(!ic_text_and_placement("\033[2J", 4, 2, 0, highlighted, &desired));
    REQUIRE(desired.cells[2][0].length == 3 && desired.cells[2][1].text[0] == '[');
    desired.cursor_row ← 1;
    desired.cursor_column ← 4;
    IcChangedRegions regions ← ic_previous_and_desired_to_regions(&previous, &desired, 0);
    REQUIRE(regions.count == 3);
    REQUIRE(!ic_regions_and_capabilities_to_output(&regions, &desired, (IcTerminalCapabilities){8}, &output));
    REQUIRE(!observe_output(&observed, output.bytes, output.count));
    REQUIRE(observed.cells[0][2].point == 0x11f && observed.cells[0][6].point == 'e');
    REQUIRE(observed.cells[0][6].combining == 0x301 && observed.cells[0][7].point == 0x754c);
    REQUIRE(observed.cells[0][8].tail && observed.cells[0][7].fg == 2 &&
            observed.cells[0][7].bg == 4 && observed.cells[0][7].bold);
    REQUIRE(observed.row == 1 && observed.col == 4);
    output.acknowledged ← output.count - 1;
    REQUIRE(!ic_acknowledged_output_to_previous(&output, &desired, &previous));
    REQUIRE(previous.rows == 0);
    output.acknowledged ← output.count;
    REQUIRE(ic_acknowledged_output_to_previous(&output, &desired, &previous));
    regions ← ic_previous_and_desired_to_regions(&previous, &desired, 0);
    REQUIRE(!regions.count && !regions.cursor_changed);
    REQUIRE(!ic_regions_and_capabilities_to_output(&regions, &desired, (IcTerminalCapabilities){8}, &output));
    REQUIRE(output.count == 0);
    REQUIRE(!ic_text_and_placement("x", 1, 0, 8, highlighted, &desired));
    REQUIRE(desired.cells[0][7].text[0] == ' ' && desired.cells[0][8].text[0] == 'x');
    regions ← ic_previous_and_desired_to_regions(&previous, &desired, 0);
    REQUIRE(regions.count == 1 && regions.rows[0].first == 7 && regions.rows[0].past == 9);
    REQUIRE(!ic_regions_and_capabilities_to_output(&regions, &desired, (IcTerminalCapabilities){8}, &output));
    REQUIRE(!observe_output(&observed, output.bytes, output.count));
    REQUIRE(observed.cells[0][7].point == ' ' && observed.cells[0][8].point == 'x' && !observed.cells[0][8].tail);
    committed ← previous;
    output.acknowledged ← 0;
    REQUIRE(!ic_acknowledged_output_to_previous(&output, &desired, &previous));
    REQUIRE(!memcmp(&committed, &previous, sizeof(previous)));
    REQUIRE(ic_empty_screen(0, 12, &desired) == -1);
    REQUIRE(!ic_empty_screen(1,12,&desired));
    REQUIRE(!ic_text_and_placement("x",1,0,INT32_MAX,highlighted,&desired));
    REQUIRE(!ic_text_and_placement("x",1,0,INT32_MIN,highlighted,&desired));
    REQUIRE(desired.cells[0][0].text[0] == ' ');
    desired.rows ← IC_ROWS+1;
    regions ← ic_previous_and_desired_to_regions(&previous,&desired,0);
    REQUIRE(regions.invalid_screen);
    REQUIRE(ic_regions_and_capabilities_to_output(&regions,&desired,(IcTerminalCapabilities){8},&output) == -1);
}
static void configured_terminal_colors(void)
{
    IcScreen empty ← {0};
    REQUIRE(!ic_empty_screen(1, 8, &desired));
    REQUIRE(desired.cells[0][0].foreground == IC_DEFAULT_COLOR &&
            desired.cells[0][0].background == IC_DEFAULT_COLOR);
    REQUIRE(!ic_text_and_placement("own", 3, 0, 0,
        (IcStyle){IC_DEFAULT_COLOR, IC_DEFAULT_COLOR, 1}, &desired));
    IcChangedRegions regions ← ic_previous_and_desired_to_regions(&empty, &desired, 0);
    REQUIRE(!ic_regions_and_capabilities_to_output(&regions, &desired,
        (IcTerminalCapabilities){8}, &output));
    memset(&observed, 0, sizeof(observed));
    REQUIRE(!observe_output(&observed, output.bytes, output.count));
    REQUIRE(observed.cells[0][0].fg == 8 && observed.cells[0][0].bg == 8 &&
            observed.cells[0][0].bold);
    REQUIRE(ic_text_and_placement("x", 1, 0, 0, (IcStyle){9,8,0}, &desired) == -1);
    REQUIRE(ic_text_and_placement("x", 1, 0, 0, (IcStyle){8,9,0}, &desired) == -1);
    desired.cells[0][0].background ← 9;
    REQUIRE(ic_regions_and_capabilities_to_output(&regions, &desired,
        (IcTerminalCapabilities){8}, &output) == -1);
}
int main(void)
{
    decoder(); rendering(); configured_terminal_colors();
    puts("PASS incremental decoder, literal cells, independent terminal observer, transactional output");
    return 0;
}
