#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>

#define TRUE 1
#define LIMIT 80

char *prompt = "> ";

void type_prompt() {
    printf("%s", prompt);
}

void read_command() {
    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
}

void exec_command(int tokens, char *buf) {
    char *args[LIMIT/2];
    char *prog;
    int i, status, pid;

    char *base = "/bin/";
    char *full_path;
    char *envp[] = {NULL};
    char *path[2];

    if (tokens == 0) {
        /* no args */
        prog = strtok(buf, "\n");
        i = 1;
    } else {
        /* one or more args */
        prog = strtok(buf, " ");
        for (i = 1; i < tokens; i++) {
            args[i] = strtok(NULL, " ");
        }
        /* trim last arg */
        args[i] = strtok(NULL, "\n");
        i += 1;
    }

    /* must terminate with null pointer */
    args[i+1] = NULL;

    printf("Prog: %s\n", prog);
    printf("Args: %d", tokens);
    for(i = 1; i < tokens+5; i++) {
        printf(", %p", args[i]);
    }
    printf("\n");

    pid = fork();
    if (pid < 0) {
        printf("Unable to fork");
        return;
    }

    if (pid != 0) {
        printf("Waiting in parent for %d\n", pid);
        waitpid(pid, &status, 0); /* wait for child */
        printf("Done waiting\n");
    } else {
        printf("Executing from child\n");

        /* Build full path */
        full_path = malloc(strlen(base)+strlen(prog)+1);
        full_path[0] = '\0';
        strcat(full_path, base);
        strcat(full_path, prog);

        args[0] = full_path; /* by convention */
        path[0] = full_path;
        path[1] = NULL;

        printf("Executing [%s] ...\n", full_path);
        if (execve(path[0], args, envp) == -1) {
            printf("Error: %s\n", strerror(errno));
        } else {
            printf("Executing..\n");
        }
    }
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT];
    int tokens = 0;
    char *read;
    char *cs;

    while (TRUE) {
        type_prompt();
        read = fgets(linebuf, LIMIT, stdin);
        if (read == linebuf) {
            printf("Read: %s", linebuf);
        } else {
            printf("Read nothing\n");
        }

        cs = strchr(linebuf, ' ');
        while (cs != NULL) {
            tokens += 1;
            cs = strchr(cs+1, ' ');
        }

        exec_command(tokens, linebuf);
    }
    return 0;
}
