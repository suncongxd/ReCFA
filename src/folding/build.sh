#! /bin/bash

g++ folding.cpp mystack.cpp -o folding -no-pie

g++ greedy.cpp -o greedy -no-pie

cp folding greedy ../../bin
