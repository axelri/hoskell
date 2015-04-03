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

const char *prompt = "> ";
pid_t childpid;

void type_prompt() {
    printf("%s", prompt);
}

void register_sighandler(int signal_code, void (*handler) (int sig) ) {
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

void child_handler(int signal_code) {
    printf("Child interrupt\n");
}

void parent_handler(int signal_code) {
    int ret;

    if (childpid > 0 && SIGINT == signal_code) {
        ret = kill(childpid, SIGKILL);
        if (-1 == ret) {
            perror("kill() failed");
            exit(1);
        }
    }
}

void read_command() {
    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
}

void fork_and_run(char *path, char *args[]) {
    clock_t t1, t2;
    int status;
    char *envp[] = {NULL};

    childpid = fork();
    if (childpid < 0) {
        printf("Unable to fork");
        return;
    }

    if (childpid != 0) {
        register_sighandler(SIGINT, parent_handler);
        t1 = clock();
        printf("Waiting in parent for %d\n", childpid);
        waitpid(childpid, &status, 0); /* wait for child */
        t2 = clock();
        printf("Execution time: %.2f ms\n", 1000.0*(t2-t1)/CLOCKS_PER_SEC);
    } else {
        register_sighandler(SIGINT, child_handler);
        printf("Executing from child\n");
        printf("Executing [%s] ...\n", path);
        if (execve(path, args, envp) == -1) {
            printf("Error: %s\n", strerror(errno));
        }
    }

}

void exec_command(int tokens, char *buf) {
    char *args[(LIMIT/2)+1];
    char *prog;
    int i;

    char *path_env = getenv("PATH");
    char *path, *path_buf;

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

    /* try each path folder */
    path = strtok(path_env, ":");
    while (path != NULL) {
        path_buf = malloc(strlen(path)+strlen("/")+strlen(prog)+1);
        path_buf[0] = '\0';
        strcat(path_buf, path);
        strcat(path_buf, "/");
        strcat(path_buf, prog);
        printf("Trying %s\n", path_buf);

        if (access(path_buf, F_OK) == 0) {
            printf("Found %s\n", path_buf);
            args[0] = path_buf; /* by convention */
            fork_and_run(path_buf, args);

            free(path_buf);
            return;
        }

        path = strtok(NULL, ":");
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
