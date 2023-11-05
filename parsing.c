/*
 * Brown University - Department of Computer Science
 * CS033 - Introduction To Computer Systems - Fall 2023
 * Prof. Thomas W. Doeppner
 * Lab02 - parsing.c
 * Due date: 9/24/2023
 */

/* XXX: Preprocessor instruction to enable basic macros; do not modify. */
#include <stddef.h>
#include <string.h>

/*
 * parse()
 *
 * - Description: creates the token and argv arrays from the buffer character
 *   array
 *
 * - Arguments: buffer: a char array representing user input, tokens: the
 * tokenized input, argv: the argument array eventually used for execv()
 *
 * - Usage:
 *
 *      For the tokens array:
 *
 *      cd dir -> [cd, dir]
 *      [tab]mkdir[tab][space]name -> [mkdir, name]
 *      /bin/echo 'Hello world!' -> [/bin/echo, 'Hello, world!']
 *
 *      For the argv array:
 *
 *       char *argv[4];
 *       argv[0] = echo;
 *       argv[1] = 'Hello;
 *       argv[2] = world!';
 *       argv[3] = NULL;
 *
 * - Hint: for this part of the assignment, you are allowed to use the built
 *   in string functions listed in the handout
 */
void parse(char buffer[1024], char *tokens[512], char *argv[512],
           int redirects[512]) {
    char *token;
    char *str = buffer;
    char *str1 = strrchr(buffer, '/');
    int i;       // tokens index
    int j = 0;   // argv array index
    int k = 0;   // redirect array index
    int r = -2;  // redirection char index set to -2 to avoid false positives as
                 // we check if i-1=r, where i starts at 0
    for (i = 0; (token = strtok(str, " \t\n")) != NULL; i++) {
        tokens[i] = token;
        str = NULL;
        if (strcmp(tokens[i], ">") != 0 && strcmp(tokens[i], ">>") != 0 &&
            strcmp(tokens[i], "<") != 0 && i - 1 != r) {
            argv[j] = tokens[i];  // ignore all redirect chars and their
                                  // succeeding files when adding to argv
            j++;
        } else if (strcmp(tokens[i], ">") == 0 ||
                   strcmp(tokens[i], ">>") == 0 ||
                   strcmp(tokens[i], "<") == 0) {
            r = i;
            redirects[k] = r;  // save redirect char index
            k++;
        }
    }
    if (str1 != NULL) {
        str1 = &str1[1];
        if ((argv[0] = strtok(str1, " \t\n")) == NULL) argv[0] = "";
    }
    argv[j] = NULL;
}
