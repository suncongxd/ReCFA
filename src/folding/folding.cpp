#include "mystack.h"
#include <iostream>
#include <stack>
#include <stdio.h>
#include <string.h>
using namespace std;

int main(int argc, char *argv[]) {
  bool dealWithRecursion = true;
  int offset = 0;

  if (argc < 2 || (argc == 3 && dealWithRecursion)) {
    printf("Usage:\ndeal with recursions and loops:\n./folding "
           "[fileName]\ndeal with loops only:\n./folding -r [fileName]\n");
    return 0;
  }

  if (strcmp("-r", argv[1]) == 0) {
    dealWithRecursion = false;
    offset = 1;
  }

  MyStack addrStack;
  MyStack circuStack;
  MyStack recStack;

  FILE *in, *out;
  char *result = new char[100];
  memset(result, 0, 100);
  strcat(result, argv[offset + 1]);
  strcat(result, "_folded");
  in = fopen(argv[offset + 1], "r");
  out = fopen(result, "w+");
  int buffer;
  int rbyte = 0;
  double count = 0;
  int times = 0;
  int lastOne = 0;
  while ((rbyte = fread(&buffer, 4, 1, in)) == 1) {
    // printf("%x\n",buffer);

    if (buffer == 0xdddddddd) {
      // 0xdddddddd标识开始一个循环，使用ffff在循环栈中进行隔离
      circuStack.push(0xffff);
    } else if (buffer == 0xcccccccc) {
      // 0xcccccccc标识结束一个循环，将当前循环栈中的内容弹出栈，
      int temp;
      do {
        temp = circuStack.pop();
      } while (temp != 0xffff);
    } else if (buffer == 0xffffffff) {
      // 0xffffffff标识执行循环判断条件，判断循环体是否再次执行
      circuStack.push(addrStack.stacktop);
    } else if (buffer == 0xeeeeeeee) {
      // buffer标识循环体结束执行一次，需要判断是否相等
      // circuStack.top()!=
      // addrStack.stacktop进行判断的原因是循环的最后一次判断条件不满足，
      // 结果中会连续出现0xfffffffff,0xeeeeeeee,其中没有地址，则不需要判断是否重复
      if (circuStack.top() != addrStack.stacktop) {
        // cout << "比较" << endl;
        int index = 2;
        // 获取本次循环执行产生的路径内容的开始位置，需要考虑是否发生重复
        int needCmpPos = circuStack.stack[circuStack.stacktop - 1];
        // 地址栈中保存的最新一组没有重复的循环单次执行结果
        int cmpedPos = circuStack.stack[circuStack.stacktop - index];
        // 获取本次循环产生的地址长度
        int needCmpLength = addrStack.stacktop - needCmpPos;
        // 获取被比较的路径结果长度
        int cmpedLength;
        while (cmpedPos != 0xffff) {
          // printf("test3 cmpedPos=%x\n",cmpedPos);
          cmpedLength =
              circuStack.stack[circuStack.stacktop - index + 1] - cmpedPos;
          if (needCmpLength == cmpedLength &&
              memcmp(addrStack.stack + needCmpPos, addrStack.stack + cmpedPos,
                     needCmpLength * 4) == 0) {
            addrStack.stacktop = circuStack.pop();
            // cout << "相等" << endl;
            break;
          } else {
            //比较下一个不重复的原子单元
            index++;
            cmpedPos = circuStack.stack[circuStack.stacktop - index];
          }
        }
        // printf("test1\n");
        if (cmpedPos == 0xffff) {
          // printf("test2\n");
        }
      }
      // printf("比较结束\n");
    } else if (dealWithRecursion && buffer == 0xaaaaaaaa) {
      // 0xaaaaaaaa标识外部函数调用递归函数调用点
      // printf("进入递归调用函数\n");
      recStack.push(0xffff);
      recStack.push(addrStack.stacktop);
    } else if (dealWithRecursion && buffer == 0xaaaabbbb) {
      // 0xaaaabbbb标识递归函数结束，返回到外部调用递归函数的调用点的下一条指令位置
      int temp;
      do {
        temp = recStack.pop();
      } while (temp != 0xffff);
      // printf("退出递归调用函数\n");
    } else if (dealWithRecursion && buffer == 0xaaaaffff) {
      // printf("rectoyp = %d\n",recStack.top());
      // 0xaaaaffff运行到递归函数内部调用点
      if (recStack.top() == 0xffff ||
          recStack.stack[recStack.stacktop - 2] == 0xffff) {
        recStack.push(addrStack.stacktop);
        // printf("0xffff第一或者第二\n");
      } else {
        // cout << "比较" << endl;
        int index = 2;
        // 获取本次执行产生的路径内容的开始位置，需要考虑是否发生重复
        int needCmpPos = recStack.stack[recStack.stacktop - 1];
        // 地址栈中保存的没有重复的单次执行结果
        int cmpedPos = recStack.stack[recStack.stacktop - index];
        // 获取本次产生的地址长度
        int needCmpLength = addrStack.stacktop - needCmpPos;
        // 获取被比较的路径结果长度
        int cmpedLength;
        while (cmpedPos != 0xffff) {
          cmpedLength =
              recStack.stack[recStack.stacktop - index + 1] - cmpedPos;
          if (needCmpLength == cmpedLength &&
              memcmp(addrStack.stack + needCmpPos, addrStack.stack + cmpedPos,
                     needCmpLength * 4) == 0) {
            addrStack.stacktop = recStack.top();
            // cout << "相等" << endl;
            break;
          } else {
            //比较下一个不重复的原子单元
            index++;
            cmpedPos = recStack.stack[recStack.stacktop - index];
          }
        }
        if (cmpedPos == 0xffff) {
          recStack.push(addrStack.stacktop);
        }
      }
    } else {
      /*
          栈满，将不重复的数据输出到文件
          是普通的路径数据，需要将普通路径数据压入栈中，压入之前需要考虑栈是否满，栈满则栈中的不重复数据输出到文件，剩下重复数据，
          如果全为重复数据，则需要将栈扩大，栈扩大在栈的push操作部分可以实现
      */
      if (addrStack.stacktop == addrStack.length) {
        if (circuStack.stacktop == 0 && recStack.stacktop == 0) {
          // 如果循环栈中没有内容，即当前地址栈中的地址不需要被判断是否重复，则将当前地址栈中的所有内容放入文件中
          // printf("循环栈和递归栈中没有重复内容：成功输出到文件的长度%d\n",fwrite(addrStack.stack,
          // 4, addrStack.stacktop, out));
          addrStack.stacktop = 0;
        } else {
          unsigned long storeLen;
          // 当前存在循环，则将最顶层循环，即circuStack.stack[1]中保存的地址之前的内容存入文件中
          if (circuStack.empty() && !recStack.empty()) {
            storeLen = recStack.stack[1];
          } else if (!circuStack.empty() && recStack.empty()) {
            storeLen = circuStack.stack[1];
          } else {
            circuStack.stack[1] <= recStack.stack[1] ? circuStack.stack[1]
                                                     : recStack.stack[1];
          }
          // 进行判断的原因是如果从地址栈的第一个开始就是处于循环中的，则只能增加栈的大小，栈的push操作可以完成
          if (storeLen != 0) {
            printf("Recursion or Loop exists: Length need to be stored %lu, "
                   "Length succeeded to output to file %lu\n",
                   storeLen, fwrite(addrStack.stack, 4, storeLen, out));
            int changeLen = addrStack.length - storeLen;
            addrStack.stacktop = changeLen;
            // 将栈中重复部分的内容移动到栈顶
            memcpy(addrStack.stack, addrStack.stack + storeLen, changeLen * 4);
            // 将循环栈中的内容（相对于地址栈栈底的偏移地址）进行修正
            //  printf("对于循环栈");
            for (int i = 0; i < circuStack.stacktop; i++) {
              if (circuStack.stack[i] != 0xffff) {
                circuStack.stack[i] = circuStack.stack[i] - storeLen;
                if (circuStack.stack[i] < 0) {
                  printf("circuStack.stack[i]=%d,stooreLen=%lu\n",
                         circuStack.stack[i], storeLen);
                }
              }
            }
            // printf("对于递归栈");
            for (int i = 0; i < recStack.stacktop; i++) {
              if (recStack.stack[i] != 0xffff) {
                recStack.stack[i] = recStack.stack[i] - storeLen;
                if (recStack.stack[i] < 0) {
                  printf("recStack.stack[i]=%d,stooreLen=%lu\n",
                         recStack.stack[i], storeLen);
                }
              }
            }
          }
        }
      }
      // count++;
      // if(count%100==0){
      //     printf("%d ",count);
      // }
      addrStack.push(buffer);
      // lastOne = buffer;
    }
  }
  // printf("最后一个是%x，地址栈栈长度%d\n", lastOne, addrStack.stacktop);
  fwrite(addrStack.stack, 4, addrStack.stacktop, out);
  fclose(in);
  fclose(out);
  return 0;
}
