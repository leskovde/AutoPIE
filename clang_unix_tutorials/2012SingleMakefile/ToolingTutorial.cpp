#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR == 5
#define CLANG_3_5
#endif 
#include <iostream>
#include <vector>

#include "llvm/Support/CommandLine.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Parse/Parser.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory MyToolCategory("");
/******************************************************************************
 *
 *****************************************************************************/
namespace tooling {

/******************************************************************************
 *
 *****************************************************************************/
class MyASTConsumer : public clang::ASTConsumer
{
public:
    MyASTConsumer() : clang::ASTConsumer() { }
    virtual ~MyASTConsumer() { }

    virtual bool HandleTopLevelDecl( clang::DeclGroupRef d)
    {
        static int count = 0;
        clang::DeclGroupRef::iterator it;
        for( it = d.begin(); it != d.end(); it++)
        {
            count++;
            clang::VarDecl *vd = llvm::dyn_cast<clang::VarDecl>(*it);
            if(!vd)
            {
                continue;
            }
            if( vd->isFileVarDecl() && !vd->hasExternalStorage() )
            {
                std::cerr << "Read top-level variable decl: '";
                std::cerr << vd->getDeclName().getAsString() ;
                std::cerr << std::endl;
            }
        }
        return true;
    }
};

/******************************************************************************
 *
 *****************************************************************************/
class MyFactory
{
  public:
    std::unique_ptr<MyASTConsumer> newASTConsumer() {
      return llvm::make_unique<MyASTConsumer>();
      }

};

} // Namespace tooling

/******************************************************************************
 *
 *****************************************************************************/
int main(int argc, const char **argv)
{
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  tooling::MyFactory myFactory;
  std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory(&myFactory);
  Tool.run(factory.get());
  return 0;
}
