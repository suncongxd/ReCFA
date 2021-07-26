import java.util.*;
import java.io.*;
import java.util.regex.*;

public class CFG {
    //key值为基本块的首部地址，value为基本块
    HashMap<String,Block> BlockStarJump=new HashMap<>();
    //key值为基本块的末尾地址，value为基本块
    HashMap<String,Block> BlockendJump=new HashMap<>();
    //key值函数首部地址，value为函数返回地址的集合，因为一个函数可能存在多个返回地址
    HashMap<String,Set<String>> Function=new HashMap<>();
    //key值为CFG中的间接调用点，Value为typearmor分析结果的跳转地址
    HashMap<String,Set<String>> FunctionEndJump=new HashMap<>();
    public void ReadDyninst(String FilePath)
    {
        try {
            File filename = new File(FilePath);
            InputStreamReader reader=new InputStreamReader(new FileInputStream(filename));
            BufferedReader br = new BufferedReader(reader);
            String in = br.readLine();
            String blcok="\\[([0-9a-f]+),([0-9a-f]+)";
            String jump="([0-9a-f]+)\" -> \"([0-9a-f]+)";
            String FunctionStar="\"([0-9a-f]+)\" \\[label = \"";
            //String FunctionRet="\"([0-9a-f]+)(.*)(\\[color=green\\])";
            String FunctionRet="\"([0-9a-f]+)(.*)(\\[color=green\\])";
            String FunctionName="";
            while(in!=null)
            {
                //正则表达式分析每个基本块
                Pattern r=Pattern.compile(blcok);
                Matcher m=r.matcher(in);
                if(m.find())
                {
                    Block insert=new Block();
                    insert.BlockStar=m.group(1);
                    insert.BlockEnd=m.group(2);
                    insert.Jump=new TreeSet();
                    //key值为基本块的首部地址
                    BlockStarJump.put(m.group(1),insert);
                    //key值为基本块的末尾地址
                    BlockendJump.put(m.group(2),insert);
                }
                //正则表达式分析基本块之间的跳转关系
                r=Pattern.compile(jump);
                m=r.matcher(in);
                if(m.find())
                {
                   Block a=BlockStarJump.get(m.group(1));
                   //根据首部地址获得基本块并添加入跳转关系
                   a.Jump.add(m.group(2));
                }
                //正则表达式判断函数的开始地址
                r=Pattern.compile(FunctionStar);
                m=r.matcher(in);
                if(m.find())
                {
                    //存储每个函数的返回地址
                    Set<String> RetSet=new TreeSet<>();
                    Function.put(m.group(1),RetSet);
                    FunctionName=m.group(1);
                }
                //正则表达判断函数的返回地址
                r=Pattern.compile(FunctionRet);
                m=r.matcher(in);
                if(m.find())
                {
                    //获得函数的返回地址集合
                    Set<String> a=Function.get(FunctionName);
                    //函数返回地址集合添加返回地址
                    a.add(m.group(1));
                }
                in=br.readLine();
            }

        }
        catch(IOException e)
        {
            e.printStackTrace();
        }
        return;

    }
    //读入typearmor的控制流图
    public void ReadTypearmor(String FilePath)
    {
        try {
            File filename = new File(FilePath);
            InputStreamReader reader = new InputStreamReader(new FileInputStream(filename));
            BufferedReader br = new BufferedReader(reader);
            String in = br.readLine();
            String IndirectRel="Indirectstar0x([0-9a-f]+)(.*)Indrectend0x([0-9a-f]+)";
            while(in!=null) {
                //正则表达式判断函数的跳转地址
                Pattern r = Pattern.compile(IndirectRel);
                Matcher m = r.matcher(in);
                String Flag="";
                if (m.find()) {
                    Flag=m.group(1);
                    TreeSet<String> next = (BlockendJump.get(Flag)).Jump;
                    //Dyninst对间接调用的处理结果为ffffffff以及该点的下一地址，所以为2
                    if (next.size() != 2)
                        throw new SetException("大小不符合");
                    next.remove("ffffffffffffffff");
                    String NextBlock="";
                    for (Iterator it = next.iterator(); it.hasNext(); )
                    {
                        NextBlock=(String)it.next();
                    }
                    //将Dyninst加入的跳转地址移除
                    next.remove(NextBlock);
                    //加入Typearmor生成的地址
                    next.add(m.group(3));
                    FunctionEndProcess(m.group(3));
                    if (!Function.containsKey(m.group(3)))
                    {
                        in=br.readLine();
                        continue;
                    }
                    Set<String> FunctionEnd=Function.get(m.group(3));
                    //对Typearmor分析出的调用函数的返回基本块进行处理，将调用点下一基本块添加到返回地址集合元素的跳转目标集合内
                    for(Iterator it=FunctionEnd.iterator();it.hasNext();)
                    {
                        String FunctionEndName=(String)it.next();
                        //若存在该返回地址，则调用该基本块直接进行添加，不存在，则添加一个key，value值进行添加。
                        if(getFunctionEndJump().containsKey(FunctionEndName))
                        {
                            getFunctionEndJump().get(FunctionEndName).add(NextBlock);
                        }
                        else
                        {
                            Set<String> insert=new TreeSet<String>();
                            insert.add(NextBlock);
                            getFunctionEndJump().put(FunctionEndName,insert);
                        }
                    }
                    in=br.readLine();
                    while(in!=null)
                    {
                          m=r.matcher(in);
                          if(m.find())
                          {
                              //若正则表达式分析结果仍然为Flag，则继续分析，否则，结束当前虚幻
                              if(!Flag.equals(m.group(1)))
                              {
                                  break;
                              }
                              next.add(m.group(3));
                              //对该加入的地址进行深度优先遍历
                              FunctionEndProcess(m.group(3));
                              //获取该函数首地址对应函数的末尾返回
                              FunctionEnd=Function.get(m.group(3));
                              //如果跳转为Dyninst不存在的函数，则忽略不进行处理
                              if (!Function.containsKey(m.group(3)))
                              {
                                  in=br.readLine();
                                  continue;
                              }
                              //对Typearmor分析出的调用函数的返回基本块进行处理，将调用点下一基本块添加到返回地址集合元素的跳转目标集合内
                              for(Iterator it=FunctionEnd.iterator();it.hasNext();)
                              {
                                  String FunctionEndName=(String)it.next();
                                  //若存在该返回地址，则调用该基本块直接进行添加，不存在，则添加一个key，value值进行添加。
                                  if(getFunctionEndJump().containsKey(FunctionEndName))
                                  {
                                      getFunctionEndJump().get(FunctionEndName).add(NextBlock);
                                  }
                                  else
                                  {
                                      Set<String> insert=new TreeSet<String>();
                                      insert.add(NextBlock);
                                      getFunctionEndJump().put(FunctionEndName,insert);
                                  }
                              }
                          }
                          in=br.readLine();
                    }
                    continue;
                }
                in=br.readLine();
            }
            //将所有的函数末尾地址映射跳转添加到函数首部CFG当中
            for(Iterator it = getFunctionEndJump().keySet().iterator(); it.hasNext();)
            {
                String s=(String)it.next();
                BlockStarJump.get(s).Jump.addAll(getFunctionEndJump().get(s));
            }
        }
        catch(IOException e)
        {
            e.printStackTrace();
        }
        catch (SetException e)
        {
            return;
        }
        return;
    }
    public void FunctionEndProcess(String Star) {
        //若函数Map不存在Star的Key值，则忽略
        if (!Function.containsKey(Star))
        {
            //System.out.println(Star+"dsfdfsfsf");
            return;
        }
        //若Function获取的集合大于0，表示已经处理过，则忽略，否则进行深度优先遍历
        if(Function.get(Star).size()>0)
            return;
        else {
            Set<String> flag=new HashSet<>();
            dps(Star, Star,flag);
        }
    }
    public void dps(String s,String Star,Set<String> flag)
    {
        //若flag存在则表示已经处理过，忽略不计
        if(flag.contains(s))
            return;
        //若s等于ffffffffffffffff，即表示遍历到函数的结束基本块上
        if(s.equals("ffffffffffffffff"))
            return;
        //标记该基本块
        flag.add(s);
        //获取当前基本块的跳转基本块
        Block a=BlockStarJump.get(s);
        //若跳转集合不存在，或者大小为0，则表示为末尾基本块进行添加
        if(a.Jump==null||a.Jump.size()==0) {
            Function.get(Star).add(a.BlockEnd);
            return;
        }
        //对跳转目标集合元素进行分析
        Iterator it=a.Jump.iterator();
        while(it.hasNext())
        {
                String First=(String)(it.next());
                //若跳转集合不存在，或者大小为0，则表示为末尾基本块进行添加
                if(Function.containsKey(First)&&Function.get(First).size()>0) {
                    Function.get(Star).addAll(Function.get(First));
                    continue;
                }
                //System.out.println(First);
                dps(First,Star,flag);
        }


    }

    public HashMap<String, Set<String>> getFunctionEndJump() {
        return FunctionEndJump;
    }

    public void setFunctionEndJump(HashMap<String, Set<String>> functionEndJump) {
        FunctionEndJump = functionEndJump;
    }
}
