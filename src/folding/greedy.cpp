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

void arraycopy(int *a, int astart, int *b, int bstart, int len) {

  for (int i = 0; i < len; i++) {
    b[bstart + i] = a[astart + i];
  }
}

int naive_folding(int *input, int *result, int bound, int inputsize) {
  int res_idx = 0;
  for (int winStart = 0; winStart < inputsize - 1; winStart++) { // L1:
    int repeat_cnt = 0;
    int slideWinSize = 1;
    while (slideWinSize < bound) {
      int companion_start = winStart + slideWinSize * (repeat_cnt + 1);
      if (companion_start >= inputsize && repeat_cnt == 0) {
        arraycopy(input, winStart, result, res_idx, inputsize - winStart);
        res_idx += (inputsize - winStart);
        return res_idx; //结束程序
      }
      if (companion_start + slideWinSize > inputsize && repeat_cnt == 0) {
        // result[res_idx ++] = input[winStart];
        break;
      }

      int j;
      for (j = 0; j < slideWinSize && companion_start + j < inputsize; j++) {
        if (input[winStart + j] != input[companion_start + j])
          break;
      }
      if (j == slideWinSize) {
        repeat_cnt++;
        continue;
      } else {
        if (repeat_cnt == 0) { // not match for the first time
          slideWinSize++;
          continue;
        } else { // matched previously but failed to match in this time
          arraycopy(input, winStart, result, res_idx, slideWinSize);

          // cout<<"重复次数为："<<repeat_cnt<<endl;

          // printArray(result, res_idx, slideWinSize);
          // should add a special number "[repeat_cnt+1, slideWinSize]" to
          // "result"
          res_idx += slideWinSize;

          winStart = winStart + (repeat_cnt + 1) * slideWinSize;
          repeat_cnt = 0;
          slideWinSize = 1;
          continue;
        }
      }
    }
    // slideWinSize reach BOUND
    result[res_idx++] = input[winStart];
  }
  return res_idx;
}
