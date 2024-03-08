#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class YVisitor : public RecursiveASTVisitor<YVisitor> {
public:
    //bool shouldTraversePostOrder() const { return true;}
    YVisitor(std::string FName, Rewriter &Rewrite) : FunctionName(FName), Rewrite(Rewrite) {}
    bool VisitCompoundStmt(CompoundStmt *S){
        S->printPretty(llvm::outs(), nullptr, Rewrite.getLangOpts());
        std::string comment = "/*func(\"" + FunctionName + "\", ID);*/";
        SourceLocation start = S->getBeginLoc();
        std::string stmtText = Lexer::getSourceText(CharSourceRange::getTokenRange(start, S->getEndLoc()), Rewrite.getSourceMgr(),Rewrite.getLangOpts()).str();
        if(stmtText.find(comment) == std::string::npos){
            Rewrite.InsertTextAfterToken(start, comment);
        }
        return true;
    }
private:

    std::string FunctionName;
    Rewriter &Rewrite;
};

class BlockHandler : public MatchFinder::MatchCallback {
public:
  BlockHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}
    
    virtual void run(const MatchFinder::MatchResult &Result) override {
      const FunctionDecl *FD = Result.Nodes.getNodeAs<FunctionDecl>("func");
      if (FD && FD->hasBody()) {
          std::string functionName;
          {
              clang::LangOptions LangOpts;
              LangOpts.CPlusPlus = true;
              clang::PrintingPolicy Policy(LangOpts);
              llvm::raw_string_ostream s(functionName);
              s << FD->getReturnType().getAsString(Policy) << " ";
              FD->printQualifiedName(s);
              s << "(";
              for (auto it = FD->param_begin(), it_end = FD->param_end(); it != it_end; ++it) {
                  if (it != FD->param_begin())
                      s << ", ";
                  s << (*it)->getType().getAsString(Policy);
              }
              s << ")";
          }
          YVisitor visitor(functionName, Rewrite);
          visitor.TraverseStmt(FD->getBody());
      }
    }
private:
  Rewriter &Rewrite;
};

class YASTConsumer : public ASTConsumer {
public:
  YASTConsumer(Rewriter &R) : handleBlock(R) {
    this->Matcher.addMatcher(functionDecl(isExpansionInMainFile()).bind("func"), &handleBlock);
  }
    void HandleTranslationUnit(ASTContext &Context) override {
        Matcher.matchAST(Context);
    }
private:
  BlockHandler handleBlock;
  MatchFinder Matcher;
};

class YFrontEndAction : public ASTFrontendAction {
public:
  void EndSourceFileAction() override {
    TheRewriter.overwriteChangedFiles();
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<YASTConsumer>(TheRewriter);
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
