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
