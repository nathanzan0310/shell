```
 ____  _          _ _
/ ___|| |__   ___| | |
\___ \| '_ \ / _ \ | |
 ___) | | | |  __/ | |
|____/|_| |_|\___|_|_|
```

Before we enter our shell loop, we set up the variables we will use in shell. In our shell loop, which continues running unless there is a read error on stdin, we first parse our input into all tokens, argv, and an array of redirect char index locations in tokens. We then check if there are any redirect chars in our command line. If there are, we check if there are any errors in terms of syntax or missing arguments, and if not we check which redirect char we have and perform the corresponding redirect operation . We error check closing and opening files; in the case where close succeeds and open does not, we must reopen the file we closed using dup2. If there are any errors with redirect chars then we print an error message and continue to a new prompt. Once we are done checking for redirect chars, we check if our command line is empty and if so continue to a new prompt, otherwise we check if a builtin command has been given using strcmp. Once we've identified our builtin command, we perform the corresponding operation and continue to a new prompt if there are any errors. If no builtin command has been identified, we fork into our child process so that we can perform the command given if there are any, writing/reading from a redirected file based on given redirects. We check for any prepended redirect chars, so we can pass in the correct arguments to execv. We then close all previously opened files and restore our original shell state by dup2()'ing stdin and stdout so we can continue to our next prompt. The shell can be compiled by typing ./33sh in the terminal.

Building off of shell 1, we first ignore SIGINT, SIGTSTP, and SIGTTOU when there is no foreground job by calling
signal(*, SIG_IGN) before we enter our loop, and then we reset these ignored signals in fork to their
default behavior so our child processes can accept and act on signals sent to it. Before our prompt in the shell
loop, we call childReaper (defined in childReaper.c) on our job_list so that we can reap any zombie processes.
After checking for redirects, we check if there is an ampersand in the last element of argv so that we can set
our background flag and make sure the associated process runs in the background. Adding on to our builtin command
strcmp() if else chain, we check if jobs, fg, and bg are the first argument in our command. If so, we do error checking
and implement the desired behavior. After we leave our fork, we check if the background flag is set to make sure our
process is running in the desired place, foreground or background. If it's a background process then we'll add it to our
job_list, and if it's a foreground process then we will wait for it to finish, and implement the desired behavior
if a signal changes the state of our process. Throughout our program, we call cleanup_job_list() whenever we exit and
error check any system calls that can possibly throw errors. The program can be compiled by calling ./33sh. We also
have two error checking functions, one for builtin commands and one for redirects, both defined in their own
eponymously named files.
