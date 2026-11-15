#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define MAX_LEN 512
#define MAXARGS 64
#define ARGLEN 30
#define PROMPT "FCIT> "

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist, int background);
int run_commandline(char* cmdline);
void add_job(pid_t pid, const char* cmd);
void reap_jobs(void);
void print_jobs(void);

int handle_builtin(char** arglist);

#define HISTORY_SIZE 50
void add_to_history(const char* cmd);
void print_history(void);
char* shell_history_expand(const char* cmdline);

int handle_variable(char** arglist);
void set_var(const char* name, const char* value);
const char* get_var(const char* name);
void print_vars(void);

#endif // SHELL_H
