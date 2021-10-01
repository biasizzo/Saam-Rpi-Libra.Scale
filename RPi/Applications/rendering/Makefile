SRC := $(wildcard *.cpp)
EXE := $(patsubst %.cpp, %, $(SRC))
FLAGS := -std=c++11 -lmatrix_creator_hal -lgflags
C++ := g++

.PHONY: all run clean

all: $(EXE)

%: %cpp
	$(C++) $(FLAGS) -o $@ $<

install: $(EXE)
