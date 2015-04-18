#define _XOPEN_SOURCE 500

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
#define FALSE 0
#define LIMIT 80
#define DEBUG 1
#define SIGDET 1
#define PIPE_READ ( 0 )
#define PIPE_WRITE ( 1 )

const char *prompt = "> ";

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
    }
    return;
}

/*
 * Parent handler for SIGTSP
 */
void parent_sigtstp(int signal_code) {
    if (DEBUG) {
        printf("\nSIGTSTP\n");
    }
    return;
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
    pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0) {
        if (DEBUG) {
            printf("Signal\n");
        }
        print_child(pid);
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
                    exit(1);
                }
            }

            /* always write to stdout at end */
            if ((len-1) != i) {
                /* redirect output to pipe of next child */
                if (dup2(new_p[PIPE_WRITE], STDOUT_FILENO) == -1) {
                        perror("Cannot dubplicate prev pipe and stdout");
                        exit(1);
                }
            }

            if (execvp(path, args) == -1) {
                printf("Error: %s\n", strerror(errno));
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
    clock_t t1, t2;
    int status, len, i;
    pid_t *children;

    /* setup child processes and link together with pipes */
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
    t1 = clock();

    /* wait for each child in order */
    for (i = 0; i < len; i++) {
        waitpid(children[i], &status, 0);
    }

    t2 = clock();
    printf("Execution time: %.2f ms\n", 1000.0*(t2-t1)/CLOCKS_PER_SEC);
    sigrelse(SIGCHLD);

    free(children);
}

/*
 * Interpret the user input as either
 * a shell command or a program to be run
 */
void exec_command(char **tokens, int bg) {
    char *path, *first, **firstparsed;
    int len;

    len = tokens_length(tokens);

    /* copy the first command string and handle
     * this case specially - it might be a shell command */
    first = malloc(sizeof(char)*strlen(tokens[0])+1);
    strcpy(first, tokens[0]);
    firstparsed = tokenize(first, ' ');

    if (strcmp(firstparsed[0], "") == 0) {
        return;
    }

    if (strcmp(firstparsed[0], "exit") == 0) {
        printf("Bye!\n");
        exit(0);
    }

    if (strcmp(firstparsed[0], "cd") == 0) {
        if (firstparsed[1] == '\0') {
            /* default dir to change to */
            path = getenv("HOME");
        } else {
            path = firstparsed[1];
        }

        if (DEBUG) {
            printf("Change Directory to %s\n", path);
        }

        if (chdir(path) == -1) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
        };
        return;
    }

    if (strcmp(firstparsed[0], "checkEnv") == 0) {
        if (DEBUG) {
            printf("Checking environment (printenv | sort | less)\n");
        }
        return;
    }

    free(first);
    free(firstparsed);
    fork_and_run(tokens, bg);
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT+1];
    char *read, *cs;
    char **tokens;
    int len, bg;

    #if SIGDET == 0
    pid_t pid;
    int status;
    #endif

    register_sighandler(SIGINT, parent_sigterm);
    register_sighandler(SIGTSTP, parent_sigtstp);

    #if SIGDET == 1
    register_sighandler(SIGCHLD, parent_sigchld);
    #endif

    while (TRUE) {
        read = NULL;
        cs = NULL;
        tokens = 0;
        bg = FALSE;

        #if SIGDET == 0
        /* poll for child process */
        while (TRUE) {
            pid = waitpid(-1, &status, WNOHANG);
            if (pid > 0) {
                if (DEBUG) {
                    printf("Polling\n");
                }
                print_child(pid);
            } else {
                break;
            }
        }
        #endif

        type_prompt();
        read = fgets(linebuf, LIMIT, stdin);
        if (read != linebuf) {
            /* check if the user wants to quit */
            if (feof(stdin)) {
                printf("Bye!\n");
                exit(0);
            }

            /* some interrupt, proceed to next read */
            continue;
        }

        /* trim whitespace right */
        len = strlen(linebuf);
        while (linebuf[len-1] == '\n' || linebuf[len-1] == ' ') {
            linebuf[len-1] = '\0';
            len -= 1;
        }

        /* check for background flag */
        if (linebuf[len-1] == '&') {
            bg = TRUE;
            linebuf[len-1] = '\0';
            len -= 1;
        }

        tokens = tokenize(linebuf, '|');
        exec_command(tokens, bg);
        free(tokens);
    }
    return 0;
}
