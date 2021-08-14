#ifndef LIB1_H_INCLUDED
#define LIB1_H_INCLUDED

#ifdef __cplusplus
   extern "C" {
#endif

// for folding
void folding_action(char *argv[], int name_idx, bool dealWithRecursion);

// for greedy
void arraycopy(int *a, int astart, int *b, int bstart, int len);
int naive_folding(int *input, int *result, int bound, int inputsize);



#ifdef __cplusplus
   }
#endif

#endif /* LIB1_H_INCLUDED */

class MyStack
{
    public:
        int* stack;
        int length;
        // 为了随时获取到栈顶相对与栈底的偏移，因此top变量为public
        int stacktop;
        
        MyStack();
        ~MyStack();
        void push(int data);
        int pop();
        int size();
        int top();
        bool empty();
        void changePos(int pos);
        void expand();
};




