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