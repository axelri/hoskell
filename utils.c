#include "defines.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

const char *prompt = "> ";

void blockingwait(pid_t pid) {
    pid_t ret;
    int status;
    /* wait for pid until exited */
    while(TRUE) {
        ret = waitpid(pid, &status, WUNTRACED);
        if (ret > 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
            return;
        }
        if (ret > 0 && WIFSTOPPED(status)) {
            printf("Process %d suspended\n", ret);
            return;
        }
    }
}

void type_prompt() {
    printf("%s", prompt);
}

void print_child(pid_t pid) {
    printf("Process %d finished\n", pid);
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

/*
 * Parent handler for SIGTERM
 */
void parent_sigterm(int signal_code) {
    if (DEBUG) {
        printf("\nSIGTERM\n");
        return;
    }
    printf("\n");
}

void parent_sigint(int signal_code) {
    if (DEBUG) {
        printf("\nSIGINT\n");
        return;
    }
    printf("\n");
}

/*
 * Parent handler for SIGTSP
 */
void parent_sigtstp(int signal_code) {
    if (DEBUG) {
        printf("\nSIGTSTP\n");
        return;
    }
    printf("\n");
}

/*
 * Parent handler for SIGCHLD
 * only registered if signal
 * flag is set
 */
void parent_sigchld(int signal_code) {
    pid_t pid;
    int status;

    /* could be already resolved in process, therefore no hang */
    pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
    if (pid > 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
        if (DEBUG) {
            printf("Signal\n");
        }
        print_child(pid);
    }
}

void poll_childs() {
    pid_t pid;
    int status;

    if (DEBUG) {
        printf("Polling\n");
    }
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            print_child(pid);
        }
    }
}

/*
 * Utility function to obtain length
 * of null-terminated token array
 */
int tokens_length(char **tokens) {
    int i;
    char *token;

    i = 0;
    token = tokens[0];
    while (NULL != token) {
        i += 1;
        token = tokens[i];
    }
    return i;
}

/*
 * Splits the string command on the
 * character delim in returns an array of
 * the tokens
 *
 * modifies input arguments in-place
 * return value must be freed
 */
char** tokenize(char *command, char delim) {
    char *cs;
    char **ret;
    char *beg;
    int len;
    int tokens = 0;
    int i = 0;

    len = strlen(command);

    /* trim right */
    while(command[len-1] == '\n' || command[len-1] == ' ') {
        command[len-1] = '\0';
        len -= 1;
    }

    /* trim left */
    while(command[0] == '\n' || command[0] == ' ') {
        command[0] = '\0';
        len -= 1;
        command += 1;
    }

    /* count middle arguments */
    cs = strchr(command, delim);
    while (cs != NULL) {
        tokens += 1;
        cs = strchr(cs+1, delim);
    }

    tokens += 1;
    ret = malloc(sizeof(char**)*tokens+1);

    if (tokens == 1) {
        ret[0] = command;
    } else {
        /* one or more args */
        /* trim middle args from space */
        beg = command;
        cs = strchr(beg, delim);
        i = 0;
        while(cs != NULL) {
            /* null-terminate every token string */
            *cs = '\0';
            ret[i] = beg;

            /* prepare next iteration */
            i += 1;
            beg = cs + 1;
            cs = strchr(beg, delim);
        }
        /* last token is already null-terminated
         * since 'command' is a string */
        ret[i] = beg;
    }
    ret[i+1] = NULL;

    return ret;
}

/*
 * Parse every command in the pipes
 * string array, start a process for each
 * command and link them together with pipes
 *
 * return value needs to be freed
 */
pid_t * setup_pipes(char **pipes) {
    int i, len;
    pid_t pid;
    pid_t *children;
    char **args;
    char *path;
    int prev_p[2], new_p[2];

    len = tokens_length(pipes);
    children = malloc(sizeof(pid_t)*(len+1));

    new_p[PIPE_READ] = 0;
    new_p[PIPE_WRITE] = 0;

    for (i = 0; i < len; i++) {
        /* execute this command */
        if (DEBUG) {
            printf("Starting pipe [%s] ...\n", pipes[i]);
        }
        args = tokenize(pipes[i], ' ');
        path = args[0];

        prev_p[PIPE_READ] = new_p[PIPE_READ];
        prev_p[PIPE_WRITE] = new_p[PIPE_WRITE];

        /* do not create more pipes at the end */
        if ((len-1) != i) {
            if (pipe(new_p) == -1) {
                perror("Cannot create new pipe");
                exit(1);
            }
        }

        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Unable to fork\n");
            exit(1);
        }

        if (pid != 0) {
            /* if in first pipe, there is no read entry to close */
            if (0 != i) {
                /* close the old pipe read entry in main process
                 * the old pipe write entry is already closed */
                if (close(prev_p[PIPE_READ]) == -1) {
                    perror("Cannot close old pipe read in parent\n");
                    exit(1);
                }
            }

            /* no new pipe created if at end */
            if ((len-1) != i) {
                /* close the new pipe write in main process
                 * do not close read yet, as it is needed later by a child */
                if (close(new_p[PIPE_WRITE]) == -1) {
                    perror("Cannot close new pipe write in parent\n");
                    exit(1);
                }
            }

            children[i] = pid;
            free(args);
        } else {

            /* always read from stdin at start */
            if (0 != i) {
                /* redirect stdin of previous child to pipe */
                if (dup2(prev_p[PIPE_READ], STDIN_FILENO) == -1) {
                    perror("Cannot dubplicate prev pipe and stdin");
                    /* try to clean up before exiting */
                    close(prev_p[PIPE_READ]);
                    close(new_p[PIPE_WRITE]);
                    exit(1);
                }
            }

            /* always write to stdout at end */
            if ((len-1) != i) {
                /* redirect output to pipe of next child */
                if (dup2(new_p[PIPE_WRITE], STDOUT_FILENO) == -1) {
                    perror("Cannot dubplicate prev pipe and stdout");
                    close(prev_p[PIPE_READ]);
                    close(new_p[PIPE_WRITE]);
                    exit(1);
                }
            }

            if (execvp(path, args) == -1) {
                printf("Error: %s\n", strerror(errno));
                close(prev_p[PIPE_READ]);
                close(new_p[PIPE_WRITE]);
                exit(1);
            }
        }
    }

    return children;
}

/*
 * Run the commands in pipes and wait for the
 * child processes to finish. Return instantly
 * if the background flag is set
 */
void fork_and_run(char **pipes, int bg) {
    int len, i;
    long elapsed;
    pid_t *children;
    struct timeval t0, t1;

    /* setup jkchild processes and link together with pipes */
    children = setup_pipes(pipes);

    /* don't wait if background flag is set */
    if (TRUE == bg) {
        free(children);
        return;
    }

    len = tokens_length(pipes);

    /* block SIGCHLD signal, as we want the wait to be
     * handled synchronously */
    sighold(SIGCHLD);

    gettimeofday(&t0, 0);

    /* wait for each child in order */
    for (i = 0; i < len; i++) {
        blockingwait(children[i]);
    }

    gettimeofday(&t1, 0);

    elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
    printf("Execution time: %0.2f ms\n", (float)(elapsed/1000.0));
    sigrelse(SIGCHLD);

    free(children);
}
