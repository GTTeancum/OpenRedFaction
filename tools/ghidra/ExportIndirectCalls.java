// Export decoded indirect calls with local context; no byte-pattern alignment guesses.
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;

public class ExportIndirectCalls extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output file");
        Listing listing = currentProgram.getListing();
        int count = 0;
        try (PrintWriter out = new PrintWriter(new File(args[0]), StandardCharsets.UTF_8)) {
            out.println("Program SHA256: " + currentProgram.getExecutableSHA256());
            out.println("Decoded instruction inventory only; undecoded code and runtime targets remain unproven.");
            InstructionIterator instructions = listing.getInstructions(true);
            while (instructions.hasNext()) {
                monitor.checkCancelled();
                Instruction instruction = instructions.next();
                if (!instruction.getFlowType().isCall() || !instruction.getFlowType().isComputed()) continue;
                Function function = getFunctionContaining(instruction.getAddress());
                out.println("CALL " + instruction.getAddress() + " function=" +
                    (function == null ? "unknown" : function.getEntryPoint()));
                ArrayList<Instruction> context = new ArrayList<>();
                Instruction previous = instruction;
                for (int n = 0; n < 9 && previous != null; ++n) {
                    context.add(previous);
                    Instruction before = listing.getInstructionBefore(previous.getAddress());
                    if (before == null || !before.getMaxAddress().next().equals(previous.getAddress())) break;
                    previous = before;
                }
                Collections.reverse(context);
                for (Instruction line : context) out.println("  " + line.getAddress() + " " + line);
                ++count;
            }
            out.println("Decoded indirect calls: " + count);
        }
        println("Exported " + count + " decoded indirect calls");
    }
}
