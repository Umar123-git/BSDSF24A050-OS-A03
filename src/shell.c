#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        const char* dir = arglist[1] ? arglist[1] : getenv("HOME");
        if (dir == NULL || chdir(dir) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        - terminate the shell\n");
        printf("  cd <dir>    - change working directory\n");
        printf("  help        - show this help message\n");
        printf("  jobs        - list running background jobs\n");
        printf("  history     - show command history\n");
        printf("  set         - show all shell variables\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    if (handle_variable(arglist)) return 1;

    return 0;
}

static char* history[HISTORY_SIZE];
static int history_count = 0;

void add_to_history(const char* cmd) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) history[i - 1] = history[i];
        history_count--;
    }
    history[history_count++] = strdup(cmd);
}

void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

char* shell_history_expand(const char* cmdline) {
    char* end;
    long n = strtol(cmdline + 1, &end, 10);
    if (end == cmdline + 1 || *end != '\0') {
        fprintf(stderr, "!: invalid usage, use !n\n");
        return NULL;
    }
    if (n < 1 || n > history_count) {
        fprintf(stderr, "!%ld: event not found\n", n);
        return NULL;
    }
    return strdup(history[n - 1]);
}


static int is_op_char(char c) {
    return c == '<' || c == '>' || c == '|' || c == ';' || c == '&';
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL) return NULL;

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0') break;

        char* tok = (char*)malloc(strlen(cp) + 2);
        int len = 0;

        if (is_op_char(*cp)) {
            tok[len++] = *cp++;            // operator is its own token
        } else {
            char quote = 0;
            while (*cp != '\0') {
                if (quote) {
                    if (*cp == quote) { quote = 0; cp++; }
                    else tok[len++] = *cp++;
                } else {
                    if (*cp == '"' || *cp == '\'') { quote = *cp; cp++; }
                    else if (*cp == ' ' || *cp == '\t' || is_op_char(*cp)) break;
                    else tok[len++] = *cp++;
                }
            }
        }
        tok[len] = '\0';
        arglist[argnum++] = tok;
    }

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }
    arglist[argnum] = NULL;
    return arglist;
}


// ---------- shell variables (linked list) ----------
typedef struct var_node {
    char* name;
    char* value;
    struct var_node* next;
} var_node;

static var_node* var_head = NULL;

void set_var(const char* name, const char* value) {
    var_node* tail = NULL;
    for (var_node* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {      // already exists: update
            char* copy = strdup(value);
            free(v->value);
            v->value = copy;
            return;
        }
        tail = v;
    }
    var_node* node = (var_node*)malloc(sizeof(var_node));
    node->name = strdup(name);
    node->value = strdup(value);
    node->next = NULL;
    if (tail == NULL) var_head = node;
    else tail->next = node;
}

// Shell variables first, then environment variables (e.g. $HOME)
const char* get_var(const char* name) {
    for (var_node* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) return v->value;
    }
    return getenv(name);
}

void print_vars(void) {
    for (var_node* v = var_head; v != NULL; v = v->next) {
        printf("%s=%s\n", v->name, v->value);
    }
}

// Handles "set" and assignments like NAME=value. Returns 1 if handled.
int handle_variable(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) return 0;

    if (strcmp(arglist[0], "set") == 0) {
        print_vars();
        return 1;
    }

    char* eq = strchr(arglist[0], '=');
    if (eq == NULL || eq == arglist[0] || arglist[1] != NULL) return 0;
    if (isdigit((unsigned char)arglist[0][0])) return 0;

    for (char* p = arglist[0]; p < eq; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) return 0;
    }

    *eq = '\0';                            // split into name and value
    set_var(arglist[0], eq + 1);
    *eq = '=';
    return 1;
}

// Tokenize a line, split it at ';' and '&', expand $VARs, run each part in order
int run_commandline(char* cmdline) {
    char** tokens = tokenize(cmdline);
    if (tokens == NULL) return 0;

    int last_status = 0;
    int i = 0;
    while (tokens[i] != NULL) {
        char* seg[MAXARGS + 1];
        int n = 0;
        int start = i;

        while (tokens[i] != NULL && strcmp(tokens[i], ";") != 0 && strcmp(tokens[i], "&") != 0) {
            seg[n++] = tokens[i++];
        }

        int background = 0;
        if (tokens[i] != NULL) {
            background = (strcmp(tokens[i], "&") == 0);
            i++;                           // skip the ; or &
        }
        seg[n] = NULL;

        if (n == 0) continue;              // empty part, e.g. ";;"

        // variable expansion: replace words starting with $ by their value
        for (int k = 0; k < n; k++) {
            if (seg[k][0] == '$' && seg[k][1] != '\0') {
                const char* val = get_var(seg[k] + 1);
                char* expanded = strdup(val ? val : "");
                free(tokens[start + k]);
                tokens[start + k] = expanded;
                seg[k] = expanded;
            }
        }

        if (handle_builtin(seg)) continue;
        last_status = execute(seg, background);
    }

    for (int k = 0; tokens[k] != NULL; k++) free(tokens[k]);
    free(tokens);
    return last_status;
}
