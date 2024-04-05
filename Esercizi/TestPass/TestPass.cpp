#include "llvm/Transforms/Utils/TestPass.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

PreservedAnalyses TestPass::run(Function &F, FunctionAnalysisManager &AM) {
	errs() <<"Nome della funzione: "<<F.getName()<<"\n";
	if(F.isVarArg()){
		errs() <<"Numero di argomenti: "<< F.arg_size()<<"+*\n";
	}else{
		errs() <<"Numero di argomenti: "<< F.arg_size()<<"\n";
	}

	int call=0;
	int bb=0;
	int inst=0;
	for(auto BB = F.begin(); BB != F.end(); BB++){
		bb++;
		for(auto I = BB->begin(); I != BB->end(); I++){
			inst++;
			if(dyn_cast<CallInst>(I)){
				call++;
			}
		}
	}
   
  errs()<<"Numero di chiamate: "<<call<<"\n";
	errs()<<"Numero di Basic Blocks: "<<bb<<"\n";
	errs()<<"Numero di Istruzioni: "<<inst<<"\n\n";

	return PreservedAnalyses::all();
}
