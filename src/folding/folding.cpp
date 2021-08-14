#include "fold-api.hpp"

//#include <iostream>
//#include <stack>
#include <stdio.h>
#include <string.h>
using namespace std;

int main(int argc, char *argv[]) {
  bool dealWithRecursion = false;
  int name_idx = 0;

  if (argc < 2 || argc > 3) {
    printf("Usage:\ndeal with recursions and loops:\n./folding -r"
           "[fileName]\ndeal with loops only:\n./folding [fileName]\n");
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-r"))
      dealWithRecursion = true;
    else
      name_idx = i;
  }
  if (name_idx == 0) // no filename found
    return 0;
  folding_action(argv, name_idx, dealWithRecursion);
  return 0;
}
