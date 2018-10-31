#include <iostream>
#include <string>
#include <functional>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;

const static auto IntType = Builder.getInt8Ty();
const static auto StrType = Builder.getInt8PtrTy();
const static auto PtrType = Builder.getInt8PtrTy();
const static auto PtrPtrType = PtrType->getPointerTo();
const static auto NodeType = StructType::create(TheContext, "node");
const static auto NodePtrType = NodeType->getPointerTo();

static Function* MainFunc;
static BasicBlock* Entry;
static BasicBlock* Next;
static Constant* LastNode = ConstantPointerNull::get(NodePtrType);
static std::map<std::string, Constant*> xtMap = {};
static std::vector<BasicBlock*> NativeBlocks = {};

static AllocaInst* pc;
static AllocaInst* w;
static AllocaInst* sp;
static Constant* stack;

static ConstantInt* GetInt(int value) {
    return ConstantInt::get(IntType, value);
}

static Constant* CreateGlobalVariable(const std::string& name, Type* type, Constant* initial,
        bool isConstant=true, GlobalVariable::LinkageTypes linkageType=GlobalVariable::LinkageTypes::PrivateLinkage) {
    auto g = TheModule->getOrInsertGlobal(name, type);
    auto val = TheModule->getGlobalVariable(name);
    val->setLinkage(linkageType);
    val->setConstant(isConstant);
    val->setInitializer(initial);
    return g;
}

static void AddNativeWord(const std::string& word, const std::function<void()>& impl) {
    auto block = BasicBlock::Create(TheContext, "i_" + word, MainFunc);
    Builder.SetInsertPoint(block);
    impl();
    NativeBlocks.push_back(block);

    auto xt = BlockAddress::get(block);
    xtMap[word] = xt;

    auto str_ptr = Builder.CreateGlobalStringPtr(word, "k_" + word);
    auto node_val = ConstantStruct::get(NodeType, LastNode, str_ptr, xt, ConstantPointerNull::get(PtrPtrType));
    LastNode = CreateGlobalVariable("d_" + word, NodeType, node_val);
}

static void Push(Value* value) {
    auto current_sp = Builder.CreateLoad(sp);
    auto addr = Builder.CreateGEP(stack, {GetInt(0), current_sp});
    Builder.CreateStore(value, addr);
    Builder.CreateStore(Builder.CreateAdd(current_sp, GetInt(1)), sp);
}

static LoadInst* Pop() {
    auto current_sp = Builder.CreateLoad(sp);
    auto top_sp = Builder.CreateSub(current_sp, GetInt(1));
    auto addr = Builder.CreateGEP(stack, {GetInt(0), top_sp});
    Builder.CreateStore(top_sp, sp);
    return Builder.CreateLoad(addr);
}

static void Initialize() {
    TheModule = make_unique<Module>("main", TheContext);
    MainFunc = Function::Create(FunctionType::get(IntType, false), Function::ExternalLinkage, "main", TheModule.get());
    Entry = BasicBlock::Create(TheContext, "entry", MainFunc);
    Next = BasicBlock::Create(TheContext, "next", MainFunc);

    NodeType->setBody(
            NodePtrType, // Previous node
            StrType,     // Word of node
            PtrType,     // Execution token (block address)
            PtrPtrType   // Pointer to array of code if colon word
    );

    Builder.SetInsertPoint(Entry);
    pc = Builder.CreateAlloca(PtrPtrType, nullptr, "pc");
    w = Builder.CreateAlloca(PtrPtrType, nullptr, "w");
    sp = Builder.CreateAlloca(IntType, nullptr, "sp");
    Builder.CreateStore(GetInt(0), sp);
    auto stack_type = ArrayType::get(IntType, 1024);
    stack = CreateGlobalVariable("stack", stack_type, UndefValue::get(stack_type), false);

    AddNativeWord("foo", [](){
        Push(GetInt(8));
        Push(GetInt(7));
        Builder.CreateBr(Next);
    });
    AddNativeWord("bar", [](){
        Pop();
        auto val = Pop();
        Builder.CreateRet(val);
        Builder.CreateBr(Next);
    });
    AddNativeWord("bye", [](){
        Builder.CreateRet(GetInt(0));
    });
}

static std::vector<Constant*> MainLoop() {
    std::string token;
    auto code = std::vector<Constant*>();
    while (std::cin >> token) {
        auto xt = xtMap.find(token);
        if (xt == xtMap.end()) { // Not found
            auto value = std::stoi(token);

        } else {
            code.push_back(xt->second);
        }
    }
    return code;
}

static void Finalize(const std::vector<Constant*>& code) {
    Builder.SetInsertPoint(Entry);
    auto code_type = ArrayType::get(PtrType, code.size());
    auto code_block = CreateGlobalVariable("code", code_type, ConstantArray::get(code_type, code));
    auto start = Builder.CreateGEP(code_block, {GetInt(0), GetInt(0)}, "start");
    Builder.CreateStore(start, pc);
    Builder.CreateBr(Next);

    Builder.SetInsertPoint(Next);
    auto current_pc = Builder.CreateLoad(pc);
    Builder.CreateStore(current_pc, w);
    Builder.CreateStore(Builder.CreateGEP(current_pc, GetInt(1)), pc);
    auto br = Builder.CreateIndirectBr(Builder.CreateLoad(Builder.CreateLoad(w)), NativeBlocks.size());
    for (auto block : NativeBlocks) {
        br->addDestination(block);
    }
}

int main() {
    Initialize();
    auto code = MainLoop();
    Finalize(code);
    TheModule->print(outs(), nullptr);
    return 0;
}
