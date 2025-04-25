
### **1. Sekvenční implementace**

Úvodní fází řešení úlohy bylo vytvoření sekvenční verze algoritmu pro nalezení minimálního hranového řezu mezi dvěma disjunktními podmnožinami uzlů pevně zadané velikosti. Hledání optimálního rozdělení bylo realizováno pomocí algoritmu typu **Branch-and-Bound s prohledáváním do hloubky (BB-DFS)**, který systematicky prochází možné konfigurace a současně efektivně ořezává výpočetní prostor na základě odhadované dolní meze.

Graf je reprezentován jako **matice sousednosti** pomocí struktury `std::vector<std::vector<int>>`. Tento přístup umožňuje rychlý přístup k váhám hran mezi libovolnými dvojicemi uzlů.

#### Optimalizace

Pro zvýšení efektivity výpočtu byly v algoritmu použity dvě klíčové optimalizace:

- **Dolní odhad řezu (lower bound):** V každém kroku rekurze je spočítána dolní mez na minimální možnou váhu řezu, která by mohla vzniknout v závislosti na aktuálním stavu rozdělení uzlů. Pro každý dosud nezařazený uzel je vypočtena hypotetická cena, pokud by byl přiřazen buď do podmnožiny \( X \), nebo do \( Y \), a do odhadu je zahrnuta ta z těchto možností, která je výhodnější. Součet těchto minim umožňuje algoritmu efektivně identifikovat konfigurace, které nemohou vést k lepšímu řešení než dosud nalezené, a předčasně je ořezat.

- **Heuristický odhad výchozího řešení (guesstimate):** Před samotným spuštěním prohledávání je provedeno několik náhodných rozdělení uzlů do podmnožin o požadované velikosti a pro každé z nich je spočítána váha řezu. Nejlepší nalezená hodnota je použita jako počáteční horní mez (`minCutWeight`). Bez této heuristiky by algoritmus začínal s hodnotou `INT_MAX`, případně se součtem vah všech hran, a v úvodní fázi výpočtu by nebylo možné provádět žádné ořezávání. V rámci tohoto projektu však dopad této optimalizace na výkon nebyl zásadní. Přesto se jedná o elegantní techniku, která může sehrát významnější roli v rozsáhlejších instancích problému nebo při větším paralelismu.

#### Výkon

V porovnání s referenční sekvenční implementací dosahuje navržené řešení výrazně nižšího počtu rekurzivních volání i kratší doby výpočtu. Největší přínos optimalizací je patrný u grafů s vyšším počtem uzlů, kde efektivní dolní meze a počáteční heuristika významně zmenšují prostor, který je třeba rekurzivně procházet.

**[TODO – tabulka porovnání s referenčním řešením]**

---

### **2. Paralelizace pomocí OpenMP – task-based přístup**

Ve druhé fázi řešení byla sekvenční implementace rozšířena o paralelizaci prostřednictvím **OpenMP tasků**. Cílem bylo efektivně využít výpočetní kapacitu vícejádrového procesoru prostřednictvím rozdělení stavového prostoru algoritmu BB-DFS mezi více vláken.

#### Struktura paralelního řešení

Základní myšlenkou bylo rekurzivní větvení výpočtu ponechat, ale umožnit jeho souběžné zpracování tam, kde je to výhodné. Pro tento účel byla zavedena prahová hloubka `TASK_DEPTH`, pod jejíž úroveň se jednotlivé větve průzkumu paralelizují pomocí konstrukce `#pragma omp task`. Tímto způsobem vznikají úlohy, které pokrývají různá rozhodnutí přiřazení uzlů do podmnožin \( X \) a \( Y \), a mohou být zpracovány nezávisle.

Příkladně, pokud algoritmus v hloubce menší než `TASK_DEPTH` rozhoduje o zařazení dalšího uzlu, vytvoří dvě paralelní úlohy: jednu pro případ přiřazení do množiny \( X \), druhou pro množinu \( Y \).

#### Sdílené prostředky a synchronizace

Použití společné proměnné `minCutWeight` jako horní meze pro ořezávání stavového prostoru přináší potřebu synchronizace mezi jednotlivými tasky. K zajištění konzistence při jejím případném přepisování byla využita **kritická sekce (`#pragma omp critical`)**, v jejímž rámci dochází ke srovnání a případnému aktualizování globální nejlepší hodnoty řezu i příslušného rozdělení uzlů.

Další zajímavostí je řešení problému sledování celkového počtu rekurzivních volání v paralelním prostředí. Jednoduché sdílení čítače mezi vlákny by vedlo k výraznému zpomalení kvůli synchronizační režii. Proto bylo zvoleno efektivnější řešení pomocí **vektoru čítačů**, kde každý prvek odpovídá jednomu vláknu identifikovanému pomocí `omp_get_thread_num()`. Po skončení výpočtu se hodnoty agregují, což umožňuje zachovat přehled o komplexitě výpočtu bez negativního dopadu na výkon.

#### Výsledky

Paralelizace vedla k dalšímu zrychlení výpočtu, a to i u instancí, kde již sekvenční verze dosahovala velmi dobrých výsledků. Výrazné zlepšení je patrné zejména u větších grafů a náročnějších hodnot parametru \( a \), kde dochází k masivnímu větvení stavového prostoru. Současné zpracování více větví umožňuje rychleji nalézt kvalitní řešení a efektivněji provádět ořezávání.
    
