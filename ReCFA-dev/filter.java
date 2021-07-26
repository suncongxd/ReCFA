import java.util.*;
public class filter {
    public static void main(String[] sb)
    {
     CFG cfg=new CFG();
     Scanner input=new Scanner(System.in);
     //输入待分析二进制的Dyninst处理结果
     System.out.println("输入dyninst文件");
     String FilePath=input.nextLine();
     //对Dyninst结果进行分析
     cfg.ReadDyninst(FilePath);
     //对Dyninst分析出的控制流图进行简化，以便后续调用点过滤处理
     CFGconstruct cfgconstruct=new CFGconstruct();
     //输入待分析二进制的汇编文件
     System.out.println("输入汇编文件");
     FilePath=input.nextLine();
     //对反汇编文件中的所有直接调用进行处理
     cfgconstruct.ReadAsm(FilePath,cfg);
    }
}
