CC = gcc
CFLAGS = -Wall -g -O3
INCLUDES = -Iinclude -Idependencies -I/usr/include/libnl3
LFLAGS =
LIBS = -lpcap -lnl-3 -lnl-genl-3 -lconfig
SRCS_RADIOSOCKETS = src/main.c src/rs_command_loop.c src/rs_channel_layer.c src/rs_channel_layer_pcap.c src/rs_channel_layer_packet.c src/rs_port_layer.c src/rs_port_layer_packet.c src/rs_packet.c src/rs_stat.c src/rs_app_layer.c src/rs_message.c
SRCS_DEPENDENCIES = dependencies/radiotap-library/radiotap.c

SRCS = $(SRCS_RADIOSOCKETS) $(SRCS_DEPENDENCIES)

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
