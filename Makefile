#####################################
##  Makefile for project: svg2ass  ##
#####################################

CC      := gcc
INCS    =
CFLAGS  = $(INCS) -W -Wall -std=c99 -D_POSIX_C_SOURCE=200809L
LD      = gcc
LIBS    =
LDFLAGS = $(LIBS)
RM      = rm -f
STRIP	= strip -s

PRJ     = svg2ass
SRC     = $(wildcard *.c)
OBJ     = $(SRC:%.c=%.o)
BIN     = $(PRJ)
DEP     = $(PRJ).dep

.PHONY: all debug clean dep

all: CFLAGS += -O2 -DNDEBUG
all: dep $(BIN)
	$(STRIP) $(BIN)

debug: CFLAGS += -O0 -DDEBUG -g3
debug: dep $(BIN)

dep:
	$(CC) -MM $(SRC) > $(DEP)

-include $(DEP)

$(BIN): $(OBJ)
	$(LD) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	${RM} $(OBJ) $(BIN) $(PRJ).dep 2> /dev/null


###########
##  EOF  ##
###########
