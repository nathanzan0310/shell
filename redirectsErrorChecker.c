#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./jobs.h"

// The function checks for any syntax error in our redirects, and if we have any
// we will return -1, otherwise we return 0
int redirectsErrorChecker(char *tokens[512], int redirects[512],
                          char *argv[512], job_list_t *job_list) {
    int i = 0;
    int oc = 0;  // output redirect counter
    int ic = 0;  // input redirect counter
    for (; redirects[i] != -1; i++) {
        if (strcmp(tokens[redirects[i]], ">") == 0 ||
            strcmp(tokens[redirects[i]], ">>") == 0)
            oc++;
        if (strcmp(tokens[redirects[i]], "<") == 0) ic++;
        if (oc > 1 || ic > 1) {
            if (fprintf(stderr,
                        "syntax error: too many output/input redirects\n") <
                0) {
                perror(
                    "Error printing too many output/input redirects "
                    "error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        }
        if (tokens[redirects[i] + 1] == NULL) {
            if (fprintf(stderr, "syntax error: no input/output file\n") < 0) {
                perror("Error printing no input/output file error");
                cleanup_job_list(job_list);
                exit(1);
            }
            return -1;
        } else {
            if (argv[0] == NULL) {
                if (fprintf(stderr,
                            "syntax error: redirects with no command\n") < 0) {
                    perror(
                        "Error printing redirects with no command "
                        "error");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                return -1;
            }
        }
    }
    return 0;
}