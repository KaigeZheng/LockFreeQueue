CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread -Wall -Wextra
LDFLAGS = -latomic
TARGET = benchmark
SRC = benchmark.cpp

all: $(TARGET)

$(TARGET): $(SRC) LockFreeQueue.h LockedQueue.h
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean