# Journal synthétique V5 — 25 octobre 2026

Branche locale `research/v5-g0`, base `2312559`. Travail individuel ; tests
techniques isolés autorisés. Aucun jeu, capture, changement d'installation,
push ou publication. La version normale reste séparée. Les sources préparées
avant V5 ne sont pas toutes validées par les tests suivants.

## R4-01 — oracle, puis première expérience CPU

`python research/R4-01/fixture_mutations.py` : témoin accepté, **12/12** mutations
privées rejetées, code retour 0. Cela valide le comparateur sur sa fixture
déclarative, pas la collecte native, l'état d'un vrai pilote ou les pixels.
G1 est acquis pour cette couche seulement ; les gates du renderer restent ouverts.

## R4-06 — Alpha : graphe inverse à deux étages

**Hypothèse avant prototype.** Dans `GroupManager::prepareGroupStateBuffer`,
un changement Alpha déclenche tous les produits de groupes, puis tous les états.
Un index `groupe → combinaisons → états` doit réduire le travail à la région
affectée quand les changements sont rares, tout en préservant l'ordre des
multiplications. Ce code du renderer statique est inactif dans l'alpha.6 :
aucun gain direct de la version installée n'est déduit de cette expérience.

**Code.** `research/R4-06/IncrementalAlpha.hpp`, noyau CPU sans dépendance GD/GL.
Deux index CSR immuables, déduplication par révision, rejet d'epoch obsolète,
restauration/invalidations complètes, recomposition canonique sans division.
Trois variantes comparées : transcription indépendante du balayage intégral,
CSR pur, CSR avec parcours contigu lorsque le fanout est dense. Le jeu et ses
triggers ne sont ni appelés ni remplacés. Les objets hors écran restent dans le graphe.

**Défauts rencontrés et conservés.** Le premier test exigeait à tort le même
nombre d'écritures privées : le balayage réécrit les NaN inchangés. Cette assertion
a été remplacée par égalité bit à bit, absence d'écritures supplémentaires et
couverture du dirty set. Un vrai écart de résultats a ensuite été détecté après
changement d'arrondi : invalidation au prochain point où le natif recomposerait.
Une fixture a aussi démontré des flags flottants manquants après leur effacement
par un consommateur. Un retour au parcours complet régénère les flags si nécessaire,
sans les fabriquer. Les contrôles SSE (arrondi, FTZ/DAZ, masques) sont suivis ;
les traps non masqués sélectionnent le parcours complet. Aucun test de trap effectif
n'a été lancé. Les logs négatifs sont conservés, pas effacés du verdict.

**Vérifications finales.** Compilation MSVC x64 `/O2 /fp:strict /W4`, sans
avertissement dans la dernière sortie. **42 147 assertions**, dont 6 000 ticks
différentiels sur les deux variantes : résultats IEEE754 bit à bit, NaN/Inf,
zéros signés, sous-normaux, désactivation de groupes, checkpoints synthétiques,
remap/reconstruction d'epoch, snapshots périmés/courts, arrondi et FTZ/DAZ,
flags d'inexact/underflow/overflow et statut SSE brut. La variante privée qui
supprime une dépendance échoue bien sur `differential output bits` (code 1
attendu). Aucun mutant n'est compilé dans un paquet Geode.

**Mesure finale, CPU synthétique exclusivement.** 1 024 groupes, 8 192 combinaisons,
16 384 états, 768 appels par variante et densité, six répétitions avec ordre
rotatif. Peuplement initial exclu et déclaré, compteurs inclus dans les deux
noyaux. CSV brut de 6 912 durées conservé. RAM/VRAM de processus, fréquences,
températures et overhead du timer non mesurés ; le test n'est pas une mesure
de frametime de GD. Allocation dynamique candidate calculée : **818 904 octets**,
hors allocateur, struct et fixtures témoins.

| Groupes modifiés | Balayage p50 / p99 (µs) | CSR pur p50 / p99 | Adaptatif p50 / p99 |
|---|---:|---:|---:|
| 1 | 137,2 / 514,5 | 2,5 / 4,3 | 2,5 / 49,1 |
| 8 | 130,1 / 478,5 | 4,9 / 18,5 | 4,9 / 22,3 |
| 1 024 | 185,0 / 620,6 | 345,8 / 1 455,7 | 191,4 / 687,9 |

Sur 768 mises à jour à un groupe, les combinaisons recomposées passent de
**6 291 456 à 16 128**, et les états visités de **12 582 912 à 32 256**.
Ce travail évité est établi par compteurs ; le speedup du petit noyau n'est
pas un multiplicateur de FPS. Le polling de tous les groupes utilisés reste
nécessaire et n'est pas supprimé. Les valeurs temporelles varient nettement
entre révisions/runs ; tous les relevés intermédiaires restent disponibles.

**Décision : GARDER le principe incrémental, RETRAVAILLER le cas dense.**
Le CSR pur est refusé pour le régime dense ; même l'adaptatif n'y bat pas
la référence sur la dernière série. Aucun remplacement normal recommandé.
Absence de preuve visuelle, de trigger natif, d'Area réel ou de gain GD.

