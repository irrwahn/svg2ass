#####################################
##  Makefile for project: svg2ass  ##
#####################################

CC      := gcc
INCS    =
CFLAGS  = $(INCS) -W -Wall -std=c99 -D_POSIX_C_SOURCE=200809L
LD      = gcc
LIBS    =
LDFLAGS = -lm $(LIBS)
CP		= cp
RM      = rm -f
SH		= sh
STRIP	= strip -s
VERGEN	= $(SH) version.sh

PRJ     = svg2ass
SRC     = $(wildcard *.c)
OBJ     = $(SRC:%.c=%.o)
BIN     = $(PRJ)
DEP     = $(PRJ).dep
VER		= version.h 

.PHONY: all debug clean gen dep

all: CFLAGS += -O2 -DNDEBUG
all: gen dep $(BIN)
	$(STRIP) $(BIN)

debug: CFLAGS += -O0 -DDEBUG -g3
debug: gen dep $(BIN)

gen: 
	-$(CP) version.in $(VER) 2> /dev/null
	-$(VERGEN) $(VER) $(MAKECMDGOALS) 2> /dev/null
	
dep:
	$(CC) -MM $(SRC) > $(DEP)

-include $(DEP)

$(BIN): $(OBJ)
	$(LD) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	-${RM} $(OBJ) $(BIN) $(PRJ).dep $(VER) 2> /dev/null


###########
##  EOF  ##
###########
