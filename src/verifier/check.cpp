#include <algorithm>
#include <bitset>
#include <fstream>
#include <iostream>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <thread>
#include <time.h>
using namespace std;
class newblock {
public:
  int Type; //间接调用是1，间接跳转是2，直接跳转是3
  int Pair;
  int next;
  int directTarget;
  set<int> Jump;
};
map<int, newblock> proMap;
map<int, int> ForNext;
void readAsmFile(char *file, set<int> *IndirectSet, int *Main,
                 set<int> *IndirectCall, int *Ret, string compiler_type);
void readDyninst(char *file, set<int> *IndirectSet);
void readTypeAmror(char *file, set<int> *IndirectSet);
void readForToNext(char *file);
void readReceive(char *file, int Main, int Ret);
void FtoN(queue<int> *sto, int For);
void verifiSecond(queue<int> *sto, FILE *in, int Ret, int Main);
bool verifi(queue<int> *sto, FILE *in, stack<int> *shadow, int Ret, int Main);
long success = 0, total = 0;
double timeRead = 0, timeTotal = 0;
int executetime = 0;

int main(int argc, char *argv[]) {
  if (argc != 8) {
    printf("Usage: ./check [disassem_file_path] [dyninst_graph_path] "
           "[policy_F_path] [policy_M_path] [folded_runtime_event_path] "
           "[num_executions] [compiler_type(gcc/llvm)].\n");
    return 0;
  }
  char *file;
  int Main = 0;
  int Ret = 0;
  set<int> *IndirectSet, *IndirectCall;
  set<int> a, b;
  IndirectSet = &a;
  IndirectCall = &b;
  // string
  // str="/home/ly/workspace/indirectjump/sourcecode/test/zuoye/AmyZhang319-resilientremoteattestation-2a9089600d1b/llvm-10/O1/gcc.cfa_huibian.txt";
  // //读汇编文件pair对和和call，顺便读取间接调用的集合,同事获取main函数 string
  // str="/root/liu/check/llvmO0/O0/gcc_base.huibian.txt";
  // string str = "";
  // printf("输入反汇编文件\n");
  // getline(cin, str);
  // string str="/root/liu/check/O0gcc/O0/gcc_base.cfa_huibian.txt";
  // file = (char *)str.c_str();
  readAsmFile(argv[1], IndirectSet, &Main, IndirectCall, &Ret, argv[7]);
  // printf("main%xret%x",Main,Ret);
  // printf("输入Dyninst分析文件\n");
  // getline(cin, str);
  // str="/root/liu/check/O0gcc/O0/gcc_base.cfa.dot";
  // file = (char *)str.c_str();
  readDyninst(argv[2], IndirectSet); //读取间接跳转
  // str="/root/liu/check/outllvm/binfo.gcc_base.llvm_O0";    //读取间接调用
  // str="/root/liu/check/outgcc/binfo.gcc_base.cfa";
  // printf("输入TypeArmor分析文件\n");
  // getline(cin, str);
  // file = (char *)str.c_str();
  readTypeAmror(argv[3], IndirectCall);
  // str="/root/liu/check/llvmO0/O0/map/gcc.txt";
  // str="/root/liu/check/O0gcc/O0/map/gcc.txt";
  // printf("输入省略点映射文件\n");
  // getline(cin, str);
  // file = (char *)str.c_str();
  readForToNext(argv[4]);
  // printf("M的大小为%d,F的大小为%d",ForNext.size(),proMap.size());
  // exit(1);
  // str="/home/ly/workspace/上下文敏感/1_30刘测试结果/ZYMhaveDC测试结果(4_16)/liu测试结果(4_18)/gcc.cfa_O0
  // str="/root/liu/check/llvm-verifi/gcc_base.llvm_O0_liu-re_allPress";
  // str="/root/liu/check/gcc-verifi/gcc_base.cfa_O0_liu-re_allPress";
  // str="/home/ly/software/codeblock/check/gcc.cfa_liu-re";
  // printf("输入待验证文件\n");
  // getline(cin, str);
  // file = (char *)str.c_str();
  // auto iter=proMap.find(4227523)->second.Jump.begin();
  // printf("%x\n",*iter);
  // clock_t start = clock();
  // printf("输入二进制执行次数\n");
  executetime = atoi(argv[6]);
  // scanf("%d", &executetime);
  readReceive(argv[5], Main, Ret);
  // clock_t end1 = clock();
  cout << (timeTotal - timeRead) / CLOCKS_PER_SEC << endl;
  cout << "execution time: " << timeTotal / CLOCKS_PER_SEC << endl;
  cout << "Num of addresses attested: " << total << endl;
}
int st = 0;
void readAsmFile(char *file, set<int> *IndirectSet, int *Main,
                 set<int> *IndirectCall, int *Ret, string binarytype) {
  fstream f(file);
  string line;
  bool flag = false;
  regex call("([0-9a-z]{6})(.*)(callq)");
  regex next("([0-9a-z]{6}):");
  regex directcall("([0-9a-z]{6})(.*)(callq)(\\s{2})([0-9a-z]{6})");
  smatch m;
  while (getline(f, line)) {
    auto ret = regex_search(line, m, call);
    while (ret) {
      newblock a;
      a.Type = 3;
      char *c;
      c = (char *)(m.str(1)).c_str();
      int b = strtoll(c, NULL, 16);
      ret = regex_search(line, m, directcall);
      if (ret) {
        int d;
        c = (char *)(m.str(5)).c_str();
        d = strtoll(c, NULL, 16);
        a.directTarget = d;
      }
      getline(f, line);
      ret = regex_search(line, m, next);
      if (ret) {
        int d;
        c = (char *)(m.str(1)).c_str();
        d = strtoll(c, NULL, 16);
        a.Pair = d;
        proMap.insert(pair<int, newblock>(b, a));
      } else {
        // cout << "失败" << endl;
        proMap.insert(pair<int, newblock>(b, a));
      }
      ret = regex_search(line, m, call);
    }
  }
  fstream f1(file);
  // printf("输入文件执行类型,llvm或者gcc\n");
  // string binarytype = "";
  // getline(cin, binarytype);
  regex Indirect("([0-9a-z]{6})(.*)(jmpq   \\*%rax)");
  if (binarytype == "gcc")
    Indirect = "([0-9a-z]{6})(.*)(jmpq   \\*%rax)";
  else
    Indirect = "([0-9a-z]{6})(.*)(jmpq   \\*%rcx)";
  regex main("([0-9a-z]{6}) \\<main\\>:");
  regex main_ret("([0-9a-z]{6}):(.*)retq");
  regex IC("([0-9a-z]{6})(.*)(callq  \\*)");
  regex stat("([0-9a-z]{6}) <__stat>:");
  while (getline(f1, line)) {
    auto ret = regex_search(line, m, main);
    if (ret) {
      char *c;
      int b;
      c = (char *)(m.str(1)).c_str();
      b = strtoll(c, NULL, 16);
      (*Main) = b;
      flag = true;
      continue;
    }
    ret = regex_search(line, m, Indirect);
    if (ret) {
      char *c;
      int b;
      c = (char *)(m.str(1)).c_str();
      b = strtoll(c, NULL, 16);
      (*IndirectSet).insert(b);
    }
    ret = regex_search(line, m, IC);
    if (ret) {
      char *c;
      int b;
      c = (char *)(m.str(1)).c_str();
      b = strtoll(c, NULL, 16);
      // printf("%xcall\n",b);
      (*IndirectCall).insert(b);
    }
    ret = regex_search(line, m, main_ret);
    if (ret && flag) {
      char *c;
      int b;
      c = (char *)(m.str(1)).c_str();
      b = strtoll(c, NULL, 16);
      (*Ret) = b;
      flag = false;
    }
    ret = regex_search(line, m, stat);
    if (ret) {
      char *c;
      int b;
      c = (char *)(m.str(1)).c_str();
      b = strtoll(c, NULL, 16);
      st = b;
      /// printf("%x state", st);
    }
  }
}

