#define _POSIX_C_SOURCE 200809L
/* Ordinary external data producer and pipe/session launcher; no view policy. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,"Usage: icurses-live ABSOLUTE_icurses-demo ABSOLUTE_POLICY.ilua\n");
        return 2;
    }
    int updates[2];
    if (pipe(updates)) { perror("Command pipe"); return 1; }
    pid_t producer ← fork();
    if (producer < 0) { perror("Producer process"); close(updates[0]); close(updates[1]); return 1; }
    if (!producer) {
        close(updates[0]);
        signal(SIGINT,SIG_IGN);
        static const char *labels[] ← {"Tuğba", "İstanbul", "é — combining", "界 — wide"};
        for (unsigned counter ← 1; ; ++counter) {
            char line[1024];
            unsigned row ← (counter - 1) % 4;
            int length ← snprintf(line,sizeof(line),"%u\t%s\t%u\n",row+1,labels[row],counter);
            size_t written ← 0;
            while (written < (size_t)length) {
                long count ← write(updates[1],line+written,length-written);
                if (count > 0) written += count;
                else if (errno != EINTR) _exit(0);
            }
            struct timespec period ← {0,500000000};
            while (nanosleep(&period,&period) && errno == EINTR) {}
        }
    }
    close(updates[1]);
    if (updates[0] != 3) {
        if (dup2(updates[0],3) < 0) { kill(producer,SIGTERM); return 1; }
        close(updates[0]);
    }
    execl(argv[1],argv[1],argv[2],"3",(char *)NULL);
    perror("Application execution");
    close(3); kill(producer,SIGTERM);
    while (waitpid(producer,NULL,0) < 0 && errno == EINTR) {}
    return 1;
}