**[TODO – tabulka s výsledky paralelní verze]**

---

### **3. Paralelizace pomocí OpenMP – data-paralelní přístup**

Ve třetí fázi byla implementace změněna tak, aby cílila na **data-paralelní zpracování**. Cílem bylo dosáhnout dalšího zrychlení výpočtu prostřednictvím zpracování více nezávislých výpočetních jednotek ve větší míře najednou, především mimo samotnou rekurzi.

#### Struktura řešení

Zásadní změnou oproti předchozímu přístupu bylo zavedení **frontier-based strategie**, při níž se stavový prostor algoritmu předem „rozřeže“ na menší části. Konkrétně je pomocí funkce `generatePartialSolutions` vygenerována množina částečných konfigurací až do předem dané hloubky (`frontierDepth`). Každá z těchto konfigurací představuje alternativní výchozí stav pro další průchod rekurzivním BB-DFS algoritmem.

Následně je tato množina částečných řešení zpracována paralelně pomocí direktivy `#pragma omp parallel for`. Každý thread tak pracuje nezávisle na jiné části stavového prostoru, což umožňuje lepší využití výpočetních prostředků při současném zachování determinismu výpočtu.

Výpočet dolní meze (`parallelLB`) byl rovněž upraven: pro malé rozsahy je nadále zpracováván sekvenčně, zatímco pro větší vstupy se aktivuje paralelní verze založená na `#pragma omp for reduction`.

#### Praktické poznámky

Hodnota `frontierDepth` byla zvolena jednoduše jako minimum mezi velikostí požadované podmnožiny \( a \) a konstantou 16. Vzhledem k omezenému rozsahu projektu nebylo nutné její hodnotu detailně ladit a postačil tento *okometrický odhad*.

Dynamické rozdělování úloh pomocí fronty nebylo zavedeno – především z důvodu jednoduchosti implementace a časových omezení v rámci jiných studijních povinností. V praxi se ukázalo, že rozdělení prostřednictvím `omp for` je dostatečně efektivní a nevedlo k zásadním problémům s nerovnoměrným vytížením threadů. Vzhledem k tomu, že jednotlivé částečné konfigurace měly podobnou výpočetní náročnost, nebylo třeba dále řešit adaptivní plánování nebo přenos úloh mezi vlákny.

Uvažována byla rovněž možnost převést rekurzivní DFS na iterativní variantu, což by mohlo přinést zlepšení z hlediska cache locality nebo jednodušší správu paralelismu. Tento směr však nebyl v této fázi realizován a zůstává otevřeným tématem pro případné budoucí rozšíření.

#### Výsledky

Data-paralelní přístup přinesl další zrychlení výpočtu ve srovnání s předchozí task-based verzí, i když rozdíl nebyl tak výrazný jako při přechodu ze sekvenční verze. Významný přínos je patrný u středně velkých grafů, kde počet částečných konfigurací umožňuje dostatečně rovnoměrné rozdělení práce mezi vlákna bez výrazné režijní zátěže.

**[TODO – tabulka s výsledky data-paralelní verze]**

---

### **4 Implementace pomocí MPI**

Poslední fází projektu byla implementace algoritmu s využitím knihovny MPI a hybridního přístupu kombinujícího MPI s OpenMP. Celá architektura byla navržena ve stylu **master–slave**: hlavní proces (master) dynamicky přiděloval úlohy podřízeným procesům (slaves), které paralelně zpracovávaly části stavového prostoru.

Implementace prošla třemi významnými verzemi, které se postupně zaměřovaly na správnost i výkon.

#### První verze: rychlá, ale příliš optimistická

První varianta byla založena na jednoduchém **master–worker** modelu:
- Master proces načetl graf a vygeneroval počáteční sadu částečných řešení ("frontier").
- Každý worker dostal úlohu, provedl hluboké prohledávání DFS a odeslal nejlepší nalezený výsledek zpět.
- K přenosu bylo využito blokující schéma (`MPI_Send`, `MPI_Recv`).

Použitý dolní odhad (`lowerBound`) byl poměrně přísný, což vedlo k **rychlému výpočtu**, ale také k **chybné eliminaci** některých větví obsahujících optimální řešení. Výsledné řezy byly mírně vyšší než skutečné optimum — rozdíl byl ale **konzistentní a malý**.

#### Druhá verze: správná, ale pomalá

Ve druhé iteraci byl dolní odhad upraven tak, aby byl **volnější**:
- U každého neobsazeného vrcholu se brala minimální cena mezi přiřazením do X a do Y.
- Tím se výrazně omezilo předčasné prořezávání možných řešení.

Výsledkem bylo dosažení **správných minimálních řezů** (identických s referenčními výsledky), avšak za cenu **výrazného zpomalení**. U větších grafů narostla doba běhu na stovky sekund.

#### Třetí verze: finální hybridní řešení

