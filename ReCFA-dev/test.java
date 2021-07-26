import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class test {
    public void verify(CFG cfg,CFGconstruct cfgconstructcon)
    {
        for(Iterator it=cfgconstructcon.P.iterator();it.hasNext();)
        {
            String s=(String)it.next();
            HashMap<String,Set<String>> b=cfgconstructcon.Reduction.get(s);
            for(Iterator it1=b.keySet().iterator();it1.hasNext();)
            {
                String a=(String)it1.next();
                if(b.get(a).equals(cfg.BlockendJump.get(a).Jump)) {
                   System.out.println(a+"true");
                    continue;
                }
                else {
                    System.out.println(cfg.BlockendJump.get(a).BlockStar);
                    continue;
                }

            }
        }
    }
}
