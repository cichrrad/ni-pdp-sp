
### **1. Sekvenční implementace**

Úvodní fází řešení úlohy bylo vytvoření sekvenční verze algoritmu pro nalezení minimálního hranového řezu mezi dvěma disjunktními podmnožinami uzlů pevně zadané velikosti. Hledání optimálního rozdělení bylo realizováno pomocí algoritmu typu **Branch-and-Bound s prohledáváním do hloubky (BB-DFS)**, který systematicky prochází možné konfigurace a současně efektivně ořezává výpočetní prostor na základě odhadované dolní meze.

Graf je reprezentován jako **matice sousednosti** pomocí struktury `std::vector<std::vector<int>>`. Tento přístup umožňuje rychlý přístup k váhám hran mezi libovolnými dvojicemi uzlů. Reprezentace byla dostatečně efektivní pro všechny fáze řešení až do paralelizace pomocí MPI, kde byla kvůli přenositelnosti mezi procesy převedena na lineární formu.

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
