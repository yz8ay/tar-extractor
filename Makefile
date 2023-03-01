CC = g++
TARGET = main
OBJS = main.o

CXXFLAGS = -O2 -Wall -std=c++2a

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

$(TARGET): $(OBJS) Makefile
	$(CC) $(OBJS) -o $@

%.o: %.cpp Makefile
	$(CC) $(CXXFLAGS) -c $<