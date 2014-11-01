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
VER_IN	= version.in
VER_H	= version.h 

.PHONY: all release debug clean gen dep

all: release

release: CFLAGS += -O2 -DNDEBUG
release: TAG = -rls
release: gen dep $(BIN)
	$(STRIP) $(BIN)

debug: CFLAGS += -O0 -DDEBUG -g3
debug: TAG = -dbg
debug: gen dep $(BIN)

gen: 
	-$(CP) $(VER_IN) $(VER_H) 2> /dev/null
	-$(VERGEN) $(VER_IN) $(VER_H) $(TAG)
	
dep:
	$(CC) -MM $(SRC) > $(DEP)

-include $(DEP)

$(BIN): $(OBJ)
	$(LD) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	-${RM} $(OBJ) $(BIN) $(DEP) 2> /dev/null


###########
##  EOF  ##
###########