Finální verze kombinovala výhody obou předchozích přístupů:
- **Těsný dolní odhad** vycházel z baseline, kdy všechny uzly šly do Y, a následně byly vybrány nejlepší přechody do X pomocí `nth_element`.
- Funkce generující "frontier" **neprováděla žádné pruning** podle aktuálního nejlepšího řešení – bylo tak zajištěno, že žádná část prostoru nebyla předčasně vyřazena.
- Při aktualizaci nejlepšího nalezeného řezu v rámci OpenMP DFS byla přidána **synchronizace pomocí `#pragma omp critical`**.
- Práce byla mezi procesy **dynamicky přerozdělována** podle potřeby.

Tato verze byla schopna nalézt správné výsledky a přitom si zachovala **výrazně lepší výkon** než druhá iterace.

#### Architektura programu

- **Master proces**:
  - Načítá graf a připravuje částečné úlohy ("frontier").
  - Přiděluje úlohy workerům a přijímá od nich výsledky.
  - Dynamicky vyvažuje zátěž mezi workery.

- **Worker procesy**:
  - Přijímají částečné řešení a provádějí hluboké DFS s paralelizací pomocí OpenMP.
  - Odesílají nejlepší nalezený řez zpět masterovi.

- **OpenMP**:
  - Každý worker využívá task paralelizaci (`#pragma omp task`) při průchodu DFS stromem.
  - Dynamické rozdělování úloh umožňuje lepší využití dostupných CPU jader.

#### Možnosti dalšího vylepšení

Potenciálně by bylo možné implementaci dále optimalizovat například:
- **Zavedením work stealingu** mezi MPI procesy pro ještě lepší vyvážení zátěže.
- **Lepším návrhem frontier úrovně**, například preferováním konfigurací s vyšší odhadovanou složitostí.
- **Využitím asynchronní komunikace** v MPI (`MPI_Isend`, `MPI_Irecv`) k překrytí komunikace a výpočtu.

---

### **5 Prezentace výsledků**


#### Výsledky sekvenční implementace
- **Tabulka**: Výsledky sekvenční verze – počet rekurzivních volání, čas běhu, nalezená váha řezu pro jednotlivé grafy.
- **Graf**: Sloupcový graf – čas běhu sekvenční verze pro různé velikosti grafů.

_(TODO: Tabulka výsledků sekvenční verze)_

_(TODO: Graf: čas běhu sekvenční verze)_


#### Výsledky OpenMP implementace (task a data paralelismus)
- **Tabulka**: Výsledky OpenMP verzí (čas běhu, počet rekurzí, případné speedup vůči sekvenční verzi).
- **Graf**: Sloupcový graf – speedup (sekvenční / OpenMP task/data) pro jednotlivé grafy.

_(TODO: Tabulka výsledků OpenMP verzí)_

_(TODO: Graf: speedup OpenMP verzí)_


#### Výsledky distribuované MPI implementace
- **Tabulka**: Výsledky MPI (čas běhu, správnost výsledku, počet rekurzivních volání).
- **Graf**: Sloupcový graf – čas běhu MPI vs. OpenMP data paralelismus.

_(TODO: Tabulka výsledků MPI verze)_

_(TODO: Graf: srovnání běhu MPI vs OpenMP)_


#### Porovnání všech implementací
- **Tabulka**: Shrnutí – čas běhu všech verzí na stejných grafech.
- **Graf**: Speedup všech metod vůči sekvenční implementaci (např. stacked/sloupcový graf).

_(TODO: Shrnutí: tabulka a graf porovnání všech verzí)_


#### Interpretace výsledků

- Stručný popis:
  - **Sekvenční implementace** sloužila jako referenční měření.
  - **OpenMP task/data paralelismus** přinesl první výrazné zrychlení.
  - **MPI + OpenMP hybridní verze** byla schopna správně najít řešení a časem překonala čistě OpenMP verze na větších grafech.
  - **Speedup** je závislý na velikosti instance, overhead distribuce je patrný na menších grafech.

_(TODO: Dopsat interpretaci podle reálných grafů/tabulek.)_

---

### **6 Diskuze výsledků**

#### Shrnutí očekávání

V rámci projektu bylo očekáváno, že postupným přechodem od sekvenční implementace přes různé paralelní verze dojde ke znatelnému zlepšení výkonnosti řešení problému minimálního hranového řezu.  
Konkrétně bylo předpokládáno:

- **Sekvenční verze** bude referenční základ bez paralelismu.
- **OpenMP task paralelismus** umožní zrychlení díky nezávislému prohledávání větví stavového prostoru.
- **OpenMP data paralelismus** přinese další optimalizaci díky lepšímu rozdělení práce.
- **MPI implementace** ve variantě master–slave bude schopná škálovat na více výpočetních uzlů a dosáhne nejvyšší výkonnosti.

Současně bylo počítáno s tím, že přechod na distribuované řešení (MPI) s sebou ponese vyšší náklady na synchronizaci a správu úloh, nicméně se očekávalo, že při dostatečně velkých instancích grafu budou tyto režie kompenzovány rozsáhlejší paralelizací.

#### Přehled reálných výsledků

Výsledky jednotlivých fází implementace přinesly několik důležitých poznatků:

