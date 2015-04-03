#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
    char* args[LIMIT/2];
    char *prog;
    int i;

    prog = strtok(buf, " ");
    for (i = 0; i < tokens-1; i++) {
        args[i] = strtok(NULL, " ");
    }

    printf("Prog: %s\n", prog);
    printf("Args: %d", tokens-1);
    for(i = 0; i < tokens-1; i++) {
        printf(", %s", args[i]);
    }
    printf("\n");
}

int main(int argc, const char *argv[]) {
    char linebuf[LIMIT];
    int tokens = 1;
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
