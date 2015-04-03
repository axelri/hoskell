#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define TRUE 1
#define LIMIT 80

const char *prompt = "> ";

void type_prompt() {
    printf("%s", prompt);
}

void read_command() {
    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
}

void exec_command(int tokens, char *buf) {
    char *args[(LIMIT/2)+1];
    char *prog;
    int i, status, pid;
    clock_t t1, t2;

    char *base = "/bin/";
    char *full_path;
    char *envp[] = {NULL};

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

    printf("Prog: %s\n", prog);
    printf("Args: %d", tokens-1);
    for(i = 1; i < tokens+1; i++) {
        printf(", %p", args[i]);
        if (args[i] != NULL) {
            printf(" (%s)", args[i]);
        }
    }
    printf("\n");

    pid = fork();
    if (pid < 0) {
        printf("Unable to fork");
        return;
    }

    if (pid != 0) {
        t1 = clock();
        printf("Waiting in parent for %d\n", pid);
        waitpid(pid, &status, 0); /* wait for child */
        t2 = clock();
        printf("Execution time: %.2f ms\n", 1000.0*(t2-t1)/CLOCKS_PER_SEC);
    } else {
        printf("Executing from child\n");

        /* Build full path */
        full_path = malloc(strlen(base)+strlen(prog)+1);
        full_path[0] = '\0';
        strcat(full_path, base);
        strcat(full_path, prog);

        args[0] = full_path; /* by convention */

        printf("Executing [%s] ...\n", full_path);
        if (execve(full_path, args, envp) == -1) {
            printf("Error: %s\n", strerror(errno));
        } else {
            printf("Executing..\n");
        }
    }
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT+1];
    int tokens;
    char *read, *cs;
    char *exit_str = "exit";

    while (TRUE) {
        read = NULL;
        cs = NULL;
        tokens = 0;

        type_prompt();
        read = fgets(linebuf, LIMIT, stdin);
        if (read == linebuf) {
            printf("Read: %s", linebuf);
        } else {
            printf("Read nothing\n");
        }

        /* handle shell commands */
        if (linebuf[0] == '\n') {
            continue;
        }

        if (strncmp(linebuf, exit_str, strlen(exit_str)) == 0) {
            printf("Bye!");
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
