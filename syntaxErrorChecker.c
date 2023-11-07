#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// For the given command, we check for syntax errors and return -1 if we have
// any, otherwise we return 0
int syntaxErrorChecker(char *command, char *argv[512], int argc,
                       job_list_t *job_list) {
    if (strcmp(command, "fg") == 0 ||
        strcmp(command, "bg") == 0) {  // builtin command: fg and bg
        if (argc != 2) {
            if (fprintf(stderr, "%s: syntax error\n", command) < 0) {
                perror("Error printing fg syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
        if (argv[1][0] != '%') {
            if (fprintf(stderr, "%s: syntax error; need %%\n", command) < 0) {
                perror("Error printing fg syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
        if (get_job_pid(job_list, (int)strtol(argv[1] + 1, NULL, 10)) == -1) {
            if (fprintf(stderr, "%s: invalid job id\n", command) < 0) {
                perror("Error printing invalid job id error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
    } else if (strcmp(command, "jobs") == 0) {  // builtin command: job
        if (argc != 1) {
            if (fprintf(stderr, "%s: syntax error\n", command) < 0) {
                perror("Error printing jobs syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
    } else if (strcmp(command, "cd") == 0) {  // builtin command: cd
        if (argc != 2) {
            if (fprintf(stderr, "%s: syntax error\n", command) < 0) {
                perror("Error printing cd syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
    } else if (strcmp(command, "rm") == 0) {  // builtin command: rm
        if (argc != 2) {
            if (fprintf(stderr, "%s: syntax error\n", command) < 0) {
                perror("Error printing rm syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
    } else if (strcmp(command, "ln") == 0) {  // builtin command: ln
        if (argc != 3) {
            if (fprintf(stderr, "%s: syntax error\n", command) < 0) {
                perror("Error printing ln syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
        size_t l1 = strlen(argv[1]);
        if (l1 > 0 && argv[1][l1 - 1] == '/') {
            if (fprintf(stderr, "%s: Not a directory\n", command) < 0) {
                perror("Error printing not a directory error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
    } else if (strcmp(command, "exit") == 0) {  // builtin command: exit
        if (argc != 1) {
            if (fprintf(stderr, "%s: syntax error\n", command) < 0) {
                perror("Error printing exit syntax error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
    }
    return 0;
}