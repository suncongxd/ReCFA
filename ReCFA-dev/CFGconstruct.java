import org.omg.Messaging.SYNC_WITH_TRANSPORT;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.util.*;
import java.io.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CFGconstruct {
    //程序调用点集合
    TreeSet<String> Call=new TreeSet<>();
    //程序间接跳转点集合
    TreeSet<String> Jump=new TreeSet<>();
    //程序返回点集合
    TreeSet<String> Ret=new TreeSet<>();
    //将需要设置为null的跳转关系进行备份
    HashMap<String,Set<String>> Backup=new HashMap<>();
    //收集所有的待处理点到P，其中包含函数开头调用，间接跳转的目标地址
    HashSet<String> P=new HashSet<String>();
    //收集所有的有关库函数调用
    HashSet<String> PltSet=new HashSet<>();
    //存储化简后的控制流图
    ConcurrentHashMap<String,HashMap<String,Set<String>>> Reduction=new ConcurrentHashMap<>();
    public void ReadAsm(String AsmFile,CFG cfg) {
        String setret="([0-9a-f]{6}) \\<(.*)longjmp\\@plt\\>";
        String setcall="([0-9a-f]{6}) \\<(.*)setjmp\\@plt\\>";
        String address="([0-9a-f]{6}):";
        String setjmp=null;
        String longjmp=null;
        //收集setjmpSet集合
        Set<String> setjmpSet=new HashSet();
        //收集间接调用集合
        Set<String> IndirectCallJump=new HashSet<>();
        try {
            File filename = new File(AsmFile);
            InputStreamReader reader = new InputStreamReader(new FileInputStream(filename));
            BufferedReader br = new BufferedReader(reader);
            String in = br.readLine();
            String call = "([0-9a-f]+)(.*)(callq  \\*)";
            String jump = "([0-9a-f]+)(.*)(jmpq   \\*\\%)";
            String main = "([0-9a-f]{6}) \\<main\\>";
            String directCall="([0-9a-f]+)(.*)(callq)";
            String ret="([0-9a-f]{6})(.*)(retq)";
            String plt="([0-9a-f]{6}) \\<(.*)\\>:";
            while(in!=null)
            {
                //收集所有的plt函数
                Pattern r1=Pattern.compile(plt);
                Matcher m1=r1.matcher(in);
                if(m1.find())
                {
                    PltSet.add(m1.group(1));
                    r1=Pattern.compile(setret);
                    m1=r1.matcher(in);
                    if(m1.find())
                    {
                        longjmp=m1.group(1);
                    }
                    r1=Pattern.compile(setcall);
                    m1=r1.matcher(in);
                    if(m1.find())
                        setjmp=m1.group(1);
                }
                //若读取的行数为Disassembly of section .text:，表示plt函数已经结束
                if(in.equals("Disassembly of section .text:"))
                    break;
                in=br.readLine();
            }
            Pattern r;
            Matcher m;
            while (in != null) {
                    //正则表达识别直接调用
                    Pattern  r1 = Pattern.compile(directCall);
                    Matcher  m1 = r1.matcher(in);
                    if (m1.find()) {
                        //正则表达识别所有调用
                        r1=Pattern.compile(call);
                        Matcher m2=r1.matcher(in);
                        if(!m2.find()) {
                            if (cfg.BlockendJump.keySet().contains(m1.group(1))) {
                                Set<String> a=new HashSet<>(PltSet);
                                //间接调用目标去除plt调用
                                a.retainAll(cfg.BlockendJump.get(m1.group(1)).Jump);
                                //对longjmp所在基本块设置为null
                                if(longjmp!=null&&cfg.BlockendJump.get(m1.group(1)).Jump.contains(longjmp))
                                    cfg.BlockendJump.get(m1.group(1)).Jump=null;
                                else {
                                    if (setjmp!=null&&cfg.BlockendJump.get(m1.group(1)).Jump.contains(setjmp)) {
                                        //对setjmp进行处理，将存在setjmp的基本块添加到SetjmpSet集合
                                        in = br.readLine();
                                        r1 = Pattern.compile(address);
                                        m2 = r1.matcher(in);
                                        if (m2.find())
                                            setjmpSet.add(m2.group(1));
                                        else {
                                            System.out.println("fail");
                                        }
                                        continue;
                                    }
                                    if (a.size() == 0&&judgeplt(in))  //判断隐形plt文件
                                        Call.add(cfg.BlockendJump.get(m1.group(1)).BlockStar);

                                }
                            }
                            //System.out.println(m1.group(1) + "gfgfgdfgdf");
                        }
                        else
                        {
                            //cfg不存在该间接调用的基本块
                            if(!cfg.BlockendJump.containsKey(m2.group(1)))
                                System.out.println(m2.group(1)+"dyninst fail to solve");
                            else
                            {
                                //集合P添加间接调用目标地址
                                P.addAll(cfg.BlockendJump.get(m2.group(1)).Jump);
                                //将间接调用的跳转目标设置为null进行标记
                                cfg.BlockendJump.get(m2.group(1)).Jump = null;
                                //将间接调用集合添加到IndirectCallJump中
                                IndirectCallJump.add(m2.group(1));
                            }
                        }
                        in = br.readLine();
                        continue;
                    }
                    //正则表达式识别main函数地址
                r = Pattern.compile(main);
                m = r.matcher(in);
                if (m.find()) {
                    //待处理集合添加main函数对应的地址
                    P.add(m.group(1));
                    in = br.readLine();
                    continue;
                }
                //正则表达式收集ret地址
                r=Pattern.compile(ret);
                m=r.matcher(in);
                if(m.find())
                {
                    if(cfg.BlockendJump.keySet().contains(m.group(1))) {
                        //Ret添加ret地址的下一位地址
                        Ret.add(cfg.BlockendJump.get(m.group(1)).BlockStar);
                        //Ret添加跳转地址的首部地址
                        P.add(cfg.BlockendJump.get(m.group(1)).BlockStar);
                    }
                    //System.out.println(m.group(1)+"retretret");
                }
                //正则表达式判断间接跳转
                r=Pattern.compile(jump);
                m=r.matcher(in);
                if(m.find())
                {
                    //集合P收集间接跳转的目标地址
                    P.addAll(cfg.BlockendJump.get(m.group(1)).Jump);
                    //将间接跳转的目标集合设置为空进行标记
                    cfg.BlockendJump.get(m.group(1)).Jump=null;
                    //IndirectCallJump集合添加间接跳转的首部地址
                    IndirectCallJump.add(m.group(1));
                }
                in=br.readLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        //对所有的间接调用设置空进行标记，而后添加到集合P中
        for (Iterator it = Call.iterator(); it.hasNext(); ) {
            String s = (String) it.next();
            Backup.put(s, cfg.BlockStarJump.get(s).Jump);
            P.addAll(cfg.BlockStarJump.get(s).Jump);
            cfg.BlockStarJump.get(s).Jump = null;
        }
        //对所有的Ret返回设置空进行标记，而后添加到集合P中
        for(Iterator it=Ret.iterator();it.hasNext();)
        {
            String s=(String)it.next();
            Backup.put(s, cfg.BlockStarJump.get(s).Jump);
            P.addAll(cfg.BlockStarJump.get(s).Jump);
            cfg.BlockStarJump.get(s).Jump = null;
        }
        //输出所有存在setjmpSet集合
        System.out.println(setjmpSet);
        //添加所有的函数首部地址
        P.addAll(cfg.Function.keySet());
        //集合P同时需要添加SetjmpSet集合
        P.addAll(setjmpSet);
        //打印出简化后的控制流图首部地址集合大小
        System.out.println(P.size());
        //打印出直接调用集合的大小
        System.out.println(Call.size()+"直接调用大小");
        //System.out.println(Call);
        //System.exit(0);
        //对原始控制流图进行简化
        construct(cfg);
        CallRet callRet=new CallRet();
        //再次读入反汇编文件，获取反汇编文件之间的调用返回对
        callRet.ReadFile(AsmFile);
        Set<String> out=new HashSet<>();
        Set<String> in=new HashSet<>();
        //Map<String,Set<String>> re=Reduction.get("59c252");
        //for(Iterator it=re.keySet().iterator();it.hasNext();)
            //System.out.println(it.next()+"weiweiwei");
        //对集合P中的每个元素进行输出
        for (Iterator it = P.iterator(); it.hasNext(); )
        {
            String a=(String)it.next();
            //集合b为当前a对应的简化控制流图的简化末尾地址
            Set<String> b=Reduction.get(a).keySet();
            //b.retainAll(new HashSet<>(callRet.CollectionCall));
            /*if(b.size()>=2)
            {
                Set<String> Insert=new HashSet<>();
                Map<String,String> before=new HashMap<>();  //key是call跳转地址，value
                for(Iterator it1=b.iterator();it1.hasNext();)
                {
                    String callAdress=(String) it1.next();  //调用地址
                    if(before.keySet().contains(callRet.CallTarget.get(callAdress)))
                    {
                       Insert.add(callAdress);
                       //System.out.println(callAdress+" "+callRet.CallTarget.get(callAdress)+" "+callRet.CallTarget.get(before.get(callRet.CallTarget.get(callAdress))));
                       Insert.add(before.get(callRet.CallTarget.get(callAdress)));
                       //System.out.println(before.get(callRet.CallTarget.get(callAdress)));
                    }
                    else
                        before.put(callRet.CallTarget.get(callAdress),callAdress);
                }
            }*/
            //b.retainAll(callRet.CallTarget.keySet());
            //若集合b的大小小于1，说明由a既能确定b的元素，否则说明不行添加到in中
            if(b.size()<=1)
            {
                //由a确定b中的元素，该元素必须是调用点所以进行求交集操作
                b.retainAll(callRet.CallTarget.keySet());
                out.addAll(b);
            }
            else
                in.addAll(b);
        }
        //for(Iterator it=out.iterator();it.hasNext();)
            //System.out.println(it.next());
        //out集合移除所有的in元素
        out.removeAll(in);
        //out集合移除所有的间接调用元素，因为间接调用不能被省略
        out.removeAll(IndirectCallJump);
        /*for(Iterator it=setjmpSet.iterator();it.hasNext();)
        {
            String a=(String)it.next();
            Set<String> b=Reduction.get(a).keySet();
            out.removeAll(b);
        }*/
        System.out.println(out.size());
        //此处输出过滤点的前驱与后继节点之间的映射关系
        /*for(Iterator it=P.iterator();it.hasNext();)
        {
            String a=(String)it.next();
            Set<String> b=Reduction.get(a).keySet();
            Set<String> c=new HashSet<>(b);
            c.retainAll(out);
            if(c.size()>0)
            {
                System.out.println(a+" "+b);
            }
        }*/
        //输出所有能被过滤的直接调用点
        for(Iterator it=out.iterator();it.hasNext();)
        {
            System.out.println(it.next());
        }
      return;
    }
    //对原始控制流进行重构
    public void construct(CFG cfg)
    {
        /*long startTime=System.currentTimeMillis();
        for(Iterator it=P.iterator();it.hasNext();) {
            String p = (String) it.next();
            process(p, cfg);
        }*/
        //设计线程池，最多线程数为5
        ExecutorService service = Executors.newFixedThreadPool(5);
        Iterator it=P.iterator();
        String lock="";
        CountDownLatch count=new CountDownLatch(P.size());
        long startTime=System.currentTimeMillis();
        for(;it.hasNext();)
        {
            String p=(String)it.next();
            service.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        //多线程同时对控制流进行简化
                        process(p,cfg);
                        count.countDown();
                    }
                }
        );
        }
        try {
            count.await();
        }
        catch (InterruptedException e)
        {
            System.out.println("闭锁处失败");
            System.exit(-1);
        }
        service.shutdown();
        long endTime=System.currentTimeMillis();
        System.out.println(i+"ddddddddddddddddddddddddddddddddddd");
        //输出程序运行时间以及集合P的大小
        System.out.println("程序运行时间： "+(endTime-startTime)+"ms"+P.size()+" "+j);
    }
    public void process(String pHead,CFG cfg) {
        //对每个处理的基本块都添加到flag当中
        HashSet<String> flag = new HashSet<>();
        //收集pHead对应的末尾集合
        HashSet<String> TailSet = new HashSet<>();
        //若化简控制流图不包含pHead，则表明没有处理过，进行处理
            if (!Reduction.keySet().contains(pHead)) {
                //对pHead对应的子控制流图进行深度优先搜索
                dps(pHead, cfg, flag, TailSet);
                //存储每个末尾地址，以及末尾地址对应的跳转集合
                HashMap<String, Set<String>> Insert = new HashMap<>();
                for (Iterator it = TailSet.iterator(); it.hasNext(); ) {
                    String s = (String) it.next();
                    //Insert添加控制流图的末尾地址，以及末尾地址对应的跳转集合
                    Insert.put(cfg.BlockStarJump.get(s).BlockEnd, Backup.get(s));
                }
                //synchronized (this) {
                //将重构首地址以及跳转集合添加到重构控制流映射当中
                Reduction.put(pHead, Insert);
                //}

            } else
                return;
    }
    volatile int i=0,j=0;
   //Set<String> sb=new HashSet<>();
    //深度优先遍历搜索
    public void dps(String pHead,CFG cfg,HashSet<String> flag,HashSet<String> TailSet)
    {
          //设计一个队列用来存储已经搜索过的基本块
          java.util.Queue<String> sto=new java.util.LinkedList<String>();
          //对pHead对应字符串进行处理
          sto.offer(pHead);
          //flag.add(pHead);
          //采用广度优先搜索的方式对控制流子图进行遍历
          while(!sto.isEmpty())
          {
              String str=sto.poll();
              //System.out.println(str+"  "+i++);
              //若队列存在，则不重复进行添加
              if(flag.contains(str))
                  continue;
              synchronized (this) {
                  i++;
              }
              //若str值为ffffffffffffffff，则进行标记，但不进行下一步搜索
              if(str.equals("ffffffffffffffff")) {
                  flag.add(str);
                  continue;
              }
              //若遍历到的基本块不存在于cfg中，同样进行标记，不进行下一步处理
              if(!cfg.BlockStarJump.containsKey(str)) {
                  flag.add(str);
                  continue;
              }
              //获取str对应基本块的跳转
              TreeSet<String> a=cfg.BlockStarJump.get(str).Jump;
              //若str对应基本块的跳转为null或者大小为0，TailSet添加str
              if(a==null||a.size()==0)
             {
               TailSet.add(str);
               flag.add(str);
               continue;
             }
              //对str同样进行标识
              flag.add(str);
              //将跳转集合的每个不重复元素添加到sto队列当中
              for(Iterator it=a.iterator();it.hasNext();)
             {
               String test=(String)it.next();
               if(flag.contains(test))
                   continue;
               else {
                   if(a.size()>1999998)
                       System.out.println(Thread.currentThread().getName()+" "+j);
                   sto.offer(test);
                   //flag.add (test);
                   //System.out.println(test+"  "+(i++)+" "+Thread.currentThread().getName());
               }
             }
          }
//        if(flag.contains(pHead))
//            return;
//        flag.add(pHead);
//        if(pHead.equals("ffffffffffffffff"))
//            return;
//        System.out.println(pHead+" "+i++);
//        if(!cfg.BlockStarJump.containsKey(pHead))
//            return;
//        TreeSet<String> a=cfg.BlockStarJump.get(pHead).Jump;
//        if(a==null||a.size()==0)
//        {
//            TailSet.add(pHead);
//            return;
//        }
//        for(Iterator it=a.iterator();it.hasNext();)
//        {
//          dps((String)it.next(),cfg,flag,TailSet);
//        }
    }
    //判断调用是否为plt调用
    boolean judgeplt(String in)
        {

            String pltcall = "([0-9a-f]+)(.*)(callq  .*\\@)";
            //正则表达式判断是否为plt调用
            Pattern r=Pattern.compile(pltcall);
            Matcher m=r.matcher(in);
            if(!m.find())
            return true;//判断跳转不为plt时
            else
                return false;
        }
}
