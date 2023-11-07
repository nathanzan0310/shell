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

int main(void) {
    /* TODO: everything! */
    char buf[1024];
    char *tokens[512];
    char *argv[512];
    int redirects[512];  // int array of redirect char indexes, excluding file
    // destinations
    char *command[1] = {""};  // command string
    int argc;
    int bytesRead = 1;
    int fileIn;      // new input file
    int fileOut;     // new output file
    int background;  // background flag
    int jid = 1;
    int stdin_copy = dup(STDIN_FILENO);
    int stdout_copy = dup(STDOUT_FILENO);
    job_list_t *job_list = init_job_list();
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("SIGINT ignore error.");
        cleanup_job_list(job_list);
        exit(1);
    }
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        perror("SIGTSTP handler error.");
        cleanup_job_list(job_list);
        exit(1);
    }
    if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
        perror("SIGTTOU handler error.");
        cleanup_job_list(job_list);
        exit(1);
    }
    while (1) {
        childReaper(job_list);
#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            cleanup_job_list(job_list);
            exit(1);
        }
        if (fflush(stdout) < 0) {
            cleanup_job_list(job_list);
            exit(1);
        }
#endif
        bytesRead = (int)read(STDIN_FILENO, buf, sizeof(buf));
        buf[bytesRead] = '\0';
        if (bytesRead == -1) {
            cleanup_job_list(job_list);
            exit(1);
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
        argc = 0;
        background = 0;
        parse(buf, tokens, argv, redirects);
        while (argv[argc] != NULL) argc++;
        if (redirects[0] != -1) {  // checking for redirects
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
                    if (fprintf(
                            stderr,
                            "syntax error: too many output/input redirects\n") <
                        0) {
                        perror(
                            "Error printing too many output/input redirects "
                            "error");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                }
                if (tokens[redirects[i] + 1] == NULL) {
                    err = 1;
                    if (fprintf(stderr,
                                "syntax error: no input/output file\n") < 0) {
                        perror("Error printing no input/output file error");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                } else {
                    if (argv[0] == NULL) {
                        err = 1;
                        if (fprintf(
                                stderr,
                                "syntax error: redirects with no command\n") <
                            0) {
                            perror(
                                "Error printing redirects with no command "
                                "error");
                            cleanup_job_list(job_list);
                            exit(1);
                        }
                    }
                }
            }
            if (err) continue;
            for (int j = 0; redirects[j] != -1; j++) {
                if (strcmp(tokens[redirects[j]], "<") == 0) {
                    if (close(STDIN_FILENO) < 0) {
                        if (fprintf(stderr, "Error closing stdin\n") < 0) {
                            perror("Error printing closing stdin error");
                            cleanup_job_list(job_list);
                            exit(1);
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
                            exit(1);
                        }
                        break;
                    }
                } else if (strcmp(tokens[redirects[j]], ">") == 0) {
                    if (close(STDOUT_FILENO) < 0) {
                        if (fprintf(stderr, "Error closing stdout\n") < 0) {
                            perror("Error printing closing stdout error");
                            cleanup_job_list(job_list);
                            exit(1);
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
                            exit(1);
                        }
                        break;
                    }
                } else if (strcmp(tokens[redirects[j]], ">>") == 0) {
                    if (close(STDOUT_FILENO) < 0) {
                        if (fprintf(stderr, "Error closing stdout\n") < 0) {
                            perror("Error printing closing stdout error");
                            cleanup_job_list(job_list);
                            exit(1);
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
                            exit(1);
                        }
                        break;
                    }
                }
            }
        }
        if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {  // checking for &
            background = 1;
            argv[argc - 1] = NULL;
        }
        if (!tokens[0])
            continue;
        else if (strcmp(tokens[0], "exit") == 0) {  // builtin functions: exit
            if (tokens[1] != NULL) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing exit syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
            } else {
                cleanup_job_list(job_list);
                exit(0);
            }
        } else if (strcmp(tokens[0], "cd") == 0) {  // builtin functions: cd
            if (argc != 2) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing cd syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            size_t l = strlen(argv[1]);
            if (l > 0 && argv[1][l - 1] == '/') argv[1][l - 1] = '\0';
            DIR *d = opendir(".");
            if (d == NULL) {
                perror("Error opening directory");
                cleanup_job_list(job_list);
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
                    cleanup_job_list(job_list);
                    exit(1);
                }
            } else {
                if (fprintf(stderr, "%s: No such file or directory.\n",
                            tokens[0]) < 0) {
                    perror("Error printing no such file or directory error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
        } else if (strcmp(tokens[0], "ln") == 0) {  // builtin functions: ln
            if (argc != 3) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing ln syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            size_t l1 = strlen(argv[1]);
            if (l1 > 0 && argv[1][l1 - 1] == '/') {
                if (fprintf(stderr, "%s: Not a directory\n", tokens[0]) < 0) {
                    perror("Error printing not a directory error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            size_t l2 = strlen(argv[2]);
            if (l2 > 0 && argv[2][l2 - 1] == '/') argv[1][l2 - 1] = '\0';
            if (link(argv[1], argv[2]) < 0) {
                if (fprintf(stderr, "%s: No such file or directory.\n",
                            tokens[0]) < 0) {
                    perror("Error printing no such file or directory error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
        } else if (strcmp(tokens[0], "rm") == 0) {  // builtin functions: rm
            if (argc != 2) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing rm syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            if (unlink(argv[1]) < 0) {
                if (fprintf(stderr, "%s: No such file or directory.\n",
                            tokens[0]) < 0) {
                    perror("Error printing no such file or directory error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
        } else if (strcmp(tokens[0], "jobs") == 0) {  // builtin functions: job
            if (argc != 1) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing jobs syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            jobs(job_list);
        } else if (strcmp(tokens[0], "bg") == 0) {
            int cur_pid;
            if (argc != 2) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing bg syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            if (argv[1][0] != '%') {
                if (fprintf(stderr, "%s: syntax error; need %%\n", tokens[0]) <
                    0) {
                    perror("Error printing bg syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            if ((cur_pid = get_job_pid(job_list, atoi(argv[1] + 1))) == -1) {
                if (fprintf(stderr, "%s: invalid job id\n", tokens[0]) < 0) {
                    perror("Error printing invalid job id error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            } else {
                if (kill(-1 * cur_pid, SIGCONT) == 0) {
                    update_job_pid(job_list, cur_pid, RUNNING);
                } else {
                    if (fprintf(stderr, "%s: kill error\n", tokens[0]) < 0) {
                        perror("Error printing kill error");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                }
            }
        } else if (strcmp(tokens[0], "fg") == 0) {
            int cur_pid;
            if (argc != 2) {
                if (fprintf(stderr, "%s: syntax error\n", tokens[0]) < 0) {
                    perror("Error printing fg syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            if (argv[1][0] != '%') {
                if (fprintf(stderr, "%s: syntax error; need %%\n", tokens[0]) <
                    0) {
                    perror("Error printing fg syntax error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            }
            if ((cur_pid = get_job_pid(job_list, atoi(argv[1] + 1))) == -1) {
                if (fprintf(stderr, "%s: invalid job id\n", tokens[0]) < 0) {
                    perror("Error printing invalid job id error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                continue;
            } else {
                if (kill(-1 * cur_pid, SIGCONT) == 0) {
                    remove_job_pid(job_list, cur_pid);
                } else {
                    if (fprintf(stderr, "%s: kill error\n", tokens[0]) < 0) {
                        perror("Error printing kill error");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                }
                int status;
                waitpid(cur_pid, &status, WUNTRACED);
                tcsetpgrp(STDIN_FILENO, cur_pid);
                if (status == -1) {
                    perror("Error waitpid");
                } else if (WIFSIGNALED(status)) {
                    if (printf("(%d) terminated by signal %d\n", cur_pid,
                               WTERMSIG(status)) < 0) {
                        perror("Error printing signal termination.");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                } else if (WIFSTOPPED(status)) {
                    add_job(job_list, jid, cur_pid, STOPPED, command[0]);
                    jid++;
                    if (printf("[%d] (%d) suspended by signal %d\n",
                               get_job_jid(job_list, cur_pid), cur_pid,
                               WSTOPSIG(status)) < 0) {
                        perror("Error printing signal suspension.");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        } else {
            int filepath = 0;  // filepath index accounting for redirects
            for (int i = 0; redirects[i] != -1; i++)
                if (redirects[i] == 0) filepath += 2;
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
                    exit(1);
                }
                if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
                    perror("SIGTSTP child handler error.");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                if (signal(SIGTTOU, SIG_DFL) == SIG_ERR) {
                    perror("SIGTTOU child handler error.");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                close(fileIn);
                close(fileOut);
                dup2(stdin_copy, STDIN_FILENO);
                dup2(stdout_copy, STDOUT_FILENO);
                execv(tokens[filepath], argv);
                perror("execv");
                cleanup_job_list(job_list);
                exit(1);
            }
            if (background && tokens[filepath]) {
                if (printf("[%d] (%d)\n", jid, pid) < 0) {
                    perror("Error add job print");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                if (add_job(job_list, jid, pid, RUNNING, command[0]) < 0) {
                    perror("Error add job");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                jid++;
            } else if (!background) {
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (status == -1) {
                    perror("Error waitpid");
                } else if (WIFSIGNALED(status)) {
                    if (printf("(%d) terminated by signal %d\n", pid,
                               WTERMSIG(status)) < 0) {
                        perror("Error printing signal termination.");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                } else if (WIFSTOPPED(status)) {
                    add_job(job_list, jid, pid, STOPPED, command[0]);
                    jid++;
                    if (printf("[%d] (%d) suspended by signal %d\n",
                               get_job_jid(job_list, pid), pid,
                               WSTOPSIG(status)) < 0) {
                        perror("Error printing signal suspension.");
                        cleanup_job_list(job_list);
                        exit(1);
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
            close(fileIn);
            close(fileOut);
            dup2(stdin_copy, STDIN_FILENO);
            dup2(stdout_copy, STDOUT_FILENO);
        }
    }
    return 0;
}
