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

/* current environment, from unistd */
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
void parent_sigchld(int signal_code) {
    pid_t pid;
    int status;

    /* could be a pending signal which is already resolved */
    pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0) {
        if (DEBUG) {
            printf("Signal\n");
        }
        print_child(pid);
    }
}

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
 * modifies command in-place
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

    /* execute files */
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
            /* make every token a string */
            *cs = '\0';
            ret[i] = beg;
            i += 1;
            beg = cs + 1;
            cs = strchr(beg, delim);
        }
        /* last token is already null-terminated */
        ret[i] = beg;
    }
    ret[i+1] = NULL;

    return ret;
}



void fork_background(char *path, char *args[]) {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Unable to fork\n");
        exit(1);
    }

    if (pid != 0) {
        if (DEBUG) {
            printf("Background process %d\n", pid);
        }
    } else {
        if (execvp(path, args) == -1) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
        }
    }
}

void fork_foreground(char **pipes) {
    clock_t t1, t2;
    pid_t pid;
    int status, len, i;
    pid_t *children;
    int prev_p[2], new_p[2];
    char **args;
    char *path;
    int opened, closed, waited;

    i = 0;
    while (NULL != pipes[i]) i++;
    len = i;
    children = malloc(sizeof(pid_t)*len);

    new_p[PIPE_READ] = 0;
    new_p[PIPE_WRITE] = 0;

    sighold(SIGCHLD);
    t1 = clock();
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
            /* if in first pipe, there is no read to close */
            if (0 != i) {
                /* close the old pipe read, no need for main process
                 * write is already closed */
                if (close(prev_p[PIPE_READ]) == -1) {
                    perror("Cannot close old pipe read in parent\n");
                    exit(1);
                }
            }

            /* no new pipe created if at end */
            if ((len-1) != i) {
                /* close the new pipe write, no need for main process
                 * do not close read yet, as it is needed by a later child */
                if (close(new_p[PIPE_WRITE]) == -1) {
                    perror("Cannot close new pipe write in parent\n");
                    exit(1);
                }
            }

            children[i] = pid;
        } else {
            /* execvp will overwrite signal handlers */

            /* read from stdin at first */
            if (0 != i) {
                /* redirect previous child to stdin */
                if (dup2(prev_p[PIPE_READ], STDIN_FILENO) == -1) {
                    perror("Cannot dubplicate prev pipe and stdin");
                    exit(1);
                }
            }

            /* output to stdout at end */
            if ((len-1) != i) {
                /* redirect output to next child */
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

    /* wait for each child in order */
    for (i = 0; i < len; i++) {
        waitpid(children[i], &status, 0);
    }
    t2 = clock();
    printf("Execution time: %.2f ms\n", 1000.0*(t2-t1)/CLOCKS_PER_SEC);
    sigrelse(SIGCHLD);
}

void exec_command(char **tokens) {
    char *path;
    int len;
    int i;

    len = tokens_length(tokens);
    printf("Tokens: %d\n", len);
    for (i = 0; i < len; i++) {
        printf("Token %d: '%s'\n", i+1, tokens[i]);
    }

    /* handle shell commands */
    if (strcmp(tokens[0], "") == 0) {
        return;
    }

    if (strcmp(tokens[0], "exit") == 0) {
        printf("Bye!\n");
        exit(0);
    }

    /* CD (Change Directory) */
    if (strcmp(tokens[0], "cd") == 0) {
        if (DEBUG) {
            printf("Change Directory to %s\n", tokens[1]);
        }

        if (tokens[1] == '\0') {
            path = getenv("HOME");
        } else {
            path = tokens[1];
        }

        chdir(path);
        return;
    }

    fork_foreground(tokens);
    /* if (strncmp(tokens[len-1], "&", 1) == 0) { */
    /*     /1* background flag not arg to program *1/ */
    /*     tokens[len-1] = NULL; */
    /*     /1* environ = all current env variables *1/ */
    /*     fork_background(tokens[0], tokens); */
    /* } else { */
    /* } */
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT+1];
    char *read, *cs;
    char **tokens, **args;
    char *token, *arg;
    int i, j, len, bg;

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
        /* empty interrupt cache */
        read = NULL;
        cs = NULL;
        tokens = 0;
        bg = FALSE;

        /* this macro expands to 0 if not defined */
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

            /* ignored interrupt */
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
        exec_command(tokens);
        free(tokens);
    }
    return 0;
}
