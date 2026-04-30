.PHONY: all clean

CC = gcc
CFLAGS += -O2 -std=c89 -s
CFLAGS += -Wall -Wextra -pedantic

NAME = gollike
ifeq ($(OS),Windows_NT)
EXE = $(NAME).exe
else
EXE = $(NAME)
endif

all: $(EXE)

clean:
	rm -f $(EXE)

$(EXE): $(NAME).c
	$(CC) -o $@ $^ $(CFLAGS)