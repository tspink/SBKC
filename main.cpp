#include <stdio.h>
#include <string.h>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Dominators.h>

#include <queue>

using namespace llvm;

struct ClassStats
{
	unsigned int VectorInt, VectorFP;
	unsigned int ScalarInt, ScalarFP;
};

struct Stats
{
	Stats() {
		bzero(this, sizeof(Stats));
	}
	
	ClassStats ArithOps;
	
	unsigned int IntToFP, FPToInt;
	unsigned int LatchBranches, Branches;
	unsigned int LatchJumps, Jumps;
	
	unsigned int GlobalMemoryLoad, GlobalMemoryStore;
	unsigned int LocalMemoryLoad, LocalMemoryStore;
};

static void AnalyseFunction(Function& fn)
{
	DominatorTree dt;
	dt.recalculate(fn);
	
	LoopInfoBase<BasicBlock, Loop> loop_info;
	loop_info.releaseMemory();
	loop_info.analyze(dt);
	
	std::map<BasicBlock *, Stats> block_stats;
	std::map<Loop *, Stats> loop_stats;
	
	std::set<BasicBlock *> seen;
	std::queue<BasicBlock *> block_stack;
	block_stack.push(&fn.getEntryBlock());
		
	while (!block_stack.empty()) {
		BasicBlock *current = block_stack.front();
		block_stack.pop();
		
		if (seen.count(current)) continue;
		seen.insert(current);
		
		Loop *loop = loop_info.getLoopFor(current);
		
		Stats& stats = block_stats[current];		
		for (Instruction& ins : *current) {
			switch (ins.getOpcode()) {
			case Instruction::Add:
			case Instruction::FAdd:

			case Instruction::Sub:
			case Instruction::FSub:

			case Instruction::Mul:
			case Instruction::FMul:

			case Instruction::UDiv:
			case Instruction::SDiv:
			case Instruction::FDiv:

			case Instruction::URem:
			case Instruction::SRem:
			case Instruction::FRem:

			/*case Instruction::ZExt:
			case Instruction::SExt:
			case Instruction::Trunc:*/
				if (ins.getType()->isVectorTy()) {
					if (ins.getType()->isFPOrFPVectorTy()) {
						stats.ArithOps.VectorFP++;
					} else {
						stats.ArithOps.VectorInt++;
					}
				} else if (ins.getType()->isFloatingPointTy()) {
					stats.ArithOps.ScalarFP++;
				} else {
					stats.ArithOps.ScalarInt++;
				}
				break;
				
			/*case Instruction::ICmp:
				if (ins.getType()->isVectorTy()) {
					stats.VectorArithOps++;
				} else {
					stats.IntArithOps++;
				}
				break;

			case Instruction::FCmp:
				if (ins.getType()->isVectorTy()) {
					stats.VectorArithOps++;
				} else {
					stats.FPArithOps++;
				}
				break;*/
				
			case Instruction::ShuffleVector:
				if (ins.getType()->isFPOrFPVectorTy()) {
					stats.ArithOps.VectorFP++;
				} else {
					stats.ArithOps.VectorInt++;
				}
				break;
				
			case Instruction::UIToFP:
			case Instruction::SIToFP:
				stats.IntToFP++;
				break;
				
			case Instruction::FPToUI:
			case Instruction::FPToSI:
				stats.FPToInt++;
				break;
				
			/*
			case Instruction::FPExt:
			case Instruction::FPTrunc:
				stats.FPArithOps++;
				break;
			*/
				
			case Instruction::Br:
				if (((BranchInst &)ins).isConditional()) {
					if (loop && loop->getLoopLatch() == current) {
						stats.LatchBranches++;
					} else {
						stats.Branches++;
					}
				} else {
					if (loop && loop->getLoopLatch() == current) {
						stats.LatchJumps++;
					} else {
						stats.Jumps++;
					}
				}
				
				break;
				
			case Instruction::Load:
			{
				stats.GlobalMemoryLoad++;
				break;
			}
			
			case Instruction::Store:
			{
				stats.GlobalMemoryStore++;
				break;
			}
			}
		}
		
		TerminatorInst *block_terminator = current->getTerminator();
		for (unsigned int successor = 0; successor < block_terminator->getNumSuccessors(); successor++) {
			block_stack.push(block_terminator->getSuccessor(successor));
		}
	}
	
	for (auto& bs : block_stats) {
		Loop *loop = loop_info.getLoopFor(bs.first);
		if (!loop) continue;
		
		Stats& ls = loop_stats[loop];
				
		ls.ArithOps.VectorFP += bs.second.ArithOps.VectorFP;
		ls.ArithOps.VectorInt += bs.second.ArithOps.VectorInt;
		ls.ArithOps.ScalarFP += bs.second.ArithOps.ScalarFP;
		ls.ArithOps.ScalarInt += bs.second.ArithOps.ScalarInt;
	}
	
	fprintf(stderr, "*** KERNEL: %s\n", fn.getName().str().c_str());
	
	Stats totals;
	for (auto& stats : block_stats) {
		fprintf(stderr, "Block: Depth=%u, VectorFP=%u, VectorInt=%u, ScalarFP=%u, ScalarInt=%u, Latch=%s\n", 
				loop_info.getLoopDepth(stats.first), 
				stats.second.ArithOps.VectorFP,
				stats.second.ArithOps.VectorInt, 
				stats.second.ArithOps.ScalarFP, 
				stats.second.ArithOps.ScalarInt, 
				stats.second.LatchBranches > 0 ? "YES" : "NO");
		
		totals.ArithOps.VectorFP += stats.second.ArithOps.VectorFP;
		totals.ArithOps.VectorInt += stats.second.ArithOps.VectorInt;
		totals.ArithOps.ScalarFP += stats.second.ArithOps.ScalarFP;
		totals.ArithOps.ScalarInt += stats.second.ArithOps.ScalarInt;
		
		totals.LatchBranches += stats.second.LatchBranches;
		totals.Branches += stats.second.Branches;

		totals.LatchJumps += stats.second.LatchJumps;
		totals.Jumps += stats.second.Jumps;
		
		totals.LocalMemoryLoad += stats.second.LocalMemoryLoad;
		totals.LocalMemoryStore += stats.second.LocalMemoryStore;
		totals.GlobalMemoryLoad += stats.second.GlobalMemoryLoad;
		totals.GlobalMemoryStore += stats.second.GlobalMemoryStore;
	}
	
	for (auto& stats : loop_stats) {
		fprintf(stderr, " Loop: Depth=%d, VectorFP=%u, VectorInt=%u, ScalarFP=%u, ScalarInt=%u\n",
				stats.first->getLoopDepth(),
				stats.second.ArithOps.VectorFP,
				stats.second.ArithOps.VectorInt,
				stats.second.ArithOps.ScalarFP,
				stats.second.ArithOps.ScalarInt);
	}
	
	fprintf(stderr, "Total Arith. Ops: VectorFP=%u (%lf%%), VectorInt=%u (%lf%%), ScalarFP=%u (%lf%%), ScalarInt=%u (%lf%%)\n", 
		totals.ArithOps.VectorFP,
		(totals.ArithOps.VectorFP * 100.0) / (totals.ArithOps.VectorFP + totals.ArithOps.VectorInt + totals.ArithOps.ScalarFP + totals.ArithOps.ScalarInt),
		
		totals.ArithOps.VectorInt,
		(totals.ArithOps.VectorInt * 100.0) / (totals.ArithOps.VectorFP + totals.ArithOps.VectorInt + totals.ArithOps.ScalarFP + totals.ArithOps.ScalarInt),
		
		totals.ArithOps.ScalarFP,
		(totals.ArithOps.ScalarFP * 100.0) / (totals.ArithOps.VectorFP + totals.ArithOps.VectorInt + totals.ArithOps.ScalarFP + totals.ArithOps.ScalarInt),
		
		totals.ArithOps.ScalarInt,
		(totals.ArithOps.ScalarInt * 100.0) / (totals.ArithOps.VectorFP + totals.ArithOps.VectorInt + totals.ArithOps.ScalarFP + totals.ArithOps.ScalarInt)
	);	
	
	fprintf(stderr, "\n");
	fprintf(stderr, "  Total Cond. Br: Normal=%u, Loop Latch=%u\n", totals.Branches, totals.LatchBranches);
	fprintf(stderr, "Total Uncond. Br: Normal=%u, Loop Latch=%u\n", totals.Jumps, totals.LatchJumps);
	fprintf(stderr, "\n");
	fprintf(stderr, "Stores: Local=%u, Global=%u\n", totals.LocalMemoryStore, totals.GlobalMemoryStore);
	fprintf(stderr, " Loads: Local=%u, Global=%u\n", totals.LocalMemoryLoad, totals.GlobalMemoryLoad);
	fprintf(stderr, "***\n");
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "error: usage: %s <bitcode file>\n", argv[0]);
		return 1;
	}
	
	LLVMContext context;
	SMDiagnostic error;
	
	std::unique_ptr<Module> module = parseIRFile(argv[1], error, context);
	
	if (!module) {
		fprintf(stderr, "error: unable to parse bitcode\n");
		return 1;
	}
	
	auto global_annotations = module->getNamedGlobal("llvm.global.annotations");
	auto anno_array = cast<ConstantArray>(global_annotations->getOperand(0));
	
	for (int i = 0; i < anno_array->getNumOperands(); i++) {
		auto anno_struct = cast<ConstantStruct>(anno_array->getOperand(i));
		
		if (auto fn = dyn_cast<Function>(anno_struct->getOperand(0)->getOperand(0))) {
			auto anno = cast<ConstantDataArray>(cast<GlobalVariable>(anno_struct->getOperand(1)->getOperand(0))->getOperand(0))->getAsCString();
			if (anno == "kernel") {
				AnalyseFunction(*fn);
			}
		}
	}
	
	return 0;
}
