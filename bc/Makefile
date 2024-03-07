# CFLAGS = -g -pg -Wall
CFLAGS = -O4 -Wall

SRC = bc.c

OBJ = $(SRC:.c=.o)

bc: $(OBJ) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:	
	-rm $(OBJ) bc

