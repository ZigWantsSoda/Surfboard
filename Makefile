CC = gcc
CFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0 webkit2gtk-4.1`
LDFLAGS = `pkg-config --libs gtk+-3.0 webkit2gtk-4.1` -pthread

SRC = surfboard.c
OBJ = $(SRC:.c=.o)
TARGET = surfboard

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)

