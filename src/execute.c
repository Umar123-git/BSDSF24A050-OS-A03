#include "shell.h"
#include <fcntl.h>

#define MAXCMDS 16

typedef struct {
    char* argv[MAXARGS + 1];
    char* infile;
    char* outfile;
} command_t;

static int is_operator(const char* t) {
    return strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, "|") == 0;
}

// Split tokens into commands (separated by |) and pick out < and > files
static int parse_commands(char** tokens, command_t cmds[], int* ncmds) {
    int cur = 0, argc = 0;
    memset(&cmds[0], 0, sizeof(command_t));

    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            if (argc == 0 || cur == MAXCMDS - 1) {
                fprintf(stderr, "syntax error near '|'\n");
                return -1;
            }
            cmds[cur].argv[argc] = NULL;
            cur++;
            argc = 0;
            memset(&cmds[cur], 0, sizeof(command_t));
        } else if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
            char* target = tokens[i + 1];
            if (target == NULL || is_operator(target)) {
                fprintf(stderr, "syntax error: missing file after '%s'\n", tokens[i]);
                return -1;
            }
            if (tokens[i][0] == '<') cmds[cur].infile = target;
            else cmds[cur].outfile = target;
            i++;
        } else {
            cmds[cur].argv[argc++] = tokens[i];
        }
    }

    if (argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        return -1;
    }
    cmds[cur].argv[argc] = NULL;
    *ncmds = cur + 1;
    return 0;
}

int execute(char* arglist[]) {
    command_t cmds[MAXCMDS];
    pid_t pids[MAXCMDS];
    int ncmds = 0;
    int prev_fd = -1;   // read end of previous pipe

    if (parse_commands(arglist, cmds, &ncmds) < 0) return -1;

    int started = 0;
    for (int i = 0; i < ncmds; i++) {
        int pipefd[2] = {-1, -1};

        if (i < ncmds - 1 && pipe(pipefd) < 0) {
            perror("pipe failed");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            break;
        }

        if (pid == 0) {   // child
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (pipefd[1] != -1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }
            if (cmds[i].infile) {
                int fd = open(cmds[i].infile, O_RDONLY);
                if (fd < 0) { perror(cmds[i].infile); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (cmds[i].outfile) {
                int fd = open(cmds[i].outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(cmds[i].outfile); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("Command not found");
            exit(1);
        }

        // parent
        pids[started++] = pid;
        if (prev_fd != -1) close(prev_fd);
        if (pipefd[1] != -1) close(pipefd[1]);
        prev_fd = pipefd[0];
    }
    if (prev_fd != -1) close(prev_fd);

    int status = 0, last_status = 0;
    for (int i = 0; i < started; i++) {
        waitpid(pids[i], &status, 0);
        if (i == started - 1)
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return last_status;
}
