- static slicer [https://github.com/mchalupa/dg]:
  - kriterium na funkce funguje divne, nemuzu ho zavolat na nic jineho nez na __assert_fail -> bude potreba studovat bitcode, abych nasel jmena volani (nejspis pomoci llvm-dis)

  - kriterium na funkce stejne k nicemu nebude, funkce mohou byt overloaded a ja davam jen nazev, nikoliv signaturu

  - kriterium na promenne na danem radku funguje dobre pro definice a ! i pro pouziti ! -> tzn. typicky usecase meho algoritmu funguje spravne.

  - moznost dumpnout dependency graph a zvyraznit v nem nodes, ktere jsou ve slicu -> uzitecne pro zkoumani, co se kde pokazilo, a pro zjisteni, jestli tento slicer umi to, co od nej vyzaduji.

  - secondary slicing criteria - bere se v potaz jen tehdy, kdyz je soucasti primarniho slicu - udajne to nema zadnou silu navic, ale mozna to lze pouzit v pripade, ze na chybove radce je vice promennych (budu pak mit polovinu volani sliceru, kdyz mu pri kazdem volani predam 2 kriteria (2 promenne))

  - slicing v cpp - option -sc povoluje specifikovat vicero primary a secondary slicing kriterii a to ve tvaru file#function#line#obj

  - option -sc je obzvlast uzitecny, jelikoz lze jako kriterium matchnout obsah cele jedne radky!

  - prevod bitcode na source jde jen pro starsi verze (starsi nez 3.1), coz nepodporuje ani jeden slicer - cpp lze rekonstruovat na verzi do 3.4, ale vygenerovany kod udajne nelze ani prelozit

- dynamic slicer [https://github.com/liuml07/giri]:
  - nelze rozjet na modernich unix-based systemech, jelikoz on nebo LLVM, se kterym shipuje (3.4), pouziva outdated include fily

  - shipuje se starsim LLVM, ktere se pri instalaci stahne a vybuildi - stejneho systemu muzu vyuzit i v me praci

  - lze rozjet na dockeru - to neni uplne feasible in the long run, protoze nevim, co je to docker a nemam poneti, jak to propojit s jakymkoliv jinym programem (treba s tim mym)

  - zatimco giri opet pracuje s llvm bitcodem, jeho vystupem je krome neuzitecneho slicu bitcodu take seznam radek zdrojoveho kodu, ktere do slicu spadaji => ! neco podobneho nejspis bude mozne udelat v DG static sliceru (viz. https://stackoverflow.com/questions/38319288/how-to-get-source-line-number-from-ll-file-llvm ) !

  - spousti se by default pomoci makefilu, protoze je to pipeline vicero programu slepenych dohromady

  - lze urcit cislem radky, ktera se ma ve slicu vyskytnout (typicky usecase meho programu), nebo pomoci cisla instrukce a jmena funkce, ktera se ma vykonat

- AST slicer [https://github.com/dwat3r/slicer]:
  - velmi kratky a nejspis jednoduchy program

  - bere c/cpp fily, vraci obrazky dependency grafu (a mimo to taky uzitecne veci)

  - vstup je omezeny na funkci, ktera se ma analyzovat (trochu problem, kdyz nevim, v jake funkci je chyba, bude se to muset nejak netrivialne resit, ale mozna umi program pracovat se vsemi funkcemi)

  - vystup je dump nekolika AST nodes, ktere odkazuji na misto v souboru zadane na vstupu a dependency graf v textovem a obrazkovem provedeni - bude potreba z programu extrahovat AST nejakym netrivialnim zpusobem

  - fungovani, uplnost a spravnost je potreba overit v diplomove praci, ktera je s nim spojena

