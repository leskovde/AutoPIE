#pragma once
#include <clang/Basic/SourceLocation.h>
#include <clang/AST/Stmt.h>

clang::SourceRange GetSourceRange(const clang::Stmt& s);

int GetChildrenCount(clang::Stmt* st);

std::string ExecCommand(const char* cmd);