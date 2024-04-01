# CFLAGS = -g -pg -Wall
# CFLAGS = -O4 -march=silvermont -Wall  -I../c-object/co -I.
# CFLAGS = -g -pg -Wall

# CFLAGS = -g -Wall -fsanitize=address -I../c-object/co -I.
CFLAGS = -g -Wall  -I../c-object/co -I.

SRC = bcutil.c bcp.c bcube.c bclcore.c bcselftest.c bccofactor.c 
SRC += bclcontainment.c bcltautology.c bclsubtract.c 
SRC += bclcomplement.c bclsubset.c bclintersection.c
SRC += bclexpand.c bclminimize.c
SRC += bcexpression.c
SRC += ../c-object/co/co.c bcjson.c
SRC += main.c

OBJ = $(SRC:.c=.o)

bc: $(OBJ) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:	
	-rm $(OBJ) bc

