CC = gcc
CFLAGS = -std=gnu99 -Wall -O0

all: echo_server echo_client echo_server_exp echo_client_exp

echo_server:
	$(CC) $(CFLAGS) echo_server.c ikcp.c -o echo_server

echo_client:
	$(CC) $(CFLAGS) echo_client.c ikcp.c -o echo_client

echo_server_exp:
	$(CC) $(CFLAGS) echo_server_exp.c ikcp.c -o echo_server_exp

echo_client_exp:
	$(CC) $(CFLAGS) echo_client_exp.c ikcp.c -o echo_client_exp

clean:
	rm -f echo_server echo_client echo_server_exp echo_client_exp