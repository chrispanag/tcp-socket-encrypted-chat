CC = gcc

CFLAGS = -Wall
CFLAGS += -g
# CFLAGS += -O2 -fomit-frame-pointer -finline-functions

LIBS = 

BINS = chatter

all: $(BINS)

encryptor.o: encryptor.c
	$(CC) $(CFLAGS) -c encryptor.c

main_new.o: main_new.c 
	$(CC) $(CFLAGS) -c main_new.c

chatter: main_new.o encryptor.o
	$(CC) $(CFLAGS) -o chatter main_new.o encryptor.o

clean:
	rm -f *.o *~ $(BINS)