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
/*
 * Interpret the user input as either
 * a shell command or a program to be run
 */
void exec_command(char **tokens, int bg) {
    char *path, *first, **firstparsed, **checkenv, *pager;
    char *pathenv, *pathcp, **pathtokens;
    char *tmp_str;
    int i;
    pid_t parent;

    /* copy the first command string and handle
     * this case specially - it might be a shell command */
    first = malloc(sizeof(char)*strlen(tokens[0])+1);
    strcpy(first, tokens[0]);
    firstparsed = tokenize(first, ' ');

    if (strcmp(firstparsed[0], "") == 0) {
        free(first);
        free(firstparsed);
        free(tokens);
        return;
    }

    if (strcmp(firstparsed[0], "exit") == 0) {
        free(first);
        free(firstparsed);
        free(tokens);
        /* try to terminate all childs before exiting */

        /* send term to whole group, but ignore in parent */
        register_sighandler(SIGTERM, SIG_IGN);
        parent = getpid();
        sighold(SIGCHLD);

        /* give processes a chance to finish */
        if (kill(-parent, SIGTERM) == -1) {
            printf("Error: %s\n", strerror(errno));
            exit(1);
        }
        sleep(1);

        poll_childs();
        sigrelse(SIGCHLD);

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
        }

        free(first);
        free(firstparsed);
        free(tokens);
        return;
    }

    if (strcmp(firstparsed[0], "checkEnv") == 0) {
        pager = getenv("PAGER");

        if (pager == NULL) {
            pathenv = getenv("PATH");
            pathcp = malloc(strlen(pathenv)+1);
            strcpy(pathcp, pathenv);
            pathtokens = tokenize(pathcp, ':');

            for (i = 0; i < tokens_length(pathtokens); i++) {
                if((tmp_str = malloc(strlen(pathtokens[i])+strlen("/less")+1)) != NULL){
                    tmp_str[0] = '\0';
                    strcat(tmp_str,pathtokens[i]);
                    strcat(tmp_str,"/less");

                    if (access(tmp_str, X_OK) != -1) {
                        pager = "less";
                        free(tmp_str);
                        break;
                    }
                    free(tmp_str);
                }
            }

            if (pager == NULL) pager = "more";
            free(pathcp);
            free(pathtokens);
        }

        if (DEBUG) {
            printf("Pager: %s\n", pager);
        }

        if (tokens_length(firstparsed) > 1) {
            tmp_str = malloc(sizeof(char*) * LIMIT + 1);
            checkenv = malloc(sizeof(char*) * 4 + 1);

            tmp_str[0] = '\0';
            strcat(tmp_str, "grep ");
            for (i = 1; firstparsed[i] != NULL; i++) {
                strcat(tmp_str, firstparsed[i]);
                strcat(tmp_str, " ");
            }
            checkenv[0] = "printenv";
            checkenv[1] = tmp_str;
            checkenv[2] = "sort";
            checkenv[3] = pager;
            checkenv[4] = NULL;
            free(tmp_str);
        } else {
            checkenv = malloc(sizeof(char*) * 3 + 1);
            checkenv[0] = "printenv";
            checkenv[1] = "sort";
            checkenv[2] = pager;
            checkenv[3] = NULL;
        }

        fork_and_run(checkenv, bg);

        free(checkenv);
        free(first);
        free(firstparsed);
        free(tokens);
        return;
    }

    /* run program */
    free(first);
    free(firstparsed);
    fork_and_run(tokens, bg);
    free(tokens);
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT+1];
    char *read;
    char **tokens;
    int len, bg;

    #if SIGDET == 0
    /*pid_t pid;
    int status;*/
    #endif

    register_sighandler(SIGINT, parent_sigint);
    register_sighandler(SIGTSTP, parent_sigtstp);

    #if SIGDET == 1
    register_sighandler(SIGCHLD, parent_sigchld);
    #endif

    while (TRUE) {
        read = NULL;
        tokens = 0;
        bg = FALSE;

        #if SIGDET == 0
        poll_childs();
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
    }
    return 0;
}
