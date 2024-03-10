# CFLAGS = -g -pg -Wall
# CFLAGS = -O4 -march=silvermont -Wall
CFLAGS = -g -pg -Wall

SRC = bc.c bcp.c bcube.c

OBJ = $(SRC:.c=.o)

bc: $(OBJ) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:	
	-rm $(OBJ) bc

