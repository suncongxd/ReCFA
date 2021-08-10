
#include "BPatch.h"
#include "BPatch_addressSpace.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_flowGraph.h"
#include "BPatch_function.h"
#include "BPatch_point.h"
#include <stdio.h>
#include <string.h>

#include "Instruction.h"
#include "Operand.h"

#define BUFLEN 512
#define OUTCHANNEL stdout
#define KCHANNEL STDOUT_FILENO

using namespace std;
using namespace Dyninst;
using namespace InstructionAPI;

BPatch bpatch;
char *noNeedDcallsFileName = new char[100];

vector<string> compiler_build_in = {"_start",
                                    " __libc_start_main",
                                    "__libc_init_first",
                                    "__libc_csu_init",
                                    "__libc_csu_fini",
                                    "_init",
                                    "__x86.get_pc_thunk.ax",
                                    "__x86.get_pc_thunk.bx",
                                    "__x86.get_pc_thunk.cx",
                                    "__x86.get_pc_thunk.dx",
                                    "__gmon_start__ ",
                                    "frame_dummy",
                                    "__do_global_dtors_aux",
                                    "__do_global_ctors_aux",
                                    "register_tm_clones",
                                    "deregister_tm_clones",
                                    "_exit",
                                    "__call_tls_dtors",
                                    "_fini",
                                    "__stat",
                                    "__fstat",
                                    "__plt_dispatcher",
                                    "__divsc3",
                                    "__mulsc3",
                                    "stat64",
                                    "fstat64",
                                    "lstat64",
                                    "fstatat64",
                                    "atexit",
                                    "_dl_relocate_static_pie",
                                    "__divsc3",
                                    "__mulsc3"};

vector<string> ignore_callback = {
    // 以下部分为h264ref中qsort使用的回调函数
    "compare_pic_by_pic_num_desc", "compare_pic_by_lt_pic_num_asc",
    "compare_fs_by_frame_num_desc", "compare_fs_by_lt_pic_idx_asc",
    "compare_pic_by_poc_desc", "compare_pic_by_poc_asc",
    "compare_fs_by_poc_desc", "compare_fs_by_poc_asc",

    // 此为gcc中使用到的回调函数
    "callback_xmalloc", "callback_gmalloc"};

enum InsType { IJUMP, ICALL, DCALL, RET, NONE };

/* to return the syscalls called at the start and end of each inserted code
 * snippet */
BPatch_snippet *syscallGen(BPatch_image *appImage, const char *str) {
  vector<BPatch_function *> syscallFuncs;
  appImage->findFunction("syscall", syscallFuncs);
  if (syscallFuncs.size() == 0) {
    fprintf(OUTCHANNEL, "error: Could not find <syscall>\n");
    return NULL;
  }
  vector<BPatch_snippet *> syscallArgs;
  BPatch_snippet *arg1 = new BPatch_constExpr(4);
  BPatch_snippet *arg2 = new BPatch_constExpr(KCHANNEL);
  BPatch_snippet *arg3 = new BPatch_constExpr(str);
  BPatch_snippet *arg4 = new BPatch_constExpr(strlen(str));

  return new BPatch_funcCallExpr(*(syscallFuncs[0]), syscallArgs);
}

//通过函数名称判断是否为编译器插入函数
bool is_compiler_build_in(string candidate_name) {
  for (auto it = compiler_build_in.begin(); it != compiler_build_in.end();
       it++) {
    if (candidate_name == *it)
      return true;
  }

  return false;
}

//通过函数名称判断函数是否为需要忽略的callback函数
bool is_ignore_callback(string candidate_name) {
  for (auto it = ignore_callback.begin(); it != ignore_callback.end(); it++) {
    if (candidate_name == *it)
      return true;
  }

  return false;
}

// 获取程序中所有需要关注的函数（即除去编译器插入函数以及可忽略的callback函数）
void findFunctionEntries(BPatch_image *appImage,
                         vector<BPatch_function *> *funcs) {
  vector<BPatch_module *> *allModule = appImage->getModules();
  vector<BPatch_module *> useModules;

  for (auto mm = allModule->begin(); mm != allModule->end(); mm++) {
    if (!(*mm)->isSharedLib()) {
      useModules.push_back(*mm);
    }
  }

  if (useModules.size() == 0) {
    fprintf(OUTCHANNEL, "All modules are dynamic.\n");
    return;
  }

  funcs->clear(); //->erase(funcs->begin(),funcs->end());//clear the original
                  // func list.

  for (auto pos = useModules.begin(); pos != useModules.end(); pos++) {
    vector<BPatch_function *> *tmpFuncs = (*pos)->getProcedures();
    funcs->insert(funcs->end(), tmpFuncs->begin(), tmpFuncs->end());
  }

  char buffer[BUFLEN];

  vector<BPatch_function *>::iterator it = funcs->begin();

  while (it != funcs->end()) {
    string func_name = (*it)->getName(buffer, BUFLEN);

    // fprintf(OUTCHANNEL, "%s\n",(*it)->getName(buffer, BUFLEN));
    // if (is_compiler_build_in(func_name))
    if (is_compiler_build_in(func_name) || is_ignore_callback(func_name)) {
      it = funcs->erase(it);
    } else
      it++;
  }

  // for(auto it=funcs->begin(); it!=funcs->end(); it++){
  //	 fprintf(OUTCHANNEL, "%s\n", (*it)->getName(buffer, BUFLEN));
  // }
  return;
}

// 根据函数的操作符和操作数判断指令类型
InsType classifyInstruction(Instruction insn) {
  Operation op = insn.getOperation();

  // printf("%s\n", op.format().c_str());
  string op_str = op.format();

  if (op_str == "jmp") {
    vector<Operand> operands;
    insn.getOperands(operands);

    if (!strncmp(operands.at(0).format(Arch_x86_64).c_str(), "0x", 2) &&
        strstr(operands.at(0).format(Arch_x86_64).c_str(), "\%rip") != NULL) {
      // direct jump to offset
    } else if (!strncmp(operands.at(0).format(Arch_x86_64).c_str(), "\%r", 2)) {
      return IJUMP;
    } else {
      // 在O0级别的优化下，没有找到werid
      // jump，但是在其他优化级别(O1-O3)，都有此情况出现，因此该情况下判断为IJUMP
      fprintf(OUTCHANNEL, "%s, weird jump! \n",
              operands.at(0).format(Arch_x86_64).c_str());
      return IJUMP;
    }
  }

  if (op_str == "call") {
    vector<Operand> operands;
    insn.getOperands(operands);

    if (!strncmp(operands.at(0).format(Arch_x86_64).c_str(), "0x", 2) &&
        strstr(operands.at(0).format(Arch_x86_64).c_str(), "\%rip") != NULL) {
      return DCALL;
    } else if (!strncmp(operands.at(0).format(Arch_x86_64).c_str(), "\%r", 2)) {
      // for(auto iter=operands.begin(); iter!=operands.end(); iter++) {
      //		   fprintf(OUTCHANNEL, "%s, ",
      //(*iter).format(Arch_x86_64).c_str());
      // }
      // printf("\n");
      return ICALL;
    } else {
      // 在O0级别的优化下，没有找到werid
      // call，但是在其他优化级别(O1-O3)，都有此情况出现,因此判断为ICALL
      fprintf(OUTCHANNEL, "%s, weird call! \n",
              operands.at(0).format(Arch_x86_64).c_str());
      return ICALL;
    }
  }

  // REP ret near在O0级别的优化中无，但是在其他优化级别(O1-O3)，都有此情况出现
  if (op_str == "ret near" || op_str == "ret" || op_str == "REP ret near") {
    return RET;
  }

  return NONE;
}

