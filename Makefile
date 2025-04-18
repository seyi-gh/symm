CXX = g++
CXXFLASGS = -Wall -std=c++17
LDFLAGS = -lssl -lcrypto -pthread

SRC = main.cpp websocket/ws.client.cpp
OBJ = ${SRC:.cpp=.o}

BIN = proxy

all: ${BIN}

${BIN}: ${OBJ}
	${CXX} -o $@ $^ ${LDFLAGS}

clean: rm -f ${BIN} ${OBJ}