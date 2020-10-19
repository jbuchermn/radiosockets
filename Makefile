CC = gcc
CFLAGS = -Wall -g -O3
INCLUDES = -Iinclude -Idependencies
LFLAGS =
LIBS = -lpcap
SRCS = src/main.c src/rs_command_loop.c src/rs_packet.c src/rs_channel_layer_pcap.c dependencies/radiotap-library/radiotap.c

OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = radiosocketd

.PHONY: clean

all: $(MAIN)
	@echo Done

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJS) $(MAIN)
