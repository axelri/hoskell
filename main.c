#define _XOPEN_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define TRUE 1
#define LIMIT 80
#define DEBUG 1
#define SIGDET 1

/* current environment, from unistd */
const char *prompt = "> ";

void type_prompt() {
    printf("%s", prompt);
}

void register_sighandler(int signal_code, void (*handler) (int) ) {
    int ret;
    struct sigaction signal_params;

    signal_params.sa_handler = handler;
    sigemptyset(&signal_params.sa_mask);
    signal_params.sa_flags = 0;

    ret = sigaction(signal_code, &signal_params, (void *) 0);
    if ( -1 == ret) {
        perror("sigaction failed");
        exit(1);
    }
}

void parent_sigterm(int signal_code) {
    if (DEBUG) {
        printf("\nSIGTERM\n");
    }
    return;
}

void parent_sigtstp(int signal_code) {
    if (DEBUG) {
        printf("\nSIGTSTP\n");
    }
    return;
}

/* send signals manually, rather than relying in sigchld */
void parent_sigusr(int signal_code) {
    int status;
    waitpid(-1, &status, 0);
    if (DEBUG) {
        printf("Bg finished with: %d\n", status);
    }
}

void remove_handlers() {
    register_sighandler(SIGTERM, SIG_DFL);
    register_sighandler(SIGTSTP, SIG_DFL);
    register_sighandler(SIGUSR1, SIG_DFL);
}

int supervisor(pid_t parent, char *path, char *args[]) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        printf("Unable to fork\n");
        exit(1);
    }

    if (pid != 0) {
        /* send signal signaling background finished */
        waitpid(pid, &status, 0); /* wait for child */
        return status;


    } else {
        /* execve will overwrite signal handlers */
        if (execvp(path, args) == -1) {
            printf("Error: %s\n", strerror(errno));
        }
        return 0;
    }
}

void fork_background(char *path, char *args[]) {
    pid_t pid, parent;
    int status;
    parent = getpid();

    pid = fork();
    if (pid < 0) {
        printf("Unable to fork\n");
        exit(1);
    }

    if (pid != 0) {
        if (DEBUG) {
            printf("Background process %d\n", pid);
        }
    } else {
        if (DEBUG) {
            printf("Executing [%s] ...\n", path);
        }
        remove_handlers();

        status = supervisor(parent, path, args);
        printf("Begin sending \n");
        if (kill(parent, SIGUSR1) == -1) {
            printf("Error: %s\n", strerror(errno));
        } else {
            printf("Sent signal \n");
        }
        /* to be caught by parent */
        exit(status);
    }

}

void fork_foreground(char *path, char *args[]) {
    clock_t t1, t2;
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        printf("Unable to fork\n");
        exit(1);
    }

    if (pid != 0) {
        t1 = clock();
        if (DEBUG) {
            printf("Waiting in parent for %d\n", pid);
        }
        waitpid(pid, &status, 0); /* wait for child */
        t2 = clock();
        printf("Execution time: %.2f ms\n", 1000.0*(t2-t1)/CLOCKS_PER_SEC);
    } else {
        if (DEBUG) {
            printf("Executing [%s] ...\n", path);
        }
        /* execvp will overwrite signal handlers */
        if (execvp(path, args) == -1) {
            printf("Error: %s\n", strerror(errno));
        }
    }

}

void exec_command(int tokens, char *buf) {
    char *args[(LIMIT/2)+1];
    char *prog;
    int i;

    args[0] = NULL;
    if (tokens == 1) {
        /* no args */
        prog = strtok(buf, "\n");
        i = 1;
    } else {
        /* one or more args */
        /* trim middle args from space */
        prog = strtok(buf, " ");
        for (i = 1; i < tokens-1; i++) {
            args[i] = strtok(NULL, " ");
        }
        /* trim last arg from newline */
        args[i] = strtok(NULL, "\n");
        i += 1;
    }

    /* must terminate with null pointer */
    args[i] = NULL;

    if (DEBUG) {
        printf("Prog: %s\n", prog);
        printf("Args: %d", tokens-1);

        for(i = 1; i < tokens+1; i++) {
            printf(", %p", args[i]);
            if (args[i] != NULL) {
                printf(" (%s)", args[i]);
            }
        }
        printf("\n");
    }

    args[0] = prog; /* by convention */
    if (strncmp(args[tokens-1], "&", 1) == 0) {
        /* background flag not arg to program */
        args[tokens-1] = NULL;
        /* environ = all current env variables */
        fork_background(prog, args);
    } else {
        fork_foreground(prog, args);
    }
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT+1];
    int tokens;
    char *read, *cs;
    const char *exit_str = "exit";

    /* TODO: foolproof? does child inherit? */
    register_sighandler(SIGINT, parent_sigterm);
    register_sighandler(SIGTSTP, parent_sigtstp);
    register_sighandler(SIGUSR1, parent_sigusr);

    while (TRUE) {
        /* empty interrupt cache */
        read = NULL;
        cs = NULL;
        tokens = 0;

        type_prompt();
        read = fgets(linebuf, LIMIT, stdin);
        if (read != linebuf) {
            /* check if the user wants to quit */
            if (feof(stdin)) {
                printf("Bye!\n");
                exit(0);
            }

            /* ignored interrupt */
            continue;
        }

        /* handle shell commands */
        /* end of file - quit */
        if (linebuf[0] == '\n') {
            continue;
        }

        if (strncmp(linebuf, exit_str, strlen(exit_str)) == 0) {
            printf("Bye!\n");
            exit(0);
        }

        /* execute files */
        /* count middle arguments */
        cs = strchr(linebuf, ' ');
        while (cs != NULL) {
            tokens += 1;
            cs = strchr(cs+1, ' ');
        }
        /* last arg ends with newline */
        tokens += 1;

        exec_command(tokens, linebuf);
    }
    return 0;
}
