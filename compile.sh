#!/bin/bash
PORT=9000

#exec 3>&1 4>&2
#exec >/dev/null 2>&1

#set -x

sh ./exec/k.port.sh
# sh ./exec/k.docker.sh
rm -r ./*.o ./symm ./*/*.o
files_found="$(find . -name "*.o")"
if [ -z "$files_found" ]; then
  echo "No .c files found in the current directory."
fi

make

#set +x

#exec 1>&3 2>&4

./symm