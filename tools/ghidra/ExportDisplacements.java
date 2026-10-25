// List functions whose instructions use every requested memory displacement
// (struct field offsets), for locating code that reads a known layout.
// Args: output file, then hexadecimal displacements. Evidence only.
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

public class ExportDisplacements extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("Output file and hexadecimal displacements required");
        Set<Long> wanted = new HashSet<>();
        for (int i = 1; i < args.length; i++) wanted.add(Long.parseLong(args[i].replaceFirst("^0[xX]", ""), 16));
        Map<Function, Set<Long>> seen = new HashMap<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            for (int op = 0; op < ins.getNumOperands(); op++) {
                for (Object o : ins.getOpObjects(op)) {
                    if (!(o instanceof Scalar)) continue;
                    long value = ((Scalar) o).getUnsignedValue();
                    if (!wanted.contains(value)) continue;
                    Function f = getFunctionContaining(ins.getAddress());
                    if (f != null) seen.computeIfAbsent(f, k -> new HashSet<>()).add(value);
                }
            }
        }
        try (PrintWriter out = new PrintWriter(new File(args[0]), StandardCharsets.UTF_8)) {
            out.println("SHA256 " + currentProgram.getExecutableSHA256());
            for (Map.Entry<Function, Set<Long>> e : seen.entrySet())
                if (e.getValue().containsAll(wanted)) out.println(e.getKey().getEntryPoint() + " " + e.getKey().getName());
            out.println("Done");
        }
    }
}
