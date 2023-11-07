#include <stdio.h>
#include <sys/wait.h>
#include "jobs.h"

void childReaper(job_list_t *job_list) {
    pid_t wret;
    int wstatus;
    while ((wret = waitpid(-1, &wstatus,
                           WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
//            int i;
//            int alr_add = 0;  // already added flag
//            while (get_next_pid(job_list) != -1) {}//reset job loop to head
//            while ((i = get_next_pid(job_list)) != -1)
//                if (i == wret) alr_add = 1;
        if (WIFEXITED(wstatus)) {
            printf("[%d] (%d) terminated with exit status %d\n",
                   get_job_jid(job_list, wret), wret, WEXITSTATUS(wstatus));
            remove_job_pid(job_list, wret);
        }
        if (WIFSIGNALED(wstatus)) {
//                if (alr_add) {
            printf("[%d] (%d) terminated by signal %d\n",
                   get_job_jid(job_list, wret), wret,
                   WTERMSIG(wstatus));
            remove_job_pid(job_list, wret);
//                } else
//                    printf("(%d) terminated by signal %d\n", wret,
//                           WTERMSIG(wstatus));
        }
        if (WIFSTOPPED(wstatus)) {
//                if (!alr_add)
//                    add_job(job_list, jid, wret, STOPPED, command[0]);
//                else

            printf("[%d] (%d) suspended by signal %d\n",
                   get_job_jid(job_list, wret), wret, WSTOPSIG(wstatus));
            update_job_pid(job_list, wret, STOPPED);
        }
        if (WIFCONTINUED(wstatus)) {
            printf("[%d] (%d) resumed\n", get_job_jid(job_list, wret),
                   wret);
            update_job_pid(job_list, wret, RUNNING);
        }
    }
}