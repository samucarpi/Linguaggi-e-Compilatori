#include "llvm/Transforms/Utils/LoopFusionPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/GenericCycleImpl.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/DependenceAnalysis.h"

using namespace llvm;

bool guardedLoopAdjacent(Loop *loop1, Loop *loop2) {
    BasicBlock *loop1Preheader = loop1->getLoopPreheader();
    if(!loop1Preheader)
        return false;

    Instruction *loop1Terminator = loop1Preheader->getTerminator();
    if(!loop1Terminator)
        return false;

    BranchInst *guardBranch = dyn_cast<BranchInst>(loop1Terminator);
    if(!guardBranch || guardBranch->isUnconditional())
        return false;

    BasicBlock *trueSucc = guardBranch->getSuccessor(0); 
    BasicBlock *falseSucc = guardBranch->getSuccessor(1);

    if (trueSucc == loop2->getHeader() || falseSucc == loop2->getHeader())
        return true;

    return false;
}

bool notGuardedLoopAdjacent(Loop *loop1, Loop *loop2){

    SmallVector<BasicBlock*> BBuscita;
    loop1->getExitBlocks(BBuscita);

    for(auto BB : BBuscita){
        if(BB != loop2->getLoopPreheader())
            return false;
    }
    return true;
}

bool isSameControlFlow(DominatorTree& DT, PostDominatorTree& PDT, Loop* loop1, Loop* loop2){
    if (DT.dominates(loop1->getHeader(),loop2->getHeader()) && PDT.dominates(loop2->getHeader(),loop1->getHeader()))
        return true;
    return false;
}

int getLoopTripCount(Loop *loop, ScalarEvolution &SE){
    return SE.getSmallConstantTripCount(loop);
}

bool search(ArrayRef<BasicBlock*> BBs, BasicBlock *BBsearch){
    bool found=false;
    for(auto BB : BBs){
        if(BB == BBsearch){
            found=true;
            break;
        }
    }
    return found;
}

SmallVector<BasicBlock*> getLoopBodyBlocks(Loop *loop){
    SmallVector<BasicBlock*> bodyBlocks;
    ArrayRef<BasicBlock*> BBs = loop->getBlocks();
    SmallVector<BasicBlock*> latches;
    loop->getLoopLatches(latches);
    SmallVector<BasicBlock*> exitings;
    loop->getExitingBlocks(exitings);
    for(BasicBlock* BB : BBs){
        if (BB == loop->getHeader() || search(latches, BB) || search(exitings, BB))
            continue;
        bodyBlocks.push_back(BB);
    }
    return bodyBlocks;
}

SmallVector<Instruction*> getInstructionsFromBlock(SmallVector<BasicBlock*> BBs){
    SmallVector<Instruction*> instrVect;
    for(auto BB : BBs){
        for(auto i=BB->begin(); i != BB->end(); i++){
            instrVect.push_back(dyn_cast<Instruction>(i));
        }
    }
    return instrVect;
}

bool checkInverseDependency(Loop *loop1, Loop *loop2, DependenceInfo &DI){
    SmallVector<BasicBlock*> bodyBlocksLoop1 = getLoopBodyBlocks(loop1);
    SmallVector<BasicBlock*> bodyBlocksLoop2 = getLoopBodyBlocks(loop2);
    SmallVector<Instruction*> bodyInstrLoop1 = getInstructionsFromBlock(bodyBlocksLoop1);
    SmallVector<Instruction*> bodyInstrLoop2 = getInstructionsFromBlock(bodyBlocksLoop2);

    for(Instruction *innerInstr : bodyInstrLoop2){
        for(Instruction *outerInstr : bodyInstrLoop1){
            auto dep = DI.depends(dyn_cast<Instruction>(innerInstr), dyn_cast<Instruction>(outerInstr), true);
            if(dep){
                outs()<<"Dependency found: "<<*innerInstr<<" -> "<<*outerInstr<<"\n";
                return false;
            }
        }
    }
    return true;
}

bool modifyUseInductionVarible(Loop *loop1, Loop *loop2,ScalarEvolution &SE){
    PHINode *ivloop1 = loop1->getCanonicalInductionVariable();
    PHINode *ivloop2 = loop2->getCanonicalInductionVariable();
    if(ivloop1 && ivloop2){
        ivloop2->replaceAllUsesWith(ivloop1);
        return true;
    }
    else
        return false;
}

void editCFG(Loop *loop1, Loop *loop2){
    BasicBlock *headerL2 = loop2->getHeader();
    BasicBlock *headerL1 = loop1->getHeader();
    BasicBlock* latchL1 = loop1->getLoopLatch();
    SmallVector<BasicBlock*> bodyL1 = getLoopBodyBlocks(loop1);
    SmallVector<BasicBlock*> bodyL2 = getLoopBodyBlocks(loop2);
    SmallVector<BasicBlock*> exitL2;
    loop2->getExitBlocks(exitL2);
    
    for(auto BB: bodyL2){
        BB->moveBefore(latchL1);
    }

    Instruction *terminatorL1 = bodyL1.back()->getTerminator();
    terminatorL1->setSuccessor(0,bodyL2.front());

    Instruction *terminatorL2 = bodyL2.back()->getTerminator();
    terminatorL2->setSuccessor(0,latchL1);
    
    Instruction *headerL2Terminator = headerL2->getTerminator();
    headerL2Terminator->setSuccessor(0, loop2->getLoopLatch());
    headerL2Terminator->setSuccessor(1, loop2->getLoopLatch());

    Instruction *headerL1Terminator = headerL1->getTerminator();
    for(auto BB : exitL2){
        headerL1Terminator->setSuccessor(1, BB);
    }
}

PreservedAnalyses LoopFusionPass::run(Function &F,FunctionAnalysisManager &AM){

    LoopInfo &loops = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    ScalarEvolution &SE =AM.getResult<ScalarEvolutionAnalysis>(F);
    DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);

    for(auto L = loops.rbegin(); L != loops.rend(); L++){
        
        auto Lnext = L;
        Lnext++;
        
        if(Lnext == loops.rend()){
            continue;
        }

        if((*L)->isGuarded()){
            outs()<<"Loop 1 guarded\n";
            if(!guardedLoopAdjacent(*L, *(Lnext))){
                outs()<<"Loop 1 e Loop 2 non adiacenti\n";
                continue;
            }else{
                outs()<<"Loop 1 e Loop 2 adiacenti\n";
            }
        }else{
            outs()<<"Loop 1 not guarded\n";
            if(!notGuardedLoopAdjacent(*L, *(Lnext))){
                outs()<<"Loop 1 e Loop 2 non adiacenti\n";
                continue;
            }else{
                outs()<<"Loop 1 e Loop 2 adiacenti\n";
            }
        }

        if(isSameControlFlow(DT,PDT,*L,*Lnext))
            outs()<<"Loop 1 e Loop 2 sono equivalenti a livello di control flow\n";
        else 
            continue;
        
        if(getLoopTripCount(*L,SE) == getLoopTripCount(*Lnext,SE))
            outs()<<"Loop 1 e Loop 2 iterano lo stesso numero di volte\n";
        else 
            continue;

        getLoopBodyBlocks(*L);

        if(!checkInverseDependency(*L, *Lnext, DI)){
            outs()<<"Loop 1 e Loop 2 soffrono di dipendenza inversa\n";
            continue;
        }

        modifyUseInductionVarible(*L, *Lnext, SE);    
        editCFG(*L, *Lnext);
    }
    return PreservedAnalyses::all();
}
