CC = gcc
CFLAGS = -Wall -g
INCLUDES =
LFLAGS =
LIBS = 
SRCS = main.c command_loop.c

OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = server

.PHONY: clean

all: $(MAIN)
	@echo Done

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)
