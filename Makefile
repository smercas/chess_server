STD = -std=gnu++20
CC = g++ $(STD)
WARNINGS = -Wall -Wextra -Wpedantic
OPTIMIZATIONS = -O0
DEBUGINFO = -g3
CFLAGS = $(WARNINGS) $(DEBUGINFO) $(OPTIMIZATIONS)
USEDLIBRARIES = -lc -lpthread -lwrappers

LIBFOLDER = libraries

WRAPPERS = $(LIBFOLDER)/wrappers
CHESS = $(LIBFOLDER)/chess

LOCAL_LIBRARIES = $(shell find $(LIBFOLDER) -mindepth 1 -maxdepth 1)

IFLAGS = $(patsubst %, -I./%/src, $(LOCAL_LIBRARIES))

LFLAGS = $(patsubst %, -L./%/lib, $(LOCAL_LIBRARIES))

comma = ,

WLFLAGS = $(patsubst %, -Wl$(comma)-rpath=./%/lib, $(LOCAL_LIBRARIES))

SRC = common
SOURCES = $(shell find $(SRC) -name "*.cpp")

OBJ = common
OBJECTS = $(patsubst $(SRC)/%.cpp, $(OBJ)/%.o, $(SOURCES))

BIN = common
MAINS = $(patsubst $(OBJ)/%.o, $(BIN)/%, $(OBJECTS))

all: $(MAINS)

release: CFLAGS = -Wall -Wextra -Wpedantic -O3 -DNDEBUG
release: clean
release: $(MAINS)

$(BIN)/%: $(OBJ)/%.o
	mkdir -p $(dir $@) ; $(CC) $(CFLAGS) $(LFLAGS) $(WLFLAGS) $< -o $@ $(USEDLIBRARIES)

$(OBJ)/%.o: $(SRC)/%.cpp $(SRC)/%.h
	mkdir -p $(dir $@) ; $(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.cpp
	mkdir -p $(dir $@) ; $(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(OBJ):
	mkdir $@

$(BIN):
	mkdir $@

clean:
	$(RM) -r -i $(BIN)/* $(OBJ)/*
