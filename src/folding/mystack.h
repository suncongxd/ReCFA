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