void readDyninst(char *file, set<int> *IndirectSet) {
  fstream f(file);
  regex block("\\[([0-9a-z]{6}),([0-9a-z]{6})");
  regex Jump("\"([0-9a-z]{6})\" -\\> \"([0-9a-z]{6})\"");
  regex Call("\"([0-9a-z]{6})\" -\\> \"([0-9a-z]{6})\" \\[color=blue");
  map<int, int> Block;
  smatch m;
  string line;
  while (getline(f, line)) {
    auto ret = regex_search(line, m, block);
    if (ret) {
      char *c = (char *)m.str(1).c_str();
      int b = strtoll(c, NULL, 16);
      c = (char *)m.str(2).c_str();
      int d = strtoll(c, NULL, 16);
      Block.insert(pair<int, int>(b, d));
      continue;
    }
    ret = regex_search(line, m, Jump);
    if (ret) {
      char *c = (char *)m.str(1).c_str();
      int b = strtoll(c, NULL, 16);
      if (!Block.count(b))
        continue;
      b = Block.find(b)->second;
      if (proMap.find(b) != proMap.end()) {
        int d;
        c = (char *)m.str(2).c_str();
        d = strtoll(c, NULL, 16);
        set<int> &set1 = (proMap.find(b)->second).Jump;
        if ((*IndirectSet).count(b)) {
          set1.insert(d);
          continue;
        }
        // cout<<m.str(2)<<"  "<<proMap.find(b)->second.Type<<endl;
        ret = regex_search(line, m, Call);
        if (ret) {
          // printf("%xjump%x\n",b,d);
          set1.insert(d);
        }
      } else {
        if ((*IndirectSet).count(b)) {
          int d;
          c = (char *)m.str(2).c_str();
          d = strtoll(c, NULL, 16);
          newblock a;
          a.Type = 2;
          a.Jump.insert(d);
          proMap.insert(pair<int, newblock>(b, a));
        }
      }
    }
  }
}

