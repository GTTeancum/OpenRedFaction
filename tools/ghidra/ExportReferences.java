// Export exact-address references for bounded reconstruction follow-up.
// @category RedFaction
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.listing.Function;
import java.io.*;
import java.nio.charset.StandardCharsets;
public class ExportReferences extends GhidraScript {
    public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length<2)throw new IllegalArgumentException("Output file and hexadecimal addresses required");
        try(PrintWriter out=new PrintWriter(new File(args[0]),StandardCharsets.UTF_8)) {
            out.println("SHA256 "+currentProgram.getExecutableSHA256());
            for(int i=1;i<args.length;i++) {
                long value=Long.parseUnsignedLong(args[i].replaceFirst("^0[xX]",""),16);
                for(Reference ref:currentProgram.getReferenceManager().getReferencesTo(toAddr(value))) {
                    Function f=getFunctionContaining(ref.getFromAddress());
                    out.println(args[i]+" from "+ref.getFromAddress()+" "+ref.getReferenceType()+" function "+(f==null?"unknown":f.getEntryPoint()));
                }
            }
        }
    }
}
