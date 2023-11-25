CC = g++
TARGET = main
OBJS = main.o tarball_unpacker.o

CXXFLAGS = -Wall -Wextra -std=c++2a

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o $(TARGET)

$(TARGET): $(OBJS) Makefile
	$(CC) $(OBJS) -o $@

%.o: %.cpp Makefile
	$(CC) $(CXXFLAGS) -c $<
