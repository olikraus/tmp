# CFLAGS = -g -pg -Wall
# CFLAGS = -O4 -march=silvermont -Wall
CFLAGS = -g -pg -Wall

SRC = bcutil.c bcp.c bcube.c bclcore.c bcselftest.c bccofactor.c 
SRC += bclcontainment.c bcltautology.c bclsubtract.c 
SRC += bclcomplement.c bclsubset.c bclintersection.c
SRC += bclexpand.c bclminimize.c
SRC += main.c

OBJ = $(SRC:.c=.o)

bc: $(OBJ) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:	
	-rm $(OBJ) bc