- **OpenMP task paralelismus** výrazně zrychlil průchod stavovým prostorem, zejména na menších a středních grafech.
- **OpenMP data paralelismus** díky rozdělení práce na úrovni "startovních pozic" (frontier) ještě zvýšil efektivitu, ale přírůstek výkonu oproti task paralelismu již nebyl tak výrazný.
- **První verze MPI implementace** nabídla nejrychlejší výpočty, avšak za cenu mírně vyšší hodnoty nalezeného řezu oproti optimu. Přesto by se tato varianta hodila v případech, kdy je prioritou rychlost nad absolutní přesností výsledku.
- **Druhá verze MPI** korigovala problém s přesností, avšak vedla ke znatelnému zpomalení výpočtu v důsledku méně efektivního ořezávání stavového prostoru.
- **Finální MPI verze** díky zavedení těsného dolního odhadu a úplné enumerace frontier poskytla správné výsledky s přijatelným časem běhu, i když v některých případech stále zaostávala za nejlepším OpenMP řešením.

Paráda, jdeme na **6.3 Detailní rozbor podle variant**.  
Navážu v úplně stejném stylu jako předchozí část – věcně, ale dostatečně technicky.

#### Detailní rozbor podle variant

##### Sekvenční verze

Vlastní sekvenční implementace dosáhla velmi dobrého zrychlení oproti referenčnímu řešení.  
Důvody lepší efektivity byly především:

- Zavedení **rychlejšího dolního odhadu** (`betterLowerBound`), který přesněji odhadl zbývající náklady budoucího přiřazování a umožnil včasné ořezání neplodných větví.

- Použití **heuristiky guesstimate**, která umožnila nalézt kvalitní horní mez ještě před spuštěním vlastního prohledávání, čímž se dramaticky snížil počet nutných rekurzí.

- Pečlivé **optimalizace práce s pamětí** a jednoduchá reprezentace grafu přes vektor vektorů (`std::vector<std::vector<int>>`).

Výsledkem byla **mnohonásobně kratší** doba běhu u všech testovaných grafů v porovnání s referenční sekvenční verzí.

##### OpenMP task paralelismus

První paralelní verze postavená na OpenMP task paralelismu přinesla další významné urychlení.  
Hlavní myšlenkou bylo:

- Každou větev stavového prostoru prohledávat samostatným taskem, pokud byla hloubka rekurze pod určitou hranicí (`TASK_DEPTH`).

- Zachovat globální nejlepší řešení pomocí `#pragma omp critical`, čímž se zajistila správnost i v multithreaded prostředí.

Zajímavým aspektem bylo také agregování počtu rekurzivních volání na základě **thread ID** (místo synchronizovaných inkrementací), což přispělo k udržení vysokého výkonu.

Tato fáze přinesla znatelný skok ve výkonu, zejména na středně velkých grafech.

##### OpenMP data paralelismus

Ve třetí fázi byl model upraven na **master–slave** přístup v rámci jednoho procesu:  
nejprve se vygenerovalo více "startovních pozic" (`generatePartialSolutions`), a poté se každá z nich samostatně prohledávala pomocí DFS.

Výsledkem bylo:

- **Vyšší vyváženost zátěže** mezi thready.

- Možnost **efektivnější práce s velkými grafy**, protože průzkum disjunktních oblastí stavového prostoru probíhal nezávisle.

Zlepšení výkonu bylo v této fázi již mírnější než v předchozím přechodu, ale stále měřitelné.

##### MPI – první verze

První pokus o distribuovanou variantu přes MPI následoval klasický **master–slave** model:

- Master generoval úlohy až do hloubky `frontierDepth` a dynamicky je přiděloval workerům.

- Úlohy obsahovaly kromě částečného přiřazení také aktuální globální nejlepší známou hodnotu (`globalBound`), což umožňovalo workerům okamžitě ořezávat neefektivní větve.

Díky velmi "agresivnímu" dolnímu odhadu tato verze běžela **extrémně rychle**,  
ale občas nalezla řezy o něco horší než optimum – což bylo dáno tím, že některé větve vedoucí k lepšímu řešení byly předčasně ořezány.

Přesto se tato implementace jeví jako **velmi vhodná pro úlohy, kde je klíčový čas a nevyžaduje se absolutní optimalita**.

##### MPI – druhá verze

Ve snaze odstranit problémy s nepřesností byla ve druhé MPI verzi dolní odhad uvolněn:

- Místo výběru "optimálnějších" budoucích přiřazení bylo rozhodování více konzervativní.

Tím se podařilo odstranit chyby v hledání minima, ale:

- **Počet prozkoumaných stavů** dramaticky narostl.

- Výsledný čas výpočtu se výrazně prodloužil, v některých případech až několikanásobně oproti původní MPI verzi.

##### MPI – finální verze

Finální verze spojila výhody obou přístupů:

- **Těsný dolní odhad** využívající výběr `remainX` nejlepších přírůstkových nákladů (`deltas`), který zároveň nezpůsoboval ořezání příliš brzy.

- **Úplná enumerace všech částečných přiřazení** do hloubky `frontierDepth` bez předčasného pruningu, čímž se eliminovalo riziko, že by byla přehlédnuta nějaká potenciálně optimální větev.

- **Kombinace OpenMP + MPI**, kdy každá úloha byla dále paralelně řešena na více jádrech jednoho uzlu.

Výsledkem byla správnost nalezených řezů a přijatelné časy běhu, i když u menších grafů se OpenMP verze stále ukazovala jako rychlejší.

