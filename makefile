.PHONY: clean

CC = gcc
CFLAGS += -O2 -std=c89 -s
CFLAGS += -Wall -Wextra -pedantic

NAME = gollike
ifeq ($(OS),Windows_NT)
EXE = $(NAME).exe
else
EXE = $(NAME)
endif

$(EXE): $(NAME).c
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(EXE)