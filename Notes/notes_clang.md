- pouzivani nekterych funkcionalit clang nekdy potrebuje soubor prelozit! velke projekty muzou trvat hodne dlouho

- napr clang-format funguje bez compile, funguje i na nefunkcni kod (ktery nejde prelozit)

- source for source transformation je komplikovane, protoze v AST nejsou reference na stredniky

- prochazet typy v AST je snadne, tezke jsou ale vnorene typy, napr int * const * x

- pruchod stromem vyzaduje dynamic cast na kazdou node 

- pri prepisovani nodes je potreba byt opatrny, napr u templates mohou byt nodes shared (ma vice rodicu)

- build-in RecursiveASTVisitor - muzu nastavit trigger na typech, ktere me zajimaji, ale nedava kontext, imposible to debug

- pro ziskani kontextu se pouzivaji AST Matchers - spojuji kontext, triggeruji na expressions

- AST dump:

  ```
  - clang -Xclang -ast-dump
  
  (includes se musi vypsat)
  ```

  

- filter (napr dump funkce):

  ```
  - clang-check <file> -ast-dump -ast-dump-filter <value>
  
  ```

- incomplete code?? - incomplete code v tomto projektu asi resit nemusim, nelze ho ani prelozit

- Manuel Klimek - we do source to source transformation by producing replacements in the callback (generate a list of replacements) - go through all the places where things are happening, collect all the edits we are trying to make, duplicate them or go back to the user and say "no you cannot do that" and apply them
- nice doxygen documentation
- 4 cases when using clang AST:
  - statements
  - declarations
  - expression (derived from statements)
  - types
- libClang - C or Python interface (Python is much more readable), stable, doesn't change at all, highlevel, less code, easier
- no base class for nodes, visit functions need to be overriden for all 4 types of nodes (statements, decl, ...)
- libTooling - C++ interface, more powerful
- libTooling changes so often that every tutorial from a year ago won't compile

- !!! - when you use libTooling, you can never actually modify the AST, clang has very strong invariants about its AST, it has to be valid

- !!! - when you use libTooling, all you are going to do is change the source code

- !!! - the methods to change the AST are there, you just might break something, it's definitely not encouraged


- transformating the source code is not recommended since the code loses macros, comments, etc., but maybe we don't have to worry about it in the minimal program

------------------------

- Expr class 
   - potomek ValueStmt, dalsi potomci ValueStmt jsou nezajimavi - napr. [asdasd] atributy
   - porad ma pod sebou dalsi Stmt a Expr...
  - zajimave metody - Expr jde evaluatnout, zjistit, jestli jde evaluatnout, jestli je RValue, jestli ma side effecty, jestli je temporary

------------------------

- Uprava AST (vkladani instrumentacniho kodu): https://www.youtube.com/watch?v=_rUwW8Awc5s


