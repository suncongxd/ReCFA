import  java.util.*;
import java.io.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CallRet {
    //存储用户函数调用
    public  HashSet<String> PersonlFun = new HashSet<>();
    //存储二进制中所有函数调用
    HashSet<String> CollectionCall = new HashSet<>();
    //存储调用点的跳转目标地址
    HashMap<String,String> CallTarget=new HashMap<>();
    public  void ReadFile(String FilePath) {
        Scanner input = new Scanner(System.in);
        //存储当前函数内的调用点
        HashMap<String, Set<String>> HeadCall = new HashMap<>();
        //存储调用地址和被调用函数之间的关系
        HashMap<String, String> CallNum = null;
        HashSet<String> Sto = null;
        try {
            File filename = new File(FilePath);
            InputStreamReader reader = new InputStreamReader(new FileInputStream(filename));
            BufferedReader br = new BufferedReader(reader);
            String in = br.readLine();
            String call = "([0-9a-f]+)(.*)(callq  )([0-9a-f]+)";
            String start = "0000000000([0-9a-f]+)";
            Pattern r, r1;
            Matcher m, m1;
            //正则表达式判断函数调用
            r = Pattern.compile(call);
            //正则表达式判断函数的开始
            r1 = Pattern.compile(start);
            String Head = null;
            int count = 0;
            while (in != null) {
                m = r1.matcher(in);
                if (m.find()) {
                    CallNum = new HashMap<>();
                    Sto = new HashSet<>();
                    //存储当前函数中存在的函数调用
                    HeadCall.put(m.group(1), Sto);
                    Head = m.group(1);
                    //System.out.println(m.group(1)+"45445454545454");
                    in = br.readLine();
                    continue;
                }
                m = r.matcher(in);
                //if (m.find() && PersonlFun.contains(m.group(4))) {
                if (m.find()) {
                    //存储函数调用点以及函数的跳转地址
                    CallTarget.put(m.group(1),m.group(4));
                    if (CallNum.keySet().contains(m.group(4))) {
                        CollectionCall.add(CallNum.get(m.group(4)));
                        CollectionCall.add(m.group(1));
                        //收集所有的调用点
                        Sto.add(m.group(1));
                    } else {
                        //存储调用地址和调用点的映射
                        CallNum.put(m.group(4), m.group(1));
                    }
                }
                in = br.readLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        //二进制分析中所有函数调用的个数
        System.out.println(CollectionCall.size());
    }

    public void ReadFileFirst(String FilePath) {
        try {
            File filename = new File(FilePath);
            InputStreamReader reader = new InputStreamReader(new FileInputStream(filename));
            BufferedReader br = new BufferedReader(reader);
            String in = br.readLine();
            String start = "0000000000([0-9a-f]+)";
            Pattern r;
            Matcher m;
            r = Pattern.compile(start);
            while (in != null) {
                m = r.matcher(in);
                if (m.find()) {
                    //System.out.println(m.group(1));
                    //收集用户函数首地址
                    PersonlFun.add(m.group(1));
                }
                in = br.readLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
