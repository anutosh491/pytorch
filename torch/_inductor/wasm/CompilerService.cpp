#include "clang/Basic/FileManager.h"
#include "clang/Basic/FileSystemOptions.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/FrontendTool/Utils.h"
#include "clang/Tooling/Tooling.h"
#include "lld/Common/Driver.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/TargetSelect.h"

#include <memory>
#include <string>
#include <vector>

LLD_HAS_DRIVER(wasm)

namespace {
class ExecuteCompilerToolAction final : public clang::tooling::ToolAction {
public:
  bool runInvocation(
      std::shared_ptr<clang::CompilerInvocation> Invocation,
      clang::FileManager *Files,
      std::shared_ptr<clang::PCHContainerOperations> PCHContainerOps,
      clang::DiagnosticConsumer *DiagConsumer) override {
    clang::CompilerInstance Compiler(std::move(Invocation),
                                     std::move(PCHContainerOps));
    Compiler.setVirtualFileSystem(Files->getVirtualFileSystemPtr());
    Compiler.setFileManager(Files);
    Compiler.createDiagnostics(DiagConsumer, false);
    Compiler.createSourceManager();
    bool Success = clang::ExecuteCompilerInvocation(&Compiler);
    Compiler.clearOutputFiles(!Success);
    return Success;
  }
};

void initializeWasmTarget() {
  static const bool Initialized = [] {
    LLVMInitializeWebAssemblyTargetInfo();
    LLVMInitializeWebAssemblyTarget();
    LLVMInitializeWebAssemblyTargetMC();
    LLVMInitializeWebAssemblyAsmPrinter();
    LLVMInitializeWebAssemblyAsmParser();
    return true;
  }();
  (void)Initialized;
}
} // namespace

extern "C" const char *torch_inductor_clang_version() {
  static const std::string Version = clang::getClangFullVersion();
  return Version.c_str();
}

extern "C" int torch_inductor_clang_compile(int Argc,
                                            const char *const *Argv) {
  if (Argc <= 0 || !Argv)
    return 2;
  initializeWasmTarget();
  std::vector<std::string> Args(Argv, Argv + Argc);
  auto Files = llvm::makeIntrusiveRefCnt<clang::FileManager>(
      clang::FileSystemOptions{});
  ExecuteCompilerToolAction Action;
  clang::tooling::ToolInvocation Invocation(
      std::move(Args), &Action, Files.get(),
      std::make_shared<clang::PCHContainerOperations>());
  return Invocation.run() ? 0 : 1;
}

extern "C" int torch_inductor_lld_link(int Argc, const char *const *Argv) {
  if (Argc <= 0 || !Argv)
    return 2;
  const lld::DriverDef Driver = {lld::Flavor::Wasm, &lld::wasm::link};
  const auto Result = lld::lldMain(
      llvm::ArrayRef<const char *>(Argv, Argc), llvm::outs(), llvm::errs(),
      llvm::ArrayRef<lld::DriverDef>(&Driver, 1));
  return Result.retCode == 0 && Result.canRunAgain ? 0 : 1;
}
