CC = gcc
CFLAGS = -Wall -g
INCLUDES = -Iinclude
LFLAGS =
LIBS = 
SRCS = src/main.c src/command_loop.c

OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = radiosocketsd

.PHONY: clean

all: $(MAIN)
	@echo Done

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)
