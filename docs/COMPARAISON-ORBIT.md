# Comparaison complete — ORBIT AUTO DECO 133260800

Les deux passes atteignent 100 %, sans mort, avec collisions normales et sans input ajoute. Le candidat ne demontre pas de gain global : 52,84 FPS moyens mesures contre 53,84 pour le controle natif. Les differences entre passages varient beaucoup ; une seule paire de passes ne permet pas une attribution fiable. Les 60 FPS stables ne sont pas atteints.

Budget 60 FPS : 16,67 ms par image. Les moyennes excluent les captures, pauses, pertes de focus, morts et frontieres de couverture. Les intervalles bruts restent dans les CSV. Le GPU conserve une requete non resolue en fin de passe.

| Pourcentage | Images candidat | FPS candidat | Retard p95 candidat | Retard p95 natif |
|---|---:|---:|---:|---:|
| 50 % | 87 | 66.8 | 18.9 ms | 19.2 ms |
| 51 % | 77 | 55.2 | 34.3 ms | 60.6 ms |
| 52 % | 12 | 8.7 | 145.8 ms | 394.1 ms |
| 53 % | 47 | 35.6 | 31.9 ms | 273.9 ms |
| 83 % | 28 | 20.8 | 91.3 ms | 97.4 ms |
| 84 % | 15 | 10.9 | 167.3 ms | 169.0 ms |
| 85 % | 14 | 10.2 | 126.7 ms | 132.7 ms |
| 86 % | 10 | 7.4 | 191.9 ms | 179.0 ms |
| 87 % | 20 | 12.8 | 185.0 ms | 72.9 ms |
| 88 % | 5 | 4.1 | 255.5 ms | 80.1 ms |
| 89 % | 6 | 3.9 | 298.6 ms | 97.1 ms |
| 90 % | 2 | 2.9 | 331.4 ms | 65.5 ms |
| 91 % | 6 | 4.5 | 237.5 ms | 63.5 ms |
| 92 % | 9 | 7.4 | 159.4 ms | 52.8 ms |
| 93 % | 19 | 13.6 | 167.3 ms | 54.8 ms |
| 94 % | 36 | 26.5 | 43.5 ms | 48.0 ms |
| 95 % | 40 | 29.8 | 55.5 ms | 37.5 ms |
| 96 % | 21 | 13.4 | 224.3 ms | 309.2 ms |
| 97 % | 4 | 3.0 | 345.5 ms | 493.4 ms |
| 98 % | 5 | 3.4 | 325.1 ms | 887.6 ms |
| 99 % | 3 | 2.1 | 458.3 ms | 1507.0 ms |

Le dernier intervalle a 100 % ne contient qu’une image de fin : il ne fournit pas une estimation statistique robuste. Certains pourcentages lourds ont egalement tres peu d’images.

Corrections confirmees : suppression du hook de fonction vide partagee par 96 exports, cache desormais actif, 12 verifications de donnees GPU dans le niveau sans divergence, repos des lots trop animes, suppression du rendu statique et de ses hooks dans le paquet normal.

Les calculs natifs de mouvement, rotation, transformation et effets de zone dominent encore les passages lourds. Le rendu des captures a 80 % a ete inspecte dans les deux modes ; cela ne prouve pas une equivalence visuelle universelle.