void getCallAfterPoints(BPatch_image *appImage, Address addr,
                        Instruction lastinsn,
                        vector<BPatch_point *> &call_after_points) {
  // get indirect call after point
  Address call_after_addr = addr + (Address)(lastinsn.size());

  call_after_points.clear();

  if (!appImage->findPoints(call_after_addr, call_after_points)) {
    fprintf(
        OUTCHANNEL,
        "At 0x%lx: Fail to build call-after point of indirect call 0x%lx.\n",
        call_after_addr, addr);
  }
}

void getDcallAdress(BPatch_function *func, vector<BPatch_point *> *exitPoints,
                    vector<Address> &dCallAddresses) {
  vector<BPatch_point *> *call_sites = func->findPoint(BPatch_subroutine);

  // first find all direct calls to other local functions.
  for (auto pp = call_sites->begin(); pp != call_sites->end(); pp++) {
    BPatch_point *tmp_site = *pp;

    if (!tmp_site->isDynamic()) {
      BPatch_function *tmp_callee = tmp_site->getCalledFunction();

      if (tmp_callee != NULL) {
        char funcName[50];

        strcpy(funcName, tmp_callee->getName().c_str());

        if (!tmp_callee->isSharedLib()) {
          // 2.
          // 对于.plt.got段的函数不会被识别为动态库中的函数的处理.因为该函数通过getName()获得的函数名为targ+十六进制首地址的形式，
          //因此获取该函数的首地址，
          //将首地址转换为十六进制字符串，若该字符串是函数名的子串，则说明是.plt.got段的动态库中的函数
          int callee_addr = (Address)

                                tmp_callee->getBaseAddr();
          char callee_addr_buf[20] = {0};

          //转为16进制字符串
          sprintf(callee_addr_buf, "%x", callee_addr);

          if (!strstr(funcName, callee_addr_buf)) {
            //排除调用的是build in函数
            if (!is_compiler_build_in(funcName) &&
                !is_ignore_callback(funcName)) {
              dCallAddresses.push_back((Address)(tmp_site->getAddress()));

              // printf("%s %x\n", funcName, callee_addr);
            }
          }
        } else {
          // 此处是寻找调用了动态库中exit函数的点，放在此处保证了被调用的函数是来自动态库（通过上方的else得出），
          // 另外exit函数可能会对应用动态库中的不同名字的exit函数，因此此处使用的是strstr判断
          if (strstr(funcName, "exit") != NULL) {
            // printf("%s\n", funcName);
            exitPoints->push_back(*pp);
          }
        }
      }
    }
  }

  // for(vector<Address>::iterator it=dCallAddresses.begin();
  // it!=dCallAddresses.end(); it++) {
  //		   fprintf(OUTCHANNEL, "0x%lx, ", (*it));
  // }
  // fprintf(OUTCHANNEL, "\n");
}

/*
@brief: getNoNeedDCallAddr
读取文件noNeedDcallsFileName，将其中保存的不需要的直接调用地址放入
        noNeedDCallAddr中
*/
void getNoNeedDCallAddr(vector<Address> &noNeedDCallAddr) {
  FILE *in;

  in = fopen(noNeedDcallsFileName, "r");
  int buffer;
  char nextLine;
  int rbyte = 0;

  //文件内容的形式是一行一个16进制的数，因此先读取16进制整数，然后读取换行符号
  while ((rbyte = fscanf(in, "%x", &buffer)) == 1) {
    noNeedDCallAddr.push_back(buffer);
    fscanf(in, "%c", &nextLine);
  }

  fclose(in);

  // printf("不需要的direct call点数量为%d\n", noNeedDCallAddr.size());
}

// 获取程序中各类型的点：iJmps(间接跳转)，iCalls(间接调用)，dcalls(直接调用点)，callAfters(跳转点的下一条指令点)，rets(返回点),exitPoints(程序退出点)
bool get_points(BPatch_image *appImage, vector<BPatch_point *> *iJmps,
                vector<BPatch_point *> *iCalls,
                vector<BPatch_point *> *callAfters,
                vector<BPatch_point *> *rets, vector<BPatch_point *> *dcalls,
                vector<BPatch_point *> *exitPoints,
                vector<BPatch_point *> *liuDcalls) {
  // 读取不需要的direct call点
  vector<Address> noNeedDCallAddr;
  getNoNeedDCallAddr(noNeedDCallAddr);

  vector<BPatch_function *> funcs;
  findFunctionEntries(appImage, &funcs);

  for (auto pf = funcs.begin(); pf != funcs.end(); pf++) {
    char funcNameBuffer[BUFLEN];

    (*pf)->getName(funcNameBuffer, BUFLEN);
    vector<Address> dCallAddresses;
    getDcallAdress(*pf, exitPoints, dCallAddresses);

    BPatch_flowGraph *cfg = (*pf)->getCFG();

    set<BPatch_basicBlock *> basicblocks;
    cfg->getAllBasicBlocks(basicblocks);

    for (auto bb = basicblocks.begin(); bb != basicblocks.end(); bb++) {
      vector<Instruction> insns;
      (*bb)->getInstructions(insns);
      Instruction lastinsn = insns[insns.size() - 1];

      InsType ins_type = classifyInstruction(lastinsn);

      if (ins_type != NONE) {
        Address addr = (Address)((*bb)->findExitPoint()->getAddress());

        vector<BPatch_point *> tmp_points;

        if (!appImage->findPoints(addr, tmp_points)) {
          fprintf(OUTCHANNEL,
                  "Fail to get patch point from exit address of bb.\n");
        }

        vector<BPatch_point *> call_after_points;
        Address call_after_addr;

        switch (ins_type) {
        case IJUMP:
          iJmps->insert(iJmps->end(), tmp_points.begin(), tmp_points.end());
          break;

        case ICALL:
          iCalls->insert(iCalls->end(), tmp_points.begin(), tmp_points.end());

          // get indirect call after point
          getCallAfterPoints(appImage, addr, lastinsn, call_after_points);
          callAfters->insert(callAfters->end(), call_after_points.begin(),
                             call_after_points.end());
          break;

        case DCALL:
          // 使用if进行判断的原因是因为需要排除调用动态库的点
          if (find(dCallAddresses.begin(), dCallAddresses.end(), addr) !=
              dCallAddresses.end()) {
            if (find(noNeedDCallAddr.begin(), noNeedDCallAddr.end(), addr) ==
                noNeedDCallAddr.end()) {
              liuDcalls->insert(liuDcalls->begin(), tmp_points.begin(),
                                tmp_points.end());
            }

            dcalls->insert(dcalls->begin(), tmp_points.begin(),
                           tmp_points.end());
            getCallAfterPoints(appImage, addr, lastinsn, call_after_points);
            callAfters->insert(callAfters->end(), call_after_points.begin(),
                               call_after_points.end());
          }

          break;

        case RET:
          rets->insert(rets->end(), tmp_points.begin(), tmp_points.end());

          if (strcmp(funcNameBuffer, "main") == 0) {
            exitPoints->insert(exitPoints->end(), tmp_points.begin(),
                               tmp_points.end());
          }

          break;
        }
      }
    }
  }

  return true;
}

