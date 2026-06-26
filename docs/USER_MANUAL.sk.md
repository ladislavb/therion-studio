# Používateľská príručka Therion Studio

Aktualizované: 2026-06-25

Táto príručka opisuje bežnú prácu v Therion Studio. Zámerne nie je úplnou referenciou jazyka Therion. Syntax Therionu, názvy príkazov, voľby a obsah ukladaný do súborov zostávajú v kanonickej podobe Therionu.

Aplikácia používa jazyk operačného systému, ak je dostupný vstavaný preklad. Súčasťou sú preklady do angličtiny, češtiny a slovenčiny. Jazyk možno prepísať v `Súbor -> Nastavenia...` (`Nastavenia...` v natívnom macOS aplikačnom menu). Zmena jazyka sa prejaví po reštarte aplikácie.

## Obsah

- [1. Čo je Therion Studio](#1-čo-je-therion-studio)
- [2. Hlavné okno](#2-hlavné-okno)
- [3. Začíname](#3-začíname)
- [4. Textový editor](#4-textový-editor)
- [5. Navigácia v projekte a práca so súbormi](#5-navigácia-v-projekte-a-práca-so-súbormi)
- [6. Vizuálna editácia máp (`.th2`)](#6-vizuálna-editácia-máp-th2)
- [7. Spúšťanie Therionu](#7-spúšťanie-therionu)
- [8. Nastavenia](#8-nastavenia)
- [9. Klávesové skratky](#9-klávesové-skratky)
- [10. Pomocník a O aplikácii](#10-pomocník-a-o-aplikácii)
- [11. Riešenie problémov](#11-riešenie-problémov)

## 1. Čo je Therion Studio

Therion Studio je desktopový editor pre projekty Therionu. Poskytuje:

- prehliadanie projektových súborov,
- textový editor pre `.th`, `.th2` a Therion config súbory,
- navigáciu v štruktúre `survey`, `centerline`, `map` a `scrap`,
- vizuálny editor máp pre `.th2`,
- integrovanú konzolu na spúšťanie Therionu.

Therion Studio neobsahuje externý Therion kompilátor. Therion nainštalujte samostatne a podľa potreby nastavte cestu k executable v Nastaveniach.

### 1.1 Terminológia

- `Projekt`: priečinok, ktorý Therion Studio práve prehliada; obvykle obsahuje Therion zdrojové súbory, configy, pozadia a podpriečinky.
- `Config`: Therion súbor na spracovanie, napríklad `thconfig`, `thconfig.*` alebo `*.thconfig`.
- `Survey`: štruktúra merania Therionu zo zdrojových `.th` súborov.
- `Mapa`: Therion objekt `map ... endmap`, ktorý referencuje scrapy.
- `Scrap`: jeden kreslený fragment mapy v `.th2` súbore.
- `Bod`, `Línia`, `Plocha`: mapové objekty uložené v scrape.
- `Pozadie`: rastrový obrázok, `.xvi` alebo PocketTopo textový export používaný ako kresliaca referencia.
- `.xvi`: vektorový referenčný formát Therionu, často generovaný z PocketTopo dát.

## 2. Hlavné okno

Hlavné okno obsahuje:

- menu (`Súbor`, `Úpravy`, `Zobrazenie`, `Pomocník`),
- hornú lištu s projektovými, ukladacími, editorovými a mapovými akciami,
- ľavý aktivitný panel (`Súbory`, `Štruktúra`, `Kompilátor`) a rýchlu akciu kompilácie,
- centrálnu oblasť so záložkami dokumentov,
- stavový riadok s pokynmi mapy, stavom kompilácie, kódovaním a stavom mapy.

Bežné akcie okna:

- `Súbor -> Nové okno` otvorí nové prázdne okno bez kopírovania aktuálneho projektu a otvorených dokumentov.
- `Súbor -> Nový -> Therion zdroj (.th)`, `Therion mapa (.th2)` alebo `Therion config (.thconfig)` otvorí nový neuložený dokument. Nové dokumenty `.th`, `.th2` a `.thconfig` začínajú direktívou `encoding utf-8`. Tlačidlo `Nový dokument` v toolbare ponúka rovnaké voľby. Prvé `Uložiť` sa spýta, kam ho uložiť.
- `Súbor -> Nastavenia...` otvorí nastavenia aplikácie.
- `Zobrazenie -> Rozbaliť bočný panel` / `Zbaliť bočný panel` zobrazí alebo skryje obsah ľavého panelu.
- `Zobrazenie -> Rozbaliť nápovedu`, `Rozbaliť inšpektor bloku` alebo `Rozbaliť inšpektor mapy` ovláda aktívny pravý panel podľa aktuálneho editora.
- `Zobrazenie -> Zobraziť mapovú lupu` / `Skryť mapovú lupu` zapína alebo vypína lupu mapy. Je to iba stav UI a nemení `.th2` súbor.
- `Zobrazenie -> Celá obrazovka` / `Ukončiť celú obrazovku` prepína celú obrazovku.

Ak je mapa oddelená do samostatného okna, hlavné okno môže zároveň zobraziť ovládanie `Inšpektor mapy` aj `Nápoveda`, pretože vizuálna mapa a zdroj môžu byť viditeľné naraz.

## 3. Začíname

### 3.1 Rýchly štart

1. Otvorte priečinok projektu cez `Súbor -> Otvoriť projekt...`; vyberte priečinok, ktorý obsahuje Therion súbory.
2. V paneli `Kompilátor` zvoľte `Cieľovú konfiguráciu` (`thconfig`, `thconfig.*` alebo `*.thconfig`), ktorá sa má použiť pre projektové spustenie. Ak začínate od nuly, vytvorte config súbor a aspoň jeden `.th` zdroj cez `Súbor -> Nový`.
3. Otvorte `.th` zdrojové súbory, ktoré definujú survey, centerline dáta, mapy a referencie na súbory. Režim `Zdroj` použite na priamu editáciu Therion zdroja, režim `Bloky` na podporované štruktúrované úpravy.
4. Otvorte existujúcu `.th2` mapu z panelu `Súbory`, alebo vytvorte novú cez `Súbor -> Nový -> Therion mapa (.th2)`. Na mapovú editáciu používajte `Vizuálne`, na priame úpravy `.th2` zdroja `Zdroj`.
5. V map editore vložte scrap pred pridávaním bodov, línií alebo plôch, ak `.th2` súbor ešte žiadny scrap nemá. Kreslite mapové objekty alebo použite `Smart Area` na vytvorenie referencovanej Therion plochy z existujúcich hraníc.
6. Overte, že config/source súbory referencujú survey a mapové súbory potrebné na kompiláciu bežnou syntaxou Therionu.
7. Dokumenty uložte a potom spustite Therion v paneli `Kompilátor`, aby ste projekt skontrolovali alebo exportovali.

### 3.2 Otvorenie projektu

1. Zvoľte `Súbor -> Otvoriť projekt...`.
2. Vyberte priečinok, ktorý obsahuje Therion zdrojové súbory, configy, pozadia a podpriečinky projektu.
3. Otvorte dokumenty z panelu `Súbory`.

Dialóg na výber priečinka projektu začína v domovskom priečinku používateľa, ak ho otvoríte cez `Otvoriť projekt...`.

Bez otvoreného projektu sa zobrazí uvítacia karta s tlačidlom `Otvoriť projekt...` a panel `Súbory` zobrazí prázdny stav s rovnakou akciou na otvorenie projektu namiesto prehliadania filesystemu počítača. Ak je projekt otvorený, ale nie je aktívna žiadna záložka, uvítacia karta ponúkne otvorenie súboru z bočného panelu.

Uvítacia karta a `Súbor -> Posledné projekty` zobrazujú najviac päť naposledy otvorených projektov. Výberom projektu z ktoréhokoľvek zoznamu ho znovu otvoríte.

Ak je projekt otvorený, uvítacia karta zobrazuje názov a cestu aktívneho projektu. Zobrazuje tiež najviac desať posledných súborov z tohto projektu; výberom súbor znovu otvoríte. Rovnaký projektový zoznam je dostupný cez `Súbor -> Posledné súbory`.

### 3.3 Otvorenie dokumentov

- Dvojklikom otvoríte súbor v paneli `Súbory`.
- `.th2` súbory sa otvárajú v map editore.
- `.th`, `thconfig`, `*.thconfig`, `thconfig.*`, `.log`, `.txt` a bežné textové súbory sa otvárajú v textovom editore.
- Nepodporované súbory, napríklad obrázky alebo PDF, zobrazia hlásenie `Nepodporovaný súbor` a akciu `Otvoriť v externej aplikácii`.

### 3.4 Vytváranie a správa súborov

Cez `Súbor -> Nový` vytvoríte neuložený `.th`, `.th2` alebo `.thconfig` dokument a cestu zvolíte pri prvom uložení. Pravým kliknutím v paneli `Súbory` možno vytvárať priečinky, vytvárať uložené `.th`, `.th2` a `.thconfig` súbory priamo v projekte, premenovať položky, duplikovať súbory, mazať položky alebo otvoriť `.th2` priamo v map editore.

Premenovanie je blokované, ak je cieľový súbor alebo priečinok otvorený v záložke. Mazanie priečinka je tiež blokované, kým sú otvorené súvisiace záložky. Zmazanie jedného otvoreného súboru sa po potvrdení pokúsi zavrieť jeho záložku, prípadne zobrazí výzvu k neuloženým zmenám, a potom súbor zmaže.

### 3.5 Uloženie zmien

- `Súbor -> Uložiť` uloží aktívnu záložku.
- Ak aktívna záložka ešte nebola uložená, `Súbor -> Uložiť` otvorí `Uložiť ako`.
- `Súbor -> Uložiť všetko` uloží všetky zmenené záložky.
- Zatvorenie zmenenej záložky sa spýta na uloženie, zahodenie zmien alebo zrušenie.

## 4. Textový editor

### 4.1 Režimy Zdroj a Bloky

Pre `.th` a Therion config súbory:

| Režim | Použitie |
|---|---|
| `Zdroj` (`Raw`) | Priama editácia zdroja so zvýraznením syntaxe, hľadaním, nahradením, dopĺňaním a kontextovou nápovedou. |
| `Bloky` (`Blocks`) | Štruktúrovaná editácia podporovaných príkazov a blokov s rovnakými metadátami nápovedy príkazov ako režim Zdroj. |

Nové `.th` a Therion config záložky sa otvárajú vo východiskovom editore vybranom v Nastaveniach. Východiskový je `Zdroj`. Prepnutie do režimu `Bloky` nevloží chýbajúci `encoding` a nijako neprepíše zdroj, kým neurobíte explicitnú editáciu.

Toolbox v režime `Bloky` je filtrovaný podľa typu dokumentu:

| Dokument | Obsah toolboxu |
|---|---|
| `.th` | Príkazy z kapitoly Therion Booku `Creating data files`. `.th2` mapové príkazy `scrap`, `point`, `line` a `area` sú skryté, pretože sa editujú v map editore. |
| `thconfig`, `thconfig.*`, `*.thconfig` | Príkazy z kapitoly `Processing data`, napríklad `source`, `select`, `export`. |

Pre `.th2` súbory:

| Režim | Použitie |
|---|---|
| `Zdroj` (`Raw`) | Priama textová editácia `.th2` zdroja. |
| `Vizuálne` (`Visual`) | Vizuálna editácia mapy s mapovým plátnom a inšpektorom. |

### 4.2 Funkcie textového editora

- čísla riadkov a zvýraznenie aktívneho riadka,
- dopĺňanie príkazov, volieb, hodnôt a ciest pri písaní,
- `Ctrl+Space` na ručné otvorenie dopĺňania,
- kontextová nápoveda pre aktuálny príkaz alebo voľbu; režimy Zdroj aj Bloky zobrazujú rovnakú úplnú nápovedu príkazov a panel nápovedy je pomenovaný podľa aktuálneho príkazu, validačného kontextu alebo vybraného cieľa nápovedy,
- diagnostika v raw editore a paneli `Validation` zobrazuje problémy ako chybne zapísané option tokeny, neznáme príkazy alebo voľby, chýbajúce argumenty, neuzavreté bloky a prázdne `line`/`area` objekty vnútri scrapov; `Validate Project` v paneli `Validation` ručne obnoví kontrolu všetkých `.th`, `.th2` a `thconfig` súborov v otvorenom projekte; projektová validácia pri `thconfig` súboroch nehlási neisté varovania `unknown command`, kým nebude k dispozícii samostatný thconfig katalóg; aktiváciou nálezu prejdete na dotknutý riadok raw zdroja a pri nálezoch s explicitným bezpečným prepisom môžete použiť `Apply Fix`, ktorý pri prázdnych scrap objektoch ukáže odstraňovaný zdrojový blok, odstráni len tento vybraný blok a nikdy sa nespustí potichu,
- pohľad `Výber` v inšpektore režimu Bloky na editáciu hlavičky vybraného bloku a podporovaných inline volieb; prvý panel je pomenovaný podľa vybraného príkazu Therionu a ukazuje zdrojový riadok,
- keď nie je vybraný žiadny blok, pohľad `Výber` v režime Bloky ukazuje `Nie je vybraný žiadny blok.`; pri výbere pevnej koreňovej karty `encoding` ukazuje príkaz a hodnotu kódovania ako text iba na čítanie,
- hľadanie a nahradenie z menu `Úpravy`,
- `Súbor -> Import -> Importovať PocketTopo text...` sa zobrazí iba vtedy, keď je aktívny existujúci alebo neuložený `.th` textový dokument, a importuje PocketTopo Therion export (`.txt`) na pozíciu kurzora ako Therion bloky `centreline`,
- pohľad `Súbor` v inšpektore s panelom pomenovaným podľa aktuálneho súboru, plnou cestou, akciou na skopírovanie cesty, veľkosťou na disku, časom poslednej zmeny, aktuálnym kódovaním a prevodom do UTF-8 pre súbory mimo UTF-8.

### 4.3 Dátové riadky v režime Bloky

V režime `Bloky` možno bloky `data ...` upravovať tabuľkou podľa aktívnej hlavičky dát. Prázdne riadky v tele dát sa pri otvorení tabuľky ignorujú, takže medzery v zdroji nevzniknú ako falošné merania.

## 5. Navigácia v projekte a práca so súbormi

### 5.1 Projektové hľadanie

Otvorte aktivitu `Hľadanie` v ľavom raili alebo stlačte `Command/Ctrl+Shift+F`. Zadajte doslovný text, podľa potreby zvoľte `Celé slovo` alebo `Rozlišovať veľkosť`, a stlačením `Enter` alebo `Hľadať` prehľadajte aktuálny projekt.

Projektové hľadanie prehľadáva Therion textové zdroje (`.th`, `.th2` a konfiguračné súbory Therionu), zahŕňa neuložené zmeny v otvorených záložkách a vypisuje zhody zoskupené podľa súboru s riadkom a stĺpcom. Dvojklikom na súbor alebo riadok zhody otvoríte súbor na zodpovedajúcom texte s pripraveným inline hľadaním pre ďalšiu/predchádzajúcu zhodu. Zhody v `.th2` sa otvárajú ako dokumenty mapového editora s aktívnym workspace Zdroj (`Raw`), takže sa potom môžete vrátiť späť do vizuálneho mapového editora.

### 5.2 Panel Štruktúra

Panel `Štruktúra` je ľahký navigačný index otvoreného projektu. Popis v bočnom paneli ho označuje ako štruktúru `survey`, `map` a `scrap` aktuálneho projektu. Zobrazuje hierarchiu `survey`, `centerline`, `map` a `scrap` a rozpoznáva obe Therion varianty: `centreline` aj `centerline`.

Kliknutím riadok iba vyberiete v strome. Dvojklikom na zdrojový riadok, alebo jeho výberom a stlačením `Enter`, otvoríte zdrojový dokument a prejdete na zodpovedajúci riadok.

Pohľady `Survey` a `Mapa` si pri otvorenom rovnakom projekte pamätajú svoj stav rozbalenia/zbalenia oddelene.

Index používa vybranú `Cieľovú konfiguráciu`, ak ukazuje do otvoreného projektu. Bez explicitného configu skúsi koreňový `thconfig`; ak neexistuje a v koreni je práve jeden pomenovaný config (`*.thconfig` alebo `thconfig.*`), použije sa ten. Ak je možných configov viac, vyberte požadovanú `Cieľovú konfiguráciu` v paneli `Kompilátor`.

Mapy a scrapy referencované vnútri `map ... endmap` sa zobrazia pod danou mapou, ak je referencia jednoznačná. Nerozriešené alebo nejednoznačné referencie sa zobrazia ako varovanie a otvoria príslušný zdrojový riadok. Autoritatívnu validáciu a exportnú logiku stále vykonáva Therion kompilátor.

## 6. Vizuálna editácia máp (`.th2`)

### 6.1 Režimy a inšpektor

Režim `Vizuálne` obsahuje:

- mapové plátno,
- inšpektor s prepínačom pohľadov `Výber`, `Objekty`, `Pozadia`, `Súbor`.

Režim `Zdroj` zostáva dostupný pre priamu editáciu zdroja.

`Vizuálne` a `Zdroj` sú dva pohľady na ten istý `.th2` zdroj. Vizuálne úpravy zapisujú Therion príkazy späť do súboru. Zdroj zostáva dostupný pre priame editácie a pokročilé konštrukcie Therionu. Prepnutie režimu nevytvára druhý dokument.

Súbory mimo UTF-8 sa otvárajú s konkrétnym zdrojovým kódovaním, keď ho možno rozpoznať, vrátane bežných stredoeurópskych legacy kódovaní ako ISO-8859-2. Pri uložení Therion Studio zachová rozpoznané zdrojové kódovanie, ak súbor výslovne neprevediete na UTF-8 v inšpektore `Súbor`.

### 6.2 Navigácia

Navigačné ovládanie použite pred kreslením alebo editáciou mapových objektov.

| Akcia | Ovládanie |
|---|---|
| Posun mapy | Podržte `Space` a ťahajte ľavým tlačidlom myši, podržte `Ctrl` a ťahajte ľavým tlačidlom myši, alebo ťahajte pravým tlačidlom myši. Zariadenia s presným posúvaním, napríklad touchpady a Apple Magic Mouse, posúvajú mapu dvojprstovým alebo povrchovým posúvaním. |
| Priblíženie / oddialenie | Použite tlačidlá `Priblížiť` / `Oddialiť` v lište, bežné neprecízne koliesko myši, alebo pri posúvaní držte `Command/Ctrl`. |
| Prispôsobiť geometriu mapy | `Prispôsobiť` zobrazí nakreslené mapové objekty vo viewporte. |
| Prispôsobiť geometriu mapy aj pozadie | `Prispôsobiť vrátane pozadia` zahrnie do prispôsobenia aj rastrové, SVG a `.xvi` vrstvy pozadia. |
| Posun posuvníkmi | Použite vodorovný a zvislý posuvník, ak sú viditeľné. |

`Space` dočasne prepne mapové plátno na posun, kým je držané, bez opustenia aktuálneho mapového nástroja. `Ctrl` + ťahanie ľavým tlačidlom a ťahanie pravým tlačidlom používajú rovnaké správanie posunu. Pravý klik, alebo `Ctrl` + ľavý klik bez ťahania, otvorí kontextové menu mapového objektu.

Therion Studio berie zariadenia s presným posúvaním ako východiskové ovládanie posunu, takže touchpady a zariadenia ako Apple Magic Mouse vedia posúvať mapu vodorovne aj zvislo. Bežné koliesko myši vo východiskovom správaní zoomuje.

### 6.3 Hlavné mapové nástroje

| Skupina | Akcie |
|---|---|
| Navigácia | `Priblížiť`, `Oddialiť`, `Prispôsobiť`, `Prispôsobiť vrátane pozadia` |
| Výber a kreslenie | `Vybrať`, `Dokončiť návrh` |
| Vkladanie | `Vložiť scrap`, `Bod`, `Línia`, `Voľná kresba`, `Plocha`, `Smart Area` |

Mapové plátno používa stabilný svetlý „papierový“ povrch vo svetlom aj tmavom vzhľade aplikácie. Lišty, záložky a inšpektory nasledujú systémový vzhľad, ale rastry, `.xvi` referencie a mapové symboly sa pre tmavý režim netónujú ani neinvertujú.

### 6.4 Vkladanie objektov

- `Bod`: kliknite raz do mapy.
- `Línia`: klikajte vrcholy a dokončite `Enter` alebo `Dokončiť návrh`.
- `Plocha`: klikajte vrcholy a dokončite `Enter` alebo `Dokončiť návrh`.
- `Smart Area`: kliknite dovnútra uzavretej plochy vytvorenej existujúcimi čiarami v tom istom scrape, skontrolujte náhľad, pomocou `[` / `]` prepnite alternatívy, ak zodpovedá viac plôch, a potvrďte `Enter` alebo `Dokončiť návrh`.
- `Voľná kresba`: stlačte, ťahajte a pustite; vloží sa zjednodušená Bezier čiara.
- `Vložiť scrap`: okamžite vytvorí nový scrap, vyberie ho vo `Výbere` aj v `Objektoch` a potom môžete upraviť jeho ID/projekciu pred vkladaním bodov, línií, voľnej kresby alebo plôch.

Na podporovaných kresliacich tabletoch a stylus zariadeniach sa klepnutie a ťah stylusom prijímajú ako primárny vstup mapového plátna na výber objektov a vkladanie bodov, vertexov línií, vertexov plôch a voľných ťahov. Myš a trackpad zostávajú dostupné bez zmeny nástroja.

Po spustení `Bod`, `Línia`, `Voľná kresba` alebo `Plocha` sa aktivuje `Inšpektor -> Výber` ešte pred prvým vložením. Nastavte tam typ, podtyp, ID, názov bodu, text popisku alebo podporovanú hodnotu bodu ešte pred potvrdením nového objektu. Ak bol pri spustení nástroja vybraný scrap alebo objekt vnútri scrapu, nový objekt sa vloží do tohto scrapu; metadata pripraveného vloženia ukazujú ID cieľového scrapu. Pomocou `Vložiť do` môžete pred potvrdením vybrať iný existujúci cieľový scrap.

Ak `.th2` nemá XTherion metadata `xth_me_area_adjust`, prvé potvrdené vloženie do mapy zapíše stabilné hlavičkové riadky `xth_me_area_adjust` a `xth_me_area_zoom_to`, aby sa ďalšie kreslenie neprepočítalo po objavení novej geometrie.

Existujúce Therion bloky `area ... endarea`, ktoré odkazujú na hranice `line -id ...`, sa vykresľujú z čiar v tom istom scrape. Hraničné čiary môžu byť otvorené; ak ich priesečníky tvoria uzavretú plochu, Therion Studio ju vyplní bez zmeny zdrojového textu referencovaných čiar.

`Smart Area` vytvára práve túto referenčnú formu plochy namiesto kreslenia novej hraničnej geometrie. Pri potvrdení môže doplniť chýbajúce ID referencovaným hraničným čiaram, aby na ne nový blok `area ... endarea` mohol odkazovať, ale nemení ich geometriu. Po potvrdení sa mapa vráti do režimu výberu. `Esc` náhľad zruší.

Počas vkladania bodov alebo kreslenia čiar a plôch sa vertexy blízkeho objektu zvýraznia ako kandidáti snapu. Aktívny snap cieľ sa zvýrazní výraznejšie; Bezier kontrolné body zostávajú bez snapu.

Keď je aktívny map editor, ľavá časť stavového riadka zobrazuje aktuálny pokyn workflow vrátane krokov kreslenia a pomôcky pre dokončenie Enter/Esc. Kompaktný odznak režimu mapy používa `M: Vybrať` alebo `M: Vložiť`.

Počas kreslenia čiary alebo plochy:

- kliknutím pridáte rovný vrchol,
- stlačením, ťahaním a pustením pri pokladaní vrcholu vytiahnete dvojicu Bezier kontrolných bodov tohto vrcholu rovnako ako v XTherione,
- viditeľné Bezier kontrolné body možno pred dokončením ťahaním doladiť,
- kliknutím na prvý vrchol čiary ju dokončíte ako uzavretú (`-close on`); kliknutím na prvý vrchol plochy dokončíte návrh plochy,
- uzavreté čiary vykresľujú posledný segment späť na prvý vrchol, vrátane dvojbodových uzavretých Bezier kriviek,
- dvojklik počas kreslenia čiary pridá kliknutý vrchol, dokončí čiaru a vráti mapu do režimu výberu,
- `Backspace` alebo `Delete` odstráni posledný draft vrchol,
- `Esc` dokončí dostatočne úplný návrh čiary alebo plochy a vráti mapu do režimu výberu; neúplné návrhy zruší.

### 6.5 Bežné mapové postupy

- Vytvorenie novej mapy: vložte scrap, nastavte jeho ID/projekciu vo `Výbere` a potom zvoľte nástroj pre bod, líniu, voľnú kresbu, plochu alebo Smart Area.
- Kreslenie steny: zvoľte `Línia`, podľa potreby nastavte typ `wall` pred prvým vrcholom, klikajte vrcholy, ťahaním pri pokladaní vrcholu vytvorte Bezier kontrolné body a dokončite `Enter`.
- Vytvorenie referencovanej plochy: zvoľte `Smart Area`, kliknite dovnútra požadovanej uzavretej plochy, pomocou `[` / `]` vyberte kandidáta, ak ich existuje viac, a potvrďte `Enter`.
- Úprava tvaru línie: prepnite na `Vybrať`, kliknite na líniu, potom kliknite na vrchol alebo Bezier kontrolný bod a ťahajte ho alebo upravte ovládanie `Line Point`.
- Pridanie kresliacej referencie: otvorte `Pozadia`, pridajte raster, `.xvi` alebo PocketTopo `.txt` a nastavte viditeľnosť, polohu, krytie alebo Gamma.

### 6.6 Zmeny zapisované do zdroja

- Nástroj Bod zapisuje príkazy `point ...` do cieľového scrapu.
- Nástroje Línia a Voľná kresba zapisujú bloky `line ... endline`. Voľná kresba sa zjednoduší do Bezier súradnicových riadkov a pri zakrivených alebo detailných ťahoch zachová pomerne viac kotiev.
- Ručná Plocha zapisuje vygenerovanú uzavretú `line border -id ... -close on` a blok `area ... endarea`, ktorý túto hraničnú líniu referencuje.
- Smart Area zapisuje blok `area ... endarea`, ktorý referencuje existujúce hraničné línie. Môže doplniť chýbajúce hodnoty `-id` potrebné pre tieto referencie, ale nemení geometriu existujúcich línií.
- Vloženie pozadia zapisuje XTherion-kompatibilné metadáta obrázka, napríklad `##XTHERION## xth_me_image_insert`. Prvé mapové vloženie v súbore bez XTherion metadát pohľadu môže zapísať aj `xth_me_area_adjust` a `xth_me_area_zoom_to`.

### 6.7 Editácia geometrie

Vyberte mapový objekt alebo jeho vrchol/kontrolný bod na mapovom plátne. Inšpektor `Výber` zobrazí zodpovedajúce ovládanie vrátane zdrojového riadka a ID obklopujúceho scrapu.

Výber je zdieľaný medzi mapovým plátnom, panelom `Objekty` a inšpektorom `Výber`. Objekt pod kurzorom sa zvýrazní azúrovo a vybraný objekt červeno. Výber plochy zároveň zvýrazní referencované hraničné línie. Výber vrcholu alebo Bezier kontrolného bodu línie prepne inšpektor na zodpovedajúce ovládanie line pointu.

Pravým kliknutím bez ťahania na mapový objekt alebo vrchol čiary otvoríte kontextové menu s bežnými akciami v štýle XTherionu. Pravý klik do prázdneho plátna toto menu neotvorí. Menu zrkadlí dostupné skupiny inšpektora `Výber`, napríklad voľby typu/podtypu, editovateľné polia objektu, `Geometry`, celý dostupný panel `Line Point`, `Line Point Actions` a `Object Actions`; voľné textové alebo číselné editory sa otvoria a fokusujú v inšpektore. Na macOS otvorí rovnaké menu aj sekundárny klik na trackpade, napríklad kliknutie dvoma prstami. Ak je menu už otvorené, ďalší sekundárny klik na iný objekt alebo vrchol precieli menu na nový výber a presunie ho na poslednú pozíciu kliknutia.

Pre čiary a hranice plôch:

- vyberte vrchol na editáciu detailov line pointu,
- pri ťahaní vertexu čiary alebo plochy poblíž iného objektu sa zvýraznia jeho vertexy ako kandidáti snapu; aktívny snap cieľ sa zvýrazní výraznejšie a Bezier kontrolné body zostávajú bez snapu,
- pravým kliknutím na segment čiary a voľbou `Insert Point Here` rozdelíte najbližší segment v mieste kliknutia,
- `Vložiť pred` / `Vložiť za` pridá vrchol poblíž vybraného vrcholu,
- `Predĺžiť pred` / `Predĺžiť za` na koncoch čiary pokračuje v existujúcej čiare,
- `Delete` / `Backspace` odstráni vybraný stredový vrchol,
- `<<` a `>>` zapínajú alebo odstraňujú prichádzajúci/odchádzajúci Bezier kontrolný bod,
- Bezier kontrolný bod možno priamo ťahať na mapovom plátne.

Ak čiara slúži ako hranica plochy, niektoré deštruktívne akcie sú zablokované; vyberte alebo upravte vlastnú plochu. Zmazanie plochy odstráni iba blok `area ... endarea` a ponechá referencované hraničné čiary v zdroji.

### 6.8 Editácia vlastností objektov

V `Inšpektor -> Výber` možno upravovať vlastnosti vybraných objektov `Scrap`, `Point`, `Line` a `Area`.

- `Scrap` zobrazuje ID/projekciu a samostatnú sekciu `Scrap Scale` pre XTherion/Therion kompatibilné kalibračné hodnoty `-scale [...]`.
- `Point`, `Line` a `Area` zobrazujú bežné polia ako ID, typ, podtyp a podporované voľby. Prázdna hodnota podtypu, prípadne `No subtype` v kontextovom menu, odstráni existujúci `-subtype`.
- `Upraviť nastavenia objektu...` otvorí úplný catalog-backed editor volieb pre vybraný príkaz `scrap`, `point`, `line` alebo `area`. Pozičné atribúty ako `x`/`y` bodu, `type` čiary a `id` scrapu sa zobrazujú ako chránené riadky atribútov, zatiaľ čo `-id`, `-text`, `-orientation` a ďalšie voľby zostávajú editovateľné ako riadky volieb.
- `point label` a `line label` majú pole `Text (-text)`. Point label sa vykresľuje pri bode; line label sa vykresľuje pozdĺž čiary, takže čiara určuje dĺžku a orientáciu textu.
- podporované bodové typy, napríklad `height`, `passage-height`, `altitude`, `dimensions` a `date`, majú pole `Value (-value)`. Hranaté Therion hodnoty ako `[fix 1300]` sa zachovajú.
- Bodové typy podporujúce `-orientation` zobrazujú prepísanie orientácie a ťahateľný orientačný kontrolný bod. Mená staníc zostávajú pre čitateľnosť vodorovné.
- Vybrané vertexy čiary zobrazujú v `Line Point` podporované voľby pre daný bod čiary. `Subtype` sa zobrazí pri typoch čiar so segmentovými podtypmi a `Altitude (auto)` sa zobrazí pri bodoch `line wall` a zapisuje `altitude .`.
- Vybrané vertexy čiary majú aj editor `Additional line-point options` pre zostávajúce samostatné voľby bodu čiary, napríklad `altitude`, `subtype`, `direction` alebo `adjust`, bez prepnutia do Raw režimu. Riadky spravované viditeľnými samostatnými ovládacími prvkami sú v tomto editore skryté.
- Úpravy v `Additional line-point options` sa použijú automaticky pri opustení poľa, bez samostatných tlačidiel Apply/Clear.
- Vertexy čiary s line-point riadkami `altitude` alebo `subtype` zobrazujú jemný metadátový krúžok aj na nevybranej čiare a výraznejší krúžok okolo aktívneho úchopu. Tooltip vertexu ďalej ukazuje náhľad všetkých ďalších line-point riadkov.

Riadok `Preview` ukazuje vzhľad vybraného alebo pripravovaného objektu. Náhľad používa svetlý mapový podklad aj v tmavom režime.

### 6.9 Objekty a Pozadia

V `Inšpektor -> Objekty` možno vyberať objekty, meniť ich poradie pretiahnutím, prepínať viditeľnosť v aktuálnom pohľade a mazať objekty s potvrdením.

V `Inšpektor -> Pozadia` možno:

- pridať, odobrať a radiť rastrové, SVG, `.xvi` alebo PocketTopo `.txt` vrstvy pozadia,
- zobraziť/skryť jednotlivé vrstvy,
- meniť polohu a krytie vrstvy,
- upraviť X/Y mierku a rotáciu vrstvy; `Lock proportions` vo východiskovom stave drží X a Y mierku rovnakú,
- nastaviť pivot rotácie kliknutím na `Set Pivot` a potom kliknutím na požadovaný stred v mape; pri rastrových a SVG vrstvách môže byť klik vnútri aj mimo viditeľného pozadia, pivot vybranej vrstvy je v mape zobrazený, keď je aktívne `Pozadia`, a `Reset Pivot` obnoví východiskový pivot,
- upraviť `Gamma` pre rastrové vrstvy (`.xvi` a SVG používajú pevnú Gamma).

Rastrové vrstvy pozadia si zachovávajú plné rozlíšenie obrázka, takže pri priblížení zostávajú ostré namiesto toho, aby sa rozmazali. Veľmi veľké skeny sú obmedzené na vysokú internú zobrazovaciu veľkosť kvôli spotrebe pamäte.

Therion Studio tiež číta a zapisuje Mapiah metadáta pozadia `##MAPIAH## image_insert_v1` pre vrstvy `format=xvi`, `format=raster` a `format=svg`, vrátane rotácie, X/Y mierky a pivotu. Vďaka tomu sa v map editore zobrazia rotované PocketTopo/XVI, rastrové alebo SVG referencie vytvorené v Mapiah a Therion Studio môže jednoduchú XTherion referenciu povýšiť na Mapiah metadáta, keď vrstvu otočíte, zmeníte mierku alebo jej nastavíte pivot. SVG referencie sa vykresľujú ako SVG vrstvy s použitím Mapiah metadát o vnútornej veľkosti a zdrojovom viewBoxe. Pri pridaní SVG pozadia Therion Studio odvodí tieto metadáta z atribútov `width`, `height` a `viewBox` koreňového SVG prvku.

Pri pridaní PocketTopo Therion exportu (`.txt`) ako mapového pozadia sa Therion Studio spýta na mierku XVI, rozlíšenie, rozostup gridu a projekciu plán alebo rozvinutý rez. Vedľa PocketTopo exportu zapíše vygenerovaný súbor `_p.xvi` alebo `_e.xvi`, pridá toto `.xvi` ako vrstvu pozadia a uloží do `.th2` zdroja XTherion-kompatibilné metadáta obrázka.

Therion Studio negeneruje samostatný metrický grid. Pre referenčnú mriežku použite vrstvy pozadia, hlavne `.xvi`.

### 6.10 Oddelené mapové okno

`Oddeliť mapu` presunie vizuálnu mapu do samostatného okna, napríklad na druhý monitor. `Vrátiť mapu` alebo zatvorenie samostatného okna mapu vráti späť.

## 7. Spúšťanie Therionu

Panel `Kompilátor` otvoríte v ľavej aktivitnej lište. Popis panelu ho označuje ako miesto na spustenie Therionu pre aktuálny projekt alebo aktívny config.

### 7.1 Hlavné polia

| Pole | Význam |
|---|---|
| `Argumenty` | Dodatočné argumenty pre aktuálnu reláciu. |
| `Cieľ spustenia` | `Aktuálna konfigurácia` alebo `Projektová konfigurácia`. |
| `Cieľová konfigurácia` | Config súbor pre projektové spustenie. |
| `Náhradný pracovný priečinok` | Voliteľné prepísanie pracovného priečinka. |

Cestu k spustiteľnému súboru Therion nastavte v `Súbor -> Nastavenia...`. Ak nie je nastavená, aplikácia skúsi `therion` a platformnú autodetekciu.

Pred spustením kompilácie Therion Studio uloží všetky zmenené otvorené záložky. Ak niektorý otvorený dokument nejde uložiť, kompilácia sa zruší a runner sa nespustí.

Zatvorenie projektu vyčistí `Cieľovú konfiguráciu` a `Náhradný pracovný priečinok`, pretože sú projektové. Cesta k spustiteľnému súboru Therion je globálna preferencia. Dodatočné argumenty sú iba pre aktuálnu reláciu.

### 7.2 Akcie

- `Spustiť Therion`
- `Zastaviť`
- `Vymazať výstup`
- `Kopírovať výstup`

Stavový riadok ukazuje stav kompilácie s kompaktným prefixom `C:`: `C: Nečinný`, `C: Beží...`, `C: OK` alebo `C: Zlyhal`.

## 8. Nastavenia

Nastavenia otvoríte cez `Súbor -> Nastavenia...` alebo `Nastavenia...` v natívnom macOS menu.

Nastavenia obsahujú:

- jazyk aplikácie (`Podľa systému`, `Angličtina`, `Čeština`, `Slovenčina`),
- cestu k spustiteľnému súboru Therion,
- východiskový editor pre novo otvorené `.th` a Therion config záložky (`Zdroj` alebo `Bloky`).

## 9. Klávesové skratky

Používajte `Command` na macOS a `Ctrl` na Windows/Linux, ak menu platformy nezobrazuje inú natívnu skratku.

| Akcia | Skratka |
|---|---|
| Nové okno | `Command/Ctrl+N` |
| Otvoriť projekt | `Command/Ctrl+O` |
| Uložiť | `Command/Ctrl+S` |
| Uložiť všetko | zobrazené v menu `Súbor` |
| Zavrieť všetky záložky | `Command/Ctrl+Shift+W` |
| Ukončiť aplikáciu | `Command/Ctrl+Q` |
| Späť | `Command/Ctrl+Z` |
| Znovu | `Command/Ctrl+Shift+Z` alebo platformné východiskové |
| Nájsť | `Command/Ctrl+F` |
| Hľadať v projekte | `Command/Ctrl+Shift+F` |
| Nájsť a nahradiť | platformná východisková skratka pre replace |
| Zavrieť panel hľadania/nahradenia | `Esc`, keď je panel hľadania/nahradenia otvorený |
| Prepnúť do editora Zdroj (`Raw`) | `Command/Ctrl+horný 1` |
| Prepnúť do editora Bloky pre `.th` / config, alebo do editora Vizuálne pre `.th2` | `Command/Ctrl+horný 2` |
| Ručné otvorenie dopĺňania (textový editor) | `Ctrl+Space` |
| Posun mapy | `Space` + ťahanie ľavým tlačidlom myši; `Ctrl` + ťahanie ľavým tlačidlom myši; ťahanie pravým tlačidlom myši; zariadenie s presným posúvaním, napríklad touchpad alebo Apple Magic Mouse |
| Zoom mapy | Tlačidlá `Priblížiť` / `Oddialiť`; bežné koliesko myši; `Command/Ctrl+posúvanie` |
| Prispôsobiť geometriu mapy | Tlačidlo `Prispôsobiť` |
| Prispôsobiť geometriu mapy aj pozadie | Tlačidlo `Prispôsobiť vrátane pozadia` |
| Dokončiť aktuálny mapový návrh | `Enter` |
| Zrušiť vkladanie/kreslenie v mape | `Esc` |
| Zmazať vybraný mapový objekt alebo vybraný line point; pri kreslení zmazať posledný bod návrhu | `Delete` / `Backspace` |

## 10. Pomocník a O aplikácii

- `Pomocník -> Používateľská príručka` otvorí lokalizovaný manuál v aplikácii. Prehliadač manuálu necháva vľavo viditeľný obsah, podporuje odkazy z obsahu v tomto dokumente a obsahuje hľadanie v manuáli s navigáciou na predchádzajúcu/ďalšiu zhodu. `Command/Ctrl+F` fokusuje hľadanie v manuáli.
- `Pomocník -> O aplikácii Therion Studio` zobrazí verziu, build label, verziu Qt, platformu, GitHub repozitár, licenciu, maintainera a third-party notices. Na macOS je O aplikácii aj v natívnom aplikačnom menu.

## 11. Riešenie problémov

### 11.1 Spustiteľný súbor Therion nebol nájdený

Riešenie:

- nastavte platnú úplnú cestu v `Súbor -> Nastavenia... -> Spustiteľný súbor Therion`,
- overte oprávnenia na spustenie.

### 11.2 Config sa nedá rozriešiť

Riešenie:

- nastavte `Cieľovú konfiguráciu`, alebo
- otvorte požadovaný Therion config a spustite `Aktuálnu konfiguráciu`, alebo
- overte pracovný priečinok / override cestu.

### 11.3 Premenovanie alebo mazanie je blokované

Riešenie:

- zatvorte záložky, ktoré odkazujú na cieľový súbor alebo priečinok, a akciu zopakujte.

### 11.4 Pozadie mapy vyzerá nesprávne

Riešenie:

- vyberte vrstvu v `Pozadia`,
- overte viditeľnosť, polohu, krytie a Gamma,
- pri `.xvi` overte, že `.xvi` a súvisiace `.th2` patria do rovnakého súradnicového kontextu.
- Mapiah metadáta rotovaného alebo meraného pozadia sú podporované pre `format=xvi`, `format=raster` a `format=svg`; SVG pozadie vyžaduje Mapiah metadáta vnútornej veľkosti a zdrojového viewBoxu,
- ak rastrová vrstva pri priblížení stále vyzerá mäkko, dosiahla rozlíšenie zdrojového obrázka; pre viac detailov použite sken vo vyššom rozlíšení.

### 11.5 Mapu možno zoomovať, ale nedá sa posúvať

Riešenie:

- podržte `Space` a ťahajte ľavým tlačidlom myši,
- podržte `Ctrl` a ťahajte ľavým tlačidlom myši,
- ťahajte pravým tlačidlom myši alebo použite posúvanie na touchpade / presnom polohovacom zariadení,
- ak presné zariadenie práve zoomuje, podržte pri posúvaní `Command/Ctrl`,
- použite vodorovný a zvislý posuvník, ak sú viditeľné.

### 11.6 Plocha nie je vidieť

Riešenie:

- overte, že blok `area ... endarea` je v tom istom `scrap` ako referencované hranice `line -id ...`,
- skontrolujte, že každé referencované ID existuje a je v danom scrape jedinečné,
- pri otvorených hraničných líniách overte, že ich priesečníky tvoria uzavretú plochu,
- skontrolujte viditeľnosť vrstiev a objektov v `Objekty`.

### 11.7 Smart Area nenájde kandidáta

Riešenie:

- kliknite dovnútra plochy ohraničenej líniami v tom istom scrape,
- priblížte mapu a kliknite ďalej od priesečníkov alebo nejednoznačných hrán,
- overte, že zamýšľané hraničné línie sa pretínajú alebo nadväzujú dostatočne blízko na vytvorenie uzavretej plochy,
- ak sa zobrazí viac kandidátov, pred potvrdením použite `[` / `]`.

### 11.8 Líniu nejde zmazať

Riešenie:

- ak je línia referencovaná plochou, najprv zmažte alebo upravte plochu,
- zmazanie plochy odstráni iba blok `area ... endarea` a ponechá referencované hraničné línie.

### 11.9 Projektové hľadanie otvorilo `.th2` výsledok v Zdroj

Riešenie:

- je to očakávané správanie pre textové výsledky hľadania, aby bol vidieť zodpovedajúci zdrojový riadok,
- prepínačom režimu editora alebo `Command/Ctrl+horný 2` sa vráťte do vizuálneho mapového editora.
