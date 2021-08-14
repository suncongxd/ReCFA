#! /bin/bash

#build .so
g++ -c -Wall -Werror -fpic fold-api.cpp mystack.cpp
g++ -shared -o libfold.so fold-api.o mystack.o

#build folding
g++  -Wall -L. folding.cpp -o folding -lfold -no-pie

#build greedy
g++ -Wall -L. greedy.cpp -o greedy -lfold -no-pie

#deploy
cp folding greedy ../../bin
cp libfold.so ../../lib
