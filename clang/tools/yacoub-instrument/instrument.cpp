#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Rewrite/Core/Rewriter.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class BlockHandler : public MatchFinder::MatchCallback {
public:
  BlockHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}
    virtual void run(const MatchFinder::MatchResult &Result) {
      const FunctionDecl *FD = Result.Nodes.getNodeAs<FunctionDecl>("func");
      if (FD && FD->hasBody()) {
        std::string MangledName;
        llvm::raw_string_ostream MangledNameStream(MangledName);
        MangleContext *MC = FD->getASTContext().createMangleContext();
        if (MC->shouldMangleDeclName(FD)) {
          MC->mangleName(FD, MangledNameStream);
          MangledNameStream.flush();
        }
        delete MC;
        for (const Stmt *S : FD->getBody()->children()) {
          if (const CompoundStmt *CS = dyn_cast<CompoundStmt>(S)) {
            SourceLocation Loc = CS->getLBracLoc();
            Rewrite.InsertText(Loc, " func1(\"" + MangledName + "\");", true, true);
          }
        }
      }
    }
private:
  Rewriter &Rewrite;
};

class YASTConsumer : public ASTConsumer {
public:
  YASTConsumer(Rewriter &R) : handleBlock(R) {
    this->Matcher.addMatcher(functionDecl(isExpansionInMainFile()).bind("func"), &Handler);
  }
private:
  BlockHandler handleBlock;
  MatchFinder Matcher;
};

class YFrontEndAction : public ASTFrontendAction {
public:
  YFrontEndAction() {}
  void EndSourceFileAction() override {
    TheRewriter.overwriteChangedFiles();
  }
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<YASTConsumer>(TheRewriter);
  }
private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
  static llvm::cl::OptionCategory MyToolCategory("my-tool options");
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  CommonOptionsParser& op = ExpectedParser.get();
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());
  // process command line option
  return Tool.run(newFrontendActionFactory<YFrontEndAction>().get());
}
