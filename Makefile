CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -g -pthread
INC = -I include

SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp, build/%.o, $(SRC))

OUT = bin/server

all: $(OUT)

$(OUT): $(OBJ)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(OUT)

build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

run: all
	./$(OUT) $(ARGS)

clean:
	rm -rf build bin