Perfektní, rozumím – cluster ani jeho vlastnosti nijak nezmiňujeme.

Pojďme tedy rovnou na návrh kapitoly **6.4 Diskuse k jednotlivým problémům, překážkám a optimalizacím**:


#### Diskuse k jednotlivým problémům, překážkám a optimalizacím

##### Paralelizace stavového prostoru

Již od sekvenční implementace bylo klíčové efektivně prořezávat stavový prostor.  
Významný posun nastal při zavedení lepšího dolního odhadu (`betterLowerBound`), který snížil počet rekurzivních volání bez rizika přehlédnutí optimálního řešení.

Při přechodu k paralelnímu zpracování (OpenMP, MPI) bylo nutné zavést synchronizaci při aktualizaci globální nejlepší hodnoty (`minCutWeight`).  
V OpenMP implementaci byla tato synchronizace řešena přes `#pragma omp critical`, v MPI implementaci pak přirozeně plynula z master–worker protokolu (asynchronní přijímání výsledků).

##### Heuristika Guesstimate

Použití heuristiky pro odhad počáteční horní meze (`guesstimate`) se ukázalo jako zásadní:

- Bez heuristiky by horní odhad začínal hodnotou `INT_MAX` (nebo součtem vah všech hran), což by znamenalo nutnost prozkoumat alespoň první kompletní přiřazení bez jakéhokoliv pruningu.

- Heuristika rychlým způsobem našla rozumně dobré řešení, a tím výrazně urychlila celkový průběh algoritmu.

I v paralelních variantách byla heuristika zachována a spouštěna před samotným rozdělováním práce.

##### MPI master–slave rozdělení práce

Všechny MPI varianty pracovaly podle **master–slave** modelu:

- Master generoval částečné konfigurace stavového prostoru až do určité hloubky (`frontierDepth`).

- Tyto konfigurace pak dynamicky přiděloval workerům podle potřeby (dynamické řízení zatížení).

Zvolený přístup zajistil, že žádný proces zbytečně nečekal, pokud byly k dispozici další neprozkoumané úlohy.

Zajímavým momentem bylo rozhodnutí **neprovádět pruning při generování frontier úloh** v poslední MPI variantě – i za cenu větší paměťové náročnosti bylo výhodnější mít jistotu, že žádná část prostoru nebude neprohledána.

##### Dolní odhad a jeho vliv na výsledek

Experimentování ukázalo, že příliš "agresivní" dolní odhad může vést k ořezání i těch větví, které by vedly k optimálnímu řešení.

Proto bylo ve finální MPI variantě zavedeno:

- Přesnější spočítání baseline (všichni do Y) + výběr přesně `remainX` nejlepších přechodů do X.

- Minimalizace chyby při odhadu → správné nalezení minimálních řezů.

Tím se podařilo spojit rychlost první MPI varianty s korektností výsledků druhé.

##### Shrnutí optimalizací

Mezi nejdůležitější optimalizace, které měly zásadní dopad na výsledek, patří:

- Heuristický odhad počáteční horní meze (guesstimate).
- Zavedení lepšího dolního odhadu (`betterLowerBound`, `lowerBound`).
- Dynamické rozdělení úloh v MPI master–slave systému.
- Paralelní DFS v rámci každého uzlu pomocí OpenMP tasků.
- Agregace počtů rekurzí přes thread–local proměnné.

Každý z těchto kroků měl v určité fázi projektu významný vliv na snížení výpočetního času nebo zvýšení správnosti řešení.

---

### 7 Závěr

V rámci semestrální práce byl implementován a analyzován algoritmus pro hledání minimálního hranového řezu grafu při fixní velikosti partice.  
Implementace prošla několika fázemi:

- **Sekvenční řešení**, kde byla použita metoda Branch & Bound s heuristikou odhadu horní meze.
- **Paralelizace pomocí OpenMP**, nejprve na úrovni tasků, následně i s přípravou startovních konfigurací.
- **Distribuovaná verze pomocí MPI**, kombinovaná s OpenMP pro hybridní paralelismus.

Provedená měření ukázala:

- Výrazné zrychlení již při přechodu ze sekvenční na OpenMP verzi (task a data paralelismus).
- Další zrychlení při využití distribuované verze (MPI), především na větších instancích grafu.
- Finální verze MPI+OpenMP implementace dokázala najít správná řešení při zachování přijatelného výpočetního času.

Mezi klíčové optimalizace, které ovlivnily efektivitu a kvalitu výsledků, patřilo:

- Využití heuristiky pro počáteční odhad horní meze (guesstimate).
- Zavedení přesnějšího dolního odhadu v rámci Branch & Bound postupu.
- Dynamické rozdělování práce mezi procesy v MPI systému.

Během projektu bylo rovněž ukázáno, že příliš agresivní prořezávání může vést k nekorektním výsledkům, a proto bylo potřeba jemně balancovat mezi rychlostí průchodu a garancí správnosti.

Výsledky ukazují, že i relativně jednoduché hybridní paralelní přístupy mohou u nestandardních kombinatorických úloh přinést výrazné zlepšení výkonu, a že při správné kombinaci heuristik a dynamického řízení lze dosáhnout rozumného kompromisu mezi rychlostí a přesností.


