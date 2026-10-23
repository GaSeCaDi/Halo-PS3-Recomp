// Analysis metadata only: known import tail calls and a verified branch-to-self.
// @category H3PS3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.*;
public class RefineControlFlow extends GhidraScript {
    @Override public void run() throws Exception {
        Path out=Paths.get(getScriptArgs()[0]);Files.createDirectories(out);
        int tails=0;boolean spinMarked=false;
        try(PrintWriter p=new PrintWriter(Files.newBufferedWriter(out.resolve("control-flow-model.tsv"),StandardCharsets.UTF_8))) {
            p.println("address\tkind\ttarget\treason");
            InstructionIterator it=currentProgram.getListing().getInstructions(true);
            while(it.hasNext()) {
                monitor.checkCancelled();Instruction ins=it.next();
                if(!ins.getFlowType().isJump()||ins.getFlowType().isConditional())continue;
                Address[] flows=ins.getFlows();if(flows.length!=1)continue;
                Function target=getFunctionAt(flows[0]);
                if(target!=null&&target.isThunk()&&target.getThunkedFunction(true).isExternal()) {
                    ins.setFlowOverride(FlowOverride.CALL_RETURN);tails++;
                    p.println(ins.getAddress()+"\ttail_call_external\t"+flows[0]+"\tunconditional branch to modeled import");
                }
            }
            Address address=toAddr("8219a308");Function spin=getFunctionAt(address);Instruction ins=getInstructionAt(address);
            if(spin!=null&&spin.getBody().getNumAddresses()==4&&ins!=null&&ins.getMnemonicString().equals("b")&&ins.getFlows().length==1&&ins.getFlows()[0].equals(address)) {
                spin.setNoReturn(true);spin.setComment("Observed single PowerPC branch to itself at 0x8219a308. No return path; original name unknown.");spinMarked=true;
                p.println(address+"\tnoreturn\t"+address+"\tverified four-byte branch to self");
            }
        }
        println("H3PS3_FLOW_MODEL tail_calls="+tails+" noreturn_spin="+spinMarked);
    }
}
