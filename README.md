```
 ____  _          _ _   ____
/ ___|| |__   ___| | | |___ \
\___ \| '_ \ / _ \ | |   __) |
 ___) | | | |  __/ | |  / __/
|____/|_| |_|\___|_|_| |_____|
```

Please submit a README.md documenting the structure of your program, any bugs you have in your code, any extra features
you added, and how to compile it.

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