#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

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

        run_commandline(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
