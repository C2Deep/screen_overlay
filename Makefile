CC = gcc
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
TARGET = overlay

LIBS = -lX11 -lXfixes -lpthread

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC)  -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)
