#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int fd_pipe[2];

void shell() {
    pipe(fd_pipe);
    pid_t pid = fork();
    if (pid == 0) {
        close(fd_pipe[1]);
        dup2(fd_pipe[0], STDIN_FILENO);
        execl("/bin/sh", "sh", "-c 'ls'",NULL);
        exit(1);
    } else if (pid > 0) {
        close(fd_pipe[0]);
        printf("Shell initialized.\n");
        wait(NULL);
    } else {
        perror("fork");
    }
}

int main () {
    printf("Initializing...");
    return 0;
}