**Suite discriminante.** Une trace passive de `processAreaActions` et
`processAreaEffects` doit établir les dépendants et barrières réellement actifs
avant d'appliquer ce principe au goulet CPU d'Orbit. Les signatures existent
dans les bindings locaux (`0x228fc0`, `0x2283e0`, GD 2.2081) ; aucune interception
de ces adresses n'est ajoutée ici. La collecte native attend l'autorisation de jeu.
Le prototype pur peut encore être éprouvé hors jeu sur membership dynamique
et statuts flottants ; aucune simulation Area n'est prétendue implémentée.

Preuves détaillées, commandes et empreintes :
[`research/R4-06/evidence.json`](../research/R4-06/evidence.json).
Rollback : ne pas sélectionner ce noyau, qui n'est lié à aucune DLL du mod.

## 2026-10-25 — V4 : régression signalée, périmètre GL réduit

**Constat.** Sur la paire exploratoire Grief AUTO 88952972 (0–40 %),
les deux passages ont atteint la cible. La V4 comptabilise environ 2,532 ms
par image dans son chemin capture/soumission, sans isoler le coût natif remplacé.
La référence est restée sans focus : aucune baisse chiffrée des FPS n'est validée.
La comparaison stricte est également refusée faute de `mod_profile_sha256`.
Les compteurs de captures complets ne prouvent pas l'absence d'écran noir.

**Hypothèse ciblée.** Le backend conserve la préparation native puis ajoute
comparaisons/copies et appels au pilote. Son ancien périmètre `SpriteDraw`
sauvegardait/restaurait aussi des états qu'il ne modifie pas. Le nouveau
`NativeQuad` conserve programme, VAO, texture 0/unité active et SSBO 4/générique.
Les autres périmètres restent disponibles. Les appels bruts restaurent les
bindings réels sans modifier le cache Cocos ; ce contrat reste à valider en jeu.

**Preuves hors jeu.** `research/R4-04/run-shaders-gpu.cmd` : 116 vérifications
GPU réussies sur Intel UHD, OpenGL 4.6.0, pilote 31.0.101.2141. Les nouvelles
sentinelles comparent tous les états capturés, dont plage SSBO et binding nul,
blend, UBO, EBO, unités de texture et buffers non modifiés. Neuf lots alternés
de 512 captures/restaurations après échauffement : médiane CPU par appel
`SpriteDraw=26,9402 µs`, `NativeQuad=6,4623 µs`. Micro-mesure sans dessin :
ni gain FPS ni fidélité GD démontrés. Sortie brute privée :
`build-research/shader-scope-check.local.txt`.

`research/R4-04/build-quad.cmd --scope-r2` : compilation et liaison réussies
avec avertissements SDK. DLL séparée dans `build-research/quad-v4-scope-r2`,
SHA256 `7300d43c2d67fb78cb03741e63a37d9a611b728048dd5520f67c47efd1367907`.
La V4 déjà testée et ses empreintes sont conservées ; aucun paquet normal
remplacé, aucun nouveau lancement de GD. Le changement reste expérimental.

**Décision : RETRAVAILLER.** Garder la réduction de périmètre en recherche,
sans annoncer la régression résolue. Prochaine preuve : comparaison appariée
avec focus contrôlé, puis coût des copies de quads si la régression persiste.


## 2026-10-25 — R4-08 : lots Area et programme causal

Nouveau noyau de quatre distances partageant une recette Area : divisions SSE
natives, sans FMA/réciproque approché ni quantification. Oracle extrait en mémoire
privée de la routine exacte de l'exécutable local, corps et constante vérifiés.
**460 672 contrôles**, 349 600 sorties comparées bit à bit, dont 198 700 voies
vectorisées ; statuts/contrôles SSE, 16 modes d'arrondi/denormaux, NaN/Inf et
recours natifs. Programme de vagues testé sur **1 200 séquences** de dépendances.
Deux mutations privées rejetées : valeur corrompue et barrière causale supprimée.
Dernière série de noyau : médianes natives 287,2 / 266,0 / 239,6 µs contre
39,5 / 53,4 / 47,7 µs, respectivement zone 0 / 0,2 / 0,5, 65 536 entrées.
Neuf lots par variante, ordre alterné. **Pas un gain FPS**, gather/commit GD absents.

Trois diagnostics natifs distincts sur Orbit 96–97 %, sans LDM/shake/noclip
ni capture périodique : quatre phases initiales, quatre phases visibles, puis
agrégats par image. Les deux premiers échantillonnent le début léger et ne
représentent pas les pics. Le dernier compte **328 phases Area / 16 compartiments
d'image**, 15 phases visibles. À 96,70 % : 51 phases, 91,54 ms Area dont
58,19 ms dans transformAreaObjects, 197 421 membres proposés. Ces cardinalités
ne sont pas des comptes d'objets réellement visités (sauts/nulls possibles).
Les scopes sont imbriqués et leurs temps ne s'additionnent pas. Les premières
et dernières fenêtres peuvent être partielles. Une mort invalide ce dernier
parcours comme référence A/B, malgré la section atteinte ; 15 lignes de frames
sont avec focus. Aucune absence de blackscreen ne peut être déduite sans lecture
visuelle. Les 90 fichiers protégés ont été restaurés et vérifiés après chacun
des trois diagnostics ; paquet normal inchangé.

