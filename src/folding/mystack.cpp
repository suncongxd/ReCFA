#include "mystack.hpp"
#include <iostream>
#include <string.h>

MyStack::MyStack() {
  stacktop = 0;
  // 初始栈大小为1M
  length = 1048576;
  stack = new int[length];
}

MyStack::~MyStack() { delete stack; }

int MyStack::pop() {
  stacktop--;
  return stack[stacktop];
}

int MyStack::top() {
  if (stacktop == 0) {
    printf("栈空，无栈顶数据\n");
    exit(-1);
  } else {
    return stack[stacktop - 1];
  }
}

int MyStack::size() { return stacktop; }

bool MyStack::empty() { return stacktop == 0; }

void MyStack::changePos(int pos) {
  if (pos < 0) {
    std::cout << "change position wrong,program exit\n";
    exit(1);
  } else {
    stacktop = pos;
  }
}

void MyStack::push(int data) {
  if (stacktop == length) {
    expand();
  }
  stack[stacktop++] = data;
}

void MyStack::expand() {
  int newlength = length * 5;
  int *newStack = new int[newlength];
  printf("stack expand to %d\n", newlength);
  memcpy(newStack, stack, length * 4);
  // memcpy(newStack, stack, length);
  delete stack;
  stack = newStack;
  length = newlength;
}
