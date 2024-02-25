CFLAGS = -g -Wall

SRC = bc.c

OBJ = $(SRC:.c=.o)

bc: $(OBJ) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:	
	-rm $(OBJ) table2html

