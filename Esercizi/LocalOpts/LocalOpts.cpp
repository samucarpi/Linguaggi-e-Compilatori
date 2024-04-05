/*
Sostituire tutte le operazioni di 
moltiplicazione che hanno tra gli operandi una costante che
è una potenza di 2 con uno shift
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
// L'include seguente va in LocalOpts.h
#include <llvm/IR/Constants.h>

using namespace llvm;

bool runOnBasicBlock(BasicBlock &B) {

  for(auto i=B.begin();i!=B.end();++i){
    if(i->getOpcode()==BinaryOperator::Mul){
      BinaryOperator *mul=dyn_cast<BinaryOperator>(i);
      outs()<<"Analizzo la moltiplicazione: "<<*mul<<"\n";
      ConstantInt *num;
      Value *op;
      num=dyn_cast<ConstantInt>(mul->getOperand(0));
      op=mul->getOperand(1);
      if(not num){
        num=dyn_cast<ConstantInt>(mul->getOperand(1));
        op=mul->getOperand(0);
      }
      if(not num){
        outs()<<"Moltiplicazione senza nessuna costante numerica\n";
      }else if(num->getValue().isPowerOf2()){
        outs()<<"Costante: "<<num->getValue()<<" Operando: "<<*op<<"\n";
        ConstantInt *shiftNumero=ConstantInt::get(num->getType(),num->getValue().exactLogBase2());
        Instruction *shift=BinaryOperator::Create(BinaryOperator::Shl,op,shiftNumero);
        shift->insertAfter(mul);
        mul->replaceAllUsesWith(shift);
        i++;
        outs()<<"Eliminata l'istruzione: "<<*mul<<"\n";
        mul->eraseFromParent();
        outs()<<"Aggiunta l'istruzione: "<<*shift<<"\n";
        continue;
      }else{
        outs()<<"Moltiplicazione con costante numerica non potenza di 2\n";
      }
    }
  }
  return true;
}


bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {

    outs()<<"\n";
    outs()<<"Codice originale:";
    outs()<<"\n";

    for(auto i=Iter->begin();i!=Iter->end();++i)
      outs()<<*i<<"\n";
    
    outs()<<"\n";
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
      outs()<<"Sostituite le moltiplicazioni con potenze di 2 con istruzioni di tipo 'shl'\n";
    }
    outs()<<"\n";
    outs()<<"Codice aggiornato: ";
    outs()<<"\n";
    for(auto i=Iter->begin();i!=Iter->end();++i)
      outs()<<*i<<"\n";
  }
  return Transformed;
}


PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
}