void readForToNext(char *file) {
  fstream f(file);
  string line;
  regex Jump("([0-9a-z]{6}) \\[([0-9a-z]{6})");
  char *c;
  int a, b;
  smatch m;
  while (getline(f, line)) {
    auto ret = regex_search(line, m, Jump);
    if (ret) {
      c = (char *)m.str(1).c_str();
      a = strtoll(c, NULL, 16);
      c = (char *)m.str(2).c_str();
      b = strtoll(c, NULL, 16);
      // cout<<m.str(1)<<" "<<m.str(2)<<endl;
      ForNext.insert(pair<int, int>(a, b));
    }
  }
}

int count1 = 0;
int Maintotal = 1;
void readReceive(char *file, int Main, int Ret) {

  FILE *in;
  in = fopen(file, "r");
  int buffer;
  int rbyte = 0;
  queue<int> sto;
  stack<int> shadow;
  FtoN(&sto, Main);
  // cout << sto.size() << "push" << endl;
  // printf("%dexecutetime\n", executetime);
  clock_t start, end;
  while (rbyte = fread(&buffer, 4, 1, in) == 1) {
    // if((buffer&16777216)&&Maintotal<executetime)
    if (buffer == 0xffffeeee && Maintotal < 2) {
      Maintotal++;
      // printf("%xssssssssssssssssssss",buffer);
      count1 = 0;
      FtoN(&sto, Main);
      fread(&buffer, 4, 1, in);
    }
    // printf("%x\n",buffer);
    if (buffer & (2147483648)) {
      // printf("%xbefore\n",buffer);
      buffer = buffer - 2147483648;
      // printf("%xafter\n",buffer);
      sto.push(buffer);
      sto.push(proMap.find(buffer)->second.directTarget);
      // printf("%xhhhhhhhhhhh\n",proMap.find(buffer)->second.directTarget);
      buffer = proMap.find(buffer)->second.directTarget;
      count1 = count1 ^ 1;
    } else
      sto.push(buffer);
    if ((count1 = (count1 ^ 1)) == 0) {
      FtoN(&sto, buffer);
    }
    // if(sto.size()>1869)
    // exit(-1);
    if (sto.size() > 10000000 && (sto.size() % 2 == 0)) {
      start = clock();
      bool out = verifi(&sto, in, &shadow, Ret, Main);
      end = clock();
      timeTotal = timeTotal + (end - start);
      if (out) {
        fclose(in);
        return;
      }
    }
  }
  /// printf("gcc");
  start = clock();
  verifi(&sto, in, &shadow, Ret, Main);
  end = clock();
  timeTotal = timeTotal + (end - start);
  fclose(in);
}
void FtoN(queue<int> *sto, int For) {
  if (ForNext.count(For)) {
    int b = ForNext.find(For)->second;
    // sto->push(b);
    if (proMap.find(b)->second.Jump.size() != 1) {
      cout << "dyninst fail  " << proMap.find(b)->second.Jump.size() << endl;
      exit(-1);
    }
    auto iter = proMap.find(b)->second.Jump.begin();
    int c = *iter;
    if (c == st) {
      // printf("%x %xprestat ",sto->front(),c);
      return;
    }
    // printf("%xinsert\n",b);
    // printf("%xinsert\n",c);
    sto->push(b);
    sto->push(c);
    // if(b==0x69ec90)
    // printf("Pre%x\n",For);
    // printf("Next%x\n",b);
    // cout<<"success"<<endl;
    success++;
    FtoN(sto, c);
  }
}

