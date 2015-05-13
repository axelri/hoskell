#ifndef _UTILS_H
#define _UTILS_H

#include "defines.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

void type_prompt();
void print_child(pid_t pid);
void register_sighandler(int,  void (*) (int));
void parent_sigterm(int);
void parent_sigint(int);
void parent_sigtstp(int);
void parent_sigchld(int);
void poll_childs();
int tokens_length(char**);
char** tokenize(char*, char);
pid_t* setup_pipes(char **);
void fork_and_run(char**, int);

#endif