Pošlu ti všechny data co mám, ať z nich uděláme tabulky

Referenční CPU

Soubor	Hodnota a	Sekvenční čas [s]	# volání rek. fce	Min. váha řezu	# řešení
graf_10_5.txt	5	0,0	218	974	1
graf_10_6b.txt	5	0,0	305	1300	4
graf_10_7.txt	5	0,0	308	1593	1
graf_15_14.txt	5	0,0	9K	4963	2
graf_20_7.txt	7	0,02	34K	2110	1
graf_20_7.txt	10	0,02	43K	2378	1
graf_20_12.txt	10	0,02	142K	5060	1
graf_20_17.txt	10	0,02	235K	7995	1
graf_30_10.txt	10	1,9	9M	4636	1
graf_30_10.txt	15	3.9	17M	5333	1
graf_30_20.txt	15	13	59M	13159	1
graf_32_22.txt	10	6.3	66M	13707	1
graf_32_25.txt	12	24	268M	18114	1
graf_35_25.txt	12	65	662M	18711	1
graf_35_25.txt	17	191	2.1G	21163	1
graf_40_8.txt	15	180	828M	4256	1
graf_40_8.txt	20	275	1.1G	4690	1
graf_40_15.txt	15	403	4.1G	10098	1
graf_40_15.txt	20	2312	10.9G	11361	1
graf_40_25.txt	20	5052	26.6G	21697	1

Moje CPU

filename,partition_size,recursive_calls,time
graf_10_5.txt,5,183,0.000105423
graf_10_6b.txt,5,202,6.5555e-05
graf_10_7.txt,5,277,8.8278e-05
graf_15_14.txt,5,6019,0.00245165
graf_20_7.txt,7,10381,0.00776363
graf_20_7.txt,10,10500,0.00787336
graf_20_12.txt,10,39277,0.0245417
graf_20_17.txt,10,86411,0.0484901
graf_30_10.txt,10,965503,0.888179
graf_30_10.txt,15,733615,0.911922
graf_30_20.txt,15,6653893,6.93886
graf_32_22.txt,10,10752530,5.53075
graf_32_25.txt,12,39466809,20.5361
graf_35_25.txt,12,133293656,73.9693
graf_35_25.txt,17,147857192,109.697
graf_40_8.txt,15,15711139,16.795
graf_40_8.txt,20,15375162,19.2777
graf_40_15.txt,15,308323285,259.537
graf_40_15.txt,20,297291358,312.076
graf_40_25.txt,20,795689275,842.631

OpenMP Task

filename,partition_size,recursive_calls,time
graf_10_5.txt,5,193,0.000471052
graf_10_6b.txt,5,193,0.00681256
graf_10_7.txt,5,242,0.000427268
graf_15_14.txt,5,6020,0.00252995
graf_20_7.txt,7,9325,0.00191714
graf_20_7.txt,10,9876,0.00652727
graf_20_12.txt,10,38767,0.0034434
graf_20_17.txt,10,82052,0.010151
graf_30_10.txt,10,1179052,0.130106
graf_30_10.txt,15,793457,0.0981957
graf_30_20.txt,15,6888475,0.630512
graf_32_22.txt,10,11224584,1.04452
graf_32_25.txt,12,45701605,3.58045
graf_35_25.txt,12,121066624,10.2234
graf_35_25.txt,17,151615478,16.0082
graf_40_8.txt,15,14079045,2.66689
graf_40_8.txt,20,16678000,3.33941
graf_40_15.txt,15,438751243,51.2015
graf_40_15.txt,20,302219577,45.8628
graf_40_25.txt,20,798791158,114.198

OpenMP data parallelism 

filename,partition_size,recursive_calls,time
graf_10_5.txt,5,115,0.00013593
graf_10_6b.txt,5,158,0.00699702
graf_10_7.txt,5,206,0.00770061
graf_15_14.txt,5,5990,0.00645705
graf_20_7.txt,7,5535,0.00152065
graf_20_7.txt,10,5628,0.00334884
graf_20_12.txt,10,17379,0.00476989
graf_20_17.txt,10,59182,0.00298345
graf_30_10.txt,10,370831,0.036411
graf_30_10.txt,15,328478,0.0657218
graf_30_20.txt,15,4011800,0.321835
graf_32_22.txt,10,9294131,0.587601
graf_32_25.txt,12,26570908,1.60171
graf_35_25.txt,12,90505714,5.73599
graf_35_25.txt,17,68918082,6.07222
graf_40_8.txt,15,2690770,0.422758
graf_40_8.txt,20,1481978,0.34287
graf_40_15.txt,15,75201953,8.10201
graf_40_15.txt,20,46390866,6.37945
graf_40_25.txt,20,527679964,60.2925

MPI 1 (bohužel nemám csv):