bool verifi(queue<int> *sto, FILE *in, stack<int> *shadow, int Ret, int Main) {
  int buffer;
  bool flag = false;
  clock_t start, end;
  while (!sto->empty()) {
    buffer = sto->front();
    sto->pop();
    total++;
    // if(total>800000)
    //{
    // printf("%x\n",buffer);
    // printf("%x\n",sto->front());
    //}
    // if(total>817200)
    // exit(-1);
    if (proMap.count(buffer)) {
      newblock block = proMap.find(buffer)->second;
      int type = block.Type;
      if (type == 1) {
        shadow->push(block.Pair);
        // cout<<"Indirect Call"<<endl;
        if (sto->empty()) {
          start = clock();
          verifiSecond(sto, in, Ret, Main);
          end = clock();
          timeRead = timeRead + (end - start);
        }
        buffer = sto->front();
        sto->pop();
        total++;
        if (block.Jump.count(buffer))
          continue;
        else {
          cout << "Indirect Call" << endl;
          exit(-1);
        }
      }
      if (type == 2) {
        if (sto->empty()) {
          start = clock();
          verifiSecond(sto, in, Ret, Main);
          end = clock();
          timeRead = timeRead + (end - start);
        }
        buffer = sto->front();
        sto->pop();
        total++;
        if (block.Jump.count(buffer))
          continue;
        else {
          cout << "Indirect Jump" << endl;
          exit(-1);
        }
      }
      if (type == 3) {
        if (sto->empty()) {
          start = clock();
          verifiSecond(sto, in, Ret, Main);
          end = clock();
          timeRead = timeRead + (end - start);
        }
        sto->pop();
        total++;
        shadow->push(block.Pair);
        // printf("shadow%x\n",block.Pair);
        continue;
      }
    } else {
      if (sto->empty()) {
        start = clock();
        verifiSecond(sto, in, Ret, Main);
        end = clock();
        timeRead = timeRead + (end - start);
      }
      if (shadow->empty()) {
        // cout<<sto->front()<<" "<<buffer<<endl;
        printf("运行结束时栈大小%lu \n", sto->size());
        printf("栈顶%x  当前验证地址%x ", sto->front(), buffer);
        // cout<<buffer<<"  "<<Ret<<endl;
        printf("Stack top:%x  Ret target:%x\n", buffer, Ret);
        if (buffer == Ret && (sto->front() == (0x7fffeeee))) {
          printf("%x ", sto->front());
          cout << "成功" << endl << total << endl;
          return true;
        } else
          continue;
      }
      if (sto->front() == shadow->top()) {
        // cout<<sto->front()<<"  "<<shadow->top()<<endl;
        sto->pop();
        total++;
        shadow->pop();
      } else
      // cout<<sto->front()<<"  "<<shadow->top()<<endl;
      {
        // printf("sto->front()%x   shadow->top()%x  buffer%x
        // total%d\n",sto->front(),shadow->top(),buffer,total); exit(-1);
        if (sto->front() == 0x7fffeeee)
          return true;
        flag = false;
        while (!shadow->empty()) {
          // printf("shadowstack1\n");
          shadow->pop();
          // printf("shadowstack2 %d\n",shadow->size());
          if (shadow->size() > 0 && sto->front() == shadow->top()) {
            sto->pop();
            total++;
            shadow->pop();
            flag = true;
            break;
          }
        }
        if (shadow->empty() && !flag) {
          printf("fail\n");
          return true;
        }
      }
    }
  }

  return false;
}

void verifiSecond(queue<int> *sto, FILE *in, int Ret, int Main) {
  int buffer = 0, last = 0;
  int rbyte = 0;
  // printf("here2\n");
  while (rbyte = fread(&buffer, 4, 1, in) == 1) {
    // if((buffer==0xffffeeee)&&Maintotal<executetime)
    if ((buffer < 0) && Maintotal < executetime) {
      Maintotal++;
      count1 = 0;
      // printf("%xssssssssssssssssssss",buffer);
      FtoN(sto, Main);
      fread(&buffer, 4, 1, in);
    }
    if (buffer & (2147483648)) {
      // printf("%d\n",buffer);
      buffer = buffer - 2147483648;
      sto->push(buffer);
      sto->push(proMap.find(buffer)->second.directTarget);
      // printf("%xhhhhhhhhhhh\n",proMap.find(buffer)->second.directTarget);
      buffer = proMap.find(buffer)->second.directTarget;
      count1 = count1 ^ 1;
    } else
      sto->push(buffer);
    if ((count1 = (count1 ^ 1)) == 0) {
      FtoN(sto, buffer);
    }
    if (sto->size() > 10000000) {
      return;
    }
  }
  // fclose(in);
  return;
}

void readTypeAmror(char *file, set<int> *IndirectSet) {
  fstream f(file);
  regex Call(
      "Indirectstar0x([0-9a-z]{6})  ([0-9]{1}) Indirectend0x([0-9a-z]{6})");
  smatch m;
  string line;
  while (getline(f, line)) {
    auto ret = regex_search(line, m, Call);
    if (ret) {
      char *c = (char *)m.str(1).c_str();
      int b = strtoll(c, NULL, 16);
      if (proMap.find(b) != proMap.end()) {
        newblock &block = (proMap.find(b)->second);
        block.Type = 1;
        int d;
        c = (char *)m.str(3).c_str();
        d = strtoll(c, NULL, 16);
        block.Jump.insert(d);
        // cout<<m.str(1)<<"  "<<proMap.find(b)->second.Type<<endl;
        ret = regex_search(line, m, Call);
      } else {
        cout << "Policy F read error." << endl;
        exit(-1);
      }
    }
  }
}
