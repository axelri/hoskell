# hoskell
A simple shell implemented in C. cd, exit and checkEnv are manually implemented commands. cd and exit works as expected. checkEnv executes the command "printenv | sort | pager" if no arguments are passed and if arguments are passed it will execute "printenv | grep <arguments> | sort | pager".

To compile, run the makefile (make lab). It will compile with the following settings:
  clang -D DEBUG=0 -pedantic -Wall -std=c89 -O3 main.o utils.o -o hoskell
and the result will be a runnable file named hoskell.