**Décision : RETRAVAILLER.** Garder les noyaux/ordonnanceur comme composants de
recherche, sans les brancher aux objets GD. La normalisation seule n'est pas la
cible dominante ; réduire les parcours et reconstructions de matrices devient
prioritaire. L'hypothèse de surcharge par rattrapage physique doit être distinguée
d'une hausse du coût par phase ; aucun plafond de physique n'est introduit.
Preuves : research/R4-08/evidence.json, census-frames.json et timings.csv.

## 2026-10-25 — R4-09 : audit transversal et corrections

Revue des chemins normal/V4/statique, ressources GL, groupes/Area et protocole
A/B, complétée par l'analyse MSVC du chemin normal. Corrections : résolution
WGL par contexte dans common.cpp, timer V4 incluant la restauration GL,
motif d'échec explicite après mort, profils de build mutuellement exclusifs,
empreintes des paquets/configurations communs du profil isolé. Les anciens
rapports restent inchangés et ne deviennent pas valides rétroactivement.

Compilation/liaison réussies dans trois sorties distinctes (audit-r3,
measurement-audit-r2, native-static-analysis). Analyse MSVC : 50 avertissements,
dont trois localisés dans les sources du projet (getenv et masquage de variable),
pas de prétention « zéro avertissement ». **36 contrôles GPU de contexte,
18 rendu CPU, 36 buffers, 7 géométrie, 6 invariants de profils réussis** ; mélange
de profils rejeté avant compilation. Sources de chaque nouveau build archivées
et empreintes ; SDK/libs externes réutilisés séparément, pas d'installation.

Risques ouverts : VAO/programmes du moteur statique insuffisamment gardés lors
d'un changement de contexte ; couverture causale GD 2.2/rollback des groupes
incomplète ; scan compute global et recomposition alpha globale ; détour atan2f
partagé du chemin normal. Aucun de ces risques non reproduits n'est présenté
comme la cause démontrée des noirs. **Pas de promotion, pas de ×3 FPS démontré.**
Le tableau d'audit reste dans research/R4-09/AUDIT.md ; preuves techniques dans
evidence.json du même dossier. L'audit ne prouve pas l'absence exhaustive de bugs.

## 2026-10-25 — R4-10 : fidélité native V5 et nouvelle base

Hypothèse : reconstruire le shader/MVP dans le pont V4 peut casser le contrat
graphique. V5 conserve le programme actif et tous ses uniformes, les sommets
natifs complets et les indices natifs en lecture seule. Stockage VBO/VAO propre,
sous-plages exactes, époques de contexte et restauration des bindings ; repli
natif si le moteur ou un autre hook ne respecte pas le contrat reconnu.

**218 contrôles GPU, dont 32 scénarios pixels identiques ; 257 contrôles de
lecture/restauration.** Le témoin MVP nul est détecté. Comparaison Heliopolis
auto 129598490, 74–78 %, tous les mods utilisateur, détails complets, sans shake
ni noclip : 189/183 lectures valides, zéro mort, couverture visuelle complète.
Aucun noir complet ni anomalie classée supplémentaire ; un événement gauche
commun aux deux parcours. V5 réellement actif : dernier compteur 5 837 images
de soumission, zéro repli. Les mesures intrusives ne prouvent **aucun gain FPS**.
Un essai antérieur avec erreur GL préexistante est exclu, sa cause reste inconnue.

**Décision : GARDER comme nouvelle base expérimentale, à la demande de l'utilisateur.**
Version v0.3.0-alpha.1 ; alpha.6 gelée dans legacy et sauvegardée localement avec
ses sources et son paquet. Le Bismuth 2.2081 fourni est inspecté sans exécution.
Le dépôt est privé. Les grandes boucles Area/Transform restent natives ; aucune
absence globale de noirs, aucun 60 FPS stable ni ×3 ne sont démontrés.
Le paquet livré garde exactement le DLL et les ressources du parcours validé ;
seuls about/changelog/support sont réécrits. Empreintes et limites : R4-10/evidence.json.

La reconstruction CMake a révélé un générateur de bindings téléchargé plus
récent que les sources sélectionnées. Le générateur est désormais construit
depuis ces mêmes sources, sans faux en-têtes de compatibilité ni modification
du SDK. La vérification de cette chaîne reste distincte du binaire testé en jeu.
Reconstruction CMake et empaquetage standard réussis (34 entrées, identité V5).
Un avertissement C4996 getenv subsiste ; pas de prétention zéro avertissement.
Le paquet de livraison conserve le DLL manuel testé, distinct de ce rebuild.
