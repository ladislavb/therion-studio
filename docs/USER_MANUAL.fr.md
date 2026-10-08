# Manuel utilisateur de Therion Studio

Dernière mise à jour : 2026-07-14

Ce guide présente les opérations courantes dans Therion Studio. Il se concentre volontairement sur l’utilisation de l’application et ne constitue pas une référence complète du langage Therion. La syntaxe source de Therion, les noms de commandes, les options et le contenu sérialisé des documents conservent leur forme Therion canonique.

L’application utilise la langue du système d’exploitation lorsqu’une traduction intégrée est disponible. L’anglais, le tchèque, le français et le slovaque sont inclus. Utilisez `Fichier -> Paramètres...` (`Preferences...` dans le menu natif de l’application sous macOS) pour choisir une autre langue. Le changement de langue prend effet après le redémarrage de Therion Studio.

## Sommaire

- [1. Présentation de Therion Studio](#1-présentation-de-therion-studio)
- [2. Fenêtre principale](#2-fenêtre-principale)
- [3. Prise en main](#3-prise-en-main)
- [4. Édition de texte](#4-édition-de-texte)
- [5. Navigation dans le projet et opérations sur les fichiers](#5-navigation-dans-le-projet-et-opérations-sur-les-fichiers)
- [6. Édition visuelle du dessin (`.th2`)](#6-édition-visuelle-du-dessin-th2)
- [7. Exécution de Therion](#7-exécution-de-therion)
- [8. Paramètres](#8-paramètres)
- [9. Raccourcis clavier](#9-raccourcis-clavier)
- [10. Aide et À propos](#10-aide-et-à-propos)
- [11. Dépannage](#11-dépannage)

## 1. Présentation de Therion Studio

Therion Studio est un éditeur de bureau destiné aux projets de topographie souterraine Therion. Il permet :

- de parcourir les fichiers du projet ;
- de parcourir les exports générés ;
- de modifier les fichiers `.th`, `.th2` et les fichiers de configuration Therion ;
- de visualiser en lecture seule les fichiers `.lox` compilés dans une vue 3D ;
- de consulter en lecture seule les fichiers `.sql` exportés depuis la base de données Therion ;
- de parcourir la structure des objets `survey`, `centerline`, `map` et `scrap` ;
- de modifier visuellement les dessins des fichiers `.th2` ;
- d’exécuter Therion depuis une console intégrée.

Therion Studio n’inclut pas le compilateur externe Therion. Installez Therion séparément et, si la détection automatique ne suffit pas, configurez le chemin de son exécutable dans les paramètres.

### 1.1 Terminologie

- `Projet` : dossier actuellement parcouru par Therion Studio, qui contient généralement les fichiers sources Therion, les fichiers de configuration, les arrière-plans et les sous-dossiers.
- `Configuration` : fichier de traitement Therion tel que `thconfig`, `thconfig.*` ou `*.thconfig`.
- `Topographie` : structure de topographie Therion provenant des fichiers sources `.th`.
- `Map` : objet Therion `map ... endmap` qui référence des scraps.
- `Scrap` : fragment de dessin contenu dans un fichier `.th2`.
- `Point`, `Ligne`, `Aire` : objets de dessin enregistrés dans un scrap.
- `Arrière-plan` : image matricielle, fichier `.xvi` ou export texte PocketTopo utilisé comme référence pour le dessin.
- `.xvi` : format vectoriel d’arrière-plan et de référence de Therion, souvent produit à partir de données PocketTopo.

## 2. Fenêtre principale

La fenêtre principale comprend :

- la barre de menus (`Fichier`, `Édition`, `Affichage`, `Aide`) ;
- une barre de commandes supérieure regroupant les actions du projet, d’enregistrement, d’édition et de dessin ;
- la barre d’activités gauche (`Structure`, `Exports`, `Rechercher`, `Validation`, `Compilateur`) et une action de compilation rapide ;
- les onglets de documents au centre ;
- une barre d’état affichant les instructions du dessin, l’état de la compilation, l’encodage et l’état du dessin.

Actions courantes de la fenêtre :

- `Fichier -> Nouvelle fenêtre` ouvre une nouvelle fenêtre vide. Elle ne reprend ni le projet actuel ni les documents ouverts.
- `Fichier -> Nouveau fichier -> Source Therion (.th)`, `Dessin Therion (.th2)` ou `Configuration Therion (.thconfig)` ouvre un nouveau document non enregistré dans le projet actif. Les nouveaux documents `.th`, `.th2` et `.thconfig` commencent par `encoding utf-8`. Le bouton `Nouveau document` de la barre d’outils propose les mêmes choix. Le premier `Enregistrer` demande où enregistrer le fichier.
- `Fichier -> Paramètres...` ouvre les paramètres de l’application.
- `Affichage -> Développer la barre latérale` / `Réduire la barre latérale` affiche ou masque le contenu de la barre latérale gauche.
- `Affichage -> Développer l'aide`, `Expand Block Inspector` ou `Expand Map Inspector` contrôle le panneau droit actif selon l’éditeur utilisé.
- `Affichage -> Afficher la loupe du dessin` / `Masquer la loupe du dessin` affiche ou masque la loupe du dessin. Il s’agit uniquement d’un état de l’interface : le fichier `.th2` n’est pas modifié.
- `Affichage -> Passer en plein écran` / `Quitter le plein écran` active ou désactive le mode plein écran.

Lorsqu’un panneau de dessin est détaché dans une fenêtre séparée, la fenêtre principale peut afficher à la fois les commandes `Inspecteur de dessin` et `Aide`, car le dessin visuel et la source raw peuvent être visibles simultanément.

## 3. Prise en main

### 3.1 Démarrage rapide

1. Créez un projet avec `Fichier -> Nouveau projet -> Projet à partir d'un modèle...` ou `Fichier -> Nouveau projet -> Projet vide...`, ou ouvrez le dossier d’un projet existant avec `Fichier -> Ouvrir le projet...`.
2. Les projets fondés sur un modèle sont créés à partir du modèle intégré par défaut, qui contient `thconfig`, `index.th`, `surveys/survey1.th`, `scraps/scrap1.th2` et un dossier `output` destiné aux exports générés. Therion Studio ouvre le nouveau projet, sélectionne son fichier `thconfig` comme `Configuration cible` du projet, puis ouvre `thconfig`, `index.th`, `surveys/survey1.th` et `scraps/scrap1.th2` dans des onglets.
3. Dans `Compilateur`, choisissez la `Configuration cible` (`thconfig`, `thconfig.*` ou `*.thconfig`) à utiliser pour les exécutions du projet. Si vous partez de zéro sans modèle, créez un fichier de configuration et au moins un fichier source `.th` depuis `Fichier -> Nouveau fichier`.
4. Ouvrez les fichiers sources `.th` qui définissent les topographies, les données de centerline, les maps et les références de fichiers. Utilisez `Raw` pour modifier directement la source Therion ou `Blocs` pour effectuer les modifications structurées prises en charge.
5. Ouvrez un dessin `.th2` existant depuis `Fichiers`, ou créez-en un avec `Fichier -> Nouveau fichier -> Dessin Therion (.th2)`. Utilisez `Visuel` pour modifier le dessin et `Raw` pour intervenir directement dans la source `.th2`.
6. Dans l’éditeur de dessin, insérez un scrap avant d’ajouter des points, des lignes ou des aires si le fichier `.th2` n’en contient pas encore. Dessinez des objets ou utilisez `Smart Area` pour créer une aire Therion référencée à partir de bordures existantes.
7. Vérifiez que les fichiers de configuration et les fichiers sources référencent les fichiers de topographie et de map nécessaires à la compilation, en employant la syntaxe source Therion habituelle.
8. Enregistrez les documents, puis exécutez Therion depuis `Compilateur` pour contrôler ou exporter le projet.

### 3.2 Ouvrir un projet

1. Sélectionnez `Fichier -> Ouvrir le projet...`.
2. Choisissez le dossier qui contient les fichiers Therion, les fichiers de configuration, les arrière-plans et les sous-dossiers du projet.
3. Ouvrez les documents depuis le panneau `Fichiers`.

Lorsque vous l’ouvrez depuis `Ouvrir le projet...`, le sélecteur de dossier démarre dans votre dossier personnel.

Lorsqu’aucun projet n’est ouvert, l’onglet d’accueil affiche les boutons `Nouveau projet vide`, `Nouveau projet à partir d'un modèle...` et `Ouvrir un projet existant...`. Au lieu de parcourir le système de fichiers de l’ordinateur, la barre latérale `Fichiers` présente un état vide avec la même action d’ouverture de projet. Lorsqu’un projet est ouvert mais qu’aucun onglet de document n’est actif, l’onglet d’accueil propose d’ouvrir un fichier depuis la barre latérale.

`Fichier -> Nouveau projet -> Projet à partir d'un modèle...` ouvre une boîte de dialogue contenant `Nom du projet` et `Emplacement`, affiche le chemin du dossier qui sera créé, puis crée un projet à partir du modèle intégré par défaut. La boîte de dialogue s’ouvre dans le dernier dossier parent où un projet a été créé avec succès ou, à défaut, dans votre dossier `Documents`. Le dossier cible du projet ne doit contenir aucun fichier. Le modèle par défaut crée `thconfig`, `index.th`, `surveys/survey1.th`, `scraps/scrap1.th2` et `output`. Le fichier `index.th` définit la topographie principale `survey cave`, inclut les fichiers de topographie et de scrap, puis définit une map qui référence le scrap. Le fichier `thconfig` généré charge `index.th`, sélectionne `cave` et produit les exports PDF et LOX dans `output`. Après la création, Therion Studio ouvre `thconfig`, `index.th`, `surveys/survey1.th` et `scraps/scrap1.th2`.

`Fichier -> Nouveau projet -> Projet vide...` ouvre une boîte de dialogue contenant `Nom du projet` et `Emplacement`, affiche le chemin du dossier qui sera créé et démarre dans le dernier dossier parent utilisé avec succès ou, à défaut, dans votre dossier `Documents`. La commande crée le dossier du projet et l’ouvre comme nouveau projet vide. Le dossier cible ne doit contenir aucun fichier. Utilisez ensuite `Fichier -> Nouveau fichier` ou le menu contextuel du panneau `Fichiers` pour ajouter des fichiers au projet.

L’onglet d’accueil et le menu `Fichier -> Projets récents` affichent jusqu’à cinq projets récemment ouverts. Sélectionnez un projet dans l’une de ces listes pour le rouvrir.

Lorsqu’un projet est ouvert, l’onglet d’accueil affiche son nom et son chemin. Il présente également jusqu’à dix fichiers récemment utilisés dans ce projet ; sélectionnez-en un pour le rouvrir. La même liste propre au projet est disponible dans `Fichier -> Fichiers récents`.

Après le redémarrage de Therion Studio, le dernier projet accessible et les documents précédemment ouverts sont restaurés lorsque c’est possible, y compris pour les projets placés dans les dossiers utilisateur standard tels que `Documents` sous macOS. Un chemin inaccessible est ignoré sans empêcher la restauration du reste de la session.

### 3.3 Ouvrir des documents

- Double-cliquez sur un fichier dans `Fichiers`.
- Les fichiers `.th2` s’ouvrent dans l’éditeur de dessin.
- Les fichiers `.lox` s’ouvrent dans la vue 3D.
- Les exports `.sql` de la base de données Therion s’ouvrent dans la visionneuse de rapports SQL.
- Les exports de map ou d’atlas `.pdf` s’ouvrent dans l’application par défaut du système.
- Les fichiers `.th`, `thconfig`, `*.thconfig`, `thconfig.*`, `.log`, `.txt` et les fichiers texte ordinaires s’ouvrent dans l’éditeur de texte.
- Les fichiers non pris en charge, comme les images ou les PDF, affichent le message `Unsupported file` avec l’action `Ouvrir dans une application externe`.

La vue 3D est en lecture seule. Sa barre d’outils propose les commandes `Réinitialiser`, `Ajuster`, `Projection orthogonale`, `Vue de dessus`, `Vue de côté`, `Tourner à gauche`, `Tourner à droite`, `Auto Rotate`, `Measure` et `Exporter une image 3D`. Vous pouvez faire tourner la vue par glisser gauche, la déplacer par glisser droit ou central, zoomer avec la molette, naviguer avec les touches fléchées et utiliser les boutons de rotation pour tourner autour de l’axe bleu, qui correspond à l’axe du monde. Lorsque l’onglet de la vue 3D est actif, les flèches gauche et droite effectuent une rotation de 5 degrés autour de l’axe Z du monde ; les flèches haut et bas modifient l’inclinaison de la caméra par pas de 5 degrés. Les boutons de rotation de la barre d’outils utilisent le même pas. `Ajuster` cadre les limites de la scène chargée selon l’orientation actuelle de la caméra et la forme de la zone d’affichage. `Projection orthogonale` bascule entre les projections perspective et orthographique sans modifier les données de la scène. Le bouton `Auto Rotate` présente une icône de lecture à l’arrêt et une icône d’arrêt carrée pendant la rotation ; il fait tourner la vue autour de l’axe Z du monde. L’action de téléchargement `Exporter une image 3D` enregistre la vue actuelle dans un fichier PNG en conservant la caméra, les calques visibles, les incrustations, la coloration du modèle et l’arrière-plan. La boîte de dialogue d’export permet de choisir la résolution de l’image : taille actuelle de la zone d’affichage, largeur de 1 920 ou 3 840 pixels, ou dimensions personnalisées. Les proportions de la zone d’affichage sont verrouillées par défaut afin d’éviter toute déformation. Le panneau de l’inspecteur suit la palette de couleurs active de l’application ; il permet de régler `Model Coloring` sur `Altitude` ou `None`, d’ajuster `Rotation Speed` et de définir précisément `Compass`, `Tilt`, `Distance` et `Focal Length`. La longueur focale est désactivée en projection orthographique, car elle n’agit que sur la projection perspective. La section `Scene Settings`, placée après `Calques`, permet de régler `Arrière-plan` sur `Black` ou `White`, ainsi que d’activer ou désactiver `Show Bounding Box`, `Show Head-Up Display` et `Show Title & Stats`. `Show Head-Up Display` contrôle la légende d’altitude, la boussole, l’indicateur d’angle de vue et la barre d’échelle. `Altitude`, valeur par défaut, colore la centerline et les maillages selon la coordonnée Z du modèle. `None` affiche les maillages dans une couleur neutre uniforme adaptée à l’arrière-plan choisi. Les surfaces maillées utilisent un éclairage lissé par sommet afin que la forme de la cavité reste lisible sur l’arrière-plan sélectionné. Les commandes des calques utilisent des noms sobres sans nombre d’éléments visible ; elles affichent les sous-calques de centerline correspondant aux catégories de visées trouvées dans le fichier `.lox` chargé et n’affichent les sous-calques de stations que si la scène contient plusieurs catégories de stations — entrée, fixe ou autre. Par défaut, la centerline souterraine, les maillages et les surfaces sont visibles ; les stations, les étiquettes, les visées de centerline de surface, les visées d’habillage, les visées dupliquées et les sous-calques de visées masquées ne le sont pas. La case principale `Stations` commande l’affichage de tous les marqueurs de stations ; les sous-calques de stations ne font que restreindre les catégories visibles lorsque `Stations` est activé. Les marqueurs et les étiquettes de stations ne s’affichent que pour les stations rattachées aux visées de centerline actuellement visibles. `Étiquettes` reste toutefois indépendant de la visibilité des marqueurs et peut afficher les noms même lorsque `Stations` est désactivé. Dans les vues denses, les marqueurs et les étiquettes de stations entièrement qualifiées sont automatiquement espacés. Survolez une station rattachée à une visée de centerline visible pour afficher sa référence complète et sa coordonnée Z dans une fiche flottante près du pointeur, même si ses étiquettes ou ses marqueurs sont masqués. Lorsqu’un fichier `.lox` ouvert est régénéré sur le disque, la vue recharge automatiquement la scène et conserve autant que possible les réglages de la caméra et de l’inspecteur. La zone d’affichage superpose la `Longueur des galeries souterraines` et la `Profondeur souterraine`, calculées uniquement à partir des visées de centerline souterraines. L’outil de mesure fonctionne comme un interrupteur : cliquez sur la règle, puis sur deux stations rattachées à des visées de centerline visibles pour consulter leur distance 3D, leur azimut et leur dénivelé ; cliquez de nouveau sur la règle ou appuyez sur `Esc` pour quitter le mode de mesure. Le canevas 3D trace un cadre rouge autour de l’étendue de la scène lorsque ses limites sont disponibles et n’affiche pas de lignes de repère indépendantes pour les axes X, Y et Z du monde. La légende d’altitude n’est superposée que lorsque la coloration du modèle est réglée sur `Altitude` et que l’affichage tête haute est visible. Les vues de dessus et de côté conservent l’orientation actuelle de la caméra. La boussole utilise cette orientation dans les deux vues ; lorsque les limites de la scène sont valides, la boussole, le demi-cercle droit indiquant l’angle de vue et la barre d’échelle restent visibles. Cette dernière prend la forme d’une ligne simple terminée par deux graduations, à côté de ce groupe de commandes. La légende d’altitude comprend plusieurs valeurs intermédiaires. L’aiguille de l’angle de vue parcourt la moitié supérieure pour une vue prise au-dessus de la scène et la moitié inférieure pour une vue prise en dessous.

La visionneuse de rapports SQL est en lecture seule. Ouvrez un fichier `.sql` produit par la commande Therion `export database` depuis le panneau `Fichiers` pour le charger dans une base SQLite en mémoire. L’importation et les requêtes s’exécutent en arrière-plan ; pendant l’importation d’un nouvel export, les commandes de requête et de préréglage restent indisponibles et la ligne d’état indique l’importation en cours. L’ouverture ou le rechargement d’un autre export, le lancement d’une requête plus récente ou la fermeture du rapport annule les travaux devenus obsolètes. Les requêtes en lecture seule disposent d’un délai maximal de dix secondes ; une requête arrivée à expiration signale une erreur et le rapport reste utilisable pour en lancer une autre. L’éditeur situé au-dessus du tableau de résultats exécute une seule requête en lecture seule de type `SELECT` ou `WITH`. La barre latérale droite contient les sections `Préréglages` et `Schéma` : les préréglages intégrés remplissent l’éditeur de requête et lancent des analyses prédéfinies — vue d’ensemble avec nombre d’explorateurs et de topographes ainsi que profondeur totale, longueurs topographiées, exploration et topographie par personne, activité par année, activité récente, stations de continuation, indicateurs de continuation, entrées et profondeur par topographie — tandis que le schéma présente les tables et les colonnes importées. Utilisez la section `Personnalisé` pour enregistrer la requête actuelle comme préréglage utilisateur, renommer le préréglage personnalisé sélectionné ou le supprimer. Ces préréglages sont conservés dans les paramètres de l’application et ne modifient pas le fichier `.sql` ouvert. Les instructions SQL qui modifient les données sont refusées et le nombre de résultats très volumineux est limité dans la visionneuse afin que l’application reste réactive. L’action de téléchargement `Exporter en CSV` de la barre d’outils enregistre dans un fichier CSV le tableau de résultats actuellement affiché. La boîte de dialogue d’enregistrement propose un nom horodaté propre au projet, tel que `therion-studio-report-babice-20260704-143215.csv`.

### Exports

Utilisez l’entrée `Exports` de la barre d’activités gauche pour parcourir les fichiers produits par les exports Therion du projet ouvert. Ils sont regroupés sous `Modèle`, `Map / Atlas` et `Base de données`. Un export unique est identifié par son nom de fichier ; si plusieurs exports d’un même groupe portent le même nom, chacune ajoute entre parenthèses son dossier relatif au projet, par exemple `map.pdf (_output)`. La liste est actualisée à la fin d’une exécution de Therion ainsi que lorsque les fichiers d’export du projet changent sur le disque. Activez un export `.lox` pour l’ouvrir dans la vue 3D interne, un export `.sql` pour l’ouvrir dans la visionneuse de rapports SQL et un export `.pdf` pour l’ouvrir dans l’application par défaut du système. Les modèles Therion `.3d` ne sont pas répertoriés tant que Therion Studio ne propose pas de visionneuse ou de procédure d’importation compatible.

### 3.4 Créer et gérer des fichiers

Utilisez `Fichier -> Nouveau fichier` pour créer dans le projet actif un document `.th`, `.th2` ou `.thconfig` non enregistré, puis choisissez son chemin lors du premier enregistrement. Cliquez avec le bouton droit dans le panneau `Fichiers` pour créer des dossiers, créer directement des fichiers `.th`, `.th2` ou `.thconfig` enregistrés dans le projet, renommer des éléments, dupliquer ou supprimer des fichiers, ou ouvrir directement des fichiers `.th2` dans l’éditeur de dessin.

Le renommage est bloqué lorsque le fichier ou le dossier concerné est ouvert dans un onglet. La suppression d’un dossier est également bloquée tant que des onglets associés restent ouverts. La suppression d’un fichier ouvert demande une confirmation, ferme son onglet après traitement de l’éventuelle demande concernant les modifications non enregistrées, puis supprime le fichier.

### 3.5 Enregistrer les modifications

- `Fichier -> Enregistrer` enregistre l’onglet actif.
- Si l’onglet actif n’a pas encore été enregistré, `Fichier -> Enregistrer` ouvre `Enregistrer sous`.
- `Fichier -> Enregistrer tout` enregistre tous les onglets modifiés.
- La fermeture d’un onglet modifié demande s’il faut enregistrer, ignorer les modifications ou annuler.
- Si un fichier ouvert change sur le disque alors que son onglet Therion Studio ne contient aucune modification non enregistrée, l’application le recharge automatiquement. Si l’onglet contient des modifications non enregistrées, Therion Studio demande s’il faut recharger le fichier depuis le disque ou conserver la version en mémoire.

## 4. Édition de texte

### 4.1 Modes Raw et Blocs

Pour les fichiers `.th` et les fichiers de configuration Therion :

| Mode | Utilisation |
|---|---|
| `Raw` | Modification directe de la source avec coloration syntaxique, recherche, remplacement, saisie semi-automatique et aide contextuelle. |
| `Blocs` | Modification structurée des commandes et des blocs pris en charge, avec les mêmes métadonnées d’aide sur les commandes qu’en mode Raw. |

Les nouveaux onglets `.th` et de configuration Therion s’ouvrent dans l’éditeur par défaut choisi dans les paramètres. `Raw` est le mode par défaut. Le passage à `Blocs` n’insère pas les directives `encoding` manquantes et ne réécrit pas la source avant une modification explicite de votre part.

La boîte à outils de `Blocs` est filtrée selon le type du document :

| Document | Contenu de la boîte à outils |
|---|---|
| `.th` | Commandes du chapitre `Creating data files` du Therion Book. Les commandes d’objets de dessin `.th2`, telles que `scrap`, `point`, `line` et `area`, sont masquées puisqu’elles se modifient dans l’éditeur de dessin. |
| `thconfig`, `thconfig.*`, `*.thconfig` | Commandes de `Processing data`, telles que `source`, `select` et `export`. |

Pour les fichiers `.th2` :

| Mode | Utilisation |
|---|---|
| `Raw` | Modification directe du texte source `.th2`. |
| `Visuel` | Modification visuelle du dessin avec le canevas et l’inspecteur. |

### 4.2 Fonctions de l’éditeur de texte

- numéros de ligne et mise en évidence de la ligne active ;
- saisie semi-automatique des commandes, options, valeurs et chemins ; les propositions de commandes sont filtrées selon le type du document (`.th`, `.th2` ou configuration Therion) ;
- `Ctrl+Space` pour ouvrir manuellement la saisie semi-automatique ;
- en mode Raw, la barre d’outils supérieure du document affiche `Formater le document`. Cette action explicite remplace l’indentation initiale par une tabulation pour chaque niveau de bloc Therion, utilise une largeur d’affichage de quatre espaces par tabulation et peut être annulée en une seule étape. Elle ne s’exécute pas automatiquement et ne modifie ni les lignes vides, ni les lignes du corps des blocs de code, ni les fins de ligne, ni l’encodage ;
- aide contextuelle pour la commande ou l’option actuelle ; Raw et Blocs affichent la même aide complète, les arguments positionnels conservent l’ordre de la documentation, les options sont classées par ordre alphabétique et le panneau d’aide porte le nom de la commande actuelle ou de la cible d’aide sélectionnée ;
- les diagnostics de l’éditeur Raw signalent notamment les tokens d’option mal formés, les commandes ou options inconnues, les commandes employées dans un type de document ou un contexte de bloc incorrect, les arguments manquants, les blocs non fermés, les identifiants dupliqués, les références d’objets non résolues, les références d’aires à des lignes absentes du scrap actuel, les chemins non portables contenant des barres obliques inverses dans les tokens de chemin pris en charge et les blocs d’objets `line` ou `area` vides dans les scraps. La validation du projet signale également les fichiers manquants référencés par `input` et `source`, ainsi que les références de map, scrap, join ou station que l’index du projet ne peut résoudre sans ambiguïté. Cela comprend les références `point station -name` qui ne correspondent pas aux données de topographie et les références `join` à des repères nommés de points de ligne absents de la ligne résolue. Les objets `point ... station` sans `-name` sont autorisés et ne sont pas validés par rapport aux stations de la topographie. Les chemins `input` et `source` entre guillemets, les préfixes `./` et les séparateurs Windows sous forme de barres obliques inverses sont résolus comme les mêmes chemins relatifs sans guillemets utilisant des barres obliques. `Appliquer la correction` peut toutefois convertir les tokens de chemin pour employer le séparateur portable `/` ; l’aperçu montre la ligne source complète dans laquelle le chemin sélectionné a été remplacé. L’ouverture ou la restauration d’un projet, l’enregistrement ou la modification d’un document du projet, ainsi que les changements internes ou externes apportés aux fichiers `.th`, `.th2` ou `thconfig`, actualisent la validation du projet en arrière-plan sans changer de panneau. Dans le panneau `Validation`, `Valider le projet` actualise manuellement tous les fichiers `.th`, `.th2` et `thconfig` du projet ouvert. La validation utilise la même `Configuration cible` que les panneaux Structure et Compilateur ; si ce chemin cible est supprimé ou renommé, Therion Studio efface la cible obsolète et relance la découverte du projet. Pendant une actualisation en arrière-plan, les constats précédemment visibles et le niveau de gravité de la barre d’activités restent affichés jusqu’à ce que le nouveau résultat soit prêt. Les constats portant sur l’ensemble du projet apparaissent en premier sous `Projet` ; leur sélection affiche les détails sans ouvrir de fichier source. L’icône Validation de la barre d’activités, la liste des constats et les diagnostics de l’éditeur Raw distinguent les résultats qui ne contiennent que des avertissements de ceux qui comportent des erreurs. En mode Raw, les soulignements ondulés et les arrière-plans discrets intégrés à l’éditeur représentent les mêmes constats de validation que le panneau ou l’infobulle ; les couleurs syntaxiques ordinaires indiquent uniquement le rôle des tokens. Le panneau Aide continue d’afficher la documentation des commandes. Activez un constat associé à un fichier pour atteindre la ligne source raw concernée, ou utilisez `Appliquer la correction` lorsqu’une modification sûre et explicite de la source est proposée. L’application d’une correction actualise toutes les représentations ouvertes du document, y compris la vue de l’éditeur de dessin pour les fichiers `.th2`. Pour un bloc de dessin `.th2` non fermé, la correction affiche un aperçu du point d’insertion, puis ajoute le `endscrap`, `endline` ou `endarea` manquant à la prochaine limite sûre entre commandes de dessin ou à la fin du fichier. Pour un objet de scrap vide, elle montre le bloc source qui sera retiré, ne supprime que le bloc sélectionné et n’est jamais appliquée silencieusement ;
- dans un scrap comportant `-station-names <prefix> <suffix>`, les noms des points de station sont validés après application du préfixe et du suffixe, avant que le projet vérifie la référence à la station de topographie. Utilisez `[]` pour un préfixe ou un suffixe vide ;
- `Exporter en Markdown...`, dans le panneau `Validation`, enregistre le résultat actuellement visible sous forme de rapport Markdown comprenant un résumé, les constats regroupés, des extraits de source et, lorsqu’ils sont disponibles, les aperçus des corrections sûres. La boîte de dialogue propose un nom horodaté propre au projet, tel que `therion-studio-validation-babice-20260704-143215.md` ;
- une vue d’inspecteur `Sélection`, en mode Blocs, permet de modifier l’en-tête du bloc sélectionné et ses options intégrées prises en charge ; le premier panneau porte le nom de la commande Therion sélectionnée et affiche sa ligne source ;
- lorsqu’aucun bloc n’est sélectionné, la vue `Sélection` de l’inspecteur Blocs affiche `Aucun bloc sélectionné.` ; lorsque l’encart racine fixe `encoding` est sélectionné, il affiche la commande et la valeur d’encodage sous forme de texte en lecture seule ;
- recherche et remplacement depuis le menu `Édition` ;
- `Fichier -> Importer -> Importer un texte PocketTopo...` n’apparaît que lorsqu’un document texte `.th`, existant ou non enregistré, est actif. La commande importe à l’emplacement du curseur un export Therion PocketTopo (`.txt`) sous forme de blocs `centreline` ;
- une vue d’inspecteur `Fichier`, dont le panneau porte le nom du fichier actuel et affiche son chemin complet, une action pour copier le chemin, sa taille sur le disque, la date et l’heure de dernière modification, l’encodage actuel et une fonction de conversion en UTF-8 pour les fichiers utilisant un autre encodage.

### 4.3 Lignes de données en mode Blocs

En mode `Blocs`, les blocs `data ...` peuvent être modifiés dans un tableau fondé sur l’en-tête de données actif. Les lignes vides du corps sont ignorées à l’ouverture du tableau : les espacements de la source ne deviennent donc pas de fausses mesures.

## 5. Navigation dans le projet et opérations sur les fichiers

### 5.1 Recherche dans le projet

Ouvrez l’activité Rechercher depuis la barre gauche ou appuyez sur `Command/Ctrl+Shift+F`. Saisissez le texte littéral, choisissez `Mot entier` ou `Sensible à la casse` si nécessaire, puis appuyez sur `Enter` ou `Rechercher` pour parcourir le projet actuel.

La recherche parcourt les sources texte Therion (`.th`, `.th2` et fichiers de configuration Therion), tient compte des modifications non enregistrées dans les onglets ouverts et regroupe les correspondances par fichier avec leur ligne et leur colonne. Double-cliquez sur un fichier ou une ligne de résultat pour ouvrir le fichier au niveau du texte correspondant ; la barre de recherche intégrée est alors prête pour naviguer vers le résultat précédent ou suivant. Les résultats `.th2` s’ouvrent comme documents de l’éditeur de dessin avec l’espace de travail Raw actif ; vous pouvez ensuite revenir à l’éditeur visuel.

### 5.2 Panneau de navigation du projet

L’activité `Structure` ouvre le panneau de navigation du projet avec un sélecteur compact `Fichiers` / `Topographie` / `Map`.

`Fichiers` affiche l’arborescence du dossier du projet et permet de parcourir les fichiers ainsi que d’utiliser les actions des menus contextuels sur les fichiers et les dossiers. `Topographie` affiche l’espace de noms de la topographie et la hiérarchie de définition des objets `survey`, `map` et `scrap`, et reconnaît les deux graphies Therion de centerline : `centreline` et `centerline`. `Map` affiche la composition des maps : maps principales, maps enfants et scraps référencés.

`Topographie` et `Map` mémorisent séparément l’état développé ou réduit de leur arborescence tant que le même projet reste ouvert.

Sélectionnez une ligne pour l’examiner dans l’arborescence. Double-cliquez sur une ligne source, ou sélectionnez-la puis appuyez sur `Enter`, pour ouvrir son document source et atteindre la ligne correspondante.

Sous chaque parent, les lignes sont regroupées par topographies, puis maps, puis scraps ; chaque groupe est classé par ordre alphabétique selon le nom affiché. Les lignes d’avertissement apparaissent après les objets du projet auxquels elles se rapportent.

L’index utilise la `Configuration cible` sélectionnée lorsqu’elle se trouve dans le projet ouvert. En l’absence de configuration cible explicite, Therion Studio utilise la première configuration racine trouvée dans l’ordre de priorité suivant : `thconfig`, `thconfig.thconfig`, `main.thconfig`, `index.thconfig`, puis `<project_name>.thconfig`, où `<project_name>` désigne le nom du dossier du projet ouvert. Si aucun de ces fichiers n’existe et que la racine contient exactement une configuration nommée (`*.thconfig` ou `thconfig.*`), celle-ci est utilisée. Si plusieurs fichiers de configuration sont possibles, choisissez la `Configuration cible` souhaitée dans le panneau `Compilateur`. Le renommage, la suppression, la création ou la modification externe des fichiers sources ou de configuration du projet actualise son index partagé ; si la `Configuration cible` sélectionnée n’existe plus, elle est effacée afin de relancer la découverte.

Dans la vue `Topographie`, les maps et les scraps restent sous l’espace de noms de la topographie où ils sont définis, même lorsqu’une autre map les référence dans sa composition. Dans la vue `Map`, les maps et les scraps apparaissent sous les maps qui les référencent. Les références de composition non résolues ou ambiguës sont présentées sous forme de lignes d’avertissement qui renvoient à la ligne source. Le compilateur Therion reste l’autorité pour valider le comportement des exports.

## 6. Édition visuelle du dessin (`.th2`)

### 6.1 Modes et inspecteur

Le mode `Visuel` comprend :

- le canevas de dessin ;
- les vues du sélecteur de l’inspecteur : `Sélection`, `Objets`, `Arrière-plans`, `Fichier`.

Le mode `Raw` reste disponible pour modifier directement la source.

`Visuel` et `Raw` sont deux vues de la même source `.th2`. Les modifications visuelles réécrivent les commandes Therion dans le fichier. Le mode Raw reste disponible pour intervenir directement dans la source et utiliser les constructions Therion avancées. Le changement de vue ne crée pas un second document.

Les fichiers qui ne sont pas en UTF-8 sont ouverts avec un encodage source déterminé lorsque celui-ci peut être identifié, notamment pour les anciens encodages courants d’Europe centrale tels qu’ISO-8859-2. Lors de l’enregistrement, Therion Studio conserve cet encodage, sauf si vous convertissez explicitement le fichier en UTF-8 depuis l’inspecteur `Fichier`.

### 6.2 Navigation

Utilisez les commandes de navigation avant de dessiner ou de modifier des objets.

| Action | Commande |
|---|---|
| Déplacer le dessin | Maintenez `Space` et faites glisser avec le bouton gauche de la souris, maintenez `Ctrl` et faites glisser avec le bouton gauche, ou faites glisser avec le bouton droit. Les dispositifs de précision tels que les pavés tactiles et l’Apple Magic Mouse déplacent le dessin avec le défilement à deux doigts ou sur la surface. |
| Zoom avant/arrière | Utilisez les boutons `Zoom avant` / `Zoom arrière` de la barre d’outils, la molette d’une souris classique ou maintenez `Command/Ctrl` pendant le défilement. |
| Ajuster la géométrie du dessin | Utilisez `Ajuster` pour cadrer les objets dessinés dans la zone d’affichage. |
| Ajuster la géométrie et les arrière-plans | Utilisez `Ajuster avec l'arrière-plan` pour inclure les calques d’arrière-plan matriciels et `.xvi` dans le cadrage. |
| Déplacer avec les barres de défilement | Utilisez les barres horizontale et verticale lorsqu’elles sont visibles. |

Maintenir `Space` bascule temporairement le canevas en mode déplacement sans quitter l’outil de dessin actuel. `Ctrl` avec un glisser gauche et le glisser droit produisent le même déplacement. Un clic droit, ou `Ctrl` avec un clic gauche sans glissement, ouvre à la place le menu contextuel de l’objet.

Par défaut, Therion Studio utilise le défilement des dispositifs de précision pour déplacer le dessin : les pavés tactiles et les appareils tels que l’Apple Magic Mouse permettent ainsi un déplacement horizontal et vertical. La molette d’une souris classique commande le zoom par défaut.

### 6.3 Principaux outils de dessin

| Groupe d’outils | Actions |
|---|---|
| Navigation | `Zoom avant`, `Zoom arrière`, `Ajuster`, `Ajuster avec l'arrière-plan` |
| Sélection et dessin | `Sélectionner`, `Finaliser le brouillon` |
| Insertion | `Insérer un scrap`, `Point`, `Ligne`, `Dessin à main levée`, `Aire`, `Smart Area` |

Ces commandes sont activées lorsque l’espace de travail `.th2` intégré se trouve en mode `Visuel`. En mode `Raw`, elles restent visibles mais inactives, car l’éditeur de source raw remplace le canevas. Lorsque le panneau de dessin est détaché, utilisez la barre d’outils de sa fenêtre pour zoomer et dessiner.

Le canevas conserve une surface claire imitant le papier dans les modes clair et sombre de l’application. Les barres d’outils, les onglets et les inspecteurs suivent l’apparence du système, mais les arrière-plans matriciels, les références `.xvi` et les symboles du dessin ne sont ni inversés ni teintés en mode sombre.

### 6.4 Insérer des objets

- `Point` : cliquez une fois dans le dessin.
- `Ligne` : cliquez pour placer les sommets, puis appuyez sur `Enter` ou `Finaliser le brouillon`.
- `Aire` : cliquez pour placer les sommets, puis appuyez sur `Enter` ou `Finaliser le brouillon`.
- `Smart Area` : cliquez à l’intérieur d’une face fermée formée par les lignes existantes du même scrap, examinez l’aperçu, utilisez `[` / `]` pour parcourir les possibilités lorsque plusieurs faces correspondent, puis appuyez sur `Enter` ou `Finaliser le brouillon`.
- `Dessin à main levée` : appuyez, faites glisser puis relâchez pour insérer une ligne de Bézier simplifiée.
- `Insérer un scrap` : crée immédiatement un scrap, le sélectionne dans `Sélection` et `Objets`, puis vous permet de modifier son identifiant et sa projection avant d’ajouter des points, des lignes, des lignes à main levée ou des aires.

La finalisation d’une insertion conserve la position et le niveau de zoom actuels. Utilisez `Ajuster` ou `Ajuster avec l'arrière-plan` pour recadrer la zone d’affichage.

Sur les tablettes graphiques et stylets compatibles, le toucher et le glissement du stylet sont acceptés comme interactions principales sur le canevas pour sélectionner des objets et insérer des points, des sommets de ligne ou d’aire et des tracés à main levée. La souris et le pavé tactile restent utilisables sans changer d’outil.

Le lancement de `Point`, `Ligne`, `Dessin à main levée` ou `Aire` active `Inspector -> Sélection` avant la pose du premier point ou sommet. Définissez-y le type, le sous-type, l’identifiant, le nom du point, le texte de l’étiquette ou la valeur de point prise en charge avant de valider le nouvel objet. L’éditeur mémorise séparément les derniers types et sous-types choisis pour les nouveaux points, lignes et aires, utilise le choix le plus récent comme valeur par défaut suivante, conserve ces choix après le redémarrage de l’application et les présente sous forme de boutons compacts dans `Récent`, sous l’aperçu du symbole. Pendant le dessin d’une nouvelle ligne ou aire, `Options` reste disponible et `Point de ligne` contrôle le sommet suivant avant sa pose ; `<<`, `Lissé (-smooth)` et `>>` règlent les poignées de Bézier et l’état lissé de ce sommet, tandis que `Sous-type` définit un sous-type de point de ligne à partir de ce sommet. `line slope` affiche également, avant chaque clic, les commandes de point de ligne prises en charge `Orientation (-orientation)` et `Taille gauche (-l-size)`. Un sous-type de point de ligne vide laisse le sommet sans remplacement et ce sous-type est réinitialisé pour chaque nouveau brouillon de ligne ou d’aire. Une fois un point, une ligne ou une aire validé alors que l’outil reste actif, `Sélection` passe au prochain objet en attente : le type, le sous-type, les choix récents et les autres champs du brouillon restent donc modifiables pour poursuivre le dessin. Les raccourcis de dessin à une seule touche sont ignorés lorsqu’un champ de texte, un éditeur de liste déroulante, un sélecteur numérique ou l’éditeur de source raw possède le focus. L’insertion répétée de `point station` incrémente le prochain `Nom (-name)` en avançant le dernier segment numérique de la station placé avant un éventuel suffixe `@survey`. Elle incrémente également les suffixes alphabétiques finaux, par exemple `1a -> 1b` et `1z -> 1aa` ; le passage du type de point en attente de `station` à un autre type efface ce nom au lieu de le reporter. Si un scrap ou un objet appartenant à un scrap était sélectionné au lancement de l’outil, le nouvel objet est inséré dans ce scrap ; la ligne des métadonnées en attente affiche l’identifiant du scrap cible. Utilisez `Insérer dans` pour choisir un autre scrap existant avant de valider. Lorsque le fichier ne contient encore aucun scrap, `Sélection` affiche un avis mis en évidence indiquant que le scrap de brouillon sera créé avant la validation du premier objet.

Lorsqu’un fichier `.th2` ne contient pas de métadonnées XTherion `xth_me_area_adjust`, la première insertion validée écrit les lignes d’en-tête stables `xth_me_area_adjust` et `xth_me_area_zoom_to`, afin que l’apparition de la nouvelle géométrie ne modifie pas le repérage des dessins suivants.

Les blocs Therion `area ... endarea` existants qui référencent des bordures `line -id ...` sont rendus à partir des lignes du même scrap. Les lignes de bordure peuvent être ouvertes ; si leurs intersections forment une face fermée, Therion Studio remplit cette face sans modifier la source des lignes référencées.

`Smart Area` crée cette forme d’aire référencée au lieu de dessiner une nouvelle géométrie de bordure. La validation peut attribuer les identifiants manquants aux lignes de bordure référencées afin que le nouveau bloc `area ... endarea` puisse les désigner, sans modifier leur géométrie. Après validation, le dessin revient au mode Sélectionner. Appuyez sur `Esc` pour annuler l’aperçu.

Pendant le placement des points ou le dessin des lignes et des aires, les sommets d’objets proches sont mis en évidence comme candidats à l’accrochage. La cible active est accentuée davantage ; les poignées de Bézier restent libres.

Lorsque l’éditeur de dessin est actif, la partie gauche de la barre d’état affiche l’instruction correspondant à l’opération actuelle, notamment les étapes du dessin et les indications de validation avec Enter ou Esc. La pastille compacte du mode emploie `M : Sélection` ou `M : Insertion`.

Pendant le dessin d’une ligne ou d’une aire :

- cliquez pour ajouter un sommet droit ;
- appuyez, faites glisser puis relâchez pendant la pose d’un sommet pour tirer sa paire de poignées de Bézier, comme dans XTherion ;
- faites glisser les poignées de Bézier visibles avant de valider afin d’affiner la courbe du brouillon ;
- cliquez de nouveau sur le premier sommet d’une ligne pour la fermer (`-close on`) ; cliquez sur le premier sommet d’une aire pour valider son brouillon ;
- les lignes fermées affichent le dernier segment jusqu’au premier sommet, y compris les courbes de Bézier fermées à deux points ;
- pendant le dessin d’une ligne, double-cliquez pour ajouter le sommet choisi, valider la ligne et revenir au mode Sélectionner ;
- appuyez sur `Backspace` ou `Delete` pour retirer le dernier sommet du brouillon ;
- appuyez sur `Esc` pour valider un brouillon de ligne ou d’aire suffisamment complet et revenir au mode Sélectionner ; les brouillons incomplets sont annulés.

### 6.5 Opérations courantes sur un dessin

- Créer un nouveau dessin : insérez un scrap, définissez son identifiant et sa projection dans `Sélection`, puis choisissez l’outil Point, Ligne, Dessin à main levée, Aire ou Smart Area.
- Dessiner une paroi : choisissez `Ligne`, définissez si nécessaire le type `wall` avant le premier sommet, placez les sommets, faites glisser pendant la pose d’un sommet pour créer les poignées de Bézier, puis appuyez sur `Enter`.
- Créer une aire référencée : choisissez `Smart Area`, cliquez dans la face fermée voulue, utilisez `[` / `]` si plusieurs possibilités sont proposées, puis appuyez sur `Enter`.
- Modifier la forme d’une ligne : passez à `Sélectionner`, cliquez sur la ligne, puis faites glisser un sommet ou une poignée de Bézier, ou modifiez les commandes `Point de ligne`.
- Ajouter des données de référence pour le dessin : ouvrez `Arrière-plans`, ajoutez une image matricielle, un fichier `.xvi` ou un fichier PocketTopo `.txt`, puis ajustez sa visibilité, sa position, son opacité ou son gamma.

### 6.6 Modifications de la source produites par les actions du dessin

- Les outils Point écrivent des commandes `point ...` dans le scrap cible.
- Les outils Ligne et Dessin à main levée écrivent des blocs `line ... endline`. Les tracés à main levée sont simplifiés en lignes de coordonnées de Bézier, tout en conservant proportionnellement davantage de points d’ancrage pour les tracés courbes ou détaillés.
- L’outil Aire manuel écrit une ligne fermée générée `line border -id ... -close on` et un bloc `area ... endarea` qui référence cette ligne de bordure.
- Smart Area écrit un bloc `area ... endarea` qui référence les lignes de bordure existantes. Il peut ajouter les valeurs `-id` manquantes nécessaires à ces références, sans modifier la géométrie des lignes existantes.
- L’insertion d’un arrière-plan écrit des métadonnées d’image compatibles avec XTherion, telles que `##XTHERION## xth_me_image_insert`. La première insertion dans un fichier dépourvu de métadonnées de vue XTherion peut également écrire `xth_me_area_adjust` et `xth_me_area_zoom_to`.

### 6.7 Modifier la géométrie

Sélectionnez un objet ou l’un de ses sommets ou poignées de contrôle dans le canevas. L’inspecteur `Sélection` affiche alors les commandes correspondantes, notamment la ligne source et l’identifiant du scrap qui contient l’objet.

Les objets conservent leurs couleurs de rendu normales pendant la modification. En mode Sélectionner, le canevas utilise un pointeur en croix ; l’objet situé sous le point actif du pointeur est mis en évidence en cyan avant la sélection, et l’objet sélectionné apparaît en rouge.

La sélection est partagée entre le canevas, le panneau `Objets` et l’inspecteur `Sélection`. La sélection d’une aire met également en évidence les lignes de bordure qu’elle référence. La sélection du sommet ou d’une poignée de Bézier d’une ligne affiche dans l’inspecteur les commandes du point de ligne correspondant.

Cliquez avec le bouton droit sur un objet ou un sommet de ligne, sans le faire glisser, pour ouvrir un menu contextuel proposant les actions courantes de type XTherion. Un clic droit dans une zone vide du canevas n’ouvre pas ce menu. Celui-ci reprend les groupes disponibles dans l’inspecteur `Sélection`, tels que les choix du type et du sous-type, les champs modifiables de l’objet, `Options`, le panneau `Point de ligne` complet, `Actions du point de ligne` et `Actions de l'objet` ; les éditeurs de texte libre ou de valeur numérique s’ouvrent dans l’inspecteur et reçoivent le focus. Sous macOS, le clic secondaire du pavé tactile, par exemple avec deux doigts, ouvre le même menu. Si le menu est déjà ouvert, un autre clic secondaire sur un objet ou un sommet différent le réaffecte à cette nouvelle sélection et le déplace à la position du dernier clic.

Pour les lignes et les bordures d’aires :

- sélectionnez un sommet pour modifier les propriétés de son point de ligne ;
- faites glisser le sommet d’une ligne ou d’une aire près d’un autre objet pour afficher les sommets de celui-ci comme candidats à l’accrochage ; la cible active est accentuée davantage et les poignées de Bézier ne sont pas accrochées ;
- cliquez avec le bouton droit sur un segment et utilisez `Insérer un point ici` pour couper le segment le plus proche à l’endroit du clic ;
- utilisez `Insérer avant` / `Insérer après` pour ajouter des sommets près du sommet sélectionné ;
- utilisez `Étendre avant` / `Étendre après` aux extrémités d’une ligne pour la prolonger ;
- utilisez `Delete` / `Backspace` pour retirer le sommet de ligne sélectionné ; si aucun sommet de ligne ne l’est, `Delete` / `Backspace` supprime l’objet sélectionné ;
- lors de la suppression d’un sommet qui comporte des options supplémentaires de point de ligne, par exemple `altitude .` ou `subtype ...`, Therion Studio demande une confirmation ;
- utilisez `<<` et `>>` pour activer ou supprimer les poignées de Bézier entrante et sortante ;
- faites glisser les poignées de Bézier directement sur le canevas pour remodeler la courbe.

Lorsqu’un déplacement fait passer un point, un sommet ou une poignée de Bézier de coordonnées entières à une position fractionnaire, Therion Studio écrit les nouvelles coordonnées avec une précision décimale supplémentaire, afin que les modifications répétées et les tangentes de Bézier lissées ne se dégradent pas visiblement.

Lorsqu’une ligne sert de bordure à une aire, certaines actions destructrices sur cette ligne sont bloquées ; sélectionnez ou modifiez plutôt l’aire propriétaire. La suppression d’une aire retire uniquement le bloc `area ... endarea` et conserve dans la source les lignes de bordure référencées.

### 6.8 Modifier les propriétés des objets

Dans `Inspector -> Sélection`, vous pouvez modifier les propriétés des objets `Scrap`, `Point`, `Ligne` ou `Aire` sélectionnés.

- `Scrap` affiche l’identifiant dans la section principale, `Projection` dans `Options` et une section distincte `Échelle du scrap` pour les valeurs d’étalonnage `-scale [...]` compatibles avec XTherion et Therion.
- `Point`, `Ligne` et `Aire` limitent la section principale à l’identité et à l’aperçu (`ID`, `Type`, `Sous-type`, `Aperçu`). Les autres champs modifiables tels que `Nom (-name)`, `Texte (-text)`, `Valeur (-value)`, le découpage, l’alignement et la projection apparaissent dans `Options`. Choisissez la valeur vide du sous-type, ou `Aucun sous-type` dans le menu contextuel, pour retirer un `-subtype` existant.
- Une syntaxe de sous-type Therion intégrée telle que `wall:debris` possède le même rendu que la forme explicite équivalente `-subtype debris`, tant dans le canevas que dans l’aperçu de Sélection.
- `Modifier tous les paramètres de l'objet...`, à la fin d’`Options`, ouvre l’éditeur complet des options du catalogue pour la commande `scrap`, `point`, `line` ou `area` sélectionnée. Les attributs positionnels tels que les coordonnées `x`/`y` d’un point, le `type` d’une ligne et l’`id` d’un scrap apparaissent sous forme de lignes protégées, tandis que `-id`, `-text`, `-orientation` et les autres options restent modifiables.
- `point label` et `line label` exposent `Texte (-text)`. Les étiquettes de point sont rendues près du point ; celles de ligne suivent le tracé de la ligne d’étiquette, qui détermine donc la longueur et l’orientation du texte.
- Les types de points pris en charge, tels que `height`, `passage-height`, `altitude`, `dimensions` et `date`, exposent `Valeur (-value)`. Les valeurs Therion entre crochets telles que `[fix 1300]` sont préservées.
- Les types de points qui prennent en charge `-orientation` affichent un remplacement de l’orientation et une poignée d’orientation déplaçable. Les noms des stations restent alignés sur l’écran pour demeurer lisibles.
- Les objets Point exposent `Alignement (-align)` lorsqu’ils sont sélectionnés. Choisissez `Par défaut` pour retirer l’option d’alignement explicite, ou un alignement Therion tel que `top-left`, `center` ou `bottom-right`.
- Les lignes, aires et points qui prennent en charge le découpage Therion exposent `Désactiver le rognage (-clip off)`. Sa désactivation retire l’option explicite `-clip` au lieu d’écrire `-clip on`.
- Les sommets de ligne sélectionnés exposent des commandes `Point de ligne` propres aux options par sommet prises en charge. `Sous-type` est affiché pour les types de lignes possédant des sous-types de segments, `Orientation (-orientation)` et `Taille gauche (-l-size)` pour les points de ligne de pente, et `Altitude (auto)` pour les points de ligne de paroi ; cette dernière commande écrit `altitude .`.
- Les sommets sélectionnés disposent également, dans `Sélection`, d’un éditeur `Options supplémentaires du point de ligne` pour les autres options autonomes propres au sommet, telles que `altitude`, `subtype`, `direction` ou `adjust`. Elles peuvent ainsi être modifiées sans passer en mode Raw. Les lignes déjà gérées par une commande dédiée visible sont masquées dans cet éditeur.
- Les modifications des `Options supplémentaires du point de ligne` sont appliquées automatiquement lorsque le champ perd le focus, sans boutons Appliquer ou Effacer distincts.
- Les sommets comportant des lignes de point `altitude` ou `subtype` affichent un cercle discret signalant les métadonnées, même lorsque la ligne n’est pas sélectionnée, et un cercle renforcé autour de la poignée active. L’infobulle du sommet continue de présenter toutes les lignes supplémentaires.

La ligne `Aperçu` montre l’apparence de l’objet sélectionné ou en attente. Elle utilise un fond clair semblable à celui du dessin, même en mode sombre.

### 6.9 Objets et arrière-plans

Dans `Inspector -> Objets`, vous pouvez sélectionner des objets, les réordonner par glisser-déposer, modifier leur visibilité dans la vue actuelle et les supprimer après confirmation. Par défaut, les scraps réduits manuellement le restent lors de l’actualisation de l’arborescence des objets et de la navigation avec le curseur de texte. La sélection d’un objet appartenant à un scrap réduit développe ce dernier afin de rendre l’objet visible. Activez `Réduire/développer automatiquement les scraps` dans cet onglet si vous souhaitez que l’inspecteur ne laisse développé que le scrap de l’objet sélectionné et réduise automatiquement les autres.

Dans `Inspector -> Arrière-plans`, vous pouvez :

- ajouter, retirer et réordonner des calques d’arrière-plan matriciels, SVG, `.xvi` ou PocketTopo `.txt` ;
- afficher ou masquer chaque calque ;
- modifier la position et l’opacité du calque ;
- modifier son échelle X/Y et sa rotation ; `Verrouiller les proportions` maintient par défaut les échelles X et Y égales ;
- définir le pivot de rotation en cliquant sur `Définir le pivot`, puis sur le centre voulu dans le dessin ; pour les calques matriciels et SVG, ce clic peut se trouver à l’intérieur ou à l’extérieur de l’arrière-plan visible. Le pivot du calque sélectionné apparaît dans le dessin lorsque `Arrière-plans` est actif, et `Réinitialiser le pivot` rétablit le pivot par défaut ;
- régler le `Gamma` des calques matriciels, tandis que les calques `.xvi` et SVG utilisent un gamma fixe.

Le canevas peut se déplacer sur toute l’étendue de chaque calque d’arrière-plan visible, y compris sur les parties déplacées ou transformées hors du cadre initial. Le déplacement, le redimensionnement ou la rotation d’un calque ne modifie pas les coordonnées des objets.
Lorsqu’un arrière-plan visible constitue le seul contenu du dessin, il reste la surface de dessin sans que le canevas de document vide ou son message d’absence de géométrie ne vienne le recouvrir.

Les calques d’arrière-plan matriciels conservent la résolution complète de l’image et restent donc nets lorsque vous zoomez au lieu de devenir flous. Les numérisations très volumineuses sont limitées à une taille d’affichage interne élevée afin de maîtriser la consommation de mémoire.

Therion Studio lit et écrit également les métadonnées d’arrière-plan Mapiah `##MAPIAH## image_insert_v1` pour les calques `format=xvi`, `format=raster` et `format=svg`, notamment la rotation, l’échelle X/Y et le pivot. Les références PocketTopo/XVI, matricielles ou SVG créées dans Mapiah avec une rotation peuvent ainsi apparaître dans l’éditeur de dessin. Therion Studio peut également convertir une référence d’arrière-plan XTherion simple en métadonnées Mapiah lorsque vous faites pivoter ou redimensionnez un calque, ou définissez son pivot. Le déplacement d’un fichier XVI XTherion non transformé conserve ses métadonnées XTherion. Les références SVG sont rendues sous forme de calques SVG en utilisant leur taille intrinsèque Mapiah et les métadonnées du viewBox source pour leur positionnement. Lorsque vous ajoutez un arrière-plan SVG, Therion Studio déduit ces informations des attributs `width`, `height` et `viewBox` de l’élément racine SVG. Si un SVG restauré est absent ou invalide, l’état du dessin indique son nom tandis que les autres calques valides continuent de se charger.

Un arrière-plan `.xvi` affiche des marqueurs de stations bleus et compacts aux positions de la table des stations, ainsi que les éventuelles enveloppes de passage LRUD. Ces enveloppes apparaissent derrière la centerline sous forme discrètement remplie et soulignée, afin que l’arrière-plan serve directement de référence de dessin. Les marqueurs de stations conservent une taille pratique pendant le zoom et n’affichent pas d’étiquette permanente. Les commandes ordinaires de visibilité et d’opacité du calque s’appliquent à l’ensemble de la référence `.xvi`.

Lorsque vous ajoutez un point de station en mode `Point`, cliquez près d’une station d’un arrière-plan `.xvi` visible pour placer le point exactement à cet endroit et renseigner son `-name`. S’il n’existe aucune station proche, ou si plusieurs stations sont possibles, le point est inséré normalement sans nom automatique. Cette fonction tient compte de la position, de l’échelle et de la rotation actuelles du calque.

Lorsque vous ajoutez un arrière-plan `.xvi` à un nouveau dessin, Therion Studio actualise automatiquement l’étendue du dessin XTherion afin que toute la référence soit disponible. Une étendue existante différente de la valeur par défaut est préservée.

Un arrière-plan n’est restauré comme contenu du dessin que lorsque la source TH2 actuelle le déclare dans des métadonnées XTherion ou Mapiah. Les paramètres locaux de la session peuvent rétablir son état d’affichage, mais ne peuvent pas ajouter un arrière-plan à un fichier TH2 vide ou sans rapport.

Lorsque vous ajoutez un export Therion PocketTopo (`.txt`) comme arrière-plan, Therion Studio demande l’échelle XVI, la résolution, l’espacement de la grille et la projection en plan ou en coupe développée. Il écrit un fichier `_p.xvi` ou `_e.xvi` généré à côté de l’export PocketTopo, ajoute ce fichier `.xvi` comme calque d’arrière-plan et conserve des métadonnées d’image compatibles avec XTherion dans la source `.th2`.

Therion Studio ne génère pas de grille métrique séparée. Utilisez les calques d’arrière-plan, en particulier `.xvi`, comme quadrillage de référence.

### 6.10 Fenêtre de dessin détachée

Utilisez `Détacher le dessin` pour placer le panneau visuel dans sa propre fenêtre, par exemple sur un second écran. Utilisez `Réintégrer le dessin` ou fermez la fenêtre détachée pour le replacer dans la fenêtre principale.

## 7. Exécution de Therion

Ouvrez le panneau `Compilateur` depuis la barre d’activités. Sa description indique qu’il permet d’exécuter Therion pour le projet actuel ou la configuration active.

### 7.1 Champs principaux

| Champ | Signification |
|---|---|
| `Arguments` | Arguments supplémentaires pour la session actuelle. |
| `Cible d'exécution` | `Configuration actuelle` ou `Configuration du projet`. |
| `Configuration cible` | Fichier de configuration employé pour les exécutions du projet. |
| `Remplacement du répertoire de travail` | Remplacement facultatif du répertoire d’exécution. |

Définissez le chemin de l’exécutable Therion dans `Fichier -> Paramètres...`. En l’absence de chemin explicite, Therion Studio essaie `therion`, puis la détection automatique propre à la plateforme.

Avant de lancer une compilation, Therion Studio enregistre tous les onglets de documents ouverts qui ont été modifiés. Si l’un de ces documents ne peut pas être enregistré, la compilation est annulée et le processus d’exécution n’est pas lancé.

La configuration cible sélectionnée apparaît sous la forme d’un nom de fichier mis en évidence, avec son chemin complet en dessous. Le lancement d’une nouvelle compilation efface la sortie précédente avant de démarrer Therion, puis inscrit la commande et le répertoire de travail au début de la nouvelle sortie.

Therion est exécuté de manière non interactive depuis Studio. Si Therion affiche après une erreur une invite telle que `Press ENTER to Exit!`, Studio ferme le flux d’entrée du processus afin que l’exécution puisse se terminer sans appuyer sur `Arrêter`.

La fermeture d’un projet efface la `Configuration cible` et le `Remplacement du répertoire de travail`, car ces chemins sont propres au projet. Le renommage ou la suppression dans le projet de la `Configuration cible` sélectionnée efface également la cible obsolète et actualise la découverte du projet. Le chemin de l’exécutable Therion est une préférence globale. Les arguments supplémentaires ne sont valables que pour la session.

### 7.2 Actions

- `Exécuter Therion`
- `Arrêter`
- `Effacer la sortie`
- `Copier la sortie`

La barre d’état indique l’état de la compilation avec le préfixe compact `C:` : `C : Inactif`, `C: Running`, `C: OK` ou `C: Failed`. `C: OK` n’est utilisé que si le processus Therion se termine correctement et si Studio ne détecte pas dans la sortie d’erreur de problème connu d’écriture de fichier, tel que `warning -- error writing`.

## 8. Paramètres

Ouvrez les paramètres depuis `Fichier -> Paramètres...` ou `Preferences...` sous macOS.

Les paramètres comprennent :

- la langue de l’application (`Système par défaut`, anglais, tchèque, français ou slovaque) ;
- le chemin de l’exécutable Therion ;
- l’éditeur par défaut des nouveaux onglets `.th` et de configuration Therion (`Raw` ou `Blocs`) ;
- la validation automatique de l’ensemble du projet après les modifications du projet, des documents ou des fichiers ;
- l’activation pendant 24 heures des journaux de dépannage, avec des actions permettant d’ouvrir leur dossier ou d’effacer les journaux existants.

Les modifications concernant les journaux de dépannage prennent effet après le redémarrage de Therion Studio. Les journaux sont écrits dans le dossier de journalisation de l’application et renouvelés automatiquement. La préférence de l’interface expire après 24 heures afin que la journalisation ne puisse pas rester activée indéfiniment par inadvertance. Lorsqu’il est actif, le journal contient des repères temporels pour le démarrage, la restauration de la session et des documents ainsi que l’ouverture du projet ; ils peuvent aider à diagnostiquer un lancement lent de l’application. Les remplacements de l’environnement de développement tels que `THERION_STUDIO_ENABLE_LOG=1` restent des paramètres explicites au lancement et ne sont pas contrôlés par la préférence de l’interface limitée à 24 heures.

## 9. Raccourcis clavier

Utilisez `Command` sous macOS et `Ctrl` sous Windows et Linux, sauf si le menu de la plateforme affiche un autre raccourci natif.

| Action | Raccourci |
|---|---|
| Nouvelle fenêtre | `Command/Ctrl+N` |
| Ouvrir un projet | `Command/Ctrl+O` |
| Enregistrer | `Command/Ctrl+S` |
| Tout enregistrer | indiqué dans le menu `Fichier` |
| Fermer tous les onglets | `Command/Ctrl+Shift+W` |
| Quitter | `Command/Ctrl+Q` |
| Annuler | `Command/Ctrl+Z` |
| Rétablir | `Command/Ctrl+Shift+Z` ou raccourci par défaut de la plateforme |
| Rechercher | `Command/Ctrl+F` |
| Rechercher dans le projet | `Command/Ctrl+Shift+F` |
| Rechercher et remplacer | raccourci de remplacement par défaut de la plateforme |
| Fermer la barre de recherche/remplacement | `Esc` lorsque la barre est ouverte |
| Passer à l’éditeur Raw | `Command/Ctrl+touche 1 de la rangée supérieure` |
| Passer à l’éditeur Blocs pour les fichiers `.th` et de configuration, ou à l’éditeur Visuel pour les fichiers `.th2` | `Command/Ctrl+touche 2 de la rangée supérieure` |
| Ouvrir manuellement la saisie semi-automatique dans l’éditeur de texte | `Ctrl+Space` |
| Déplacer le dessin | `Space` + glisser gauche ; `Ctrl` + glisser gauche ; glisser avec le bouton droit ; dispositif de défilement de précision tel qu’un pavé tactile ou une Apple Magic Mouse |
| Zoomer dans le dessin | Barre d’outils `Zoom avant` / `Zoom arrière` ; molette de souris classique ; `Command/Ctrl+défilement` |
| Ajuster la géométrie du dessin | Barre d’outils `Ajuster` |
| Ajuster la géométrie et les arrière-plans | Barre d’outils `Ajuster avec l'arrière-plan` |
| Commencer à dessiner un point | `P` |
| Commencer à dessiner une ligne | `L` |
| Commencer à dessiner une aire | `A` |
| Finaliser le brouillon actuel | `Enter` |
| Annuler l’insertion ou le dessin | `Esc` |
| Supprimer l’objet ou le point de ligne sélectionné ; pendant le dessin, supprimer le dernier point du brouillon | `Delete` / `Backspace` |
| Inverser la ligne sélectionnée | `R` |
| Fermer ou ouvrir la ligne sélectionnée ; pendant son dessin, la finaliser comme ligne fermée | `C` |
| Lisser ou délisser le point de ligne sélectionné ou suivant | `S` |
| Activer ou désactiver la poignée de contrôle précédente du point de ligne sélectionné ou suivant | `,` |
| Activer ou désactiver la poignée de contrôle suivante du point de ligne sélectionné ou suivant | `.` |

Les opérations de dessin et de modification de la source conservent les 200 dernières étapes d’annulation pour chaque onglet de l’éditeur de dessin.

## 10. Aide et À propos

- `Aide -> Manuel utilisateur`, ou `Manuel utilisateur` dans l’onglet d’accueil, ouvre le manuel localisé dans l’application. La visionneuse garde le sommaire visible à gauche, prend en charge les liens du sommaire de ce document et propose une recherche interne avec navigation vers les résultats précédent et suivant. Le champ de recherche reçoit le focus à l’ouverture du manuel et `Command/Ctrl+F` permet de le sélectionner de nouveau.
- `Aide -> À propos de Therion Studio` affiche la version, l’identifiant de la compilation, la version de Qt, la plateforme, le dépôt GitHub, la licence, le mainteneur et les mentions relatives aux composants tiers. Sous macOS, À propos est également disponible dans le menu natif de l’application.

## 11. Dépannage

### 11.1 Exécutable Therion introuvable

Solution :

- définissez un chemin absolu valide dans `Fichier -> Paramètres... -> Exécutable Therion` ;
- vérifiez les autorisations d’exécution.

### 11.2 Impossible de résoudre la configuration

Solution :

- définissez la `Configuration cible` ; ou
- ouvrez la configuration Therion voulue et lancez l’exécution avec `Configuration actuelle` ; ou
- vérifiez le répertoire de travail ou son chemin de remplacement.

### 11.3 Renommage ou suppression bloqué

Solution :

- fermez les onglets qui référencent le fichier ou le dossier concerné, puis réessayez.

### 11.4 Affichage incorrect de l’arrière-plan

Solution :

- sélectionnez le calque dans `Arrière-plans` ;
- vérifiez sa visibilité, sa position, son opacité et son gamma ;
- pour `.xvi`, vérifiez que le fichier `.xvi` et le fichier `.th2` référencé utilisent le même système de coordonnées ;
- les métadonnées Mapiah d’arrière-plan avec rotation ou changement d’échelle sont prises en charge pour `format=xvi`, `format=raster` et `format=svg` ; les arrière-plans SVG nécessitent les métadonnées Mapiah de taille intrinsèque et de viewBox source ;
- si un calque matriciel reste flou lorsque vous zoomez, il a atteint la résolution de son image source ; utilisez une numérisation à plus haute résolution pour obtenir davantage de détails.

### 11.5 Le zoom fonctionne, mais pas le déplacement du dessin

Solution :

- maintenez `Space` et faites glisser avec le bouton gauche de la souris ;
- maintenez `Ctrl` et faites glisser avec le bouton gauche de la souris ;
- faites glisser avec le bouton droit, ou utilisez le défilement du pavé tactile ou d’un dispositif de précision ;
- maintenez `Command/Ctrl` pendant le défilement si votre dispositif de précision commande actuellement le zoom ;
- utilisez les barres de défilement horizontale et verticale lorsqu’elles sont visibles.

### 11.6 Aire invisible

Solution :

- vérifiez que le bloc `area ... endarea` se trouve dans le même `scrap` que les bordures `line -id ...` référencées ;
- vérifiez que chaque identifiant référencé existe et est unique dans ce scrap ;
- pour les lignes de bordure ouvertes, vérifiez que leurs intersections forment une face fermée ;
- vérifiez la visibilité des calques et des objets dans `Objets`.

### 11.7 Smart Area ne trouve aucune possibilité

Solution :

- cliquez à l’intérieur d’une face délimitée par des lignes du même scrap ;
- zoomez et cliquez à distance des intersections de lignes ou des bordures ambiguës ;
- vérifiez que les lignes de bordure voulues se croisent ou se rejoignent suffisamment près pour former une face fermée ;
- si plusieurs possibilités sont proposées, utilisez `[` / `]` pour choisir celle qui convient avant de valider.

### 11.8 Impossible de supprimer une ligne

Solution :

- si la ligne est référencée par une aire, supprimez ou modifiez d’abord cette aire ;
- la suppression d’une aire retire uniquement le bloc `area ... endarea` et conserve les lignes de bordure référencées.

### 11.9 La recherche dans le projet a ouvert un résultat `.th2` en mode Raw

Solution :

- ce comportement est normal pour les résultats d’une recherche textuelle : il permet d’afficher la ligne source correspondante ;
- utilisez le sélecteur de mode de l’éditeur ou `Command/Ctrl+touche 2 de la rangée supérieure` pour revenir à l’éditeur visuel.
