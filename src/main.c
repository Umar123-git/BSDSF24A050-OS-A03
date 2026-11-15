#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = readline(PROMPT)) != NULL) {
        // !n: replace with n-th history command before tokenizing/storing
        if (cmdline[0] == '!') {
            char* expanded = shell_history_expand(cmdline);
            free(cmdline);
            if (expanded == NULL) continue;
            cmdline = expanded;
            printf("%s\n", cmdline);
        }

        if (cmdline[0] != '\0') {
            add_to_history(cmdline);   // our own history (history, !n)
            add_history(cmdline);      // readline history (up/down arrows)
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0)
                execute(arglist);

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
