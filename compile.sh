#!/bin/bash
PORT=9000

sh ./exec/k.port.sh
# sh ./exec/k.docker.sh

rm -r ./*.o ./symm | true
make

./symm