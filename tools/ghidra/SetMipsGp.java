// Pre-analysis: assume a constant $gp across the program (PS2 SLUS-20073
// .reginfo gp value) so gp-relative globals resolve to addresses.
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import java.math.BigInteger;

public class SetMipsGp extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long gp = Long.decode(args.length > 0 ? args[0] : "0x2b36f0");
        Register reg = currentProgram.getRegister("gp");
        for (ghidra.program.model.mem.MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isExecute() || !block.getStart().isMemoryAddress()) continue;
            currentProgram.getProgramContext().setValue(reg, block.getStart(), block.getEnd(), BigInteger.valueOf(gp));
        }
        println("gp set to 0x" + Long.toHexString(gp));
    }
}
