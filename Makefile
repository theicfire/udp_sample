CXX      := g++
CXXFLAGS := -Wall -Wuninitialized -std=c++17 -ggdb3

.PHONY: all
all: client host

common.o: common.cpp common.h
	$(CXX) $(CXXFLAGS) -c common.cpp

client: common.o client.cpp
	$(CXX) $(CXXFLAGS) common.o client.cpp -o client

host: common.o host.cpp
	$(CXX) $(CXXFLAGS) common.o host.cpp -o host -lpthread

.PHONY: clean
clean:
	rm -f host client *.o
	rm -rf *.dSYM