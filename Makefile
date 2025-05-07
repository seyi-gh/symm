CXX = g++
CXXFLASGS = -Wall -std=c++17
LDFLAGS = -lssl -lcrypto -pthread

SRC = main.cpp \
			./conn/proxy.cpp \
			./websocket/ws.cpp 

OBJ = ${SRC:.cpp=.o}

BIN = symm

all: ${BIN}

${BIN}: ${OBJ}
	${CXX} -o $@ $^ ${LDFLAGS}

# clean: rm -f ${BIN} ${OBJ}