/*
##########################################
            ALGEBRAIC IDENTITY                                                        
##########################################
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include <llvm/IR/Constants.h>

using namespace llvm;

bool runOnBasicBlock(BasicBlock &B) {
  auto i=B.begin();
  while(i!=B.end()){
    if(i->getOpcode()==BinaryOperator::Add){                                     // Se l'istruzione è add:
      BinaryOperator *add=dyn_cast<BinaryOperator>(i);
      outs()<<"Analizzo l'addizione: "<<*add<<"\n";
      ConstantInt *num;
      Value *op;
      num=dyn_cast<ConstantInt>(add->getOperand(0));                             // Provo ad assegnare ad una variabile di tipo CostantInt il primo operando
      op=add->getOperand(1);                                                     // e ad una variabile di tipo Value il secondo operando
      if(not num){                                                               // Se il primo operando non è un valore costante intero
        num=dyn_cast<ConstantInt>(add->getOperand(1));                           // rifaccio le associazioni alle due variabili ma invertendo gli operandi
        op=add->getOperand(0);
      }
      if(not num){                                                               // Se anche questa volta non è presente un valore costante intero
        outs()<<"Addizione senza nessuna costante numerica\n";
      }else if(num->getValue().isZero()){                                        // Nel caso si sia trovato un valore costante intero si verifica che sia zero
        outs()<<"Costante: "<<num->getValue()<<" Operando: "<<*op<<"\n";
        add->replaceAllUsesWith(op);                                             // Rimpiazzo tutte le occorrenze del valore dell'istruzione con l'operando utilizzato nell'add
                                                                                 // Quindi se Y=X+0 (o 0+X) => Y=X e posso sostituire tutte le Y con X
        i++;                                                                     // E dopo aver incrementato l'istruzione (per evitare crash dato che successivamente si 
                                                                                 // svolgerebbero operazioni su un oggetto non più esistente)
        outs()<<"Eliminata l'istruzione: "<<*add<<"\n";
        add->eraseFromParent();                                                  // Elimino l'operazione add, ormai inutile
        continue;
      }else{
        outs()<<"Addizione con costante numerica non nulla\n";
      }
    }else if(i->getOpcode()==BinaryOperator::Mul){                               // Se l'istruzione è mul
      BinaryOperator *mul=dyn_cast<BinaryOperator>(i);
      outs()<<"Analizzo la moltiplicazione: "<<*mul<<"\n";
      ConstantInt *num;
      Value *op;
      num=dyn_cast<ConstantInt>(mul->getOperand(0));                             // Provo ad assegnare ad una variabile di tipo CostantInt il primo operando
      op=mul->getOperand(1);                                                     // e ad una variabile di tipo Value il secondo operando
      if(not num){                                                               // Se il primo operando non è un valore costante intero
        num=dyn_cast<ConstantInt>(mul->getOperand(1));                           // rifaccio le associazioni alle due variabili ma invertendo gli operandi
        op=mul->getOperand(0);
      }
      if(not num){                                                               // Se anche questa volta non è presente un valore costante intero
        outs()<<"Moltiplicazione senza nessuna costante numerica\n";
      }else if(num->getValue().isOne()){                                         // Nel caso si sia trovato un valore costante intero si verifica che sia uno
        outs()<<"Costante: "<<num->getValue()<<" Operando: "<<*op<<"\n";
        mul->replaceAllUsesWith(op);                                             // Rimpiazzo tutte le occorrenze del valore dell'istruzione con l'operando utilizzato nel mul
                                                                                 // Quindi se Y=X*1 (o 1*X) => Y=X e posso sostituire tutte le Y con X
        i++;                                                                     // E dopo aver incrementato l'istruzione (per evitare crash dato che successivamente si
                                                                                 // svolgerebbero operazioni su un oggetto non più esistente)
        outs()<<"Eliminata l'istruzione: "<<*mul<<"\n";
        mul->eraseFromParent();                                                  // Elimino l'operazione mul, ormai inutile
        continue;
      }else{
        outs()<<"Moltiplicazione con costante numerica non =1\n";
      }
    }
    i++;                                                                         // Passo alla prossima istruzione nel Basic Block
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
