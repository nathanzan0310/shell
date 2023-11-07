CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g 
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror -D_GNU_SOURCE -std=gnu99
PROMPT = -DPROMPT

all: 33sh 33noprompt

33sh: jobs.c jobs.h
	gcc $(CFLAGS) $(PROMPT) sh.c jobs.c jobs.h -o 33sh
33noprompt: jobs.c jobs.h
	gcc $(CFLAGS) sh.c jobs.c jobs.h -o 33noprompt
clean:
	rm 33sh 33noprompt
