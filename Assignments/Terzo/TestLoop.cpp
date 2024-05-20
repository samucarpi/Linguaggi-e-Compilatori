#include "llvm/Transforms/Utils/TestLoop.h"
#include "llvm/IR/PassManager.h"
#include <llvm/IR/Constants.h>
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Dominators.h"

#include <vector>

using namespace llvm;

bool isArgument(Use *op){
    if(dyn_cast<Argument>(op)){
        return true;
    }
    return false;
}

bool isConstant(Use *op){
    if(dyn_cast<Constant>(op)){
        return true;
    }
    return false;
}

bool search(std::vector<Instruction*> founds, Instruction *Instr){
    bool found=false;
    for(unsigned int i=0; i<founds.size(); i++){
        if(founds[i] == Instr){
            found=true;
            break;
        }
    }
    return found;
}

bool isLoopInvariant(Loop &L, Instruction &Instr, std::vector<Instruction*> founds){
    bool check;
    auto op=Instr.op_begin();
    while(op!=Instr.op_end()){
        check=false;
        outs()<<"Operatore "<<**op;

        if(isArgument(op)){
            outs()<<" -> ARGOMENTO\n";
            check=true;
        }else if(isConstant(op)){
            outs()<<" -> COSTANTE\n";
            check=true;
        }else if(dyn_cast<BinaryOperator>(op)){
            Instruction* i = dyn_cast<Instruction>(op);
            if(L.contains(i) && !search(founds, i)){
                return false;
            }else{
                outs()<<" -> DIPENDE DA UN LOOP INVARIANT\n";
                check=true;
            }
        }
        if(!check)
            return false;
        else
            op++;
    }
    return check;
}

bool dominaUscite(DominatorTree &DomTree, Instruction *Instr, SmallVector<BasicBlock*> BBuscita){
    bool dominato=true;
    BasicBlock* BBInstr=Instr->getParent();
    for(auto BB: BBuscita){
        if(!DomTree.dominates(BBInstr, BB)){
            dominato=false;
            break;
        }
    }
    return dominato;
}

bool mortoDopoLoop(Instruction *Instr, SmallVector<BasicBlock*> BBsuccessori){
    bool dead=true;
    for(auto u=Instr->user_begin(); u!=Instr->user_end(); u++){
        BasicBlock* BBu=dyn_cast<Instruction>(*u)->getParent();
        for(auto BB=BBsuccessori.begin(); BB!=BBsuccessori.end(); BB++){
            if(*BB==BBu){
                dead=false;
                break;
            }
        }
    }
    return dead;
}

bool dominaUsi(DominatorTree &DomTree, Instruction *Instr){
    bool dominato=true;
    for(auto u=Instr->use_begin(); u!=Instr->use_end(); u++){
        if(!DomTree.dominates(Instr,*u)){
            dominato=false;
            break;
        }
    }
    return dominato;
}


PreservedAnalyses TestLoop::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU){
    auto BBs=L.getBlocks();
    std::vector<Instruction*> founds;

    outs()<<"\nCHECK ISTRUZIONI:\n";
    for(BasicBlock* BB : BBs){
        for(auto Inst=BB->begin(); Inst!=BB->end(); Inst++){
            if(dyn_cast<BinaryOperator>(Inst)){
                outs()<<"_____________________________________________"<<"\n";
                outs()<<"ISTRUZIONE: "<<*Inst<<"\n";
                if(isLoopInvariant(L, *Inst, founds)){
                    outs()<<"--- LOOP INVARIANT ---\n";
                    founds.push_back(dyn_cast<Instruction>(Inst));
                }else{
                    outs()<<"\n--- NON LOOP INVARIANT ---\n";
                }
                outs()<<"_____________________________________________"<<"\n";
            }
        }
    }

    DominatorTree &DomTree = LAR.DT;
    SmallVector<BasicBlock*> BBuscita;
    SmallVector<BasicBlock*> BBsuccessori;
    L.getExitingBlocks(BBuscita);
    L.getExitBlocks(BBsuccessori);

    outs()<<"\nISTRUZIONI CANDIDATE ALLA CODE MOTION:\n";
    for(auto found : founds){
        outs()<<*found<<"\n";
    }

    outs()<<"\nPULIZIA:\n";
    unsigned int i=0;
    while(i<founds.size()){
        if(!dominaUscite(DomTree, founds[i], BBuscita)){
            if(mortoDopoLoop(founds[i], BBsuccessori)){
                founds[i]->printAsOperand(outs(), false);
                outs()<<" morta dopo il loop, non rimossa\n";
            }else{
                founds[i]->printAsOperand(outs(), false);
                outs()<<" non fa parte di un BasicBlock che domina tutti i blocchi di uscita\n";
                founds.erase(founds.begin()+i);
                i--;
                outs()<<"--> RIMOSSA DAI CANDIDATI\n";
                continue;
            }
        }else if(!dominaUsi(DomTree,founds[i])){
            founds[i]->printAsOperand(outs(), false);
            outs()<<" non fa parte di un BasicBlock che domina tutti gli utilizzi nel loop\n";
            founds.erase(founds.begin()+i);
            i--;
            outs()<<"--> RIMOSSA DAI CANDIDATI\n";
            continue;
        }
        i++;
    }

    outs()<<"\nISTRUZIONI FINALI CANDIDATE ALLA CODE MOTION\n";
    for(auto found : founds){
        outs()<<*found<<"\n";
    }
    
    outs()<<"CODE MOTION\n";
    BasicBlock* preHeader=L.getLoopPreheader();
    Instruction* terminatore=preHeader->getTerminator();
    if(preHeader) {
        for(auto i : founds){
            i->moveBefore(terminatore);
            outs()<<*i<<"\n";
        }
        outs()<<"CODE MOTION COMPLETATA\n";
    } else {
        outs()<<"Preheader non esistente\n";
    }

    return PreservedAnalyses::none();
}
