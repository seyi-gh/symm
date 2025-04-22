CXX = g++
CXXFLASGS = -Wall -std=c++17
LDFLAGS = -lssl -lcrypto -pthread

SRC = main.cpp \
			conn/handler.cpp \
			websocket/client.cpp \
			websocket/server.cpp \

OBJ = ${SRC:.cpp=.o}

BIN = symm

all: ${BIN}

${BIN}: ${OBJ}
	${CXX} -o $@ $^ ${LDFLAGS}

# clean: rm -f ${BIN} ${OBJ}