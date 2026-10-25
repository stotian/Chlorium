# Chlorium — préflight V5, 25 octobre 2026

> Photographie du préflight initial. Les tests techniques isolés ont ensuite
> été autorisés et exécutés : voir le [journal unique V5](docs/EXPERIMENTS-V5.md).
> Les statuts `not_run` ci-dessous décrivent cette photographie initiale.

**Verdict : CONTINUER hors jeu. G0 documenté ; G1 à G5 non validés pour
les sources actuelles. Les défauts de rendu et les écrans noirs restent ouverts.**

## Consignes appliquées

La demande actuelle impose un travail individuel, aucune capture périodique et
aucun test pour le moment. Le ZIP V5 demande une première livraison bornée :
état réel, hypothèses vérifiables, backlog ordonné et premières fixtures.
Le contrat §-1 et les compléments V5-D à V5-O gouvernent cette livraison.
Les anciens prompts et le catalogue de prototypes ne sont pas des tâches à
lancer automatiquement. La formule documentaire « aucun travail Codex préalable »
décrit la préparation du document ; le workspace contient bien du travail antérieur.

Archive reçue : `Chlorium_Codex_V5.zip`, SHA-256
`AAF341AB5BBC472870B4183F00D5D0B2750FAB2C04C6738FDE497C8892AFD402`.

## Révision et séparation

- HEAD inspecté : `2312559c47fdaea2c03ee2934c3db8cbd77e45ac`, identique au
  SHA du mandat. Le dossier principal est sur `main` et contient des modifications
  non commitées : ce HEAD seul ne décrit donc pas les sources actuelles.
- Nouveau worktree `Chlorium-v5-research`, branche locale `research/v5-g0`.
  Les 36 fichiers signalés comme modifiés/non suivis avant V5 y ont été copiés
  et comparés par SHA-256. Ce nombre inclut des différences de fins de ligne.
  Le dossier principal a été conservé ; aucun nouveau correctif V5 n'y est écrit.
- Le manifeste du paquet **installé**, lu sans charger sa DLL, est
  `local.chlorium`, `v0.2.0-alpha.6`, GD `2.2081`, Geode `5.11.2`.
  Son SHA-256 et celui de `dist` correspondent :
  `7939B0339E33C2F0F8B8E2A9FEDA9B1793A5C36D6F2558BDC4BC4C2877533AEA`.
  Cela prouve l'identité des fichiers, pas l'activation effective dans une partie.
- Le cache CMake préexistant appartient au prototype : native quads/snapshots
  **ON**, static renderer, profiler, audit, atlas cache et autres expériences
  **OFF**. Il ne décrit pas le paquet normal installé, ni une compilation du
  nouveau worktree. `mod.json` conserve alpha.6 ; le script de packaging distingue
  l'expérience alpha.609. Les sources ont encore changé après ce paquet.
- Le chemin normal sélectionne le rendu natif et le cache mathématique historique.
  Le moteur statique Bismuth reste désactivé. Le prototype de quads constitue un
  troisième chemin distinct ; il conserve les calculs natifs des sprites.

## Environnement observé, sans exécution du jeu

Windows 11 Famille x64, version `10.0.26200` ; Core i3-10110U, 2 cœurs/4 threads ;
8 469 188 608 octets de RAM physique ; Intel UHD Graphics, pilote
`31.0.101.2141`. WMI expose 1 Gio dans `AdapterRAM` : cette valeur ne prouve
pas le budget GPU disponible d'un iGPU à mémoire partagée. Environ 2 Gio de RAM
étaient libres lors du premier relevé : aucun grand stress-test n'est préparé
comme exécution automatique.

SDK Geode local `VERSION=5.11.2`. Git, Python et les fichiers MSVC, CMake et Ninja
référencés par `build-local.cmd` existent. CMake/MSVC/Ninja ne sont pas dans le
PATH de cette session PowerShell ; le script fournit leurs chemins. La présence
des fichiers ne prouve pas que toutes les dépendances peuvent être liées.
La bibliothèque `Geode.lib` n'a pas été localisée dans le premier inventaire :
vérifier sa résolution avant de déclarer un build prêt. Aucun compilateur lancé.

Aucun processus GeometryDash, python/pythonw, cl ou ninja n'était actif dans
l'inventaire ciblé. Aucun sous-agent actif. Aucune capture produite par cette
passe. La version OpenGL 4.6 est historique ; aucun nouveau contexte n'a été
créé pour revérifier version, extensions, limites ou mémoire disponible.

## Goulots : preuves historiques et inconnues actuelles

Les chiffres suivants sont relus dans les documents du dépôt, **sans nouvelle
mesure ni reproduction indépendante** :

