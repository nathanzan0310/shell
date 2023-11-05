#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#include "parsing.c"
#include "./jobs.h"

int install_handler(int sig, void (*handler)(int));

void sigint_handler(int sig);

void sigtstp_handler(int sig);

//void sigquit_handler(int sig);

int counter;

int main(void) {
    /* TODO: everything! */
    char buf[1024];
    char *tokens[512];
    char *argv[512];
    int redirects[512];  // int array of redirect char indexes, excluding file
    // destinations
    int argc;
    int n = 1;
    int stdin_copy = dup(STDIN_FILENO);
    int stdout_copy = dup(STDOUT_FILENO);
    int fileIn;   // new input file
    int fileOut;  // new output file
    sigset_t old;
    sigset_t full;
    sigfillset(&full);

    // Ignore signals while installing handlers
    sigprocmask(SIG_SETMASK, &full, &old);

    // Install signal handlers
    if (install_handler(SIGINT, &sigint_handler))
        perror("Warning: could not install handler for SIGINT");

    if (install_handler(SIGTSTP, &sigtstp_handler))
        perror("Warning: could not install handler for SIGTSTP");

//    if (install_handler(SIGQUIT, &sigquit_handler))
//        perror("Warning: could not install handler for SIGQUIT");

    // Restore signal mask to previous value
    sigprocmask(SIG_SETMASK, &old, NULL);

    while (n > 0) {
#ifdef PROMPT
        if (printf("33sh> ") < 0) exit(1);
        if (fflush(stdout) < 0) exit(1);
#endif
        n = (int) read(STDIN_FILENO, buf, sizeof(buf));
        buf[n] = '\0';
        memset(tokens, '\0', 512 * 4);  // reset tokens, argv, and redirects
        // every time we loop through
        memset(argv, '\0', 512 * 4);
        memset(redirects, -1,
               512 * 4);  // redirects must be set to -1 due to overlap with
        // possible redirect index values (e.g. 0)
        argc = 0;
        parse(buf, tokens, argv, redirects);


        if (redirects[0] != -1) {
            int i = 0;
            int oc = 0;   // output redirect counter
            int ic = 0;   // input redirect counter
            int err = 0;  // redirect error flag
            for (; redirects[i] != -1 && err == 0; i++) {
                if (strcmp(tokens[redirects[i]], ">") == 0 ||
                    strcmp(tokens[redirects[i]], ">>") == 0) {
                    oc++;
                }
                if (strcmp(tokens[redirects[i]], "<") == 0) {
                    ic++;
                }
                if (oc > 1 || ic > 1) {
                    err = 1;
                    fprintf(stderr,
                            "syntax error: too many output/input redirects\n");
                }
                if (tokens[redirects[i] + 1] == NULL) {
                    err = 1;
                    fprintf(stderr, "syntax error: no input/output file\n");
                } else {
                    if (argv[0] == NULL) {
                        err = 1;
                        fprintf(stderr, "syntax error: redirects with no command\n");
                    }
                }
            }
            if (err) continue;
            for (int j = 0; redirects[j] != -1; j++) {
                if (strcmp(tokens[redirects[j]], "<") == 0) {
                    if (close(STDIN_FILENO) < 0) {
                        fprintf(stderr, "Error closing stdin\n");
                        break;
                    }
                    if ((fileIn = open(tokens[redirects[j] + 1], O_RDONLY)) <
                        0) {
                        dup2(stdin_copy, 0);
                        fprintf(stderr, "open: No such file or directory\n");
                        break;
                    }
                } else if (strcmp(tokens[redirects[j]], ">") == 0) {
                    if (close(STDOUT_FILENO) < 0) {
                        fprintf(stderr, "Error closing stdout\n");
                        break;
                    }
                    if ((fileOut = open(tokens[redirects[j] + 1],
                                        O_RDWR | O_CREAT | O_TRUNC, 0777)) <
                        0) {
                        dup2(stdout_copy, 1);
                        fprintf(stderr, "open: No such file or directory\n");
                        break;
                    }
                } else if (strcmp(tokens[redirects[j]], ">>") == 0) {
                    if (close(STDOUT_FILENO) < 0) {
                        fprintf(stderr, "Error closing stdout\n");
                        break;
                    }
                    if ((fileOut = open(tokens[redirects[j] + 1],
                                        O_RDWR | O_CREAT | O_APPEND, 0777)) <
                        0) {
                        dup2(stdout_copy, 1);
                        fprintf(stderr, "open: No such file or directory\n");
                        break;
                    }
                }
            }
        }
        if (!tokens[0])
            continue;
        else if (strcmp(tokens[0], "exit") == 0) {  // builtin functions: exit
            if (tokens[1] != NULL) {
                fprintf(stderr, "%s: syntax error\n", tokens[0]);
            } else
                exit(0);
        } else if (strcmp(tokens[0], "cd") == 0) {  // builtin functions: cd
            while (argv[argc] != NULL) argc++;
            if (argc != 2) {
                fprintf(stderr, "%s: syntax error\n", tokens[0]);
                continue;
            }
            size_t l = strlen(argv[1]);
            if (l > 0 && argv[1][l - 1] == '/') argv[1][l - 1] = '\0';
            DIR *d = opendir(".");
            if (d == NULL) {
                perror("Error opening directory");
                exit(1);
            }
            int found = 0;
            struct dirent *di;
            while ((di = readdir(d)) != NULL) {
                if (strcmp(di->d_name, argv[1]) == 0) {
                    found = 1;
                    break;
                }
            }
            closedir(d);
            if (found) {
                if (chdir(argv[1]) != 0) {
                    perror("Error changing directory");
                    exit(1);
                }
            } else {
                fprintf(stderr, "%s: No such file or directory.\n", tokens[0]);
                continue;
            }
        } else if (strcmp(tokens[0], "ln") == 0) {  // builtin functions: ln
            while (argv[argc] != NULL) argc++;
            if (argc != 3) {
                fprintf(stderr, "%s: syntax error\n", tokens[0]);
                continue;
            }
            size_t l1 = strlen(argv[1]);
            if (l1 > 0 && argv[1][l1 - 1] == '/') {
                fprintf(stderr, "%s: Not a directory\n", tokens[0]);
                continue;
            }
            size_t l2 = strlen(argv[2]);
            if (l2 > 0 && argv[2][l2 - 1] == '/') argv[1][l2 - 1] = '\0';
            if (link(argv[1], argv[2]) < 0) {
                fprintf(stderr, "%s: No such file or directory.\n", tokens[0]);
                continue;
            }
        } else if (strcmp(tokens[0], "rm") == 0) {  // builtin functions: rm
            while (argv[argc] != NULL) argc++;
            if (argc != 2) {
                fprintf(stderr, "%s: syntax error\n", tokens[0]);
                continue;
            }
            if (unlink(argv[1]) < 0) {
                fprintf(stderr, "%s: No such file or directory.\n", tokens[0]);
                continue;
            }
        } else {
            if (!fork()) {
                int filepath = 0;  // filepath index accounting for redirects
                for (int i = 0; redirects[i] != -1; i++) {
                    if (redirects[i] == 0) {
                        filepath += 2;
                    }
                }
                execv(tokens[filepath], argv);
                perror("execv");
                exit(1);
            }
            wait(0);
            close(fileIn);
            close(fileOut);
            dup2(stdin_copy, STDIN_FILENO);
            dup2(stdout_copy, STDOUT_FILENO);
        }
    }
    return 0;
}

/* install_handler
 * Installs a signal handler for the given signal
 * Returns 0 on success, -1 on error
 */
int install_handler(int sig, void (*handler)(int)) {
    // TODO: Use sigaction() to install a the given function
    // as a handler for the given signal.
    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    if (sigaction(sig, &act, NULL) < 0) {
        perror("Error installing handler");
        return -1;
    }
    return 0;
}

/* sigint_handler
 * Respond to SIGINT signal (CTRL-C)
 *
 * Argument: int sig - the integer code representing this signal
 */
void sigint_handler(int sig) {
    // printf("Caught signal %d", sig);
    signal(sig, SIG_IGN);
}

/* sigtstp_handler
 * Respond to SIGTSTP signal (CTRL-Z)
 *
 * Argument: int sig - the integer code representing this signal
 */
void sigtstp_handler(int sig) {
    // printf("Caught signal %d", sig);
    signal(sig, SIG_IGN);
}

/* sigquit_handler
 * Catches SIGQUIT signal (CTRL-\)
 *
 * Argument: int sig - the integer code representing this signal
 */
//void sigquit_handler(int sig) {
//     printf("Caught signal %d", sig);
//    signal(sig, sig_ign);
//}
