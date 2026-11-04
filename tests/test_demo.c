#define _GNU_SOURCE
#include "icurses_posix.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define REQUIRE(condition) do { if (!(condition)) { fprintf(stderr,"FAIL demo:%d %s: %s\n",__LINE__,#condition,strerror(errno)); exit(1); } } while (0)
static char captured[131072];
static size_t captured_count;
static size_t collect(int master, int timeout)
{
    struct pollfd source ← {master,POLLIN,0};
    if (poll(&source,1,timeout) <= 0) return 0;
    long count ← read(master,captured+captured_count,sizeof(captured)-captured_count-1);
    if (count <= 0) return 0;
    captured_count += count;
    captured[captured_count] ← 0;
    return count;
}
static long until_text(int master, const char *text)
{
    int64_t start ← ic_monotonic_ms();
    int64_t deadline ← start+3000;
    while (!strstr(captured,text) && ic_monotonic_ms() < deadline) collect(master,30);
    if (!strstr(captured,text)) fprintf(stderr,"Captured: %s\n",captured);
    REQUIRE(strstr(captured,text));
    return ic_monotonic_ms()-start;
}
static void reset_capture(void) { captured_count ← 0; captured[0] ← 0; }
static int modes_equal(struct termios first, struct termios second)
{
    return first.c_iflag == second.c_iflag && first.c_oflag == second.c_oflag &&
        first.c_cflag == second.c_cflag && first.c_lflag == second.c_lflag &&
        !memcmp(first.c_cc,second.c_cc,sizeof(first.c_cc));
}
static void run_application(const char *binary, const char *policy, const char *down, int interrupted)
{
    int master,slave,updates[2];
    struct winsize size ← {.ws_row ← 12, .ws_col ← 70};
    REQUIRE(!openpty(&master,&slave,NULL,NULL,&size));
    REQUIRE(!pipe(updates));
    struct termios encountered,restored;
    REQUIRE(!tcgetattr(slave,&encountered));
    int64_t start ← ic_monotonic_ms();
    pid_t process ← fork();
    REQUIRE(process >= 0);
    if (!process) {
        close(master); close(updates[1]);
        REQUIRE(setsid() == getpid());
        REQUIRE(!ioctl(slave,TIOCSCTTY,0));
        REQUIRE(!tcsetpgrp(slave,getpgrp()));
        REQUIRE(dup2(slave,0) == 0 && dup2(slave,1) == 1 && dup2(slave,2) == 2);
        REQUIRE(dup2(updates[0],3) == 3);
        if (slave > 3) close(slave);
        if (updates[0] > 3 && updates[0] != slave) close(updates[0]);
        execl(binary,binary,policy,"3",(char *)NULL);
        _exit(127);
    }
    close(updates[0]);
    REQUIRE(fcntl(master,F_SETFL,O_NONBLOCK) == 0);
    reset_capture();
    until_text(master,"Selected: Tuğba");
    long startup ← ic_monotonic_ms()-start;
    while (collect(master,10)) {}
    reset_capture();
    int64_t idle_start ← ic_monotonic_ms();
    while (ic_monotonic_ms()-idle_start < 1000) REQUIRE(collect(master,50) == 0);
    REQUIRE(write(master,down,1) == 1);
    long input_latency ← until_text(master,"İstanbul");
    while (collect(master,10)) {}
    reset_capture();
    const char *update ← "2\tÇınar 界\t88\n";
    REQUIRE(write(updates[1],update,strlen(update)) == (long)strlen(update));
    long pipe_latency ← until_text(master,"Çınar 界");
    while (collect(master,10)) {}
    reset_capture();
    size.ws_row ← 17; size.ws_col ← 91;
    REQUIRE(!ioctl(master,TIOCSWINSZ,&size));
    until_text(master,"Selected: Çınar 界");
    if (interrupted) REQUIRE(!kill(process,SIGTERM));
    else REQUIRE(write(master,"q",1) == 1);
    int status;
    struct rusage usage;
    REQUIRE(wait4(process,&status,0,&usage) == process);
    REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    while (collect(master,10)) {}
    REQUIRE(!tcgetattr(slave,&restored) && modes_equal(encountered,restored));
    REQUIRE(strstr(captured,"\0338"));
    long cpu_us ← usage.ru_utime.tv_sec × 1000000+usage.ru_utime.tv_usec+
                  usage.ru_stime.tv_sec × 1000000+usage.ru_stime.tv_usec;
    printf("PASS application policy=%s exit=%s startup_ms=%ld input_visible_ms=%ld pipe_visible_ms=%ld total_cpu_us=%ld idle_emitted_bytes=0\n",
           policy,interrupted ? "SIGTERM" : "command",startup,input_latency,pipe_latency,cpu_us);
    const char *statistics ← strstr(captured,"frames=");
    REQUIRE(statistics);
    size_t frames, bytes, allocations, peak, idle_allocations;
    long long idle_wall, idle_cpu;
    REQUIRE(sscanf(statistics,"frames=%zu emitted_bytes=%zu lua_allocation_calls=%zu lua_peak_bytes=%zu idle_wall_ms=%lld idle_cpu_us=%lld idle_allocations=%zu",
        &frames,&bytes,&allocations,&peak,&idle_wall,&idle_cpu,&idle_allocations) == 7);
    REQUIRE(idle_wall >= 900 && idle_cpu >= 0 && idle_allocations == 0);
#if !defined(__ANDROID__)
    /* Bounds are derived in docs/MEASUREMENTS.md; Android earns its own baseline. */
    REQUIRE(input_latency <= 32 && pipe_latency <= 32);
    REQUIRE(idle_cpu × 1000 <= idle_wall × 4000);
    REQUIRE(frames <= 4 && bytes <= 4096 && allocations <= 1024 && peak <= 65536);
#endif
    printf("%s\n",statistics);
    close(master); close(slave); close(updates[1]);
}
static void run_live(const char *launcher, const char *binary, const char *policy)
{
    int master,slave;
    struct winsize size ← {.ws_row ← 12, .ws_col ← 70};
    REQUIRE(!openpty(&master,&slave,NULL,NULL,&size));
    struct termios encountered, restored;
    REQUIRE(!tcgetattr(slave,&encountered));
    pid_t process ← fork();
    REQUIRE(process >= 0);
    if (!process) {
        close(master);
        REQUIRE(setsid() == getpid() && !ioctl(slave,TIOCSCTTY,0));
        REQUIRE(!tcsetpgrp(slave,getpgrp()));
        REQUIRE(dup2(slave,0) == 0 && dup2(slave,1) == 1 && dup2(slave,2) == 2);
        if (slave > 2) close(slave);
        execl(launcher,launcher,binary,policy,(char *)NULL);
        _exit(127);
    }
    REQUIRE(fcntl(master,F_SETFL,O_NONBLOCK) == 0);
    reset_capture();
    until_text(master,"Selected: Tuğba");
    while (collect(master,10)) {}
    reset_capture();
    REQUIRE(collect(master,1500) > 0);
    REQUIRE(write(master,"q",1) == 1);
    int status;
    REQUIRE(waitpid(process,&status,0) == process && WIFEXITED(status) && !WEXITSTATUS(status));
    REQUIRE(!tcgetattr(slave,&restored) && modes_equal(encountered,restored));
    close(master); close(slave);
    puts("PASS interactive launcher with a live external pipe producer");
}
int main(int argc,char **argv)
{
    alarm(20);
    if (argc == 5 && !strcmp(argv[1],"--live")) {
        run_live(argv[2],argv[3],argv[4]); return 0;
    }
    REQUIRE(argc == 4);
    run_application(argv[1],argv[2],argv[3],0);
    run_application(argv[1],argv[2],argv[3],1);
    return 0;
}
