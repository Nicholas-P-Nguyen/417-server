# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
CFLAGS = -Wall
SRC = ./src
BIN = ./bin
CONTENTS = ./contents
OBJS = $(BIN)/pserver.o $(BIN)/pclient.o $(BIN)/request.o $(BIN)/io_helper.o

all: $(BIN)/pserver $(BIN)/pclient $(CONTENTS)/spin.cgi

$(BIN)/pserver: $(BIN)/pserver.o $(BIN)/request.o $(BIN)/io_helper.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(BIN)/pserver.o $(BIN)/request.o $(BIN)/io_helper.o 

$(BIN)/pclient: $(BIN)/pclient.o $(BIN)/io_helper.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(BIN)/pclient.o $(BIN)/io_helper.o

$(CONTENTS)/spin.cgi: $(SRC)/spin.c | $(CONTENTS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/spin.c

$(BIN)/%.o: $(SRC)/%.c | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN):
	mkdir -p $(BIN)

clean:
	-rm -f $(OBJS) $(BIN)/pserver $(BIN)/pclient $(CONTENTS)/spin.cgi
