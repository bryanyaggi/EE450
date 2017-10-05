# Makefile for EE 450 Socket Programming Project
#
# Usage: make <command>

CC = gcc
EXES = client edge server_and server_or

# make all compiles all c files
all:
	$(CC) -o client client.c
	$(CC) -o edge edge.c
	$(CC) -o server_and server_and.c
	$(CC) -o server_or server_or.c

# make edge runs the edge executable
edge:
	./edge

# make edge runs the server_and executable
server_and:
	./server_and

# make edge runs the server_or executable
server_or:
	./server_or

# make clean removes executable files from the directory
clean:
	rm $(EXES)

# make tar creates a compressed file containing all project files
tar:
	tar cvzf ee450_yaggi_den.tar.gz client.c edge.c server_and.c server_or.c \
Makefile README

.PHONY: all edge server_and server_or clean tar

