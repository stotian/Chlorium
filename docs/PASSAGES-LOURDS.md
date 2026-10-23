# Mesures par pourcentage — 21 octobre 2026

Niveau teste : **133260800, ORBIT AUTO DECO**. Collisions normales, aucun
noclip, aucun input ajoute. Les passes completes recentes atteignent 100 %
sans mort. La mort historique a 89,98 % ne permet donc pas d'affirmer sa cause.

Les premiers releves ont signale 52 %, 83–89 %, puis les passes completes ont
revele des blocages encore plus longs a 96–100 %. Les valeurs varient beaucoup
entre passes ; une variation seule ne prouve pas un gain du mod.

Une passe de diagnostic complete, avant correction du cache, donne ces temps
CPU moyens parmi les images mesurees de chaque plage :

| Plage | Images | Boucle du jeu | Mise a jour | Mouvement | Rotation | Transformation | Effets de zone |
|---|---:|---:|---:|---:|---:|---:|---:|
| 50–54 % | 201 | 26,83 ms | 21,31 ms | 4,77 ms | 4,27 ms | 4,95 ms | 1,94 ms |
| 83–90 % | 101 | 94,33 ms | 88,70 ms | 16,86 ms | 13,24 ms | 21,40 ms | 18,47 ms |
| 96–100 % | 37 | 162,92 ms | 160,98 ms | 14,81 ms | 21,54 ms | 16,82 ms | 84,20 ms |

Ces etapes peuvent etre imbriquees et ne doivent pas etre additionnees comme
des couts independants. Le budget d'une image a 60 FPS est **16,67 ms**. Le temps
GPU est egalement trop eleve dans plusieurs passages, mais il n'explique pas
a lui seul les blocages CPU de plusieurs centaines de millisecondes.

## Corrections de cette iteration

- Suppression d'une interception incorrecte d'une fonction vide du moteur,
  partagee par 96 exports : elle effacait continuellement le cache du mod.
- Cache des plages effectivement connues, allocation bornee et controle exact
  des octets ; 12 lectures du VBO dans le niveau sans divergence.
- Repos de 120 dessins apres quatre changements denses consecutifs. La
  couverture precedente est invalidee avant ce repos ; des octets anciens ne
  peuvent pas etre reutilises lors de la reprise.
- Suppression des hooks et dependances du rendu statique dans le chemin natif
  normal. Le profiler historique et son raccourci restent reserves aux builds
  de developpement qui le demandent explicitement.

Les transferts economises ne suffisent pas a prouver un gain FPS. Les calculs
de mouvement, rotation, transformation et zone restent la cible principale
pour les prochains correctifs CPU. **Les 60 FPS stables ne sont pas atteints.**

Les fichiers CSV/JSON de comparaison conservent les intervalles bruts,
les mesures GPU associees par image, les exclusions et les comptages par
pourcentage. Les captures, pauses, pertes de focus, morts et frontieres de
couverture sont exclues. Un pourcentage avec peu d'images ne fournit pas une
estimation robuste des percentiles.