| Zone | Indice historique | Ce qu'il permet de décider |
|---|---|---|
| Orbit 96–100 % | boucle 162,92 ms, update 160,98 ms, Area 84,20 ms, 37 images | La simulation CPU mérite une enquête ; une optimisation d'envoi GPU seule ne supprime pas ce travail. Chronos imbriqués, non additionnables. |
| Orbit 83–88,5 %, pipeline math | moyenne et p99 dégradés dans la paire documentée | Abandonner la promotion du pipeline combiné ; des millions de hits ne prouvent aucun gain. |
| Grief/Heliopolis alpha.6 | résultats mixtes et noirs natifs dans les comparaisons consignées | Aucun gain universel ni absence de noirs partiels démontrés. |
| Prototype de quads après HEAD | activation et résultats partiels consignés localement, retours utilisateur défavorables | Couverture, fidélité et chemin critique actuels non démontrés ; aucun choix d'architecture sur la moyenne FPS. |

Références : [passages lourds](docs/PASSAGES-LOURDS.md),
[pipeline math](docs/MATH-PIPELINE-20261024.md),
[alpha.6](docs/VALIDATION-ALPHA6-20261023.md).

L'affirmation « le GPU est maintenant le goulot » n'est pas établie.
La fraction accélérable, les p99 actuels, le coût de première image, les
allocations et l'identité du backend dans une nouvelle partie sont inconnus.
Ne pas calculer de promesse ×5 ou de borne d'Amdahl à partir de chronos inclusifs.

## Triage des écrans noirs

| Hypothèse concurrente | Indice à chercher | Statut actuel |
|---|---|---|
| Obscurité native | référence au même événement, caméra et progression | Historique partiellement apparié ; plainte actuelle non attribuée. |
| Géométrie/ordre/couverture | DrawToken absent, doublé, réordonné ou quad différent | Risque du prototype ; oracle préparé, aucune trace native actuelle. |
| État de transparence contaminé | draw sentinelle différent après substitution | Revue source : ancien prototype réglait les facteurs sans reproduire l'activation native du blending. Le correctif préparé conserve la soumission native jusqu'à l'atlas ; pas de reproduction exécutée. |
| Ressource/contexte obsolète | génération de contexte, buffer/programme/texture réutilisé | Gardes de contexte préparées ; poignée numérique seule insuffisante, lifecycle non validé. |
| Effet/hook tiers contourné | écart après updateTransform/draw d'un autre mod | Le prototype préparé appelle maintenant le draw natif et intercepte l'atlas ; ordre réel des hooks non observé. |
| Erreur du diagnostic | lecture GL invalide, capture manquante, mauvais appariement | Gardes readback préparées ; aucun test d'injection ni nouveau readback. |

Toutes restent `unresolved` pour les symptômes de l'utilisateur. Les correctifs
préparés avant V5 ont été conservés pour étude ; ils ne sont ni compilés dans
cette passe, ni installés, ni certifiés par l'audit de documents fourni.

## Tickets ordonnés et critères de décision

| Ordre | Ticket | Coût de preuve / impact potentiel / risque | Prochain résultat discriminant |
|---|---|---|---|
| 1 | R4-00 préflight | Faible, bloque les faux résultats, risque faible | Identifier fichiers, options et permissions. Livraison présente. |
| 2 | R4-01 ledger/oracles | Faible à moyen, fidélité structurante, risque faible hors jeu | 12 mutations privées doivent être rejetées, témoin accepté ; zéro test exécuté aujourd'hui. |
| 3 | R4-02 + R4-04 couverture/lifetime | Moyen, vise les défauts de rendu, risque élevé si substitution | Batch mixte, sous-plage, contexte réutilisé, repli même frame et sentinelle GL. Observer d'abord. |
| 4 | R4-03 + R4-07 profil/trace | Moyen, impact potentiellement fort sur Orbit, instrumentation à borner | Temps exclusifs et un événement Area avec ordre/dépendants ; jeu soumis à accord. |
| 5 | R4-08 face à R4-11/R4-09 | Élevé, impact inconnu, fidélité à haut risque | Si submission dominante : NativeRangeHijack vs transport natif. Si simulation dominante : IR causal shadow vs trace native, puis symbolique seulement si éligible. Aucun gagnant présélectionné. |
| 6 | R4-05/R4-10 spatial | Élevé, potentiel sur travail visuel total, faux négatifs à exclure | V constant/N variable et inverse, budget mémoire plafonné ; différé tant que le goulet n'est pas prouvé. |

La concurrence de deux architectures appropriées reste une étape conditionnelle,
pas deux backends à activer d'office. N0 natif, N1 alpha.6, N2 candidate désactivée,
N3 active et N4 fixture sabotée sont des témoins distincts, tous non exécutés ici.

## Livrables et statut

[Dossier G0](research/R4-00/DECISION.md),
[preuves machine](research/R4-00/evidence.json),
[contrat du ledger](research/R4-01/DRAW_LEDGER_SCHEMA.md),
[oracle préparé](research/R4-01/ledger_oracle.py).

Inventaires et empreintes : exécutés en lecture seule. Copie des sources :
comparée octet par octet via SHA-256. Build, fixtures CPU, GPU, jeu, comparaison
visuelle et A/B : **`not_run`**, conformément à la consigne. Aucun push, PR,
commit, paquet ou release V5. Aucune modification des sauvegardes ou du mod installé.

Prochain gate : vérifier l'oracle sur sa fixture privée lorsque les tests seront
autorisés. En attendant, poursuivre la revue hors jeu ; ne pas substituer de dessin.
