
public class Filter {
	public static void main(String[] args) {
		if (args.length != 3) {
			System.out.println("Usage: csfilter.jar [dot-file-path] [asm-file-path] [filterred call-sites output path]");
			return;
		}
		CFG cfg = new CFG();

		// 对Dyninst结果进行分析
		cfg.ReadDyninst(args[0]);
		// 对Dyninst分析出的控制流图进行简化，以便后续调用点过滤处理
		CFGconstruct cfgconstruct = new CFGconstruct();
		// 对反汇编文件中的所有直接调用进行处理
		cfgconstruct.ReadAsm(args[1], cfg, args[2]);

	}
}