void display_for_sanity_check(vector<BPatch_point *> points, const char *type) {
  for (auto iter = points.begin(); iter != points.end(); iter++) {
    fprintf(OUTCHANNEL, "%s:0x%lx\n", type, (Address)(*iter)->getAddress());
  }
}

// Find a point at function "name"
vector<BPatch_point *> *find_point_of_func(BPatch_image *appImage,
                                           const char *name,
                                           BPatch_procedureLocation loc) {
  vector<BPatch_function *> functions;
  vector<BPatch_point *> *points;

  // Scan for functions named "name"
  appImage->findFunction(name, functions);

  if (functions.size() == 0) {
    fprintf(OUTCHANNEL, "No function %s\n", name);
    return points;
  }

  // Locate the relevant points
  points = functions[0]->findPoint(loc);

  if (points == NULL) {
    fprintf(OUTCHANNEL, "No wanted point for function %s\n", name);
  }

  return points;
}

/* @ entry of main:
   FILE *outFile=fopen(path_name, "wb+");*/
bool createAndInsertFopen(BPatch_addressSpace *app, char *path_name) {
  BPatch_image *appImage = app->getImage();

  /*******************************/
  BPatch_type *FILEPtr =
      bpatch.createPointer("FILEPtr", appImage->findType("FILE"));
  BPatch_variableExpr *filePointer =
      app->malloc(*(appImage->findType("FILEPtr")), "outFile");

  vector<BPatch_function *> fopenFuncs;
  appImage->findFunction("fopen", fopenFuncs);

  if (fopenFuncs.size() == 0) {
    fprintf(OUTCHANNEL, "error: Could not find <fopen>\n");
    return false;
  }

  vector<BPatch_snippet *> fopenArgs;
  BPatch_snippet *param1 = new BPatch_constExpr(path_name);

  // 此处使用a+而非wb+的原因是因为测试spec
  // 的test集中可能会以不同的输入多次运行该被测函数
  BPatch_snippet *param2 = new BPatch_constExpr("a+");

  fopenArgs.push_back(param1);
  fopenArgs.push_back(param2);
  BPatch_funcCallExpr fopenCall(*(fopenFuncs[0]), fopenArgs);
  BPatch_arithExpr *fileAssign =
      new BPatch_arithExpr(BPatch_assign, *filePointer, fopenCall);

  /*syscalls*/
  BPatch_snippet *syscall_s = syscallGen(appImage, "Fopen-S");
  BPatch_snippet *syscall_t = syscallGen(appImage, "Fopen-E");

  /*******************************/
  vector<BPatch_snippet *> items;
  items.push_back(syscall_s);
  items.push_back(fileAssign);
  items.push_back(syscall_t);
  BPatch_sequence allItems(items);

  vector<BPatch_point *> *entryPoint =
      find_point_of_func(appImage, "main", BPatch_entry);

  if (entryPoint->size() != 0 && !app->insertSnippet(allItems, *entryPoint)) {
    fprintf(OUTCHANNEL, "error: Fail to insert <fopen>\n");
    return false;
  }

  return true;
}

