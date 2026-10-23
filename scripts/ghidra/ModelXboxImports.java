// Model Xbox imports as external functions without patching instruction bytes.
// @category H3PS3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
import java.nio.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.*;
import java.util.*;
public class ModelXboxImports extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args=getScriptArgs();if(args.length<2)throw new IllegalArgumentException("XEX and output required");
        byte[] raw=Files.readAllBytes(Paths.get(args[0]));ByteBuffer b=ByteBuffer.wrap(raw).order(ByteOrder.BIG_ENDIAN);
        int count=b.getInt(20),off=-1;
        for(int i=0;i<count;i++)if(b.getInt(24+i*8)==0x103ff)off=b.getInt(28+i*8);
        if(off<0)throw new IllegalStateException("No import table");
        int total=b.getInt(off),namesSize=b.getInt(off+4),nameCount=b.getInt(off+8);
        ArrayList<String> names=new ArrayList<>();int cursor=off+12;
        while(cursor<off+12+namesSize&&names.size()<nameCount){int start=cursor;while(cursor<off+12+namesSize&&raw[cursor]!=0)cursor++;if(cursor>start)names.add(new String(raw,start,cursor-start,StandardCharsets.US_ASCII).trim());while(cursor<off+12+namesSize&&raw[cursor]==0)cursor++;}
        Path out=Paths.get(args[1]);Files.createDirectories(out);
        int funcs=0,records=0,failures=0;
        try(PrintWriter p=new PrintWriter(Files.newBufferedWriter(out.resolve("xbox-imports.tsv"),StandardCharsets.UTF_8))) {
            p.println("library\tordinal\trecord\tkind\tname\tmodeled");
            int libpos=off+12+namesSize;
            while(libpos+40<=off+total) {
                int size=b.getInt(libpos),idx=Short.toUnsignedInt(b.getShort(libpos+36)),n=Short.toUnsignedInt(b.getShort(libpos+38));
                if(size<40||libpos+size>off+total||40+n*4>size)throw new IllegalStateException("Invalid import bounds");
                String library=names.get(idx),lastName=null;int lastOrdinal=-1;
                for(int i=0;i<n;i++) {
                    Address address=toAddr(Integer.toUnsignedLong(b.getInt(libpos+40+i*4)));
                    int word=currentProgram.getMemory().getInt(address),kind=word>>>24,ordinal=word&0xffff;records++;
                    if(kind==0) {
                        lastOrdinal=ordinal;lastName=null;
                        for(Symbol s:currentProgram.getSymbolTable().getSymbols(address))if(s.getName().startsWith("__imp__")){lastName=s.getName().substring(7);break;}
                        if(lastName==null)lastName=library.replace('.','_')+"_ordinal_"+ordinal;
                        p.println(library+"\t"+ordinal+"\t"+address+"\tIAT\t"+lastName+"\tlabel_only");
                    } else if(kind==1&&lastName!=null) {
                        try {
                            ExternalLocation ext=currentProgram.getExternalManager().addExtFunction(library,lastName,null,SourceType.IMPORTED);
                            Function f=getFunctionAt(address);
                            if(f==null)f=currentProgram.getFunctionManager().createFunction(null,address,new AddressSet(address,address.add(15)),SourceType.IMPORTED);
                            f.setThunkedFunction(ext.getFunction());funcs++;
                            p.println(library+"\t"+lastOrdinal+"\t"+address+"\tTHUNK\t"+lastName+"\texternal_function");
                        }catch(Exception e){failures++;p.println(library+"\t"+lastOrdinal+"\t"+address+"\tTHUNK\t"+lastName+"\tERROR: "+e.getMessage());}
                    } else p.println(library+"\t"+ordinal+"\t"+address+"\t"+kind+"\tunknown\tunmodeled");
                }
                libpos+=size;
            }
        }
        try(PrintWriter p=new PrintWriter(Files.newBufferedWriter(out.resolve("instruction-diagnostics.txt"),StandardCharsets.UTF_8))) {
            Address address=toAddr("8219832c");byte[] bytes=new byte[32];currentProgram.getMemory().getBytes(address,bytes);
            p.println("Bytes at 8219832c: "+HexFormat.of().formatHex(bytes));
            p.println("Instruction: "+getInstructionAt(address));
            p.println("Records read: "+records);p.println("External function thunks modeled: "+funcs);p.println("Model failures: "+failures);
        }
        println("H3PS3_IMPORT_MODEL records="+records+" functions="+funcs+" failures="+failures);
    }
}
