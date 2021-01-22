CC = gcc

CFLAGS = -Wall
CFLAGS += -g -Ofast

LIBS = 

BINS = chatter

all: $(BINS)

encryptor.o: encryptor.c
	$(CC) $(CFLAGS) -c encryptor.c

main.o: main.c 
	$(CC) $(CFLAGS) -c main.c

chatter: main.o encryptor.o
	$(CC) $(CFLAGS) -o chatter main.o encryptor.o

clean:
	rm -f *.o *~ $(BINS)