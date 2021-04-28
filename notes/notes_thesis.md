- Vyber nastroje - clang, gcc, ..., detaily v mailech.

- Udelat test suite a udelat si nejaky obrazek o tom, jak ma testovani vubec probihat.

- Master thesis - Lukáš Krížik Bobox Runtime Optimization - info o clang, pomuze s vyberem nastroje.

- Chci vybrat mezi libtooling a libclang, ale nevim, co je lepsi...

- Testu suite - http://courses.cs.vt.edu/~cs1206/Fall00/bugs_CAS.html

- Naivni a systematicky pristup - https://www.csm.ornl.gov/~sheldon/bucket/Automated-Debugging.pdf

- Zachovat vsechny zpusoby redukce a validace - nemazat neefektivni kod - ve finale mam vice zpusobu, ktere muzu porovnat.

- Generovani kodu:

  - Variace 
    - pro statementy zbytecne mnoho, nejlepsi by bylo omezeni na expressions => je potreba najit idealni node v AST takovy, aby jeho odstraneni davalo smysl.
  - STMTDemo
     - moznost odstranit vybrany kus kodu, prolezt strom do urcite miry
    - nekolik zmen pri vicero volani, moznost pouzivani cyklu

- Performance:
  	- "velke" projekty trvaji prilis dlouho, overhead je budto ve vytvareni toolu nebo v prochazeni AST -> dalsi overhead je v pouziti systemovych hlavickovych souboru, je potreba se jich v AST zbavit
  	- AST lze ulozit do souboru a pote ho lze nacist primo v toolu, neni treba ho pokazde delat znovu -> potencialni uspora casu

- Validace kodu:

  - Clang udela pro FrontEndAction kompilaci, lze videt errory, warningy, atd.

- Naivni validace: Nejnaivnejsi (pouzita v NaiveGDB a DeltaGDB) preklada vygenerovane soubory pomoci gcc a pokud preklad probehne spravne, tak je spusti v prostredi gdb. Po dobehnuti se chyta chybova hlaska, je potreba ale pridat i cislo radky, na kterem se chyba vyskytla. 

- Webova appka - hostovani toolu na github pages nebo podobne.

- Systematicka validace kodu:

  - Prozkoumat statickou analyzu v clangu (clang-check atd.).
  - Zkusit prekladani programu (nebo nejlepe jen jeho casti) do IL a spousteni programu takto.

- Dvojkombinace static a dynamic - vyhodnejsi pro obecnejsi domenu (muze odstranit kusy nedobihajciho kodu, nefunguje s vice procesy).

- Hybrid - vhodnejsi pro jednodussi programky, zavisi na dobe behu uzivatelova programu a na funkcionalite clang debuggeru.

- Pokus: zkusit dvojkombinaci a hybridni slicing na serverovou aplikaci. Podivat se, co odebral static a jestli to stacilo k zamezeni dlouheho runtime. Porovnat presnost a vykon, pokud oba dobehnou.

- **Super napad: dosadit si hodnoty argumentu do jednotlivych promennych pomoci AST a spustit static slicing.**

- V BP popsat externi kod, high level overview. Pokud je potreba zmenit kod, popsat to do BP.

- Manual pro spusteni musi zahrnout systemove pozadavky.

- Popsat v BP narocne / divne postupy pouzite behem implementace.

- Validace - jak zjistit, ze se kod chova stejne, jako predtim? prolozim ho "instrumentancim kodem" - tzv. cross-checking, e.g.

- ```
  // Pred slice:
  neco();
  instr_1();	<- byl jsem tady
  i = 1 + 1;
  instr_2();	<- byl jsem tady
  ...
  instr_3();	<- byl jsem tady
  ```

- ```
  // Po slice:
  instr_1();	<- stale jsem tady
  i = 1 + 1;
  instr_2();	<- stale jsem tady
  ...
  instr_3();	<- stale jsem tady
  ```

- 
  Spojitost staticke analyzy a halting problemu: https://youtu.be/UcxF6CVueDM?t=177

- 	- problemy s tim spojene - heuristiky, spatne vysledky, dilema false positives / false negatives (co spise reportovat?)

- Staticka analyza si neporadi s templaty v c++ - jak moje BP resi templaty? pokud resi, napsat jak, pokud neresi, napsat proc je to tezky problem.

- Kompilacni model c/c++ ma header fily, coz dela analyzu tezsi (vyzaduje vicero preparsovani) - vysvetlit, jak header fily stezuji praci mne.

- Rice's theorem: Every interesting question about a program is undecidable.

- It cannot be determined whether a program will terminate.

- Instead of deciding something undecidable, we can approximate.

- False negatives - bad for verification - bugs can slip undetected

- False positives - bad for production - time waster for developers

- Cool zpusob - pattern recognition - take all the execution flows and do a deep pattern recognition of how the program is executing - do this in multiple different ways and choose the best result (voting classifier)

- https://youtu.be/G4hL5Om4IJ4?t=3992 LLVM is the de facto platform for compilers

