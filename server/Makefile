STD = -std=gnu++20
CC = g++ $(STD)
WARNINGS = -Wall -Wextra -Wpedantic
OPTIMIZATIONS = -O0
DEBUGINFO = -g3
CFLAGS = $(WARNINGS) $(DEBUGINFO) $(OPTIMIZATIONS)
USEDLIBRARIES = -lc -lpthread

SRC = src
SOURCES = $(shell find $(SRC) -name "*.cpp")

COMMON_SRCS = $(shell find ../common -name "*.cpp")

OBJ = obj
OBJECTS = $(patsubst $(SRC)/%.cpp, $(OBJ)/%.o, $(SOURCES)) $(patsubst %.cpp, %.o, $(COMMON_SRCS))

BIN = bin
MAIN = server

all: $(MAIN)

release: CFLAGS = -Wall -Wextra -Wpedantic -O3 -DNDEBUG
release: clean
release: $(MAIN)

$(MAIN): $(OBJECTS)
	mkdir -p $(dir $@) ; $(CC) $(CFLAGS) $(OBJECTS) -o $@ $(USEDLIBRARIES)

$(OBJ)/%.o: $(SRC)/%.cpp $(SRC)/%.h
	mkdir -p $(dir $@) ; $(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.cpp
	mkdir -p $(dir $@) ; $(CC) $(CFLAGS) -c $< -o $@

$(OBJ):
	mkdir $@

$(BIN):
	mkdir $@

clean:
	$(RM) -r -i $(OBJ)/* ../common/*.o
