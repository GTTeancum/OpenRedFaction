// Headless analysis evidence, not compilable recovered game source.
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import ghidra.program.model.symbol.SourceType;

public class ExportBaseline extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output directory");
        File dir = new File(args[0]);
        dir.mkdirs();
        // Verified from original instructions: ECX is the destination object;
        // ret 8 / ret 4 consume stack arguments. Keep this visible to callers.
        for (long target : new long[]{0x409f40L, 0x409f70L, 0x4faa90L, 0x40a030L, 0x40a070L, 0x409fe0L}) {
            Function helper = getFunctionAt(toAddr(target));
            if (helper != null) {
                helper.setCallingConvention("__thiscall");
                if (target == 0x409f40L || target == 0x409f70L)
                    helper.setName(target == 0x409f40L ? "rf_vec3_assign_return_copy" : "rf_vec3_copy", SourceType.USER_DEFINED);
            }
        }
        try (PrintWriter out = new PrintWriter(new File(dir, "functions.tsv"), StandardCharsets.UTF_8)) {
            out.println("address\tname\tbytes\tprototype");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                out.printf("%s\t%s\t%d\t%s%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(), f.getPrototypeString(false, false));
            }
        }
        // Community addresses are candidates only until matched against this binary.
        long[] targets = {0x5760c3L, 0x52c070L, 0x52bb50L, 0x52be70L, 0x52bd40L, 0x54f160L};
        LinkedHashSet<Long> addresses = new LinkedHashSet<>();
        for (long target : targets) addresses.add(target);
        for (long target : new long[]{0x40ddf0L, 0x40d760L, 0x40d780L, 0x547150L}) addresses.add(target);
        for (long target : new long[]{0x425830L, 0x41ac60L, 0x40d850L}) addresses.add(target);
        addresses.add(0x409f40L); addresses.add(0x409f70L);
        addresses.add(0x4194e0L);
        addresses.add(0x51cb50L);
        addresses.add(0x5696f0L);
        addresses.add(0x569880L);
        for (long target : new long[]{0x5034f0L, 0x503230L, 0x40ea80L, 0x40a3b0L, 0x4facb0L}) addresses.add(target);
        for (long target : new long[]{0x51ba80L, 0x51b110L, 0x51bfd0L, 0x51c090L, 0x51c1c0L, 0x51c270L, 0x503360L, 0x503390L}) addresses.add(target);
        addresses.add(0x422360L);
        addresses.add(0x419a00L); addresses.add(0x51cc10L);
        for (long target : new long[]{0x539ed0L, 0x53a130L, 0x539e10L, 0x53a040L, 0x51a000L, 0x417e90L}) addresses.add(target);
        addresses.add(0x5698d0L); addresses.add(0x569920L);
        for (long target : new long[]{0x5142d0L, 0x51cbe0L, 0x53b408L, 0x51b500L, 0x514ca0L, 0x51ca50L, 0x53ae5fL}) addresses.add(target);
        // Character model tag lookup and pose evaluation reached from eye setup.
        for (long target : new long[]{0x51d5b0L, 0x51c590L, 0x51c190L, 0x501ab0L, 0x501ca0L, 0x51b2e0L}) {
            Function helper = getFunctionAt(toAddr(target));
            if (helper != null) helper.setCallingConvention("__thiscall");
            addresses.add(target);
        }
        try (PrintWriter out = new PrintWriter(new File(dir, "solid-mode-xrefs.tsv"), StandardCharsets.UTF_8)) {
            for (long global : new long[]{0x1808328L, 0x1cfcc1dL, 0x595b10L, 0x595b30L, 0x595f18L, 0x596484L, 0x5a4e7cL, 0x5a4e8cL, 0x5a04d0L, 0x5a7a88L, 0x62f208L}) for (var reference : getReferencesTo(toAddr(global))) {
                Function f = getFunctionContaining(reference.getFromAddress());
                out.printf("%x\t%s\t%s\t%s%n", global, reference.getFromAddress(), reference.getReferenceType(), f == null ? "none" : f.getEntryPoint());
                if (f != null) addresses.add(f.getEntryPoint().getOffset());
            }
        }
        DecompInterface decomp = new DecompInterface();
        try {
            decomp.openProgram(currentProgram);
            for (long target : addresses) {
                Function f = getFunctionAt(toAddr(target));
                if (f == null) { println("No function at candidate " + Long.toHexString(target)); continue; }
                DecompileResults result = decomp.decompileFunction(f, 90, monitor);
                File output = new File(dir, Long.toHexString(target) + ".c.txt");
                try (PrintWriter out = new PrintWriter(output, StandardCharsets.UTF_8)) {
                    out.println("/* Raw Ghidra output; candidate semantics require verification. */");
                    out.println("/* Program SHA256: " + currentProgram.getExecutableSHA256() + " */");
                    if (result.decompileCompleted()) out.print(result.getDecompiledFunction().getC());
                    else out.println("/* DECOMPILE FAILED: " + result.getErrorMessage() + " */");
                }
            }
        } finally { decomp.dispose(); }
    }
}
