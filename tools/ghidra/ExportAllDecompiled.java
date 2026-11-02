// Headless analysis evidence, not compilable recovered game source.
// Decompiles every function into one text file (used for the PS2 SLUS-20073
// executable, where no prior function map exists).
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.*;
import java.nio.charset.StandardCharsets;

public class ExportAllDecompiled extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output file");
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int count = 0;
        try (PrintWriter out = new PrintWriter(new File(args[0]), StandardCharsets.UTF_8)) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
                out.printf("/* ==== %s %s ==== */%n", f.getEntryPoint(), f.getName());
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "/* decompile failed: " + r.getErrorMessage() + " */");
                ++count;
            }
            out.printf("/* Decompiled functions: %d */%n", count);
        }
        decompiler.dispose();
    }
}
