#include "fold-api.hpp"

#include <iostream>
#include <stdio.h>
#include <string.h>
#define BOUND 32           // BOUND窗口最大长度
#define BUFFSIZE 1024 * 10 // BUFFSIZE的大小
#define LEN 2048

/*
运行指令: ./fold 待去重的文件名
输出结果为去重后的文件
*/

using namespace std;

void printArray(int array[], int start, int sz);
void arraycopy(int *a, int astart, int *b, int bstart, int len);
int naive_folding(int *input, int *result, int bound, int inputsize);

int main(int argc, char *argv[]) {
  if (argc != 2)
    return 0;

  FILE *in, *out;
  in = fopen(argv[1], "rb");

  char *resultName = new char[LEN];
  memset(resultName, 0, LEN);
  strcat(resultName, argv[1]);
  strcat(resultName, "_gr");
  // printf("target path name: %s\n", resultName);

  out = fopen(resultName, "wb+");

  int buffer[BUFFSIZE];
  int rbyte = 0;

  int *presult = new int[BUFFSIZE];
  int sz;
  while ((rbyte = fread(&buffer, sizeof(int), BUFFSIZE, in)) > 0) {
    sz = naive_folding(buffer, presult, BOUND, rbyte);
    fwrite(presult, sizeof(int), sz, out);
  }

  fclose(in);
  fclose(out);
  delete[] presult;
  delete[] resultName;
}

void printArray(int array[], int start, int sz) {
  for (int k = start; k <= start + sz - 1; k++)
    cout << hex << array[k] << "," << dec << endl;
  printf("\n");
}