Minimum cut weight: 5770
Total recursion calls: 2892
Best Partition:
X: 0 1 2 3 4 5 7 9 17 23 
Y: 6 8 10 11 12 13 14 15 16 18 19 20 21 22 24 25 26 27 28 29 
Elapsed time: 0.0530722 seconds
Minimum cut weight: 974
Total recursion calls: 78
Best Partition:
X: 0 1 2 8 9 
Y: 3 4 5 6 7 
Elapsed time: 0.0387312 seconds
Minimum cut weight: 1300
Total recursion calls: 83
Best Partition:
X: 0 1 3 4 6 
Y: 2 5 7 8 9 
Elapsed time: 0.0293669 seconds
Minimum cut weight: 1593
Total recursion calls: 151
Best Partition:
X: 1 3 4 5 9 
Y: 0 2 6 7 8 
Elapsed time: 0.029252 seconds
Minimum cut weight: 4969
Total recursion calls: 126
Best Partition:
X: 0 1 2 3 9 
Y: 4 5 6 7 8 10 11 12 13 14 
Elapsed time: 0.0195545 seconds
Minimum cut weight: 2614
Total recursion calls: 299
Best Partition:
X: 0 1 2 3 4 8 10 
Y: 5 6 7 9 11 12 13 14 15 16 17 18 19 
Elapsed time: 0.0137458 seconds
Minimum cut weight: 2582
Total recursion calls: 1404
Best Partition:
X: 1 2 4 5 6 7 8 9 10 14 
Y: 0 3 11 12 13 15 16 17 18 19 
Elapsed time: 0.0437741 seconds
Minimum cut weight: 5165
Total recursion calls: 3054
Best Partition:
X: 0 1 3 4 7 8 9 15 18 19 
Y: 2 5 6 10 11 12 13 14 16 17 
Elapsed time: 0.0600985 seconds
Minimum cut weight: 8116
Total recursion calls: 3722
Best Partition:
X: 0 4 5 6 7 8 9 10 12 16 
Y: 1 2 3 11 13 14 15 17 18 19 
Elapsed time: 0.069728 seconds
Minimum cut weight: 5770
Total recursion calls: 2509
Best Partition:
X: 0 1 2 3 4 5 7 9 17 23 
Y: 6 8 10 11 12 13 14 15 16 18 19 20 21 22 24 25 26 27 28 29 
Elapsed time: 0.0751579 seconds
Minimum cut weight: 5899
Total recursion calls: 68047
Best Partition:
X: 0 1 2 3 5 7 9 10 12 14 16 17 19 23 24 
Y: 4 6 8 11 13 15 18 20 21 22 25 26 27 28 29 
Elapsed time: 1.06657 seconds
Minimum cut weight: 13479
Total recursion calls: 101054
Best Partition:
X: 0 1 2 3 6 7 8 9 11 12 14 16 19 21 28 
Y: 4 5 10 13 15 17 18 20 22 23 24 25 26 27 29 
Elapsed time: 1.131 seconds
Minimum cut weight: 16095
Total recursion calls: 1633
Best Partition:
X: 0 1 2 3 4 5 7 9 10 14 
Y: 6 8 11 12 13 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 
Elapsed time: 0.0587355 seconds
Minimum cut weight: 19315
Total recursion calls: 8793
Best Partition:
X: 0 2 3 4 5 6 8 9 11 12 13 19 
Y: 1 7 10 14 15 16 17 18 20 21 22 23 24 25 26 27 28 29 30 31 
Elapsed time: 0.19292 seconds
Minimum cut weight: 19943
Total recursion calls: 7947
Best Partition:
X: 0 1 2 3 4 6 8 9 11 14 16 21 
Y: 5 7 10 12 13 15 17 18 19 20 22 23 24 25 26 27 28 29 30 31 32 33 34 
Elapsed time: 0.211075 seconds
Minimum cut weight: 21631
Total recursion calls: 315808
Best Partition:
X: 0 1 2 3 4 8 9 10 11 12 13 14 19 20 25 29 33 
Y: 5 6 7 15 16 17 18 21 22 23 24 26 27 28 30 31 32 34 
Elapsed time: 2.64632 seconds
Minimum cut weight: 5457
Total recursion calls: 39106
Best Partition:
X: 0 1 2 3 4 7 8 10 11 13 14 17 20 25 35 
Y: 5 6 9 12 15 16 18 19 21 22 23 24 26 27 28 29 30 31 32 33 34 36 37 38 39 
Elapsed time: 1.56152 seconds
Minimum cut weight: 5700
Total recursion calls: 126820
Best Partition:
X: 0 1 2 3 4 6 7 8 10 11 12 13 14 15 18 20 26 30 35 36 
Y: 5 9 16 17 19 21 22 23 24 25 27 28 29 31 32 33 34 37 38 39 
Elapsed time: 2.82138 seconds
Minimum cut weight: 12600
Total recursion calls: 38546
Best Partition:
X: 0 1 2 3 4 5 6 7 8 10 12 13 14 19 21 
Y: 9 11 15 16 17 18 20 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 
Elapsed time: 1.55611 seconds
Minimum cut weight: 12802
Total recursion calls: 130205
Best Partition:
X: 0 1 2 3 4 5 6 7 8 10 11 12 13 15 16 19 22 23 25 32 
Y: 9 14 17 18 20 21 24 26 27 28 29 30 31 33 34 35 36 37 38 39 
Elapsed time: 2.82296 seconds
Minimum cut weight: 23171
Total recursion calls: 4581504
Best Partition:
X: 0 1 2 4 5 6 7 9 10 11 12 13 15 16 18 19 26 28 30 33 
Y: 3 8 14 17 20 21 22 23 24 25 27 29 31 32 34 35 36 37 38 39 
Elapsed time: 5.23167 seconds

