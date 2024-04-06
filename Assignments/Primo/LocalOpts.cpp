/*
######################################
          PRIMO ASSIGNMENT                                                                              
######################################
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include <llvm/IR/Constants.h>
#include "cmath"

using namespace llvm;

bool check(BinaryOperator *instr,ConstantInt *&num,Value *&op){
  num=dyn_cast<ConstantInt>(instr->getOperand(0));
  op=instr->getOperand(1);
  if(not num){
    num=dyn_cast<ConstantInt>(instr->getOperand(1));
    op=instr->getOperand(0);
  }
  if(not num){
    return false;
  }
  return true;
}

bool algebraicIdentity(llvm::BasicBlock::iterator &i) {
  ConstantInt *num=nullptr;
  Value *op=nullptr;
  if(i->getOpcode()==BinaryOperator::Add){    
    BinaryOperator *add=nullptr;
    add=dyn_cast<BinaryOperator>(i);
    if(check(add,num,op)){
      if(num->getValue().isZero()){                                       
        add->replaceAllUsesWith(op);                                           
        i++;                                                                                  
        add->eraseFromParent();
        return true;
      }
    }
  }else if(i->getOpcode()==BinaryOperator::Mul){
    BinaryOperator *mul=nullptr;
    mul=dyn_cast<BinaryOperator>(i);
    if(check(mul,num,op)){
      if(num->getValue().isOne()){                                         
        mul->replaceAllUsesWith(op);
        i++;
        mul->eraseFromParent();  
        return true;
      }
    }
  }
  return false;
}

bool multiInstOpt(llvm::BasicBlock::iterator &i, Value *&opFound, Value *&found){
  ConstantInt *num=nullptr;
  Value *op=nullptr;
  if(i->getOpcode()==BinaryOperator::Add and not found){
    BinaryOperator *add=nullptr;
    add=dyn_cast<BinaryOperator>(i);
    if(check(add,num,op)){
      if(num->getValue().isOne()){
        found=add;
        opFound=op;
        i++;
        return true;
      }
    }
  }else if(i->getOpcode()==BinaryOperator::Sub and found){
    BinaryOperator *sub=nullptr;
    sub=dyn_cast<BinaryOperator>(i);
    if(check(sub,num,op)){
      if(num->getValue().isOne() and op==found){
        sub->replaceAllUsesWith(opFound);
        i++;                                                                     
        sub->eraseFromParent();
        return true;
      }
    }
  }
  return false;
}

bool StrengthReduction(llvm::BasicBlock::iterator &i){
  ConstantInt *num=nullptr;
  Value *op=nullptr;
  if(i->getOpcode()==BinaryOperator::Mul){
    BinaryOperator *mul=nullptr;
    mul=dyn_cast<BinaryOperator>(i);
    if(check(mul,num,op)){
      if(num->getValue().isPowerOf2()){
        ConstantInt *shiftValue=ConstantInt::get(num->getType(),num->getValue().exactLogBase2());
        Instruction *shiftSx=BinaryOperator::Create(BinaryOperator::Shl,op,shiftValue);
        shiftSx->insertAfter(mul);
        mul->replaceAllUsesWith(shiftSx);
        i++;
        mul->eraseFromParent();
        return true;
      }else{
        unsigned int ceilLog=num->getValue().ceilLogBase2();
        ConstantInt *shiftValue=ConstantInt::get(num->getType(), ceilLog);
        Instruction *shiftSx=BinaryOperator::Create(BinaryOperator::Shl, op, shiftValue);
        shiftSx->insertAfter(mul);

        int difference=(pow(2, ceilLog))-(num->getSExtValue());
        ConstantInt *subValue=ConstantInt::get(num->getType(), difference);
        Instruction *sub=BinaryOperator::Create(BinaryOperator::Sub,shiftSx,subValue);
        sub->insertAfter(shiftSx);
        mul->replaceAllUsesWith(sub);
        i++;
        mul->eraseFromParent();
        return true;
      }
    }
  }else if(i->getOpcode()==BinaryOperator::SDiv){
    BinaryOperator *sdiv=nullptr;
    sdiv=dyn_cast<BinaryOperator>(i);
    if(check(sdiv,num,op)){
      if(num->getValue().isPowerOf2()){
        ConstantInt *shiftValue=ConstantInt::get(num->getType(),num->getValue().exactLogBase2());
        Instruction *shiftDx=BinaryOperator::Create(BinaryOperator::LShr, op, shiftValue);
        shiftDx->insertAfter(sdiv);
        sdiv->replaceAllUsesWith(shiftDx);
        i++;
        sdiv->eraseFromParent();
        return true;
      }else{
        unsigned int ceilLog=num->getValue().ceilLogBase2();
        ConstantInt *shiftValue=ConstantInt::get(num->getType(), ceilLog);
        Instruction *shiftDx=BinaryOperator::Create(BinaryOperator::LShr, op, shiftValue);
        shiftDx->insertAfter(sdiv);
        int difference=(pow(2, ceilLog))-(num->getSExtValue());
        ConstantInt *addValue=ConstantInt::get(num->getType(), difference);
        Instruction *add=BinaryOperator::Create(BinaryOperator::Add, shiftDx, addValue);
        add->insertAfter(shiftDx);
        sdiv->replaceAllUsesWith(add);
        i++;
        sdiv->eraseFromParent();
        return true;
      }
    }
  }
  return false;
}

bool runOnBasicBlock(BasicBlock &B) {
  auto i=B.begin();
  Value *found=nullptr;
  Value *opFound=nullptr;

  while(i!=B.end()){
    if(algebraicIdentity(i)){
      continue;
    }
    if(multiInstOpt(i, opFound, found)){
      continue;
    }
    if(StrengthReduction(i)){
      continue;
    }
    
    i++;
  }
  return true;
}

bool runOnFunction(Function &F) {
  bool Transformed = false;
  for(auto Iter=F.begin();Iter!=F.end();++Iter){
    outs()<<"\n";
    outs()<<"Codice originale:";
    outs()<<"\n";
    for(auto i=Iter->begin();i!=Iter->end();++i)                                 // Istruzioni prima della modifica
      outs()<<*i<<"\n";
    outs()<<"\n";
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
    outs()<<"\n";
    outs()<<"Codice aggiornato: ";
    outs()<<"\n";
    for(auto i=Iter->begin();i!=Iter->end();++i)                                 // Istruzioni dopo la modifica
      outs()<<*i<<"\n";
  }
  return Transformed;
}

PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM){
  for(auto Fiter=M.begin();Fiter!=M.end();++Fiter)
    if(runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