bool createAndInsertInitialization(BPatch_addressSpace *app) {
  BPatch_image *appImage = app->getImage();

  // int addrsCube[0..519];
  BPatch_type *intArray = bpatch.createArray(
      "intArray", appImage->findType("int"), 0,
      520); //设置为256个地址对，即512个int形式，稍微给大点520
  BPatch_variableExpr *dynAddrsCube =
      app->malloc(*(appImage->findType("intArray")), "addrsCube");

  /* int *beginPtr;
  int *endPtr;
  int *currentPtr; */
  BPatch_type *intPtr =
      bpatch.createPointer("intPtr", appImage->findType("int"));
  BPatch_variableExpr *beginPtr =
      app->malloc(*(appImage->findType("intPtr")), "beginPtr");
  BPatch_variableExpr *endPtr =
      app->malloc(*(appImage->findType("intPtr")), "endPtr");
  BPatch_variableExpr *currentPtr =
      app->malloc(*(appImage->findType("intPtr")), "currentPtr");

  // beginPtr = &addrsCube[0];
  BPatch_snippet *firstElem =
      new BPatch_arithExpr(BPatch_ref, *dynAddrsCube, BPatch_constExpr(0));
  BPatch_snippet *beginPtrInit = new BPatch_arithExpr(
      BPatch_assign, *beginPtr, BPatch_arithExpr(BPatch_addr, *firstElem));

  // endPtr = &addrsCube[511];
  BPatch_snippet *lastElem =
      new BPatch_arithExpr(BPatch_ref, *dynAddrsCube, BPatch_constExpr(511));
  BPatch_snippet *endPtrInit = new BPatch_arithExpr(
      BPatch_assign, *endPtr, BPatch_arithExpr(BPatch_addr, *lastElem));

  // currentPtr = beginPtr;
  BPatch_snippet *currentPtrInit =
      new BPatch_arithExpr(BPatch_assign, *currentPtr, *beginPtr);

  // bool retStatus = false;
  BPatch_variableExpr *retStatus =
      app->malloc(*(appImage->findType("boolean")), "retStatus");
  BPatch_snippet *retStatusInit =
      new BPatch_arithExpr(BPatch_assign, *retStatus, BPatch_constExpr(false));

  /*syscalls*/
  BPatch_snippet *syscall_s = syscallGen(appImage, "Init-S");
  BPatch_snippet *syscall_t = syscallGen(appImage, "Init-E");

  vector<BPatch_snippet *> items;
  items.push_back(syscall_s);
  items.push_back(beginPtrInit);
  items.push_back(endPtrInit);
  items.push_back(currentPtrInit);
  items.push_back(retStatusInit);
  items.push_back(syscall_t);
  BPatch_sequence allItems(items);

  vector<BPatch_point *> *points =
      find_point_of_func(appImage, "main", BPatch_entry);

  if (points->size() != 0 &&
      !app->insertSnippet(allItems, *points, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

/* if(currentPtr > endPtr){
   fwrite(beginPtr, 1, currentPtr-beginPtr, outFile);
   currentPtr = beginPtr;
   }
*/
BPatch_snippet *arrayFullSaveInFile(BPatch_image *appImage) {
  BPatch_variableExpr *beginPtr = appImage->findVariable("beginPtr");
  BPatch_variableExpr *endPtr = appImage->findVariable("endPtr");
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");

  // Find the fwrite function
  BPatch_snippet *fwriteCall;

  vector<BPatch_function *> fwriteFuncs;
  appImage->findFunction("fwrite", fwriteFuncs);

  if (fwriteFuncs.size() == 0) {
    fprintf(OUTCHANNEL, "error:Could not find <fwrite>\n");
    fwriteCall = NULL;
  } else {
    // fwrite(beginPtr, 1, currentPtr-beginPtr, outFile);
    vector<BPatch_snippet *> fwriteArgs;
    fwriteArgs.push_back(beginPtr);
    fwriteArgs.push_back(new BPatch_constExpr(1));
    fwriteArgs.push_back(
        new BPatch_arithExpr(BPatch_minus, *currentPtr, *beginPtr));
    fwriteArgs.push_back(appImage->findVariable("outFile"));
    fwriteCall = new BPatch_funcCallExpr(*(fwriteFuncs[0]), fwriteArgs);
  }

  // currentPtr = beginPtr;
  BPatch_snippet *restoreCurrentPtr =
      new BPatch_arithExpr(BPatch_assign, *currentPtr, *beginPtr);

  vector<BPatch_snippet *> prvItems;

  if (fwriteCall != NULL)
    prvItems.push_back(fwriteCall);

  prvItems.push_back(restoreCurrentPtr);
  BPatch_sequence prvAllItems(prvItems);

  // currentPtr > endPtr
  BPatch_boolExpr conditional(BPatch_ge, *currentPtr, *endPtr);

  return new BPatch_ifExpr(conditional, prvAllItems);
}

// Inserting syscall cause workload error.
// Save current address to array, dump to file if array is full.
bool createAndInsertCurrentAddrStore(BPatch_addressSpace *app,
                                     vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_originalAddressExpr());

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(isArrayFull);
  BPatch_sequence allItems(items);

  if (points.size() != 0 &&
      !app->insertSnippet(allItems, points, BPatch_firstSnippet)) {
    return false;
  }

  return true;
}

// Save target address to array, dump to file if array is full.
bool createAndInsertTargetAddrStore(BPatch_addressSpace *app,
                                    vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // *currentPtr = BPatch_dynamicTargetExpr()
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *targetAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_dynamicTargetExpr());

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  /*syscalls*/
  BPatch_snippet *syscall_s = syscallGen(appImage, "TarAddrSto-S");
  BPatch_snippet *syscall_t = syscallGen(appImage, "TarAddrSto-E");

  vector<BPatch_snippet *> items;
  items.push_back(syscall_s);
  items.push_back(targetAddr);
  items.push_back(currentPtrAddone);
  items.push_back(isArrayFull);
  items.push_back(syscall_t);
  BPatch_sequence allItems(items);

  if (points.size() != 0 &&
      !app->insertSnippet(allItems, points, BPatch_firstSnippet)) {
    return false;
  }

  return true;
}

// Inserting syscall cause workload error.
// 对点进行插装，获取该点的源地址，目标地址，
bool createAndInsertDynamicEdges(BPatch_addressSpace *app,
                                 vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_originalAddressExpr());

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  // *currentPtr = BPatch_dynamicTargetExpr()
  BPatch_snippet *targetAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_dynamicTargetExpr());

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(targetAddr);
  items.push_back(currentPtrAddone);
  items.push_back(isArrayFull);
  BPatch_sequence allItems(items);

  if (points.size() != 0 &&
      !app->insertSnippet(allItems, points, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

// Inserting syscall cause workload error.
//在ret处只获取当前地址，ret的目标地址通过在callafter处的当前地址获得，使用了retStatus变量标识是否是从ret到的callafter点，在ret点做两件事：
// 1.输出当前的地址作为ret的起始地址，存储到数组中，currentPtr移动到下一个存储位置
// 2.将retstatus=true
bool createAndInsertForRet(BPatch_addressSpace *app,
                           vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // 1. *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_originalAddressExpr());

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  // 2.retstatus=true
  BPatch_variableExpr *retStatus = appImage->findVariable("retStatus");
  BPatch_snippet *retStatusChange =
      new BPatch_arithExpr(BPatch_assign, *retStatus, BPatch_constExpr(true));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(retStatusChange);
  items.push_back(isArrayFull);
  BPatch_sequence allItems(items);

  if (points.size() != 0 &&
      !app->insertSnippet(allItems, points, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

// Inserting syscall cause workload error.
//对call
// after的点进行插装，获取当前地址作为ret的目标地址，需要先判断retstatus==true是否成立，成立则此时是从ret处到的callafter点，则需要做三件事情：
// 1.输出当前的地址作为ret的目标地址，存储到数组中，currentPtr移动到下一个存储位置
// 2.将retstatus=false
// 3.判断数组是否满，满则输出到文件
bool createAndInsertForCallAfter(BPatch_addressSpace *app,
                                 vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // 0. 判断retstatus==true是否成立
  BPatch_variableExpr *retStatus = appImage->findVariable("retStatus");
  BPatch_boolExpr condition(BPatch_eq, *retStatus, BPatch_constExpr(true));

  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_originalAddressExpr());

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  // retstatus=false
  BPatch_snippet *retStatusRestore =
      new BPatch_arithExpr(BPatch_assign, *retStatus, BPatch_constExpr(false));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(retStatusRestore);
  items.push_back(isArrayFull);
  BPatch_sequence thenbody(items);

  BPatch_ifExpr getRetDest(condition, thenbody);

  if (points.size() != 0 &&
      !app->insertSnippet(getRetDest, points, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

//对于直接调用点，输出该点的当前地址
bool createAndInsertForDCall(BPatch_addressSpace *app,
                             vector<BPatch_point *> &points) {
  if (!createAndInsertCurrentAddrStore(app, points)) {
    return false;
  }

  return true;
}

// Inserting syscall cause workload error.
// Dump the rest content of array to file for the program exit funtion
bool createAndInsertDumpAtProgramExitPoints(BPatch_addressSpace *app,
                                            vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  BPatch_variableExpr *beginPtr = appImage->findVariable("beginPtr");
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");

  BPatch_snippet *fwriteCall;

  vector<BPatch_function *> fwriteFuncs;
  appImage->findFunction("fwrite", fwriteFuncs);

  if (fwriteFuncs.size() == 0) {
    fprintf(OUTCHANNEL, "error:Could not find <fwrite>\n");
    fwriteCall = NULL;
  } else {
    // fwrite(beginPtr, 1, currentPtr-beginPtr, outFile);
    vector<BPatch_snippet *> fwriteArgs;
    fwriteArgs.push_back(beginPtr);
    fwriteArgs.push_back(new BPatch_constExpr(1));
    fwriteArgs.push_back(
        new BPatch_arithExpr(BPatch_minus, *currentPtr, *beginPtr));
    fwriteArgs.push_back(appImage->findVariable("outFile"));
    fwriteCall = new BPatch_funcCallExpr(*(fwriteFuncs[0]), fwriteArgs);
  }

  // vector<BPatch_snippet *> printfArgs;
  // BPatch_snippet *fmt = new BPatch_constExpr("test for exit func: %d\n");
  // printfArgs.push_back(fmt);
  // printfArgs.push_back(new BPatch_arithExpr(BPatch_minus, *currentPtr,
  // *beginPtr));
  // //BPatch_snippet *eae = new BPatch_effectiveAddressExpr();
  // //printfArgs.push_back(eae);
  // vector<BPatch_function *> printfFuncs;
  // appImage->findFunction("printf", printfFuncs);
  // if (printfFuncs.size() == 0)
  // {
  //	fprintf(OUTCHANNEL, "Could not find printf\n");
  //	return false;
  // }
  // BPatch_snippet *printfCall = new BPatch_funcCallExpr(*(printfFuncs[0]),
  // printfArgs);

  vector<BPatch_snippet *> items;

  if (fwriteCall != NULL)
    items.push_back(fwriteCall);

  // items.push_back(printfCall);
  BPatch_sequence allItems(items);

  if (points.size() != 0 &&
      !app->insertSnippet(allItems, points, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

/* @ exit of program exit points:
   fclose(outFile);*/
bool createAndInsertFcloseAtProgramExitPoints(BPatch_addressSpace *app,
                                              vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  /**********************************/
  vector<BPatch_function *> fcloseFuncs;
  appImage->findFunction("fclose", fcloseFuncs);

  if (fcloseFuncs.size() == 0) {
    fprintf(OUTCHANNEL, "error: Could not find <fclose>\n");
    return false;
  }

  vector<BPatch_snippet *> fcloseArgs;
  BPatch_variableExpr *var = appImage->findVariable("outFile");

  if (!var) {
    fprintf(OUTCHANNEL, "Could not find 'outFile' variable\n");
    return false;
  } else {
    fcloseArgs.push_back(var);
  }

  BPatch_funcCallExpr fcloseCall(*(fcloseFuncs[0]), fcloseArgs);

  /**********************************/
  if (!app->insertSnippet(fcloseCall, points, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

void getBasicBlockLoopsforFunc(BPatch_addressSpace *app,
                               BPatch_loopTreeNode *rootLoop,
                               std::vector<BPatch_basicBlockLoop *> &bBLs,
                               vector<BPatch_point *> &dCalls) {
  if (rootLoop->loop != NULL) {
    bool sign = false;

    std::vector<BPatch_basicBlock *> basicBlocksInLoop;
    rootLoop->loop->getLoopBasicBlocks(basicBlocksInLoop);

    for (auto bb = basicBlocksInLoop.begin(); bb != basicBlocksInLoop.end();
         bb++) {
      vector<Instruction> insns;
      (*bb)->getInstructions(insns);
      Instruction lastinsn = insns[insns.size() - 1];
      InsType ins_type = classifyInstruction(lastinsn);

      if (ins_type == IJUMP || ins_type == ICALL) {
        sign = true;
        break;
      } else if (ins_type == DCALL) {
        Address addr = (Address)((*bb)->findExitPoint()->getAddress());

        for (auto pp = dCalls.begin(); pp != dCalls.end(); pp++) {
          Address pointAddr = (Address)(*pp)->getAddress();

          if (addr == pointAddr) {
            sign = true;
            break;
          }
        }
      }
    }

    if (sign) {
      bBLs.push_back(rootLoop->loop);
      std::vector<BPatch_basicBlock *> entries;
      rootLoop->loop->getLoopEntries(entries);

      for (auto ee = entries.begin(); ee != entries.end(); ee++) {
        fprintf(OUTCHANNEL, "%lx ", (*ee)->getStartAddress());
      }
    }
  }

  std::vector<BPatch_loopTreeNode *> loops = rootLoop->children;

  if (loops.size() != 0) {
    fprintf(OUTCHANNEL, "loopsize = %lu\n", loops.size());

    for (auto ll = loops.begin(); ll != loops.end(); ll++) {
      getBasicBlockLoopsforFunc(app, *ll, bBLs, dCalls);
    }
  }
}

bool insertSymbolSnippetForPoints(BPatch_addressSpace *app,
                                  vector<BPatch_point *> &points, int symbol) {
  BPatch_image *appImage = app->getImage();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *symbolSnippet = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_constExpr(symbol));

  // //currentPtr移动到下一个存储位置
  // // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(symbolSnippet);
  items.push_back(currentPtrAddone);
  items.push_back(isArrayFull);
  BPatch_sequence allItems(items);

  if (!app->insertSnippet(
          allItems, points,
          BPatch_lastSnippet)) // if (!app->insertSnippet(printfCall,
                               // loopEntryPoints, BPatch_lastSnippet))
  {
    fprintf(OUTCHANNEL, "insert local addr printing snippet failed.\n");
    return false;
  }

  return true;
}

void getAndInsertForLoop(BPatch_addressSpace *app,
                         vector<BPatch_point *> &dCalls) {
  BPatch_image *appImage = app->getImage();

  vector<BPatch_function *> funcs;
  findFunctionEntries(appImage, &funcs);
  fprintf(OUTCHANNEL, "get loop:%lu\n", funcs.size());
  int i = 0;

  // 循环体循环起始点
  vector<BPatch_point *> loopStartPoints;

  // 循环体循环结束点
  vector<BPatch_point *> loopEndPoints;

  // 循环进入点BPatch_locLoopEntry
  vector<BPatch_point *> loopEntryPoints;

  // 循环退出点BPatch_locLoopExit
  vector<BPatch_point *> loopExitPoints;

  for (auto pf = funcs.begin(); pf != funcs.end(); pf++) {
    char buffer[BUFLEN];

    fprintf(OUTCHANNEL, "%d, %s\n", i++, (*pf)->getName(buffer, BUFLEN));
    BPatch_flowGraph *cfg = (*pf)->getCFG();
    BPatch_loopTreeNode *rootLoop = cfg->getLoopTree();

    vector<BPatch_basicBlockLoop *> basicBlockLoops;
    getBasicBlockLoopsforFunc(app, rootLoop, basicBlockLoops, dCalls);
    fprintf(OUTCHANNEL, "Num of current loop: %lu\n", basicBlockLoops.size());

    // 1.通过BPatch_flowGraph 函数findLoopInstPoints获取到入口点
    // printf("getloop\n");
    for (auto bb = basicBlockLoops.begin(); bb != basicBlockLoops.end(); bb++) {
      // BPatch_locLoopStartIter，BPatch_locLoopEndIter循环体起始点，结束点
      // BPatch_locLoopEntry，BPatch_locLoopExit循环起始点，结束点
      std::vector<BPatch_point *> *points1 =
          cfg->findLoopInstPoints(BPatch_locLoopStartIter, *bb);
      std::vector<BPatch_point *> *points2 =
          cfg->findLoopInstPoints(BPatch_locLoopEndIter, *bb);
      std::vector<BPatch_point *> *points3 =
          cfg->findLoopInstPoints(BPatch_locLoopEntry, *bb);
      std::vector<BPatch_point *> *points4 =
          cfg->findLoopInstPoints(BPatch_locLoopExit, *bb);

      if (points1 != NULL) {
        loopStartPoints.insert(loopStartPoints.end(), points1->begin(),
                               points1->end());
      }

      if (points2 != NULL) {
        loopEndPoints.insert(loopEndPoints.end(), points2->begin(),
                             points2->end());
      }

      if (points3 != NULL) {
        loopEntryPoints.insert(loopEntryPoints.end(), points3->begin(),
                               points3->end());
      }

      if (points4 != NULL) {
        loopExitPoints.insert(loopExitPoints.end(), points4->begin(),
                              points4->end());
      }

      // else
      // {
      //	printf("no points");
      // }
    }
  }

  fprintf(OUTCHANNEL,
          "Nums of loop-body-start:%lu, loop-body-end:%lu，loop-entry:%lu, "
          "loop-exit:%lu\n",
          loopStartPoints.size(), loopEndPoints.size(), loopEntryPoints.size(),
          loopExitPoints.size());
  display_for_sanity_check(loopStartPoints, "loopStartPoints");
  display_for_sanity_check(loopEndPoints, "loopEndPoints");
  display_for_sanity_check(loopEntryPoints, "loopEntryPoints");
  display_for_sanity_check(loopExitPoints, "loopExitPoints");

  insertSymbolSnippetForPoints(app, loopStartPoints, 0xffffffff);
  insertSymbolSnippetForPoints(app, loopEndPoints, 0xeeeeeeee);
  insertSymbolSnippetForPoints(app, loopEntryPoints, 0xdddddddd);
  insertSymbolSnippetForPoints(app, loopExitPoints, 0xcccccccc);
}

/*
  recusiveFunAddr：递归函数的首地址，用于之后寻找递归函数在外部的调用点
  recFuncInnerPoints：递归函数内部的点，用于对递归函数进行插装
*/
bool getRecFuncAddrAndInnerPoint(BPatch_addressSpace *app,
                                 vector<Address> &recusiveFunAddr,
                                 vector<BPatch_point *> &recFuncInnerPoints) {
  BPatch_image *appImage = app->getImage();

  vector<BPatch_function *> funcs;
  findFunctionEntries(appImage, &funcs);
  vector<BPatch_point *> recPoints;
  fprintf(OUTCHANNEL, "get all func,func.size = %lu\n", funcs.size());
  int allSum = 0;

  for (auto pf = funcs.begin(); pf != funcs.end(); pf++) {
    Address funcAddr = (Address)((*pf)->getBaseAddr());

    vector<BPatch_point *> *call_sites = (*pf)->findPoint(BPatch_subroutine);
    vector<Address> dCallAddresses;
    int count = 0;

    // first find all direct calls to other local functions.
    for (auto pp = call_sites->begin(); pp != call_sites->end(); pp++) {

      BPatch_point *tmp_site = *pp;

      if (!tmp_site->isDynamic()) {
        BPatch_function *tmp_callee = tmp_site->getCalledFunction();

        if (tmp_callee == NULL) {
          // bzip中是找不到<strcat@plt>
          //
          // printf("找不到调用函数，调用点的地址为0x%x\n", (Address
          // *)(*pp)->getAddress());
        } else {
          Address callFuncAddr = (Address)(tmp_callee->getBaseAddr());

          if (funcAddr == callFuncAddr) {
            fprintf(OUTCHANNEL, "Addr of recursively called function: 0x%lx;\n",
                    callFuncAddr);
            recPoints.push_back(*pp);
            count++;
            break;
          }
        }
      }
    }

    if (count != 0) {
      allSum++;

      // 如果是递归函数，则将该函数的首地址放在recusiveFunAddr
      recusiveFunAddr.push_back(funcAddr);

      // 将递归函数的起始点放到recFuncInnerPoints中
      vector<BPatch_point *> *funcEntryPoint = (*pf)->findPoint(BPatch_entry);
      recFuncInnerPoints.insert(recFuncInnerPoints.end(),
                                funcEntryPoint->begin(), funcEntryPoint->end());

      // 将递归函数的退出点放到recFuncInnerPoints中
      // vector<BPatch_point *> *funcExitPoint = (*pf)->findPoint(BPatch_exit);
      // vector<Address>	exitPointAddr;
      // for(auto pp = funcExitPoint->begin(); pp != funcExitPoint->end();
      // pp++){
      //	exitPointAddr.push_back((Address)(*pp)->getAddress());
      // }
      // 由于对于函数的ret插装获取ret语句源地址是在ret所在基本块开始获取的，要使得对于递归函数ret的标志
      // 在ret源地址之前，对于递归函数ret的插装也需要在ret所在基本块开始
      BPatch_flowGraph *cfg = (*pf)->getCFG();

      std::vector<BPatch_basicBlock *> exitBlock;
      cfg->getExitBasicBlock(exitBlock);

      for (auto eb = exitBlock.begin(); eb != exitBlock.end(); eb++) {
        BPatch_point *exitPoint = (*eb)->findEntryPoint();

        recFuncInnerPoints.push_back(exitPoint);
      }
    }
  }

  fprintf(OUTCHANNEL, "Num of recursively called functions: %d\n", allSum);
  return true;
}

//获取递归函数被其他函数调用的调用点
void getRecusiveOuterCalledPoint(BPatch_addressSpace *app,
                                 vector<Address> &recusiveFunAddr,
                                 vector<BPatch_point *> &calledBeforePoints,
                                 vector<BPatch_point *> &calledAfterpoints) {
  BPatch_image *appImage = app->getImage();

  vector<BPatch_function *> funcs;
  findFunctionEntries(appImage, &funcs);
  vector<BPatch_point *> recPoints;
  fprintf(OUTCHANNEL, "get all func, func.size = %lu\n", funcs.size());
  int allSum = 0;

  for (auto pf = funcs.begin(); pf != funcs.end(); pf++) {
    Address funcAddr = (Address)((*pf)->getBaseAddr());

    vector<BPatch_point *> *call_sites = (*pf)->findPoint(BPatch_subroutine);
    vector<Address> dCallAddresses;
    int count = 0;

    // first find all direct calls to other local functions.
    for (auto pp = call_sites->begin(); pp != call_sites->end(); pp++) {

      BPatch_point *tmp_site = *pp;

      if (!tmp_site->isDynamic()) {
        BPatch_function *tmp_callee = tmp_site->getCalledFunction();

        if (tmp_callee != NULL) {
          Address callSiteAddr = (Address)

                                     tmp_site->getAddress();
          Address callFuncAddr = (Address)(tmp_callee->getBaseAddr());

          for (auto pa = recusiveFunAddr.begin(); pa != recusiveFunAddr.end();
               pa++) {
            if (funcAddr != callFuncAddr && *pa == callFuncAddr) {
              //保存递归函数被外部函数调用的调用点
              calledBeforePoints.push_back(tmp_site);

              //获取递归函数被外部函数调用的下一条指令点,直接调用指令的长度为5
              Address callNextAddr = callSiteAddr + 5;

              vector<BPatch_point *> call_after_points;

              if (!appImage->findPoints(callNextAddr, call_after_points)) {
                fprintf(OUTCHANNEL,
                        "At 0x%lx: Fail to build call-after point of recursion "
                        "call site: 0x%lx.\n",
                        callFuncAddr, callNextAddr);
              } else {
                calledAfterpoints.insert(calledAfterpoints.end(),
                                         call_after_points.begin(),
                                         call_after_points.end());
              }
            }
          }
        }
      }
    }
  }
}

// Inserting syscall cause workload error.
bool createAndInsertForRecusiveOuterCalledbeforePoint(
    BPatch_addressSpace *app, vector<BPatch_point *> &outerCalledbeforePoints) {
  BPatch_image *appImage = app->getImage();

  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_constExpr(0xaaaaaaaa));

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(isArrayFull);
  BPatch_sequence allItems(items);

  if (outerCalledbeforePoints.size() != 0 &&
      !app->insertSnippet(allItems, outerCalledbeforePoints,
                          BPatch_firstSnippet)) {
    return false;
  }

  return true;
}

// Inserting syscall cause workload error.
bool createAndInsertForRecusiveOuterCalledAfterPoint(
    BPatch_addressSpace *app, vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // 0. 判断retstatus==true是否成立
  BPatch_variableExpr *retStatus = appImage->findVariable("retStatus");
  BPatch_boolExpr condition(BPatch_eq, *retStatus, BPatch_constExpr(true));

  // 1.输出当前的地址作为ret的目标地址，存储到数组中，
  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_originalAddressExpr());

  // currentPtr移动到下一个存储位置
  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  BPatch_snippet *recEndSigal = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_constExpr(0xaaaabbbb));

  // 2.将retstatus=false
  BPatch_snippet *retStatusRestore =
      new BPatch_arithExpr(BPatch_assign, *retStatus, BPatch_constExpr(false));

  // 3.判断数组是否满，满则输出到文件
  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(recEndSigal);
  items.push_back(currentPtrAddone);
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(retStatusRestore);
  items.push_back(isArrayFull);
  BPatch_sequence thenbody(items);

  BPatch_ifExpr getRetDest(condition, thenbody);

  if (points.size() != 0 &&
      !app->insertSnippet(getRetDest, points, BPatch_lastSnippet)) {
    fprintf(OUTCHANNEL, "error: storing current address failed!\n");
    return false;
  }

  return true;
}

// Inserting syscall cause workload error.
bool createAndInsertForRecusiveInnerPoint(BPatch_addressSpace *app,
                                          vector<BPatch_point *> &innerPoints) {
  BPatch_image *appImage = app->getImage();

  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *oriAddr = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_constExpr(0xaaaaffff));

  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  BPatch_snippet *isArrayFull = arrayFullSaveInFile(appImage);

  vector<BPatch_snippet *> items;
  items.push_back(oriAddr);
  items.push_back(currentPtrAddone);
  items.push_back(isArrayFull);
  BPatch_sequence allItems(items);

  if (innerPoints.size() != 0 &&
      !app->insertSnippet(allItems, innerPoints, BPatch_lastSnippet)) {
    return false;
  }

  return true;
}

bool createAndInsertEndSignAtProgramExitPoints(BPatch_addressSpace *app,
                                               vector<BPatch_point *> &points) {
  BPatch_image *appImage = app->getImage();

  // 1.输出当前的地址作为ret的目标地址，存储到数组中，
  // *currentPtr = BPatch_originalAddressExpr();
  BPatch_variableExpr *currentPtr = appImage->findVariable("currentPtr");
  BPatch_snippet *endSign = new BPatch_arithExpr(
      BPatch_assign, BPatch_arithExpr(BPatch_deref, *currentPtr),
      BPatch_constExpr(0xffffeeee));

  // currentPtr移动到下一个存储位置
  // currentPtr = currentPtr + 4;
  BPatch_snippet *currentPtrAddone = new BPatch_arithExpr(
      BPatch_assign, *currentPtr,
      BPatch_arithExpr(BPatch_plus, *currentPtr, BPatch_constExpr(4)));

  /*syscalls*/
  BPatch_snippet *syscall_s = syscallGen(appImage, "EndSignAtProgExitPoints-S");
  BPatch_snippet *syscall_t = syscallGen(appImage, "EndSignAtProgExitPoints-E");

  vector<BPatch_snippet *> items;
  items.push_back(syscall_s);
  items.push_back(endSign);
  items.push_back(currentPtrAddone);
  items.push_back(syscall_t);
  BPatch_sequence allItem(items);

  if (points.size() != 0 &&
      !app->insertSnippet(allItem, points, BPatch_lastSnippet)) {
    fprintf(OUTCHANNEL, "error: storing current address failed!\n");
    return false;
  }

  return true;
}

void getNotRecCallAfters(vector<BPatch_point *> &callAfters,
                         vector<BPatch_point *> &recFuncOuterCalledAfterPoints,
                         vector<BPatch_point *> &notRecCallAfters) {
  vector<Address> address;

  for (auto pp = recFuncOuterCalledAfterPoints.begin();
       pp != recFuncOuterCalledAfterPoints.end(); pp++) {
    address.push_back((Address)(*pp)->getAddress());
  }

  for (auto pp = callAfters.begin(); pp != callAfters.end(); pp++) {
    Address signalAddr = (Address)(*pp)->getAddress();

    if (find(address.begin(), address.end(), signalAddr) == address.end()) {
      notRecCallAfters.push_back(*pp);
    }
  }

  fprintf(OUTCHANNEL,
          "callAfters = %lu; recFuncOuterCalledAfterPoints = %lu; "
          "notRecCallAfters = %lu\n",
          callAfters.size(), recFuncOuterCalledAfterPoints.size(),
          notRecCallAfters.size());
}

int main(int argc, char **argv) {

  if (argc != 4) {
    fprintf(
        OUTCHANNEL,
        "Usage: mutator [mutatee_path] [mutatee_out_path] [noNeed_dcall_file]");
    return 0;
  }

  // Set up information about the program to be instrumented
  const char *mutatee_path = argv[1];

  memset(noNeedDcallsFileName, 0, 100);
  strcat(noNeedDcallsFileName, argv[3]);

  // open mutatee
  BPatch_addressSpace *app = bpatch.openBinary(mutatee_path, true);

  if (!app) {
    fprintf(OUTCHANNEL, "openBinary failed\n");
    exit(1);
  }

  BPatch_image *appImage = app->getImage();

  //为提高插装后文件的运行速度
  bpatch.setTrampRecursive(true);

  //对于使用到浮点型数据的程序的插装程序，需要注释掉bpatch.setSaveFPR(false)才能够正确运行
  // bpatch.setSaveFPR(false);
  vector<BPatch_point *> iJmps;
  vector<BPatch_point *> iCalls;
  vector<BPatch_point *> callAfters;
  vector<BPatch_point *> notRecCallAfters;
  vector<BPatch_point *> rets;
  vector<BPatch_point *> dcalls;
  vector<BPatch_point *> liuDcalls;
  vector<BPatch_point *> exitFuncPoints;

  //递归相关的变量声明
  vector<BPatch_point *> recFuncOuterCalledBeforePoints;
  vector<BPatch_point *> recFuncOuterCalledAfterPoints;
  vector<BPatch_point *> recFuncInnerPoints;

  vector<Address> recusiveFunAddr;

  //获取递归相关的点变量
  getRecFuncAddrAndInnerPoint(app, recusiveFunAddr, recFuncInnerPoints);
  getRecusiveOuterCalledPoint(app, recusiveFunAddr,
                              recFuncOuterCalledBeforePoints,
                              recFuncOuterCalledAfterPoints);

  //获取其他控制流事件点
  get_points(appImage, &iJmps, &iCalls, &callAfters, &rets, &dcalls,
             &exitFuncPoints, &liuDcalls);
  getNotRecCallAfters(callAfters, recFuncOuterCalledAfterPoints,
                      notRecCallAfters);

  // display_for_sanity_check(iJmps, "IJMP");
  // display_for_sanity_check(iCalls, "ICALL");
  // display_for_sanity_check(rets, "RET");
  // display_for_sanity_check(callAfters, "CALLAFTER");
  // display_for_sanity_check(dcalls, "DCALL");
  fprintf(OUTCHANNEL, "Num of iJmps: %lu\n", iJmps.size());
  fprintf(OUTCHANNEL, "Num of iCalls: %lu\n", iCalls.size());
  fprintf(OUTCHANNEL, "Num of rets: %lu\n", rets.size());
  fprintf(OUTCHANNEL, "Num of callAfterPoints: %lu\n", callAfters.size());
  fprintf(OUTCHANNEL, "Num of dCalls: %lu\n", dcalls.size());
  fprintf(OUTCHANNEL, "Num of skipped Dcalls: %lu\n", liuDcalls.size());
  fprintf(OUTCHANNEL, "Num of FuncExitPoints: %lu\n", exitFuncPoints.size());

  char addr_file_path[BUFLEN];

  strcpy(addr_file_path, argv[2]);
  strcat(addr_file_path, "-re");

  if (!createAndInsertFopen(app, addr_file_path)) {
    fprintf(OUTCHANNEL, "createAndInsertFopen failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "Fopen success\n");
  }

  if (!createAndInsertInitialization(app)) {
    fprintf(OUTCHANNEL, "createAndInsertInitialization failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "Initialization success\n");
  }

  // 处理循环
  getAndInsertForLoop(app, dcalls);

  // 处理递归
  if (!createAndInsertForRecusiveOuterCalledbeforePoint(
          app, recFuncOuterCalledBeforePoints)) {
    fprintf(OUTCHANNEL, "RecusiveOuterCalledbeforePoint failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "RecusiveOuterCalledbeforePoint success\n");
  }

  if (!createAndInsertForRecusiveOuterCalledAfterPoint(
          app, recFuncOuterCalledAfterPoints)) {
    fprintf(OUTCHANNEL, "RecusiveOuterCalledAfterPoint failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "RecusiveOuterCalledAfterPoint success\n");
  }

  if (!createAndInsertForRecusiveInnerPoint(app, recFuncInnerPoints)) {
    fprintf(OUTCHANNEL, "RecusiveInnerPoint failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "RecusiveInnerPoint success\n");
  }

  // 插装控制流事件点
  if (!createAndInsertDynamicEdges(app, iJmps)) {
    fprintf(OUTCHANNEL, "createAndInsertDynamicEdges for ijumps failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "ijumps success\n");
  }

  if (!createAndInsertDynamicEdges(app, iCalls)) {
    fprintf(OUTCHANNEL, "createAndInsertDynamicEdges for icalls failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "icalls success\n");
  }

  if (!createAndInsertForRet(app, rets)) {
    fprintf(OUTCHANNEL, "createAndInsert for returns failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "returns success\n");
  }

  if (!createAndInsertForCallAfter(app, notRecCallAfters)) {
    fprintf(OUTCHANNEL, "createAndInsert for notRecCallAfters failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "notRecCallAfters success\n");
  }

  if (!createAndInsertForDCall(app, liuDcalls)) {
    fprintf(OUTCHANNEL, "createAndInsert for liuDcalls failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "skipped Dcalls success\n");
  }

  //在结束一次执行的地方进行标注
  if (!createAndInsertEndSignAtProgramExitPoints(app, exitFuncPoints)) {
    fprintf(OUTCHANNEL, "createAndInsertEndSignAtProgramExitPoints failed\n");
    exit(1);
  }

  // // /*****************************************************************/
  // // //dump array should be ahead of fclose.
  if (!createAndInsertDumpAtProgramExitPoints(app, exitFuncPoints)) {
    fprintf(OUTCHANNEL, "createAndInsertDumpExitFunc failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "DumpExitFunc success\n");
  }

  if (!createAndInsertFcloseAtProgramExitPoints(app, exitFuncPoints)) {
    fprintf(OUTCHANNEL, "createAndInsertFclose failed\n");
    exit(1);
  } else {
    fprintf(OUTCHANNEL, "Fclose success\n");
  }

  // Finish instrumentation
  const char *mutatee_out_path = argv[2];
  BPatch_binaryEdit *appBin = dynamic_cast<BPatch_binaryEdit *>(app);

  if (appBin) {
    if (!appBin->writeFile(mutatee_out_path)) {
      fprintf(OUTCHANNEL, "write binary failed\n");
    }
  }
}
