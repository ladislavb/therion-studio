# Uživatelská příručka Therion Studio

Aktualizováno: 2026-06-25

Tato příručka popisuje běžnou práci v Therion Studio. Záměrně není úplnou referencí jazyka Therion. Syntaxe Therionu, názvy příkazů, volby a obsah ukládaný do souborů zůstávají v kanonické podobě Therionu.

Aplikace používá jazyk operačního systému, pokud je dostupný vestavěný překlad. Součástí jsou překlady do angličtiny, češtiny a slovenštiny. Jazyk lze přepsat v `Soubor -> Nastavení...` (`Nastavení...` v nativním macOS aplikačním menu). Změna jazyka se projeví po restartu aplikace.

## Obsah

- [1. Co je Therion Studio](#1-co-je-therion-studio)
- [2. Hlavní okno](#2-hlavní-okno)
- [3. Začínáme](#3-začínáme)
- [4. Textový editor](#4-textový-editor)
- [5. Navigace v projektu a práce se soubory](#5-navigace-v-projektu-a-práce-se-soubory)
- [6. Vizuální editace map (`.th2`)](#6-vizuální-editace-map-th2)
- [7. Spouštění Therionu](#7-spouštění-therionu)
- [8. Nastavení](#8-nastavení)
- [9. Klávesové zkratky](#9-klávesové-zkratky)
- [10. Nápověda a O aplikaci](#10-nápověda-a-o-aplikaci)
- [11. Řešení problémů](#11-řešení-problémů)

## 1. Co je Therion Studio

Therion Studio je desktopový editor pro projekty Therionu. Poskytuje:

- procházení projektových souborů,
- textový editor pro `.th`, `.th2` a Therion config soubory,
- navigaci ve struktuře `survey`, `centerline`, `map` a `scrap`,
- vizuální editor map pro `.th2`,
- integrovanou konzoli pro spouštění Therionu.

Therion Studio neobsahuje externí Therion kompilátor. Therion nainstalujte samostatně a v případě potřeby nastavte cestu k executable v Nastavení.

### 1.1 Terminologie

- `Projekt`: složka, kterou Therion Studio právě prochází; obvykle obsahuje Therion zdrojové soubory, configy, pozadí a podsložky.
- `Config`: Therion soubor pro zpracování, například `thconfig`, `thconfig.*` nebo `*.thconfig`.
- `Survey`: struktura měření Therionu ze zdrojových `.th` souborů.
- `Mapa`: Therion objekt `map ... endmap`, který referencuje scrapy.
- `Scrap`: jeden kreslený fragment mapy v `.th2` souboru.
- `Bod`, `Linie`, `Plocha`: mapové objekty uložené ve scrapu.
- `Pozadí`: rastrový obrázek, `.xvi` nebo PocketTopo textový export používaný jako kreslicí reference.
- `.xvi`: vektorový referenční formát Therionu, často generovaný z PocketTopo dat.

## 2. Hlavní okno

Hlavní okno obsahuje:

- menu (`Soubor`, `Úpravy`, `Zobrazení`, `Nápověda`),
- horní lištu s projektovými, ukládacími, editorovými a mapovými akcemi,
- levý aktivitní panel (`Soubory`, `Struktura`, `Kompilátor`) a rychlou akci kompilace,
- centrální oblast se záložkami dokumentů,
- stavový řádek s pokyny mapy, stavem kompilace, kódováním a stavem mapy.

Běžné akce okna:

- `Soubor -> Nové okno` otevře nové prázdné okno bez kopírování aktuálního projektu a otevřených dokumentů.
- `Soubor -> Nový -> Therion zdroj (.th)`, `Therion mapa (.th2)` nebo `Therion config (.thconfig)` otevře nový neuložený dokument. Nové dokumenty `.th`, `.th2` a `.thconfig` začínají direktivou `encoding utf-8`. Tlačítko `Nový dokument` v toolbaru nabízí stejné volby. První `Uložit` se zeptá, kam ho uložit.
- `Soubor -> Nastavení...` otevře nastavení aplikace.
- `Zobrazení -> Rozbalit postranní panel` / `Sbalit postranní panel` zobrazí nebo skryje obsah levého panelu.
- `Zobrazení -> Rozbalit nápovědu`, `Rozbalit inspektor bloku` nebo `Rozbalit inspektor mapy` ovládá aktivní pravý panel podle aktuálního editoru.
- `Zobrazení -> Zobrazit mapovou lupu` / `Skrýt mapovou lupu` zapíná nebo vypíná lupu mapy. Je to jen stav UI a nemění `.th2` soubor.
- `Zobrazení -> Celá obrazovka` / `Ukončit celou obrazovku` přepíná celou obrazovku.

Pokud je mapa oddělená do samostatného okna, hlavní okno může současně zobrazit ovládání `Inspektor mapy` i `Nápověda`, protože vizuální mapa a zdroj mohou být viditelné zároveň.

## 3. Začínáme

### 3.1 Rychlý start

1. Otevřete složku projektu přes `Soubor -> Otevřít projekt...`; vyberte složku, která obsahuje Therion soubory.
2. V panelu `Kompilátor` zvolte `Cílovou konfiguraci` (`thconfig`, `thconfig.*` nebo `*.thconfig`), která se má použít pro projektové spuštění. Pokud začínáte od nuly, vytvořte config soubor a alespoň jeden `.th` zdroj přes `Soubor -> Nový`.
3. Otevřete `.th` zdrojové soubory, které definují survey, centerline data, mapy a reference na soubory. Režim `Zdroj` použijte pro přímou editaci Therion zdroje, režim `Bloky` pro podporované strukturované úpravy.
4. Otevřete existující `.th2` mapu z panelu `Soubory`, nebo vytvořte novou přes `Soubor -> Nový -> Therion mapa (.th2)`. Pro mapovou editaci používejte `Vizuálně`, pro přímé úpravy `.th2` zdroje `Zdroj`.
5. V map editoru vložte scrap před přidáváním bodů, linií nebo ploch, pokud `.th2` soubor ještě žádný scrap nemá. Kreslete mapové objekty nebo použijte `Smart Area` pro vytvoření referencované Therion plochy z existujících hranic.
6. Ověřte, že config/source soubory referencují survey a mapové soubory potřebné pro kompilaci běžnou syntaxí Therionu.
7. Dokumenty uložte a potom spusťte Therion v panelu `Kompilátor`, abyste projekt zkontrolovali nebo exportovali.

### 3.2 Otevření projektu

1. Zvolte `Soubor -> Otevřít projekt...`.
2. Vyberte složku, která obsahuje Therion zdrojové soubory, configy, pozadí a podsložky projektu.
3. Otevřete dokumenty z panelu `Soubory`.

Dialog pro výběr složky projektu začíná v domovské složce uživatele, pokud ho otevřete přes `Otevřít projekt...`.

Bez otevřeného projektu se zobrazí uvítací karta s tlačítkem `Otevřít projekt...` a panel `Soubory` zobrazí prázdný stav se stejnou akcí pro otevření projektu místo procházení filesystemu počítače. Pokud je projekt otevřený, ale není aktivní žádná záložka, uvítací karta nabídne otevření souboru z postranního panelu.

Uvítací karta a `Soubor -> Poslední projekty` zobrazují až pět naposledy otevřených projektů. Výběrem projektu z libovolného seznamu jej znovu otevřete.

Pokud je projekt otevřený, uvítací karta zobrazuje název a cestu aktivního projektu. Zobrazuje také až deset posledních souborů z tohoto projektu; výběrem soubor znovu otevřete. Stejný projektový seznam je dostupný přes `Soubor -> Poslední soubory`.

### 3.3 Otevření dokumentů

- Dvojklikem otevřete soubor v panelu `Soubory`.
- `.th2` soubory se otevírají v map editoru.
- `.th`, `thconfig`, `*.thconfig`, `thconfig.*`, `.log`, `.txt` a běžné textové soubory se otevírají v textovém editoru.
- Nepodporované soubory, například obrázky nebo PDF, zobrazí hlášení `Nepodporovaný soubor` a akci `Otevřít v externí aplikaci`.

### 3.4 Vytváření a správa souborů

Přes `Soubor -> Nový` vytvoříte neuložený `.th`, `.th2` nebo `.thconfig` dokument a cestu zvolíte při prvním uložení. Pravým kliknutím v panelu `Soubory` lze vytvářet složky, vytvářet uložené `.th`, `.th2` a `.thconfig` soubory přímo v projektu, přejmenovávat položky, duplikovat soubory, mazat položky nebo otevřít `.th2` přímo v map editoru.

Přejmenování je blokované, pokud je cílový soubor nebo složka otevřená v záložce. Mazání složky je také blokované, dokud jsou otevřené související záložky. Smazání jednoho otevřeného souboru se po potvrzení pokusí zavřít jeho záložku, případně zobrazí výzvu k neuloženým změnám, a potom soubor smaže.

### 3.5 Uložení změn

- `Soubor -> Uložit` uloží aktivní záložku.
- Pokud aktivní záložka ještě nebyla uložena, `Soubor -> Uložit` otevře `Uložit jako`.
- `Soubor -> Uložit vše` uloží všechny změněné záložky.
- Zavření změněné záložky se zeptá na uložení, zahození změn nebo zrušení.

## 4. Textový editor

### 4.1 Režimy Zdroj a Bloky

Pro `.th` a Therion config soubory:

| Režim | Použití |
|---|---|
| `Zdroj` (`Raw`) | Přímá editace zdroje se zvýrazněním syntaxe, hledáním, nahrazením, doplňováním a kontextovou nápovědou. |
| `Bloky` (`Blocks`) | Strukturovaná editace podporovaných příkazů a bloků se stejnými metadaty nápovědy příkazů jako režim Zdroj. |

Nové `.th` a Therion config záložky se otevírají ve výchozím editoru vybraném v Nastavení. Výchozí je `Zdroj`. Přepnutí do režimu `Bloky` nevloží chybějící `encoding` a nijak nepřepíše zdroj, dokud neprovedete explicitní editaci.

Toolbox v režimu `Bloky` je filtrován podle typu dokumentu:

| Dokument | Obsah toolboxu |
|---|---|
| `.th` | Příkazy z kapitoly Therion Booku `Creating data files`. `.th2` mapové příkazy `scrap`, `point`, `line` a `area` jsou skryté, protože se editují v map editoru. |
| `thconfig`, `thconfig.*`, `*.thconfig` | Příkazy z kapitoly `Processing data`, například `source`, `select`, `export`. |

Pro `.th2` soubory:

| Režim | Použití |
|---|---|
| `Zdroj` (`Raw`) | Přímá textová editace `.th2` zdroje. |
| `Vizuálně` (`Visual`) | Vizuální editace mapy s mapovým plátnem a inspektorem. |

### 4.2 Funkce textového editoru

- čísla řádků a zvýraznění aktivního řádku,
- doplňování příkazů, voleb, hodnot a cest při psaní,
- `Ctrl+Space` pro ruční otevření doplňování,
- V režimu Zdroj (`Raw`) ukazuje horní lišta dokumentu `Formátovat dokument`. Jde o výslovnou akci: nahradí počáteční odsazení jedním tabulátorem pro každou úroveň bloku Therionu, zobrazuje tabulátor jako čtyři mezery a lze ji vrátit jedním krokem. Nespouští se automaticky a nemění prázdné řádky, obsah bloků `code`, konce řádků ani kódování.
- kontextová nápověda pro aktuální příkaz nebo volbu; režimy Zdroj i Bloky zobrazují stejnou úplnou nápovědu příkazů a panel nápovědy je pojmenovaný podle aktuálního příkazu, validačního kontextu nebo vybraného cíle nápovědy,
- diagnostika v raw editoru a panelu `Validation` zobrazuje problémy jako chybně zapsané option tokeny, neznámé příkazy nebo volby, chybějící argumenty, neuzavřené bloky a prázdné `line`/`area` objekty uvnitř scrapů; `Validate Project` v panelu `Validation` ručně obnoví kontrolu všech `.th`, `.th2` a `thconfig` souborů v otevřeném projektu; projektová validace u `thconfig` souborů nehlásí nejistá varování `unknown command`, dokud nebude k dispozici samostatný thconfig katalog; projektové nálezy se zobrazují nahoře pod `Project` a jejich výběr zobrazí detail bez otevírání zdrojového souboru; aktivací souborového nálezu přejdete na dotčený řádek raw zdroje a u nálezů s explicitním bezpečným přepisem můžete použít `Apply Fix`, který u prázdných scrap objektů ukáže odstraňovaný zdrojový blok, odstraní jen tento vybraný blok a nikdy se nespustí tiše,
- pohled `Výběr` v inspektoru režimu Bloky pro editaci hlavičky vybraného bloku a podporovaných inline voleb; první panel je pojmenovaný podle vybraného příkazu Therionu a ukazuje zdrojový řádek,
- když není vybraný žádný blok, pohled `Výběr` v režimu Bloky ukazuje `Není vybrán žádný blok.`; při výběru pevné kořenové karty `encoding` ukazuje příkaz a hodnotu kódování jako text pouze pro čtení,
- hledání a nahrazení z menu `Úpravy`,
- `Soubor -> Import -> Importovat PocketTopo text...` se zobrazí jen tehdy, když je aktivní existující nebo neuložený `.th` textový dokument, a importuje PocketTopo Therion export (`.txt`) na pozici kurzoru jako Therion bloky `centreline`,
- pohled `Soubor` v inspektoru s panelem pojmenovaným podle aktuálního souboru, plnou cestou, akcí pro zkopírování cesty, velikostí na disku, časem poslední změny, aktuálním kódováním a převodem do UTF-8 pro soubory mimo UTF-8.

### 4.3 Datové řádky v režimu Bloky

V režimu `Bloky` lze bloky `data ...` upravovat tabulkou podle aktivní hlavičky dat. Prázdné řádky v těle dat se při otevření tabulky ignorují, takže mezery ve zdroji nevzniknou jako falešná měření.

## 5. Navigace v projektu a práce se soubory

### 5.1 Projektové hledání

Otevřete aktivitu `Hledání` v levém railu nebo stiskněte `Command/Ctrl+Shift+F`. Zadejte doslovný text, podle potřeby zvolte `Celé slovo` nebo `Rozlišovat velikost`, a stisknutím `Enter` nebo `Hledat` prohledejte aktuální projekt.

Projektové hledání prohledává Therion textové zdroje (`.th`, `.th2` a konfigurační soubory Therionu), zahrnuje neuložené změny v otevřených záložkách a vypisuje shody seskupené podle souboru s řádkem a sloupcem. Dvojklikem na soubor nebo řádek shody otevřete soubor na odpovídajícím textu s připraveným inline hledáním pro další/předchozí shodu. Shody v `.th2` se otevírají jako dokumenty mapového editoru s aktivním workspace Zdroj (`Raw`), takže se potom můžete vrátit zpět do vizuálního mapového editoru.

### 5.2 Panel Struktura

Panel `Struktura` je lehký navigační index otevřeného projektu. Popis v postranním panelu jej označuje jako strukturu `survey`, `map` a `scrap` aktuálního projektu. Zobrazuje hierarchii `survey`, `centerline`, `map` a `scrap` a rozpoznává obě Therion varianty: `centreline` i `centerline`.

Kliknutím řádek pouze vyberete ve stromu. Dvojklikem na zdrojový řádek, nebo jeho výběrem a stiskem `Enter`, otevřete zdrojový dokument a přejdete na odpovídající řádek.

Pohledy `Survey` a `Mapa` si při otevřeném stejném projektu pamatují svůj stav rozbalení/sbalení odděleně.

Index používá vybranou `Cílovou konfiguraci`, pokud ukazuje do otevřeného projektu. Bez explicitního configu použije první kořenový config v pořadí `thconfig`, `thconfig.thconfig`, `main.thconfig`, `index.thconfig` a `<nazev_projektu>.thconfig`, kde `<nazev_projektu>` je název otevřené projektové složky. Pokud žádný z nich neexistuje a v kořeni je právě jeden pojmenovaný config (`*.thconfig` nebo `thconfig.*`), použije se ten. Pokud je možných configů víc, vyberte požadovanou `Cílovou konfiguraci` v panelu `Kompilátor`.

Mapy a scrapy referencované uvnitř `map ... endmap` se zobrazí pod danou mapou, pokud je reference jednoznačná. Nerozřešené nebo nejednoznačné reference se zobrazí jako varování a otevřou příslušný zdrojový řádek. Autoritativní validaci a exportní logiku stále provádí Therion kompilátor.

## 6. Vizuální editace map (`.th2`)

### 6.1 Režimy a inspektor

Režim `Vizuálně` obsahuje:

- mapové plátno,
- inspektor s přepínačem pohledů `Výběr`, `Objekty`, `Pozadí`, `Soubor`.

Režim `Zdroj` zůstává dostupný pro přímou editaci zdroje.

`Vizuálně` a `Zdroj` jsou dva pohledy na stejný `.th2` zdroj. Vizuální úpravy zapisují Therion příkazy zpět do souboru. Zdroj zůstává dostupný pro přímé editace a pokročilé konstrukce Therionu. Přepnutí režimu nevytváří druhý dokument.

Soubory mimo UTF-8 se otevírají s konkrétním zdrojovým kódováním, když ho lze rozpoznat, včetně běžných středoevropských legacy kódování jako ISO-8859-2. Při uložení Therion Studio zachová rozpoznané zdrojové kódování, pokud soubor výslovně nepřevedete na UTF-8 v inspektoru `Soubor`.

### 6.2 Navigace

Navigační ovládání použijte před kreslením nebo editací mapových objektů.

| Akce | Ovládání |
|---|---|
| Posun mapy | Podržte `Space` a táhněte levým tlačítkem myši, podržte `Ctrl` a táhněte levým tlačítkem myši, nebo táhněte pravým tlačítkem myši. Zařízení s přesným posouváním, například touchpady a Apple Magic Mouse, posouvají mapu dvouprstým nebo povrchovým posouváním. |
| Přiblížení / oddálení | Použijte tlačítka `Přiblížit` / `Oddálit` v liště, běžné neprecizní kolečko myši, nebo při posouvání držte `Command/Ctrl`. |
| Přizpůsobit geometrii mapy | `Přizpůsobit` zobrazí nakreslené mapové objekty ve viewportu. |
| Přizpůsobit geometrii mapy i pozadí | `Přizpůsobit včetně pozadí` zahrne do přizpůsobení i rastrové, SVG a `.xvi` vrstvy pozadí. |
| Posun posuvníky | Použijte vodorovný a svislý posuvník, pokud jsou viditelné. |

`Space` dočasně přepne mapové plátno na posun, dokud je držené, aniž by opustilo aktuální mapový nástroj. `Ctrl` + tažení levým tlačítkem a tažení pravým tlačítkem používají stejné chování posunu. Pravý klik, nebo `Ctrl` + levý klik bez tažení, otevře kontextové menu mapového objektu.

Therion Studio bere zařízení s přesným posouváním jako výchozí ovládání posunu, takže touchpady a zařízení jako Apple Magic Mouse umí posouvat mapu vodorovně i svisle. Běžné kolečko myši ve výchozím chování zoomuje.

### 6.3 Hlavní mapové nástroje

| Skupina | Akce |
|---|---|
| Navigace | `Přiblížit`, `Oddálit`, `Přizpůsobit`, `Přizpůsobit včetně pozadí` |
| Výběr a kreslení | `Vybrat`, `Dokončit návrh` |
| Vkládání | `Vložit scrap`, `Bod`, `Linie`, `Volná kresba`, `Plocha`, `Smart Area` |

Mapové plátno používá stabilní světlý „papírový“ povrch ve světlém i tmavém vzhledu aplikace. Lišty, záložky a inspektory následují systémový vzhled, ale rastry, `.xvi` reference a mapové symboly se pro tmavý režim netónují ani neinvertují.

### 6.4 Vkládání objektů

- `Bod`: klikněte jednou do mapy.
- `Linie`: klikejte vrcholy a dokončete `Enter` nebo `Dokončit návrh`.
- `Plocha`: klikejte vrcholy a dokončete `Enter` nebo `Dokončit návrh`.
- `Smart Area`: klikněte dovnitř uzavřené plochy vytvořené existujícími čarami ve stejném scrapu, zkontrolujte náhled, pomocí `[` / `]` přepněte alternativy, pokud odpovídá více ploch, a potvrďte `Enter` nebo `Dokončit návrh`.
- `Volná kresba`: stiskněte, táhněte a pusťte; vloží se zjednodušená Bezier čára.
- `Vložit scrap`: okamžitě vytvoří nový scrap, vybere ho ve `Výběru` i v `Objektech` a potom můžete upravit jeho ID/projekci před vkládáním bodů, linií, volné kresby nebo ploch.

Na podporovaných kreslicích tabletech a stylus zařízeních se klepnutí a tah stylusem přijímají jako primární vstup mapového plátna pro výběr objektů a vkládání bodů, vertexů linií, vertexů ploch a volných tahů. Myš a trackpad zůstávají dostupné bez změny nástroje.

Po spuštění `Bod`, `Linie`, `Volná kresba` nebo `Plocha` se aktivuje `Inspektor -> Výběr` ještě před prvním vložením. Nastavte tam typ, podtyp, ID, název bodu, text popisku nebo podporovanou hodnotu bodu ještě před potvrzením nového objektu. Pokud byl při spuštění nástroje vybraný scrap nebo objekt uvnitř scrapu, nový objekt se vloží do tohoto scrapu; metadata připraveného vložení ukazují ID cílového scrapu. Pomocí `Vložit do` můžete před potvrzením vybrat jiný existující cílový scrap.

Pokud `.th2` nemá XTherion metadata `xth_me_area_adjust`, první potvrzené vložení do mapy zapíše stabilní hlavičkové řádky `xth_me_area_adjust` a `xth_me_area_zoom_to`, aby se další kreslení nepřemapovalo po objevení nové geometrie.

Existující Therion bloky `area ... endarea`, které odkazují na hranice `line -id ...`, se vykreslují z čar ve stejném scrapu. Hraniční čáry mohou být otevřené; pokud jejich průsečíky tvoří uzavřenou plochu, Therion Studio ji vyplní bez změny zdrojového textu referencovaných čar.

`Smart Area` vytváří právě tuto referenční formu plochy místo kreslení nové hraniční geometrie. Při potvrzení může doplnit chybějící ID referencovaným hraničním čarám, aby na ně nový blok `area ... endarea` mohl odkazovat, ale nemění jejich geometrii. Po potvrzení se mapa vrátí do režimu výběru. `Esc` náhled zruší.

Během vkládání bodů nebo kreslení linií a ploch se vertexy blízkého objektu zvýrazní jako kandidáti snapu. Aktivní snap cíl se zvýrazní výrazněji; Bezier kontrolní body zůstávají bez snapu.

Když je aktivní map editor, levá část stavového řádku ukazuje aktuální pokyn workflow, včetně kroků kreslení a nápovědy pro dokončení Enter/Esc. Kompaktní odznak režimu mapy používá `M: Vybrat` nebo `M: Vložit`.

Během kreslení čáry nebo plochy:

- kliknutím přidáte rovný vrchol,
- stisknutím, tažením a puštěním při pokládání vrcholu vytáhnete dvojici Bezier kontrolních bodů tohoto vrcholu stejně jako v XTherionu,
- viditelné Bezier kontrolní body lze před dokončením tažením doladit,
- kliknutím na první vrchol čáry ji dokončíte jako uzavřenou (`-close on`); kliknutím na první vrchol plochy dokončíte návrh plochy,
- uzavřené čáry vykreslují poslední segment zpět na první vrchol, včetně dvoubodových uzavřených Bezier křivek,
- dvojklik při kreslení čáry přidá kliknutý vrchol, dokončí čáru a vrátí mapu do režimu výběru,
- `Backspace` nebo `Delete` odstraní poslední draft vrchol,
- `Esc` dokončí dostatečně úplný návrh čáry nebo plochy a vrátí mapu do režimu výběru; neúplné návrhy zruší.

### 6.5 Běžné mapové postupy

- Vytvoření nové mapy: vložte scrap, nastavte jeho ID/projekci ve `Výběru` a potom zvolte nástroj pro bod, linii, volnou kresbu, plochu nebo Smart Area.
- Kreslení stěny: zvolte `Linie`, podle potřeby nastavte typ `wall` před prvním vrcholem, klikejte vrcholy, tažením při pokládání vrcholu vytvořte Bezier kontrolní body a dokončete `Enter`.
- Vytvoření referencované plochy: zvolte `Smart Area`, klikněte dovnitř požadované uzavřené plochy, pomocí `[` / `]` vyberte kandidáta, pokud jich existuje víc, a potvrďte `Enter`.
- Úprava tvaru linie: přepněte na `Vybrat`, klikněte na linii, potom klikněte na vrchol nebo Bezier kontrolní bod a táhněte ho nebo upravte ovládání `Line Point`.
- Přidání kreslicí reference: otevřete `Pozadí`, přidejte rastr, `.xvi` nebo PocketTopo `.txt` a nastavte viditelnost, polohu, krytí nebo Gamma.

### 6.6 Změny zapisované do zdroje

- Nástroj Bod zapisuje příkazy `point ...` do cílového scrapu.
- Nástroje Linie a Volná kresba zapisují bloky `line ... endline`. Volná kresba se zjednoduší do Bezier souřadnicových řádků a u zakřivených nebo detailních tahů zachová poměrně více kotev.
- Ruční Plocha zapisuje vygenerovanou uzavřenou `line border -id ... -close on` a blok `area ... endarea`, který tuto hraniční linii referencuje.
- Smart Area zapisuje blok `area ... endarea`, který referencuje existující hraniční linie. Může doplnit chybějící hodnoty `-id` potřebné pro tyto reference, ale nemění geometrii existujících linií.
- Vložení pozadí zapisuje XTherion-kompatibilní metadata obrázku, například `##XTHERION## xth_me_image_insert`. První mapové vložení v souboru bez XTherion metadat pohledu může zapsat také `xth_me_area_adjust` a `xth_me_area_zoom_to`.

### 6.7 Editace geometrie

Vyberte mapový objekt nebo jeho vrchol/kontrolní bod na mapovém plátně. Inspektor `Výběr` zobrazí odpovídající ovládání včetně zdrojového řádku a ID obalujícího scrapu.

Výběr je sdílený mezi mapovým plátnem, panelem `Objekty` a inspektorem `Výběr`. Objekt pod kurzorem se zvýrazní azurově a vybraný objekt červeně. Výběr plochy zároveň zvýrazní referencované hraniční linie. Výběr vrcholu nebo Bezier kontrolního bodu linie přepne inspektor na odpovídající ovládání line pointu.

Pravým kliknutím bez tažení na mapový objekt nebo vrchol čáry otevřete kontextové menu s běžnými akcemi ve stylu XTherionu. Pravý klik do prázdného plátna toto menu neotevře. Menu zrcadlí dostupné skupiny inspektoru `Výběr`, například volby typu/podtypu, editovatelná pole objektu, `Geometry`, celý dostupný panel `Line Point`, `Line Point Actions` a `Object Actions`; volné textové nebo číselné editory se otevřou a fokusují v inspektoru. Na macOS otevře stejné menu i sekundární klik na trackpadu, například kliknutí dvěma prsty. Pokud je menu už otevřené, další sekundární klik na jiný objekt nebo vrchol přecílí menu na nový výběr a přesune ho na poslední pozici kliknutí.

Pro čáry a hranice ploch:

- vyberte vrchol pro editaci detailů line pointu,
- při tažení vertexu čáry nebo plochy poblíž jiného objektu se zvýrazní jeho vertexy jako kandidáti snapu; aktivní snap cíl se zvýrazní výrazněji a Bezier kontrolní body zůstávají bez snapu,
- pravým kliknutím na segment čáry a volbou `Insert Point Here` rozdělíte nejbližší segment v místě kliknutí,
- `Vložit před` / `Vložit za` přidá vrchol poblíž vybraného vrcholu,
- `Prodloužit před` / `Prodloužit za` na koncích čáry pokračuje v existující čáře,
- `Delete` / `Backspace` odstraní vybraný středový vrchol,
- `<<` a `>>` zapínají nebo odstraňují příchozí/odchozí Bezier kontrolní bod,
- Bezier kontrolní bod lze přímo táhnout na mapovém plátně.

Pokud čára slouží jako hranice plochy, některé destruktivní akce jsou zablokované; vyberte nebo upravte vlastní plochu. Smazání plochy odstraní pouze blok `area ... endarea` a ponechá referencované hraniční čáry ve zdroji.

### 6.8 Editace vlastností objektů

V `Inspektor -> Výběr` lze upravovat vlastnosti vybraných objektů `Scrap`, `Point`, `Line` a `Area`.

- `Scrap` zobrazuje ID/projekci a samostatnou sekci `Scrap Scale` pro XTherion/Therion kompatibilní kalibrační hodnoty `-scale [...]`.
- `Point`, `Line` a `Area` zobrazují běžná pole jako ID, typ, podtyp a podporované volby. Prázdná hodnota podtypu, případně `No subtype` v kontextovém menu, odstraní existující `-subtype`.
- `Upravit nastavení objektu...` otevře úplný catalog-backed editor voleb pro vybraný příkaz `scrap`, `point`, `line` nebo `area`. Poziční atributy jako `x`/`y` bodu, `type` čáry a `id` scrapu se zobrazují jako chráněné řádky atributů, zatímco `-id`, `-text`, `-orientation` a další volby zůstávají editovatelné jako řádky voleb.
- `point label` a `line label` mají pole `Text (-text)`. Point label se vykresluje u bodu; line label se vykresluje podél čáry, takže čára určuje délku a orientaci textu.
- podporované bodové typy, například `height`, `passage-height`, `altitude`, `dimensions` a `date`, mají pole `Value (-value)`. Hranaté Therion hodnoty jako `[fix 1300]` se zachovají.
- Bodové typy podporující `-orientation` zobrazují přepsání orientace a tažitelný orientační kontrolní bod. Jména stanic zůstávají kvůli čitelnosti vodorovná.
- Vybrané vertexy čáry zobrazují v `Line Point` podporované volby pro daný bod čáry. `Subtype` se zobrazí u typů čar se segmentovými podtypy a `Altitude (auto)` se zobrazí u bodů `line wall` a zapisuje `altitude .`.
- Vybrané vertexy čáry mají také editor `Additional line-point options` pro zbývající samostatné volby bodu čáry, například `altitude`, `subtype`, `direction` nebo `adjust`, bez přepnutí do Raw režimu. Řádky spravované viditelnými samostatnými ovládacími prvky jsou v tomto editoru skryté.
- Úpravy v `Additional line-point options` se použijí automaticky při opuštění pole, bez samostatných tlačítek Apply/Clear.
- Vertexy čáry s line-point řádky `altitude` nebo `subtype` zobrazují jemný metadatový kroužek i na nevybrané čáře a výraznější kroužek kolem aktivního úchopu. Tooltip vertexu dál ukazuje náhled všech dalších line-point řádků.

Řádek `Preview` ukazuje vzhled vybraného nebo připravovaného objektu. Náhled používá světlý mapový podklad i v tmavém režimu.

### 6.9 Objekty a Pozadí

V `Inspektor -> Objekty` lze vybírat objekty, měnit jejich pořadí přetažením, přepínat viditelnost v aktuálním pohledu a mazat objekty s potvrzením.

V `Inspektor -> Pozadí` lze:

- přidat, odebrat a řadit rastrové, SVG, `.xvi` nebo PocketTopo `.txt` vrstvy pozadí,
- zobrazit/skrýt jednotlivé vrstvy,
- měnit polohu a krytí vrstvy,
- upravit X/Y měřítko a rotaci vrstvy; `Lock proportions` ve výchozím stavu drží X a Y měřítko stejné,
- nastavit pivot rotace kliknutím na `Set Pivot` a potom kliknutím na požadovaný střed v mapě; u rastrových a SVG vrstev může být klik uvnitř i mimo viditelné pozadí, pivot vybrané vrstvy je v mapě zobrazený, když je aktivní `Pozadí`, a `Reset Pivot` obnoví výchozí pivot,
- upravit `Gamma` pro rastrové vrstvy (`.xvi` a SVG používají pevnou Gamma).

Rastrové vrstvy pozadí si zachovávají plné rozlišení obrázku, takže při přiblížení zůstávají ostré místo toho, aby se rozmazaly. Velmi velké skeny jsou omezené na vysokou interní zobrazovací velikost kvůli spotřebě paměti.

Therion Studio také čte a zapisuje Mapiah metadata pozadí `##MAPIAH## image_insert_v1` pro vrstvy `format=xvi`, `format=raster` a `format=svg`, včetně rotace, X/Y měřítka a pivotu. Díky tomu se v map editoru zobrazí rotované PocketTopo/XVI, rastrové nebo SVG reference vytvořené v Mapiah a Therion Studio může jednoduchou XTherion referenci povýšit na Mapiah metadata, když vrstvu otočíte, změníte měřítko nebo jí nastavíte pivot. SVG reference se vykreslují jako SVG vrstvy s použitím Mapiah metadat o vnitřní velikosti a zdrojovém viewBoxu. Při přidání SVG pozadí Therion Studio odvodí tato metadata z atributů `width`, `height` a `viewBox` kořenového SVG prvku.

Při přidání PocketTopo Therion exportu (`.txt`) jako mapového pozadí se Therion Studio zeptá na měřítko XVI, rozlišení, rozestup gridu a projekci plán nebo rozvinutý řez. Vedle PocketTopo exportu zapíše vygenerovaný soubor `_p.xvi` nebo `_e.xvi`, přidá toto `.xvi` jako vrstvu pozadí a uloží do `.th2` zdroje XTherion-kompatibilní metadata obrázku.

Therion Studio negeneruje samostatný metrický grid. Pro referenční mřížku použijte vrstvy pozadí, hlavně `.xvi`.

### 6.10 Oddělené mapové okno

`Oddělit mapu` přesune vizuální mapu do samostatného okna, například na druhý monitor. `Vrátit mapu` nebo zavření samostatného okna mapu vrátí zpět.

## 7. Spouštění Therionu

Panel `Kompilátor` otevřete v levé aktivitní liště. Popis panelu jej označuje jako místo pro spuštění Therionu pro aktuální projekt nebo aktivní config.

### 7.1 Hlavní pole

| Pole | Význam |
|---|---|
| `Argumenty` | Dodatečné argumenty pro aktuální relaci. |
| `Cíl spuštění` | `Aktuální konfigurace` nebo `Projektová konfigurace`. |
| `Cílová konfigurace` | Config soubor pro projektové spuštění. |
| `Náhradní pracovní složka` | Volitelné přepsání pracovní složky. |

Cestu ke spustitelnému souboru Therion nastavte v `Soubor -> Nastavení...`. Pokud není nastavena, aplikace zkusí `therion` a platformní autodetekci.

Před spuštěním kompilace Therion Studio uloží všechny změněné otevřené záložky. Pokud některý otevřený dokument nejde uložit, kompilace se zruší a runner se nespustí.

Vybraná cílová konfigurace se zobrazuje jako výrazný název souboru a pod ním plná cesta. Spuštění nové kompilace vyčistí předchozí výstup compileru před spuštěním Therionu a na začátek nového výstupu vypíše příkaz a pracovní složku.

Therion se ze Studia spouští neinteraktivně. Pokud Therion po chybě vypíše výzvu jako `Press ENTER to Exit!`, Studio zavře vstupní stream procesu, aby běh mohl skončit bez stisknutí `Zastavit`.

Zavření projektu vyčistí `Cílovou konfiguraci` a `Náhradní pracovní složku`, protože jsou projektové. Cesta ke spustitelnému souboru Therion je globální preference. Dodatečné argumenty jsou jen pro aktuální relaci.

### 7.2 Akce

- `Spustit Therion`
- `Zastavit`
- `Vymazat výstup`
- `Kopírovat výstup`

Stavový řádek ukazuje stav kompilace s kompaktním prefixem `C:`: `C: Nečinný`, `C: Běží...`, `C: OK` nebo `C: Selhal`.

## 8. Nastavení

Nastavení otevřete přes `Soubor -> Nastavení...` nebo `Nastavení...` v nativním macOS menu.

Nastavení obsahuje:

- jazyk aplikace (`Podle systému`, `Angličtina`, `Čeština`, `Slovenština`),
- cestu ke spustitelnému souboru Therion,
- výchozí editor pro nově otevřené `.th` a Therion config záložky (`Zdroj` nebo `Bloky`).

## 9. Klávesové zkratky

Používejte `Command` na macOS a `Ctrl` na Windows/Linux, pokud menu platformy nezobrazuje jinou nativní zkratku.

| Akce | Zkratka |
|---|---|
| Nové okno | `Command/Ctrl+N` |
| Otevřít projekt | `Command/Ctrl+O` |
| Uložit | `Command/Ctrl+S` |
| Uložit vše | zobrazena v menu `Soubor` |
| Zavřít všechny záložky | `Command/Ctrl+Shift+W` |
| Ukončit aplikaci | `Command/Ctrl+Q` |
| Zpět | `Command/Ctrl+Z` |
| Znovu | `Command/Ctrl+Shift+Z` nebo platformní výchozí |
| Najít | `Command/Ctrl+F` |
| Hledat v projektu | `Command/Ctrl+Shift+F` |
| Najít a nahradit | platformní výchozí zkratka pro replace |
| Zavřít panel hledání/nahrazení | `Esc`, když je panel hledání/nahrazení otevřený |
| Přepnout do editoru Zdroj (`Raw`) | `Command/Ctrl+horní 1` |
| Přepnout do editoru Bloky pro `.th` / config, nebo do editoru Vizuálně pro `.th2` | `Command/Ctrl+horní 2` |
| Ruční otevření doplňování (textový editor) | `Ctrl+Space` |
| Posun mapy | `Space` + tažení levým tlačítkem myši; `Ctrl` + tažení levým tlačítkem myši; tažení pravým tlačítkem myši; zařízení s přesným posouváním, například touchpad nebo Apple Magic Mouse |
| Zoom mapy | Tlačítka `Přiblížit` / `Oddálit`; běžné kolečko myši; `Command/Ctrl+posouvání` |
| Přizpůsobit geometrii mapy | Tlačítko `Přizpůsobit` |
| Přizpůsobit geometrii mapy i pozadí | Tlačítko `Přizpůsobit včetně pozadí` |
| Dokončit aktuální mapový návrh | `Enter` |
| Zrušit vkládání/kreslení v mapě | `Esc` |
| Smazat vybraný mapový objekt nebo vybraný line point; při kreslení smazat poslední bod návrhu | `Delete` / `Backspace` |

Mapové kreslení a source-edit operace uchovávají posledních 200 kroků zpět pro každou záložku mapového editoru.

## 10. Nápověda a O aplikaci

- `Nápověda -> Uživatelská příručka` otevře lokalizovaný manuál v aplikaci. Prohlížeč manuálu nechává vlevo viditelný obsah, podporuje odkazy z obsahu v tomto dokumentu a obsahuje hledání v manuálu s navigací na předchozí/další shodu. `Command/Ctrl+F` fokusuje hledání v manuálu.
- `Nápověda -> O aplikaci Therion Studio` zobrazí verzi, build label, verzi Qt, platformu, GitHub repozitář, licenci, maintainera a third-party notices. Na macOS je O aplikaci také v nativním aplikačním menu.

## 11. Řešení problémů

### 11.1 Spustitelný soubor Therion nebyl nalezen

Řešení:

- nastavte platnou úplnou cestu v `Soubor -> Nastavení... -> Spustitelný soubor Therion`,
- ověřte oprávnění ke spuštění.

### 11.2 Config nelze rozřešit

Řešení:

- nastavte `Cílovou konfiguraci`, nebo
- otevřete požadovaný Therion config a spusťte `Aktuální konfiguraci`, nebo
- ověřte pracovní složku / override cestu.

### 11.3 Přejmenování nebo mazání je blokované

Řešení:

- zavřete záložky, které odkazují na cílový soubor nebo složku, a akci zopakujte.

### 11.4 Pozadí mapy vypadá špatně

Řešení:

- vyberte vrstvu v `Pozadí`,
- ověřte viditelnost, polohu, krytí a Gamma,
- u `.xvi` ověřte, že `.xvi` a související `.th2` patří do stejného souřadnicového kontextu.
- Mapiah metadata rotovaného nebo měřeného pozadí jsou podporovaná pro `format=xvi`, `format=raster` a `format=svg`; SVG pozadí vyžaduje Mapiah metadata vnitřní velikosti a zdrojového viewBoxu,
- pokud rastrová vrstva při přiblížení pořád vypadá měkce, dosáhla rozlišení zdrojového obrázku; pro více detailů použijte sken ve vyšším rozlišení.

### 11.5 Mapu lze zoomovat, ale nejde posouvat

Řešení:

- podržte `Space` a táhněte levým tlačítkem myši,
- podržte `Ctrl` a táhněte levým tlačítkem myši,
- táhněte pravým tlačítkem myši nebo použijte posouvání na touchpadu / přesném polohovacím zařízení,
- pokud přesné zařízení právě zoomuje, podržte při posouvání `Command/Ctrl`,
- použijte vodorovný a svislý posuvník, pokud jsou viditelné.

### 11.6 Plocha není vidět

Řešení:

- ověřte, že blok `area ... endarea` je ve stejném `scrap` jako referencované hranice `line -id ...`,
- zkontrolujte, že každé referencované ID existuje a je v daném scrapu jedinečné,
- u otevřených hraničních linií ověřte, že jejich průsečíky tvoří uzavřenou plochu,
- zkontrolujte viditelnost vrstev a objektů v `Objekty`.

### 11.7 Smart Area nenajde kandidáta

Řešení:

- klikněte dovnitř plochy ohraničené liniemi ve stejném scrapu,
- přibližte mapu a klikněte dál od průsečíků nebo nejednoznačných hran,
- ověřte, že zamýšlené hraniční linie se protínají nebo navazují dostatečně blízko na vytvoření uzavřené plochy,
- pokud se zobrazí více kandidátů, před potvrzením použijte `[` / `]`.

### 11.8 Linii nejde smazat

Řešení:

- pokud je linie referencovaná plochou, nejdřív smažte nebo upravte plochu,
- smazání plochy odstraní pouze blok `area ... endarea` a ponechá referencované hraniční linie.

### 11.9 Projektové hledání otevřelo `.th2` výsledek ve Zdroj

Řešení:

- je to očekávané chování pro textové výsledky hledání, aby byl vidět odpovídající zdrojový řádek,
- přepínačem režimu editoru nebo `Command/Ctrl+horní 2` se vraťte do vizuálního mapového editoru.
