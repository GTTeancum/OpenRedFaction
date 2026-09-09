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
        addresses.add(0x40f4f0L); // Shared table skin declarations ($Skin: at 0x594300).
        try (PrintWriter out = new PrintWriter(new File(dir,"model-extension-xrefs.tsv"),StandardCharsets.UTF_8)) {
            for(long target : new long[]{5918332L,5853020L,5918348L,5918359L}) for(var reference : getReferencesTo(toAddr(target))) {
                Function f=getFunctionContaining(reference.getFromAddress());
                out.printf("%x\t%s\t%s%n",target,reference.getFromAddress(),f==null?"none":f.getEntryPoint());
                if(f!=null)addresses.add(f.getEntryPoint().getOffset());
            }
        }
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
        for (long target : new long[]{0x41f270L, 0x41f400L, 0x42a8e0L, 0x40a1e0L, 0x423b90L, 0x501af0L, 0x51c340L, 0x51c390L, 0x51c3f0L}) addresses.add(target);
        for (long target : new long[]{0x41f950L, 0x41f9f0L, 0x429ae0L, 0x428d10L, 0x408dc0L, 0x408e90L, 0x48aaf0L}) addresses.add(target);
        for (long target : new long[]{0x41ae70L, 0x4a4e80L, 0x4a4a50L, 0x4a6f10L, 0x4a5910L, 0x4ae0d0L, 0x4a3260L, 0x4c9e30L,
                0x4aa0b0L, 0x4ad8a0L, 0x4ab180L, 0x4a9380L, 0x4aa080L, 0x4c8350L,
                0x50f6e0L, 0x50f580L, 0x50eee0L}) addresses.add(target);
        addresses.add(0x419a00L); addresses.add(0x51cc10L);
        addresses.add(0x539be0L); addresses.add(0x539d00L);
        for(long target : new long[]{0x402ab0L,0x42a060L,0x4289d0L,0x428a60L,0x498e80L,0x499ed0L,0x4a0840L,0x42da40L,0x48a8d0L}) addresses.add(target);
        for(long target : new long[]{0x508b70L,0x4df1c0L,0x499190L}) addresses.add(target);
        for(long target : new long[]{0x4deab0L,0x4dec10L}) addresses.add(target);
        for(long target : new long[]{0x436d70L,0x436db0L,0x465ec0L,0x465ee0L,0x539460L,0x4faaf0L}) addresses.add(target);
        addresses.add(0x4ed520L); // Pinned Dash solid_read patches identify this function.
        for(long target : new long[]{0x4cfab0L,0x4ccec0L,0x4f02e0L}) addresses.add(target);
        for(long target : new long[]{0x4dfe20L,0x4ce160L,0x4cf9a0L,0x4ccf50L}) addresses.add(target);
        addresses.add(0x4f9340L); // Room collision-tree construction from 4ccf50.
        for(long target : new long[]{0x4ccec0L,0x4ce200L,0x4ce240L,0x4ce110L,0x45ebb0L,0x507990L}) addresses.add(target); // Room attachment, detail-list routing and broad phase.
        for(long target : new long[]{0x4f8fd0L,0x4f9050L,0x4f8f90L,0x4d30e0L}) addresses.add(target);
        for(long target : new long[]{0x506550L,0x4e1f50L,0x5071b0L}) addresses.add(target);
        addresses.add(0x5072e0L); // Swept sphere against edge and endpoint fallback.
        for (long target : new long[]{0x539ed0L, 0x53a130L, 0x539e10L, 0x53a040L, 0x51a000L, 0x417e90L}) addresses.add(target);
        addresses.add(0x5698d0L); addresses.add(0x569920L);
        addresses.add(0x569d20L);
        addresses.add(0x52fcf0L);
        addresses.add(0x52dad0L);addresses.add(0x52d980L);
        addresses.add(0x51c620L);addresses.add(0x51ba00L);
        addresses.add(0x51ce60L);
        addresses.add(0x5473f0L);addresses.add(0x547540L);
        for(long target : new long[]{0x549e00L,0x549270L,0x5477a0L,0x40ef60L}) addresses.add(target);
        for(long target : new long[]{0x549bd0L,0x549310L,0x5492d0L,0x549710L,0x5492f0L}) addresses.add(target);
        try (PrintWriter out = new PrintWriter(new File(dir,"model-render-callers.tsv"),StandardCharsets.UTF_8)) {
            for(long target : new long[]{0x52e9e0L,0x53ae5fL,0x5696f0L}) for(var reference : getReferencesTo(toAddr(target))) {
                Function f=getFunctionContaining(reference.getFromAddress());
                out.printf("%x\t%s\t%s%n",target,reference.getFromAddress(),f==null?"none":f.getEntryPoint());
                if(f!=null)addresses.add(f.getEntryPoint().getOffset());
            }
        }
        for(long target : new long[]{0x503f50L,0x504000L,0x54e200L,0x565890L,0x52de10L,0x52e9e0L}) addresses.add(target);
        // Candidate byte-weight normalization consumers; classify before reuse.
        try (PrintWriter out = new PrintWriter(new File(dir,"byte-weight-xrefs.tsv"),StandardCharsets.UTF_8)) {
            for(long target : new long[]{0x5895dcL,0x589d68L}) for(var reference : getReferencesTo(toAddr(target))) {
                Function f=getFunctionContaining(reference.getFromAddress());
                out.printf("%x\t%s\t%s%n",target,reference.getFromAddress(),f==null?"none":f.getEntryPoint());
                if(f!=null)addresses.add(f.getEntryPoint().getOffset());
            }
        }
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
