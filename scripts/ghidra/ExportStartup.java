// Export a bounded startup investigation. Generated C is unreviewed pseudocode.
// @category H3PS3
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public class ExportStartup extends GhidraScript {
    private PrintWriter writer(Path path) throws IOException {
        return new PrintWriter(Files.newBufferedWriter(path, StandardCharsets.UTF_8));
    }
    private String clean(Object value) {
        return String.valueOf(value).replace('\t',' ').replace('\n',' ');
    }
    @Override public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length<2) throw new IllegalArgumentException("Output directory and entry address required");
        Path out=Paths.get(args[0]);Files.createDirectories(out);
        Address entry=toAddr(args[1]);
        if(!currentProgram.getMemory().contains(entry)) throw new IllegalStateException("Entry is not mapped: "+entry);
        try(PrintWriter p=writer(out.resolve("memory-blocks.tsv"))) {
            p.println("name\tstart\tend\tbytes\tread\twrite\texecute\tinitialized");
            for(MemoryBlock b:currentProgram.getMemory().getBlocks())
                p.println(clean(b.getName())+"\t"+b.getStart()+"\t"+b.getEnd()+"\t"+b.getSize()+"\t"+b.isRead()+"\t"+b.isWrite()+"\t"+b.isExecute()+"\t"+b.isInitialized());
        }
        if(getInstructionAt(entry)==null) disassemble(entry);
        Function first=getFunctionAt(entry);
        if(first==null) first=createFunction(entry,null);
        if(first==null) throw new IllegalStateException("No function at entry");
        DecompInterface decompiler=new DecompInterface();
        if(!decompiler.openProgram(currentProgram)) throw new IllegalStateException(decompiler.getLastMessage());
        Set<Address> visited=new LinkedHashSet<>();
        ArrayDeque<Address> queue=new ArrayDeque<>();queue.add(entry);for(int i=2;i<args.length;i++)queue.add(toAddr(args[i]));
        int ok=0,failed=0,warnings=0,importsSkipped=0;
        try(PrintWriter calls=writer(out.resolve("startup-calls.tsv"));
            PrintWriter summary=writer(out.resolve("analysis-summary.txt"))) {
            calls.println("caller\tcallsite\ttarget\ttarget_name");
            summary.println("Program: "+currentProgram.getName());
            summary.println("Language: "+currentProgram.getLanguageID());
            summary.println("Compiler spec: "+currentProgram.getCompilerSpec().getCompilerSpecID());
            summary.println("Image base: "+currentProgram.getImageBase());
            summary.println("Entry investigated: "+entry);
            summary.println("Automatic whole-program analysis was disabled. This is bounded startup investigation.");
            while(!queue.isEmpty()&&visited.size()<20&&!monitor.isCancelled()) {
                Address address=queue.remove();if(!visited.add(address))continue;
                if(getInstructionAt(address)==null)disassemble(address);
                Function f=getFunctionAt(address);if(f==null)f=createFunction(address,null);
                if(f==null){failed++;continue;}
                if(f.isExternal()||f.isThunk()){importsSkipped++;continue;}
                String id=address.toString();
                try(PrintWriter asm=writer(out.resolve(id+".asm"))) {
                    InstructionIterator it=currentProgram.getListing().getInstructions(f.getBody(),true);
                    int count=0;
                    while(it.hasNext()&&count++<4096) {
                        Instruction ins=it.next();asm.println(ins.getAddress()+"\t"+ins);
                        if(ins.getFlowType().isCall())for(Address target:ins.getFlows()) {
                            Function targetFunction=getFunctionAt(target);
                            calls.println(address+"\t"+ins.getAddress()+"\t"+target+"\t"+(targetFunction==null?"":clean(targetFunction.getName())));
                            MemoryBlock block=currentProgram.getMemory().getBlock(target);
                            if(block!=null&&block.isExecute()&&(targetFunction==null||(!targetFunction.isThunk()&&!targetFunction.isExternal()))&&!visited.contains(target)&&queue.size()<100)queue.add(target);
                        }
                    }
                }
                DecompileResults result=decompiler.decompileFunction(f,45,monitor);
                try(PrintWriter c=writer(out.resolve(id+".c"))) {
                    c.println("/* UNREVIEWED GHIDRA PSEUDOCODE: "+f.getName()+" at "+address+". Not portable or buildable source. */");
                    if(result.decompileCompleted()&&result.getDecompiledFunction()!=null) {
                        String code=result.getDecompiledFunction().getC(); c.println(code);ok++;if(code.contains("halt_baddata")||code.contains("Bad instruction")||code.contains("Control flow encountered bad instruction"))warnings++;
                    } else {c.println("/* DECOMPILATION FAILED: "+clean(result.getErrorMessage())+" */");failed++;}
                }
                println("H3PS3 exported "+address+" "+f.getName());
            }
            summary.println("Visited functions: "+visited.size());summary.println("Outputs returned by decompiler: "+ok);summary.println("Failures: "+failed);summary.println("Outputs with bad-instruction warnings: "+warnings);summary.println("External imports skipped: "+importsSkipped);
        } finally {decompiler.dispose();}
        try(PrintWriter p=writer(out.resolve("functions.tsv"))) {
            p.println("address\tname\tbody_bytes\texternal\tthunk");
            FunctionIterator it=currentProgram.getFunctionManager().getFunctions(true);
            while(it.hasNext()) {Function f=it.next();p.println(f.getEntryPoint()+"\t"+clean(f.getName())+"\t"+f.getBody().getNumAddresses()+"\t"+f.isExternal()+"\t"+f.isThunk());}
        }
        try(PrintWriter p=writer(out.resolve("symbols.tsv"))) {
            p.println("address\tname\ttype\texternal");
            SymbolIterator it=currentProgram.getSymbolTable().getAllSymbols(true);
            while(it.hasNext()) {Symbol s=it.next();p.println(s.getAddress()+"\t"+clean(s.getName(true))+"\t"+s.getSymbolType()+"\t"+s.isExternal());}
        }
        println("H3PS3_EXPORT_COMPLETE");
    }
}
