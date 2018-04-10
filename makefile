CXX = clang++
PROJECT = TinyJS
SOURCE = $(wildcard *.cpp)
# SOURCE = main.cpp parser.cpp eval.cpp log.cpp
OUPUT = out
OPT1 = -g $(SOURCE) -Wall -o $(PROJECT).o
OPT = -g $(SOURCE) `llvm-config --cflags --ldflags --system-libs --libs core` -Wall -o $(PROJECT).o

target:
	$(CXX) $(OPT1)
	./$(PROJECT).o

run:
	./$(PROJECT).o

.PHONY: clean
clean:
	rm *.o
	rm -rf *.dSYM