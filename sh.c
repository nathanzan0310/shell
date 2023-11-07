#include <dirent.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#include "./jobs.h"
#include "childReaper.c"
#include "parsing.c"
#include "redirectsErrorChecker.c"
#include "syntaxErrorChecker.c"

int main(void) {
    /* TODO: everything! */
    char buf[1024];
    char *tokens[512];
    char *argv[512];
    int redirects[512];  // int array of redirect char indexes, excluding file
    // destinations
    char *command[1] = {""};  // command string
    int argc;
    ssize_t bytesRead = 1;
    int fileIn;        // new input file
    int fileOut;       // new output file
    int filepath = 0;  // filepath index accounting for redirects
    int background;    // background flag
    int jid = 1;       // job id
    int stdin_copy = dup(STDIN_FILENO);
    int stdout_copy = dup(STDOUT_FILENO);
    job_list_t *job_list = init_job_list();
    if (signal(SIGINT, SIG_IGN) ==
        SIG_ERR) {  // Ignore following signals when no foreground process
        perror("SIGINT ignore error.");
        cleanup_job_list(job_list);
        exit(0);
    }
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        perror("SIGTSTP handler error.");
        cleanup_job_list(job_list);
        exit(0);
    }
    if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
        perror("SIGTTOU handler error.");
        cleanup_job_list(job_list);
        exit(0);
    }
    while (bytesRead > 0) {
        childReaper(job_list);  // reap zombie processes
#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            cleanup_job_list(job_list);
            exit(0);
        }
        if (fflush(stdout) < 0) {
            cleanup_job_list(job_list);
            exit(0);
        }