- Static analysis:

  - no execution required
  - a good (cheap) substitute for tests
  - hard for C++: the compilation model (include for header files) implies a lot of reparsing, type information required for parsing, macros make it hard to cache the representation of header files
  - simulated execution: try to enumerate every possible execution path = path sensitive algorithm - exponential in the branching factor
  - cross translation unit analysis - when dealing with multiple .cpp files, we might encounter a usage of a function from the different file - this might be a challenge since libtooling allows only one ASTContext at a time - might be good to save some information about different translation units
  - analysis across multiple projects - will this be supported?

- SpotBugs

  - formerly FindBugs
  - Java, works with bytecode of JDK8 and newer, doesn't require source code
  - portable (platform independent) and free, under the GNU Lesser General Public Licence
  - uses static analysis to discover bug patterns - sequence of code that might contain errors
  - looks for misused language features, misused API methods, changes created during code maintenance, over 400 bug patterns
  - can be used with a GUI or as a plugin for Ant, Eclipse, Maven or Gradle

- Clang static analyzer

  - open source
  - collection of code analysis techniques
  - automatic bug finding, similar to compiler warnings but for runtime bugs as well
  - supports C, C++ and Objective C
  - extensible
  - command-line tool, can be integrated in other tools
  - checks from array indexing to stack address scope guarding

- CodeChecker

  - wraps the clang static analyzer into a more sophisticated tool
  - includes refactoring via Clang-Tidy
  - deals with false positives
  - visualizes the result to HTML
  - analyses only relevant files (recently changed)

- Infer

  - translates C, C++, Objective C, and Java to an intermediate language
  - uses compilation information
  - IL is then analyzed method by method, function by function
  - supports checks for invalid address access, thread safety violation

- ANTLR

  - available on Linux and OSX
  - grammar recognition
  - generates a lexer and a parser based on given grammar
  - used for data formats, query languages, and programming languages
  - can be used to generate a syntax tree and walk through it using a visitor
  - requires the complete grammar of the given language
  - parsing C or C++ would be difficult due to ambiguous syntax
  - due to high popularity, many grammars have already been written
  - for C++ its only the c++14 standard => limited support

- DMS

  - toolkit for creating custom analysis
  - proprietary SW from Semantic Designs
  - mainly used for reliable refactoring, duplicate code detection, and language migration
  - takes a grammar and produces a parser that constructs ASTs
  - created ASTs can be converted back to source code via prettyprinters
  - saves info about the original code such as comments and formatting to recreate the file
  - avoid ambiguity of complicated grammars by using a GLR parser (generalized left-to-right rightmost derivation)
  - allows for transformation rules in the grammar
  - control flow and data flow analysis and graph generation - talked about earlier
  - points-to analysis - talked about earlier
  - supports many languages including C and C++ (ending with c++17)
  
  ------
  
  - the result of delta is a concat of unremovable strings
  - delta produces bad results - might depend on undefined behaviour, too large
  - effective program reduction requires more than straightforward delta debugging
  - Bugpoint tool - reduces input on LLVM IR level
  - some reducers stop at a local minimum, which is still large
  - line based delta debugging might be easy to implement, but its output is large
  - when given only the initial failing test case, the only way to find the minimum variant is by using exhaustive search
  - suffers from the same issue - exponentially many variants, no way of predicting whether an error will be present in the result
  - it is possible to use a SAT solver for semantic verification
  - heuristics - starting with the initial input and transforming it while testing it => test case REDUCTION, not MINIMIZATION
  - dd - minimizes the difference between a failure-inducing test case and a given template
  - ddmin - the template is empty => ddmin minimizes the size
  - ddmin - greedy search, can skip optimal solution by accident by being stuck on a local minimum
  - hierarchical delta debugging - better results and execution time that ddmin, uses AST
  - Berkley delta - line based ddmin - a utility called topformflat transforms the input so that all nesting below a certain depth is on one line
  - precise reduction requires steps such as removal of arguments from functions and their calling sites
  - generalized delta: a combination of search algorithm, transformation operator, validity-checking function and, a fitness function (with respect to what should the reduction be done)
  - ! getting rid of undefined behaviour - check for warnings in the compiler's output !
  - ! getting rid of more undefined behaviour - run Clang static analyzer (or other analyzers such as Infer) and terminate when warnings / errors are produced !
  	- not all problematic behaviour can be discovered by these two approaches !
  - problematic behaviours:
  	- use before initialization
  	- pointer and array errors
  	- integer overflow and shift past bandwidth
  	- struct and union errors
  	- mismatches between printf's arguments and format string
  - validation - must be done using semantic analyzers, not static analyzers
  - creduce's handling of reduction / validation suggests they are tailored towards a specific language... the goal of autopie is to run on both C and C++
  - instrumentation is useful for AST-based reduction - think about how it can be used in autopie
  - function inlining has great effect on reduction
  - generally, their reduction implementations are much larger than mine
  - examples of useful transformations:
  	 - removing bodies of compound statements, changing integer constants to 0 or 1
  	- removing just the parentheses or curly braces and leaving their bodies in the code, replacing ternary operators with the code from one of their branches
  	- removing chunks of code, starting from the size of the input file and halving down to one line - similar to Berkley delta
  	- running a formating tool to fix indentations, etc. - done inside the loop, not as a pre/postprocessing step

