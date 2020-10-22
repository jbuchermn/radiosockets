CC = gcc
CFLAGS = -Wall -g -O3
INCLUDES = -Iinclude -Idependencies -I/usr/include/libnl3
LFLAGS =
LIBS = -lpcap -lnl-3 -lnl-genl-3
SRCS = src/main.c src/rs_command_loop.c src/rs_channel_layer.c src/rs_channel_layer_pcap.c src/rs_channel_layer_pcap_packet.c src/rs_port_layer.c src/rs_port_layer_packet.c src/rs_packet.c dependencies/radiotap-library/radiotap.c

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
