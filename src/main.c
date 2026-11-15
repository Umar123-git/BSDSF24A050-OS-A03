#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define MAXBLOCK 32

// Remove leading/trailing spaces in place; returns pointer to first non-space
static char* trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
    *e = '\0';
    return s;
}

static int is_if_line(const char* s) {
    return strncmp(s, "if", 2) == 0 && (s[2] == '\0' || s[2] == ' ' || s[2] == '\t');
}

// Read lines until 'fi', store them in cond/then/else blocks, then run
static void handle_if(char* firstline) {
    char* cond[MAXBLOCK];
    char* thenb[MAXBLOCK];
    char* elseb[MAXBLOCK];
    int nc = 0, nt = 0, ne = 0;
    int state = 0;                         // 0 = condition, 1 = then, 2 = else
    int err = 0;

    char* rest = trim(firstline + 2);
    if (*rest == '\0') {
        fprintf(stderr, "syntax error: missing condition after 'if'\n");
        err = 1;
    } else {
        cond[nc++] = strdup(rest);
    }

    while (1) {
        char* raw = readline("> ");
        if (raw == NULL) {
            fprintf(stderr, "syntax error: unexpected end of input, missing 'fi'\n");
            err = 1;
            break;
        }
        char* line = trim(raw);

        if (strcmp(line, "fi") == 0) {
            if (state == 0) {
                fprintf(stderr, "syntax error: 'fi' before 'then'\n");
                err = 1;
            }
            free(raw);
            break;
        } else if (strcmp(line, "then") == 0) {
            if (state != 0) {
                fprintf(stderr, "syntax error: unexpected 'then'\n");
                err = 1;
            } else {
                state = 1;
            }
        } else if (strcmp(line, "else") == 0) {
            if (state != 1) {
                fprintf(stderr, "syntax error: unexpected 'else'\n");
                err = 1;
            } else {
                state = 2;
            }
        } else if (*line != '\0') {
            char** block = (state == 0) ? cond : (state == 1) ? thenb : elseb;
            int* cnt = (state == 0) ? &nc : (state == 1) ? &nt : &ne;
            if (*cnt == MAXBLOCK) {
                fprintf(stderr, "syntax error: block too long\n");
                err = 1;
            } else {
                block[(*cnt)++] = strdup(line);
            }
        }
        free(raw);
    }

    if (!err) {
        int status = 0;
        for (int i = 0; i < nc; i++) status = run_commandline(cond[i]);

        char** run = (status == 0) ? thenb : elseb;
        int n = (status == 0) ? nt : ne;
        for (int i = 0; i < n; i++) run_commandline(run[i]);
    }

    for (int i = 0; i < nc; i++) free(cond[i]);
    for (int i = 0; i < nt; i++) free(thenb[i]);
    for (int i = 0; i < ne; i++) free(elseb[i]);
}

int main() {
    char* cmdline;

    while (1) {
        reap_jobs();                       // collect finished background jobs
        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

        // !n: replace with n-th history command before tokenizing/storing
        if (cmdline[0] == '!') {
            char* expanded = shell_history_expand(cmdline);
            free(cmdline);
            if (expanded == NULL) continue;
            cmdline = expanded;
            printf("%s\n", cmdline);
        }

        if (cmdline[0] != '\0') {
            add_to_history(cmdline);       // our own history (history, !n)
            add_history(cmdline);          // readline history (up/down arrows)
        }

        if (is_if_line(cmdline))
            handle_if(cmdline);
        else
            run_commandline(cmdline);

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
