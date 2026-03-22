#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[], char *envp[]) {
    if(argc == 1) {
        fprintf(stderr, "USAGE: yadb [TARGET] [ARGS]\n");
        return 1;
    }


    pid_t debugee = fork();
    if(debugee == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);

        if(execve(argv[1], argv + 1, envp) == -1)
            perror("execve\n");
    } else {
        int status;

        wait(&status);
        printf("Program status: %d\n", status);
    }

    return 0;
}