#endif
        bytesRead = read(STDIN_FILENO, buf, sizeof(buf));
        buf[bytesRead] = '\0';
        if (bytesRead == -1) {
            cleanup_job_list(job_list);
            exit(0);
        }
        if (bytesRead == 0) {
            cleanup_job_list(job_list);
            exit(0);
        }
        memset(tokens, '\0', 512 * 4);  // reset tokens, argv, and redirects
        // every time we loop through
        memset(argv, '\0', 512 * 4);
        memset(redirects, -1,
               512 * 4);  // redirects must be set to -1 due to overlap with
        // possible redirect index values (e.g. 0)
        argc = 0;        // reset argc
        background = 0;  // reset background flag
        parse(buf, tokens, argv, redirects);
        while (argv[argc] != NULL) argc++;  // argv count set to argc
        if (redirects[0] != -1) {           // checking for redirects
            if (redirectsErrorChecker(tokens, redirects, argv, job_list) == -1)
                continue;
            for (int j = 0; redirects[j] != -1; j++) {
                if (strcmp(tokens[redirects[j]], "<") == 0) {
                    if (close(STDIN_FILENO) < 0) {
                        if (fprintf(stderr, "Error closing stdin\n") < 0) {
                            perror("Error printing closing stdin error");
                            cleanup_job_list(job_list);
                            exit(0);
                        }
                        break;
                    }
                    if ((fileIn = open(tokens[redirects[j] + 1], O_RDONLY)) <
                        0) {
                        dup2(stdin_copy, 0);
                        if (fprintf(stderr,
                                    "open: No such file or directory\n") < 0) {
                            perror(
                                "Error printing no such file or directory "
                                "error");
                            cleanup_job_list(job_list);
                            exit(0);
                        }
                        break;
                    }
                } else if (strcmp(tokens[redirects[j]], ">") == 0) {
                    if (close(STDOUT_FILENO) < 0) {
                        if (fprintf(stderr, "Error closing stdout\n") < 0) {
                            perror("Error printing closing stdout error");
                            cleanup_job_list(job_list);
                            exit(0);
                        }
                        break;
                    }
                    if ((fileOut = open(tokens[redirects[j] + 1],
                                        O_RDWR | O_CREAT | O_TRUNC, 0777)) <
                        0) {
                        dup2(stdout_copy, 1);
                        if (fprintf(stderr,
                                    "open: No such file or directory\n") < 0) {
                            perror("Error printing no such file or directory");
                            cleanup_job_list(job_list);
                            exit(0);
                        }
                        break;
                    }
                } else if (strcmp(tokens[redirects[j]], ">>") == 0) {
                    if (close(STDOUT_FILENO) < 0) {
                        if (fprintf(stderr, "Error closing stdout\n") < 0) {
                            perror("Error printing closing stdout error");
                            cleanup_job_list(job_list);
                            exit(0);
                        }
                        break;
                    }
                    if ((fileOut = open(tokens[redirects[j] + 1],
                                        O_RDWR | O_CREAT | O_APPEND, 0777)) <
                        0) {
                        dup2(stdout_copy, 1);
                        if (fprintf(stderr,
                                    "open: No such file or directory\n") < 0) {
                            perror(
                                "Error printing no such file or directory "
                                "error");
                            cleanup_job_list(job_list);
                            exit(0);
                        }
                        break;
                    }
                }
            }
        }
        if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {  // checking for &
            background = 1;         // set background process flag
            argv[argc - 1] = NULL;  // remove & from command line
        }
        if (!tokens[0])
            continue;
        else if (strcmp(tokens[0], "exit") == 0) {  // builtin command: exit
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            else {
                cleanup_job_list(job_list);
                exit(0);
            }
        } else if (strcmp(tokens[0], "cd") == 0) {  // builtin command: cd
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            size_t l = strlen(argv[1]);
            if (l > 0 && argv[1][l - 1] == '/') argv[1][l - 1] = '\0';
            DIR *d = opendir(".");
            if (d == NULL) {
                perror("Error opening directory");
                cleanup_job_list(job_list);
                exit(0);
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
                    cleanup_job_list(job_list);
                    exit(0);
                }
            } else {
                if (fprintf(stderr, "%s: No such file or directory.\n",
                            tokens[0]) < 0) {
                    perror("Error printing no such file or directory error");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                continue;
            }
        } else if (strcmp(tokens[0], "ln") == 0) {  // builtin command: ln
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            size_t l2 = strlen(argv[2]);
            if (l2 > 0 && argv[2][l2 - 1] == '/') argv[1][l2 - 1] = '\0';
            if (link(argv[1], argv[2]) < 0) {
                if (fprintf(stderr, "%s: No such file or directory.\n",
                            tokens[0]) < 0) {
                    perror("Error printing no such file or directory error");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                continue;
            }
        } else if (strcmp(tokens[0], "rm") == 0) {  // builtin command: rm
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            if (unlink(argv[1]) < 0) {
                if (fprintf(stderr, "%s: No such file or directory.\n",
                            tokens[0]) < 0) {
                    perror("Error printing no such file or directory error");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                continue;
            }
        } else if (strcmp(tokens[0], "jobs") == 0) {  // builtin command: job
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            jobs(job_list);
        } else if (strcmp(tokens[0], "bg") == 0) {  // builtin command: bg
            int cur_pid;
            int cur_jid;
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            else {
                cur_jid = (int)strtol(argv[1] + 1, NULL, 10);
                cur_pid = get_job_pid(job_list, cur_jid);
                if (kill(-1 * cur_pid, SIGCONT) == 0) {
                    update_job_pid(job_list, cur_pid, RUNNING);
                } else {
                    if (fprintf(stderr, "%s: kill error\n", tokens[0]) < 0) {
                        perror("Error printing kill error");
                        cleanup_job_list(job_list);
                        exit(0);
                    }
                }
            }
        } else if (strcmp(tokens[0], "fg") == 0) {  // builtin command: fg
            int cur_pid;
            int cur_jid;
            if (syntaxErrorChecker(tokens[0], argv, argc, job_list) == -1)
                continue;
            else {
                cur_jid = (int)strtol(argv[1] + 1, NULL, 10);
                cur_pid = get_job_pid(job_list, cur_jid);
                if (kill(-1 * cur_pid, SIGCONT) == 0) {
                    remove_job_jid(job_list, cur_jid);
                } else {
                    if (fprintf(stderr, "%s: kill error\n", tokens[0]) < 0) {
                        perror("Error printing kill error");
                        cleanup_job_list(job_list);
                        exit(0);
                    }
                }
                int status;
                tcsetpgrp(STDIN_FILENO, cur_pid);
                waitpid(cur_pid, &status, WUNTRACED);
                if (status == -1) {
                    perror("Error waitpid");
                    cleanup_job_list(job_list);
                    exit(0);
                } else if (WIFSIGNALED(status)) {
                    if (printf("(%d) terminated by signal %d\n", cur_pid,
                               WTERMSIG(status)) < 0) {
                        perror("Error printing signal termination.");
                        cleanup_job_list(job_list);
                        exit(0);
                    }
                } else if (WIFSTOPPED(status)) {
                    add_job(job_list, cur_jid, cur_pid, STOPPED, command[0]);
                    if (printf("[%d] (%d) suspended by signal %d\n", cur_jid,
                               cur_pid, WSTOPSIG(status)) < 0) {
                        perror("Error printing signal suspension.");
                        cleanup_job_list(job_list);
                        exit(0);
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        } else {
            for (int i = 0; redirects[i] != -1;
                 i++)  // getting command index while accounting for redirect
                // chars
                if (redirects[i] == 0) filepath += 2;
            if (strcmp(tokens[filepath], "fg") != 0)  // non-fg command setting
                command[0] = tokens[filepath];
            pid_t pid = fork();
            if (!pid) {
                setpgid(pid, getpid());
                if (!background) {
                    tcsetpgrp(STDIN_FILENO, getpgrp());
                }
                if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                    perror("SIGINT child handler error.");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
                    perror("SIGTSTP child handler error.");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                if (signal(SIGTTOU, SIG_DFL) == SIG_ERR) {
                    perror("SIGTTOU child handler error.");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                close(fileIn);
                close(fileOut);
                dup2(stdin_copy, STDIN_FILENO);
                dup2(stdout_copy, STDOUT_FILENO);
                execv(tokens[filepath], argv);
                perror("execv");
                cleanup_job_list(job_list);
                exit(0);
            }
            if (background && tokens[filepath]) {
                if (printf("[%d] (%d)\n", jid, pid) < 0) {
                    perror("Error add job print");
                    cleanup_job_list(job_list);
                    exit(0);
                }
                add_job(job_list, jid, pid, RUNNING, tokens[filepath]);
                jid++;
            } else if (!background) {
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (status == -1) {
                    perror("Error waitpid");
                    cleanup_job_list(job_list);
                    exit(0);
                } else if (WIFSIGNALED(status)) {
                    if (printf("(%d) terminated by signal %d\n", pid,
                               WTERMSIG(status)) < 0) {
                        perror("Error printing signal termination.");
                        cleanup_job_list(job_list);
                        exit(0);
                    }
                } else if (WIFSTOPPED(status)) {
                    add_job(job_list, jid, pid, STOPPED, tokens[filepath]);
                    jid++;
                    if (printf("[%d] (%d) suspended by signal %d\n",
                               get_job_jid(job_list, pid), pid,
                               WSTOPSIG(status)) < 0) {
                        perror("Error printing signal suspension.");
                        cleanup_job_list(job_list);
                        exit(0);
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        }
    }
    return 0;
}
