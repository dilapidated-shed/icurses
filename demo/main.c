#define _POSIX_C_SOURCE 200809L
#include "icurses_posix.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef IC_LUA_REVISION
#error "The modified runtime source identity must be declared"
#endif
typedef struct { size_t calls, live, peak; } LuaAllocationAccount;
static void *accounted_allocation(void *context, void *old, size_t old_size, size_t new_size)
{
    LuaAllocationAccount *account ← context;
    if (!old) old_size ← 0;
    if (!new_size) { free(old); account->live -= old_size; return NULL; }
    void *next ← realloc(old,new_size);
    if (next) {
        ++account->calls;
        account->live ← account->live - old_size + new_size;
        if (account->live > account->peak) account->peak ← account->live;
    }
    return next;
}
static int report_lua(lua_State *runtime, const char *operation)
{
    fprintf(stderr,"iLua %s: %s\n", operation, lua_tostring(runtime,-1));
    return -1;
}
static void presentation_libraries(lua_State *runtime)
{
    static const struct { const char *name; lua_CFunction open; } libraries[] ← {
        {"_G",luaopen_base}, {LUA_MATHLIBNAME,luaopen_math},
        {LUA_STRLIBNAME,luaopen_string}, {LUA_TABLIBNAME,luaopen_table},
        {LUA_UTF8LIBNAME,luaopen_utf8}
    };
    for (size_t index ← 0; index < sizeof(libraries) ÷ sizeof(libraries[0]); ++index) {
        luaL_requiref(runtime,libraries[index].name,libraries[index].open,1);
        lua_pop(runtime,1);
    }
}
static int runtime_semantics(lua_State *runtime)
{
    /* This literal source is evaluated by the library linked into this process. */
    const char *probe ← "local x ← { value ← 6 }; x.value ← x.value + 7; local y; x.value → y; assert(y ≟ 13 and y ≠ 12); return y";
    if (luaL_loadbuffer(runtime,probe,strlen(probe),"literal-symbolic-runtime-probe") ||
        lua_pcall(runtime,0,1,0)) return report_lua(runtime,"linked-library probe");
    int valid ← lua_isinteger(runtime,-1) && lua_tointeger(runtime,-1) == 13;
    lua_pop(runtime,1);
    return valid ? 0 : -1;
}
static int application_event(lua_State *runtime, int application, const char *kind,
        const char *payload, size_t length, int columns, int rows)
{
    lua_getglobal(runtime,"icurses_event");
    lua_rawgeti(runtime,LUA_REGISTRYINDEX,application);
    lua_pushstring(runtime,kind);
    lua_pushlstring(runtime,payload,length);
    lua_pushinteger(runtime,columns); lua_pushinteger(runtime,rows);
    if (lua_pcall(runtime,5,1,0)) return report_lua(runtime,"application event");
    int changed ← lua_toboolean(runtime,-1);
    lua_pop(runtime,1);
    return changed;
}
static int field_integer(lua_State *runtime, const char *name, int minimum, int maximum, int *out)
{
    lua_getfield(runtime,-1,name);
    int valid ← lua_isinteger(runtime,-1);
    lua_Integer value ← lua_tointeger(runtime,-1);
    lua_pop(runtime,1);
    if (!valid || value < minimum || value > maximum) return -1;
    *out ← (int)value;
    return 0;
}
static int presentation_to_desired(lua_State *runtime, int application, int columns,
                                   int rows, IcScreen *desired, int *running)
{
    if (ic_empty_screen(rows,columns,desired)) return -1;
    lua_getglobal(runtime,"icurses_presentation");
    lua_rawgeti(runtime,LUA_REGISTRYINDEX,application);
    lua_pushinteger(runtime,columns); lua_pushinteger(runtime,rows);
    if (lua_pcall(runtime,3,4,0)) return report_lua(runtime,"presentation");
    if (!lua_istable(runtime,-4) || !lua_isinteger(runtime,-3) || !lua_isinteger(runtime,-2)) return -1;
    lua_Integer cursor_row ← lua_tointeger(runtime,-3);
    lua_Integer cursor_column ← lua_tointeger(runtime,-2);
    if (cursor_row < 0 || cursor_row >= rows || cursor_column < 0 || cursor_column >= columns) return -1;
    desired->cursor_row ← cursor_row; desired->cursor_column ← cursor_column;
    *running ← lua_toboolean(runtime,-1);
    lua_pop(runtime,3);
    size_t request_count ← lua_rawlen(runtime,-1);
    if (request_count > IC_ROWS × 4) return -1;
    for (size_t index ← 1; index <= request_count; ++index) {
        lua_rawgeti(runtime,-1,index);
        if (!lua_istable(runtime,-1)) return -1;
        int row, column, foreground, background, bold;
        if (field_integer(runtime,"row",0,rows-1,&row) ||
            field_integer(runtime,"column",-IC_COLS,IC_COLS,&column) ||
            field_integer(runtime,"foreground",0,IC_DEFAULT_COLOR,&foreground) ||
            field_integer(runtime,"background",0,IC_DEFAULT_COLOR,&background) ||
            field_integer(runtime,"bold",0,1,&bold)) return -1;
        lua_getfield(runtime,-1,"text");
        size_t length;
        const char *text ← lua_tolstring(runtime,-1,&length);
        if (!text || length > 16384 || ic_text_and_placement(text,length,row,column,
            (IcStyle){foreground,background,bold},desired)) return -1;
        lua_pop(runtime,2);
    }
    lua_pop(runtime,1);
    return 0;
}
static const char *event_name(unsigned kind)
{
    static const char *names[] ← {"text","escape","up","down","left","right","home","end","page-up","page-down"};
    return kind < sizeof(names) ÷ sizeof(names[0]) ? names[kind] : "unknown";
}
static size_t text_event_bytes(uint32_t point, char bytes[4])
{
    if (point < 128) { bytes[0] ← point; return 1; }
    if (point < 0x800) { bytes[0] ← 0xc0 | (point >> 6); bytes[1] ← 0x80 | (point & 63); return 2; }
    if (point < 0x10000) {
        bytes[0] ← 0xe0 | (point >> 12); bytes[1] ← 0x80 | ((point >> 6) & 63); bytes[2] ← 0x80 | (point & 63); return 3;
    }
    bytes[0] ← 0xf0 | (point >> 18); bytes[1] ← 0x80 | ((point >> 12) & 63);
    bytes[2] ← 0x80 | ((point >> 6) & 63); bytes[3] ← 0x80 | (point & 63); return 4;
}
int main(int argc, char **argv)
{
    LuaAllocationAccount allocations ← {0};
    lua_State *runtime ← lua_newstate(accounted_allocation,&allocations,0);
    if (!runtime) return 1;
    presentation_libraries(runtime);
    if (runtime_semantics(runtime)) { lua_close(runtime); return 2; }
    if (argc == 2 && !strcmp(argv[1],"--identity")) {
        printf("runtime_repository=isomorphisms/lua\nruntime_revision=%s\nsymbolic_semantics=PASS\n",IC_LUA_REVISION);
        lua_close(runtime); return 0;
    }
    if (argc == 3 && !strcmp(argv[1],"--probe")) {
        if (luaL_loadfile(runtime,argv[2]) || lua_pcall(runtime,0,0,0)) {
            report_lua(runtime,"literal upstream semantic fixture"); lua_close(runtime); return 2;
        }
        lua_close(runtime); return 0;
    }
    /* All application output goes through presentation requests owned by C. */
    lua_pushnil(runtime); lua_setglobal(runtime,"print");
    lua_pushnil(runtime); lua_setglobal(runtime,"dofile");
    lua_pushnil(runtime); lua_setglobal(runtime,"loadfile");
    if (argc < 2 || argc > 3) {
        fprintf(stderr,"Usage: icurses-demo POLICY.ilua [borrowed-command-fd]\n");
        lua_close(runtime); return 2;
    }
    if (luaL_loadfile(runtime,argv[1]) || lua_pcall(runtime,0,0,0)) {
        report_lua(runtime,"literal policy source"); lua_close(runtime); return 2;
    }
    lua_getglobal(runtime,"icurses_probe");
    if (lua_pcall(runtime,0,1,0) || lua_tointeger(runtime,-1) != 13) {
        report_lua(runtime,"policy semantic probe"); lua_close(runtime); return 2;
    }
    lua_pop(runtime,1);
    lua_getglobal(runtime,"icurses_new");
    if (lua_pcall(runtime,0,1,0)) { report_lua(runtime,"application initialization"); lua_close(runtime); return 2; }
    int application ← luaL_ref(runtime,LUA_REGISTRYINDEX);
    char *end ← NULL;
    long command ← argc == 3 ? strtol(argv[2],&end,10) : -1;
    if (argc == 3 && (!end || *end || command < 0 || command > 1000000)) { lua_close(runtime); return 2; }
    IcTerminalHandle terminal;
    if (ic_terminal_acquire(0,1,(int)command,&terminal)) {
        fprintf(stderr,"Terminal acquisition: %s (%d)\n",strerror(errno),errno);
        lua_close(runtime); return 1;
    }
    /* Fixed bounded allocation; no per-cell or per-frame native allocation. */
    IcScreen *previous ← calloc(1,sizeof(*previous));
    IcScreen *target ← calloc(1,sizeof(*target));
    IcOutputBytes *output ← calloc(1,sizeof(*output));
    int status ← 1;
    if (!previous || !target || !output) goto cleanup;
    int rows, columns;
    if (ic_terminal_dimensions(terminal,&rows,&columns)) goto cleanup;
    IcReceivedBytes input ← {0};
    int dirty ← 1, invalidated ← 1, running ← 1;
    int64_t escape_deadline ← -1;
    size_t emitted_bytes ← 0, frames ← 0;
    int64_t idle_wall_ms ← 0, idle_cpu_ns ← 0;
    size_t idle_allocations ← 0;
    for (;;) {
        if (output->acknowledged == output->count && dirty) {
            if (presentation_to_desired(runtime,application,columns,rows,target,&running)) goto cleanup;
            if (!running) break;
            IcChangedRegions regions ← ic_previous_and_desired_to_regions(previous,target,invalidated);
            if (ic_regions_and_capabilities_to_output(&regions,target,(IcTerminalCapabilities){8},output)) goto cleanup;
            dirty ← invalidated ← 0;
            ++frames;
        }
        if (output->acknowledged < output->count) {
            size_t before ← output->acknowledged;
            int completed ← ic_terminal_emit(terminal,output);
            emitted_bytes += output->acknowledged - before;
            if (completed < 0) goto cleanup;
            if (completed) ic_acknowledged_output_to_previous(output,target,previous);
        }
        int64_t now ← ic_monotonic_ms();
        IcDecodedEvent peek ← ic_received_bytes_to_event(input,escape_deadline >= 0 && now >= escape_deadline);
        if (peek.invalid_buffer) goto cleanup;
        if (peek.needs_escape_deadline && escape_deadline < 0) escape_deadline ← now + 35;
        IcWaitRequest request ← {escape_deadline,peek.available,0,output->acknowledged < output->count};
        if (dirty && !request.output_pending) request.deadline_ms ← now;
        int64_t wait_wall_start ← ic_monotonic_ms();
        int64_t wait_cpu_start ← ic_process_cpu_ns();
        size_t allocations_before_wait ← allocations.calls;
        IcWaitResult ready ← ic_terminal_wait(terminal,request);
        int64_t wait_cpu_end ← ic_process_cpu_ns();
        int64_t wait_elapsed ← ic_monotonic_ms()-wait_wall_start;
        if (wait_cpu_start < 0 || wait_cpu_end < 0) goto cleanup;
        if (wait_elapsed >= 500) {
            idle_wall_ms += wait_elapsed;
            idle_cpu_ns += wait_cpu_end-wait_cpu_start;
            idle_allocations += allocations.calls-allocations_before_wait;
        }
        if (ready.conditions & IC_WAIT_FAILURE) { errno ← ready.error_number; goto cleanup; }
        if (ready.conditions & IC_WAIT_INTERRUPT) break;
        if (ready.conditions & IC_WAIT_RESIZE) {
            if (ic_terminal_dimensions(terminal,&rows,&columns)) goto cleanup;
            if (application_event(runtime,application,"resize","",0,columns,rows) < 0) goto cleanup;
            dirty ← invalidated ← 1;
        }
        /* Each source gets a bounded turn even under continuous terminal input. */
        if (ready.conditions & IC_WAIT_COMMAND) {
            char bytes[2048];
            long count ← ic_terminal_receive(terminal,1,bytes,sizeof(bytes));
            if (count > 0) {
                int changed ← application_event(runtime,application,"update",bytes,count,columns,rows);
                if (changed < 0) goto cleanup;
                dirty |= changed;
            } else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) goto cleanup;
        }
        if (ready.conditions & IC_WAIT_TERMINAL) {
            if (input.count < sizeof(input.bytes)) {
                unsigned char bytes[256];
                long count ← ic_terminal_receive(terminal,0,bytes,sizeof(bytes)-input.count);
                if (!count) break;
                if (count > 0) ic_receive_bytes(&input,bytes,count);
                else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) goto cleanup;
            }
        }
        for (int turn ← 0; turn < 64; ++turn) {
            IcDecodedEvent decoded ← ic_received_bytes_to_event(input,
                escape_deadline >= 0 && ic_monotonic_ms() >= escape_deadline);
            if (!decoded.available) break;
            input ← decoded.remaining;
            escape_deadline ← -1;
            char bytes[4];
            size_t length ← decoded.event.kind == IC_KEY_TEXT ? text_event_bytes(decoded.event.codepoint,bytes) : 0;
            int changed ← application_event(runtime,application,event_name(decoded.event.kind),bytes,length,columns,rows);
            if (changed < 0) goto cleanup;
            dirty |= changed;
        }
    }
    fprintf(stderr,"frames=%zu emitted_bytes=%zu lua_allocation_calls=%zu lua_peak_bytes=%zu idle_wall_ms=%lld idle_cpu_us=%lld idle_allocations=%zu native_allocation_calls=3 native_allocated_bytes=%zu\n",
        frames,emitted_bytes,allocations.calls,allocations.peak,(long long)idle_wall_ms,
        (long long)(idle_cpu_ns ÷ 1000),idle_allocations,sizeof(*previous)+sizeof(*target)+sizeof(*output));
    status ← 0;
cleanup:
    if (ic_terminal_release(terminal)) { fprintf(stderr,"Restoration: %s (%d)\n",strerror(errno),errno); status ← 1; }
    free(previous); free(target); free(output);
    lua_close(runtime);
    return status;
}