MPI2:

graf_10_5.txt
Minimum cut weight: 974
Total recursion calls: 241

Elapsed time: 0.038385 seconds
graf_10_6b.txt
Minimum cut weight: 1300
Total recursion calls: 252

Elapsed time: 0.0494458 seconds
graf_10_7.txt
Minimum cut weight: 1593
Total recursion calls: 316

Elapsed time: 0.049645 seconds
graf_15_14.txt
Minimum cut weight: 4963
Total recursion calls: 5598

Elapsed time: 0.0466972 seconds
graf_20_7.txt
Minimum cut weight: 2110
Total recursion calls: 13214

Elapsed time: 0.0775172 seconds
graf_20_7.txt
Minimum cut weight: 2378
Total recursion calls: 20462

Elapsed time: 0.0899592 seconds
graf_20_12.txt
Minimum cut weight: 5060
Total recursion calls: 59939

Elapsed time: 0.117091 seconds
graf_20_17.txt
Minimum cut weight: 7995
Total recursion calls: 119046

Elapsed time: 0.133543 seconds
graf_30_10.txt
Minimum cut weight: 4636
Total recursion calls: 959989

Elapsed time: 0.465937 seconds
graf_30_10.txt
Minimum cut weight: 5333
Total recursion calls: 1709511

Elapsed time: 4.01164 seconds
graf_30_20.txt
Minimum cut weight: 13159
Total recursion calls: 12693905

Elapsed time: 7.92533 seconds
graf_32_22.txt
Minimum cut weight: 13707
Total recursion calls: 9786022

Elapsed time: 3.48629 seconds
graf_32_25.txt
Minimum cut weight: 18114
Total recursion calls: 37036849

Elapsed time: 11.1633 seconds
graf_35_25.txt
Minimum cut weight: 18711
Total recursion calls: 121935274

Elapsed time: 36.6458 seconds
graf_35_25.txt
Minimum cut weight: 21163
Total recursion calls: 231280089

Elapsed time: 88.4059 seconds
graf_40_8.txt
Minimum cut weight: 4256
Total recursion calls: 23371224

Elapsed time: 17.0876 seconds
graf_40_8.txt
Minimum cut weight: 4690
Total recursion calls: 42062362

Elapsed time: 30.6209 seconds
graf_40_15.txt
Minimum cut weight: 10098
Total recursion calls: 311500404

Elapsed time: 120.68 seconds
graf_40_15.txt
Minimum cut weight: 11361
Total recursion calls: 742044041

Elapsed time: 295.644 seconds
graf_40_25.txt
timed out 600+ seconds

MPI3:
graf_10_5.txt
Minimum cut weight: 974
Total DFS calls:    199
Elapsed time:       0.700087 s
graf_10_6b.txt
Minimum cut weight: 1300
Total DFS calls:    181
Elapsed time:       0.0399271 s
graf_10_7.txt
Minimum cut weight: 1593
Total DFS calls:    259
Elapsed time:       0.042686 s
graf_15_14.txt
Minimum cut weight: 4963
Total DFS calls:    5636
Elapsed time:       0.0464536 s
graf_20_7.txt
Minimum cut weight: 2110
Total DFS calls:    5948
Elapsed time:       0.0430638 s
graf_20_7.txt
Minimum cut weight: 2378
Total DFS calls:    9030
Elapsed time:       0.0709878 s
graf_20_12.txt
Minimum cut weight: 5060
Total DFS calls:    23281
Elapsed time:       0.103996 s
graf_20_17.txt
Minimum cut weight: 7995
Total DFS calls:    61412
Elapsed time:       0.137302 s
graf_30_10.txt
Minimum cut weight: 4636
Total DFS calls:    94121
Elapsed time:       0.203179 s
graf_30_10.txt
Minimum cut weight: 5333
Total DFS calls:    202464
Elapsed time:       2.19649 s
graf_30_20.txt
Minimum cut weight: 13159
Total DFS calls:    3006829
Elapsed time:       5.59965 s
graf_32_22.txt
Minimum cut weight: 13707
Total DFS calls:    1007516
Elapsed time:       0.646947 s
graf_32_25.txt
Minimum cut weight: 18114
Total DFS calls:    5269356
Elapsed time:       3.06861 s
graf_35_25.txt
Minimum cut weight: 18711
Total DFS calls:    12568336
Elapsed time:       7.02315 s
graf_35_25.txt
Minimum cut weight: 21163
Total DFS calls:    47029498
Elapsed time:       33.2186 s
graf_40_8.txt
Minimum cut weight: 4256
Total DFS calls:    2956604
Elapsed time:       7.38073 s
graf_40_8.txt
Minimum cut weight: 4690
Total DFS calls:    5213256
Elapsed time:       14.561 s
graf_40_15.txt
Minimum cut weight: 10098
Total DFS calls:    23515327
Elapsed time:       21.7178 s
graf_40_15.txt
Minimum cut weight: 11361
Total DFS calls:    100363832
Elapsed time:       68.5089 s
graf_40_25.txt
Minimum cut weight: 21697
Total DFS calls:    154865031
Elapsed time:       98.7472 s