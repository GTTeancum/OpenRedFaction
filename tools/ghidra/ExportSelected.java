// Export explicitly requested functions for bounded follow-up work.
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public class ExportSelected extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("Expected output directory and hexadecimal addresses");
        File dir = new File(args[0]);
        dir.mkdirs();
        DecompInterface decomp = new DecompInterface();
        ArrayList<String> exported = new ArrayList<>();
        try {
            decomp.openProgram(currentProgram);
            for (int i = 1; i < args.length; ++i) {
                long address = Long.parseUnsignedLong(args[i].replaceFirst("^0[xX]", ""), 16);
                Function function = getFunctionAt(toAddr(address));
                if (function == null) function = getFunctionContaining(toAddr(address));
                boolean created = false;
                if (function == null) {
                    // Virtual table targets can be absent from automatic analysis.
                    // The caller supplies the entry point; never scan or guess one.
                    disassemble(toAddr(address));
                    function = createFunction(toAddr(address), "FUN_" + Long.toHexString(address));
                    created = function != null;
                }
                if (function == null) throw new IllegalArgumentException("No function at " + args[i]);
                DecompileResults result = decomp.decompileFunction(function, 90, monitor);
                if (!result.decompileCompleted()) throw new IOException(result.getErrorMessage());
                try (PrintWriter out = new PrintWriter(new File(dir, Long.toHexString(address) + ".c.txt"), StandardCharsets.UTF_8)) {
                    out.println("/* Raw Ghidra output; candidate semantics require verification. */");
                    out.println("/* Program SHA256: " + currentProgram.getExecutableSHA256() + " */");
                    out.println("/* Containing function entry: " + function.getEntryPoint() + " */");
                    out.println("/* Created at explicit requested entry: " + created + " */");
                    out.print(result.getDecompiledFunction().getC());
                }
                println("Exported " + Long.toHexString(address));
                exported.add(Long.toHexString(address));
            }
            try (PrintWriter out = new PrintWriter(new File(dir, "selected-functions.txt"), StandardCharsets.UTF_8)) {
                out.println(currentProgram.getExecutableSHA256());
                for (String address : exported) out.println(address);
            }
        } finally { decomp.dispose(); }
    }
}
