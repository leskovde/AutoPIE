- ~~pouzivani nekterych funkcionalit clang nekdy potrebuje soubor prelozit! velke projekty muzou trvat hodne dlouho~~

- ~~napr clang-format funguje bez compile, funguje i na nefunkcni kod (ktery nejde prelozit)~~

- ~~source for source transformation je komplikovane, protoze v AST nejsou reference na stredniky~~

- ~~prochazet typy v AST je snadne, tezke jsou ale vnorene typy, napr int * const * x~~

- pruchod stromem vyzaduje dynamic cast na kazdou node 

- ~~pri prepisovani nodes je potreba byt opatrny, napr u templates mohou byt nodes shared (ma vice rodicu)~~

- ~~build-in RecursiveASTVisitor - muzu nastavit trigger na typech, ktere me zajimaji, ale nedava kontext, imposible to debug~~

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

- ~~incomplete code?? - incomplete code v tomto projektu asi resit nemusim, nelze ho ani prelozit~~

- Manuel Klimek - we do source to source transformation by producing replacements in the callback (generate a list of replacements) - go through all the places where things are happening, collect all the edits we are trying to make, duplicate them or go back to the user and say "no you cannot do that" and apply them
- ~~nice doxygen documentation~~
- ~~4 cases when using clang AST:~~
  
  - ~~statements~~
  - ~~declarations~~
  - ~~expression (derived from statements)~~
  - ~~types~~
- ~~libClang - C or Python interface (Python is much more readable), stable, doesn't change at all, highlevel, less code, easier~~
- ~~no base class for nodes, visit functions need to be overriden for all 4 types of nodes (statements, decl, ...)~~
- ~~libTooling - C++ interface, more powerful~~
- ~~libTooling changes so often that every tutorial from a year ago won't compile~~

- ~~!!! - when you use libTooling, you can never actually modify the AST, clang has very strong invariants about its AST, it has to be valid~~

- ~~!!! - when you use libTooling, all you are going to do is change the source code~~

- ~~!!! - the methods to change the AST are there, you just might break something, it's definitely not encouraged~~


- ~~transformating the source code is not recommended since the code loses macros, comments, etc., but maybe we don't have to worry about it in the minimal program~~

------------------------

- Expr class 
   - potomek ValueStmt, dalsi potomci ValueStmt jsou nezajimavi - napr. [asdasd] atributy
   - porad ma pod sebou dalsi Stmt a Expr...
  - zajimave metody - Expr jde evaluatnout, zjistit, jestli jde evaluatnout, jestli je RValue, jestli ma side effecty, jestli je temporary

------------------------

- Uprava AST (vkladani instrumentacniho kodu): https://www.youtube.com/watch?v=_rUwW8Awc5s
- EXISTUJE ClangSharp ! https://github.com/SimonRichards/clang-sharp

- Clang plugins

  - extra custom action during compilation
  - run FrontendActions, there are several FrontendActions, such as ASTFrontendAction (show options also for LibTooling)
  - PluginASTAction is inherited from and ParseArgs method is overriden
  - plugins are loaded dynamically and have to registered using FrontendPluginRegistry::Add
  - libraries with plugins must be loaded with the -load command when running clang
  - all arguments given to the plugin are specified in the command that calls the compiler as well, they are prefixed with -Xclang
  - another option is to use -fplugin=`plugin`, in which case the plugin runs automatically (-load is set to `plugin`)
  - plugins can generally use the same AST functionality as LibTooling with some exceptions - internal stuff is hidden (e.g. TreeTransform)
  - plugins are not stand-alone tools
- Source-to-source

  - If indirect transformation (visitor and rewriter) is used, we must be careful with rewriter instances - each file creates a new FrontendAction, so we need to have a new rewriter instance for each FrontendAction
- LibTooling

  - compilation databases - needed for accurate recreation of a compilation
  - contains namely macro definitions (`-D`) and include paths (`-I`)
  - CD is in json format with the name `compile_commands.json`
  - the json is searched for in the current directory or its parent directories
  - the json file contains entries for source files, each entry contains a directory, file, and a compilation command
  - multiple entries for a single source file are also possible (in repeated compilation, etc.)
  - build tools such as CMake and Ninja can generate these CDs
  - if the project is not using any of the compatible build tools, the user can either make a CD manually or use tools such as Build EAR https://github.com/rizsotto/Bear
  - tools created via libtooling don't always need compilation databases, they can instead use the `--` argument that separates the tool's arguments from the compilation arguments
  - arguments after `--` are used as a ad-hoc version of a compilation database
- Clang AST

  - huge class hierarchy that represents every supported source code construct
  - attempts to represent the source code as faithfully as possible
  - concidered immutable - changes are discouraged, some changes happen internally (such as due to template instantiation)
  - Clang AST is closer to C and C++ code and grammar than other ASTs
  - it for example has compile time constants available in unreduced form
  - traversal using LibTooling requires an ASTFrontendAction, a consumer and a visitor
  - when the action is run, it should create an ASTConsumer and pass any necessary data to it
  - ASTConsumer reads the AST and handles actions on certain AST items, e.g. HandleTopLevelDecl(), functions are overridable
  - the consumer also keeps track of a visitor and calls its visit and traverse functions in overriden Handle methods
  - handling specific events in the consumer may lead to a case in which a part of the code is parsed while the rest is not - this can be avoided by overriding just the HandleTranslationUnit() method, which is called after the entire code is parsed
  - for each source file, an ASTContext is used to represent the AST (ASTContext has many useful methods)
  - ASTContext bundles the AST for a translation unit and allows to traverse it from getTranslationUnitDecl
  - ASTContext has for example access to the indentifier table and source manager
  - nodes store their SourceLocation, however, it is not complete (because it is required to be small) - instead, the full location is referenced in SourceManager
  - Visit methods on the ASTVisitor are overridable and should not be called directly - instead, call TraverseDecl, which calls the visit function internally
  - typically, when using less specific visit methods, a good way to differentiate node types is to dyn_cast them
- AST Matchers
  - a domain specific language (DSL) for querying clang ast nodes
  - matchers = predicates on nodes
  - DSL is written in and used from C++
  - useful for query tools and code transformation (refactoring tools)
  - three basic categories:
    - node matchers - match specific AST nodes
    - narrowing matchers - match attributes of AST nodes
    - traversal matchers - allow traversal between AST nodes
  - each matcher matches on some type of a node, even multiple times, if it's possible
  - a matcher expression combines matchers into a query like expression
  - matcher expressions read like an english sentence by alternating type and narrowing/traversal matchers
  - by default (`AsIs` mode), all ast nodes are visited, including implicit nodes (parentheses, etc.)
  - the traversal mode can be changed to for example `IgnoreUnlessSpelledInSource` to ignore implicit nodes
  - Node Matchers:
    - the core of matcher expressions, expressions start with node matchers
    - specify which node type is expected
    - traversal matchers take node matchers as arguments
    - allow to bind nodes in order to retrieve them later for tasks such as code transformation
  - Narrowing Matchers:
    - match attributes on the current node
    - this narrows down the search range
    - this includes logical matchers: allOf, anyOf, anything, unless
  - Traversal Matchers:
    - specify the relationship to other reachable nodes
    - notably specifying children (`has`, `hasDescendant`, `forEachDescendant`)
    - take node matchers as arguments
  - matcher expressions are created by calling a creator function and building a tree of matchers
  - creating custom matchers can be done by either inheriting an existing Matcher class or by using a matcher creation macro
  - these macros specify the type, the name and the parameters (and their types) of the matcher