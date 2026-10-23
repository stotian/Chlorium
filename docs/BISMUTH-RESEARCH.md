# Recherche Bismuth et changements Chlorium — 21 octobre 2026

## Ce que Bismuth cherche réellement à accélérer

Le projet ne se limite pas à filtrer les objets hors écran. Son objectif est
de réécrire le dessin et de déplacer une partie des transformations et de la
gestion des couleurs du processeur vers la carte graphique. Pour limiter les
pixels dessinés inutilement, il utilise aussi une géométrie adaptée aux sprites.

Le mécanisme central de `GroupManager` regroupe les objets par combinaison
d'identifiants de groupes : une transformation commune est calculée par
combinaison, puis appliquée aux objets en parallèle dans les shaders. Cela
évite de refaire toute l'opération individuellement pour chaque décoration.

Sources primaires consultées :

- [Présentation de l'auteur](https://github.com/iliasHDZ/Bismuth/blob/master/about.md)
- [Principe du GroupManager](https://github.com/iliasHDZ/Bismuth/blob/master/src/renderer/GroupManager.hpp)
- [Intégration dans les mises à jour natives](https://github.com/iliasHDZ/Bismuth/blob/master/src/renderer/hooks.cpp)
- [Organisation du rendu](https://github.com/iliasHDZ/Bismuth/blob/master/src/renderer/Renderer.cpp)
- [Calcul et sélection des objets](https://github.com/iliasHDZ/Bismuth/blob/master/resources/shaders/objectCulling.comp)

## Limites confirmées en amont

Le TODO laisse les déclencheurs Scale et Area inachevés. La présentation avertit
que les niveaux 2.2 ne s'affichent pas correctement. L'issue 10 décrit aussi des
textes absents, propriétés de groupes perdues et décorations disparaissant
après changement de position de départ ou checkpoint. L'auteur confirme que
ces cas ne sont pas encore pris en charge. Une mise à jour de version seule
ne peut donc pas établir la fidélité visuelle.

- [TODO](https://github.com/iliasHDZ/Bismuth/blob/master/TODO.md)
- [Issue 10 et réponse de l'auteur](https://github.com/iliasHDZ/Bismuth/issues/10)

## Travail concret dans Chlorium

Le rendu expérimental est conservé dans les sources, mais n'est pas activé dans
le paquet compatible. Une optimisation du transfert des atlas ne résout pas à
elle seule le travail CPU ciblé par Bismuth. Les traces d'Orbit identifient
notamment de très lourdes mises à jour Area et Transform en fin de parcours.

La version alpha.3 corrige une corruption de cache reproduite sur le GPU :
`mapBuffers` pouvait remplacer son contenu sans modifier ses identifiants.
Le test observe 102 au lieu de 77 avant correction, puis 77 après correction.
Cette preuve porte sur cette corruption précise, pas sur tous les écrans noirs.

La candidate alpha.4 ajoute une optimisation CPU réelle dans Transform : un
cache borné des résultats CRT `atan2f` identiques, limité aux deux appels natifs
identifiés dans la décomposition des matrices. Il conserve les bits des
entrées/résultats et les contrôles d'arrondi, distingue les zéros signés et
évite les cas exceptionnels. Un faible taux de réutilisation désactive le cache
pour la phase en cours. Les mises à jour d'objets et collisions natives restent
exécutées. L'empreinte du binaire et l'import CRT doivent correspondre avant
toute installation du changement.

Les microbenchmarks vérifient l'intérêt des répétitions et le risque des entrées
toutes différentes. Ils ne constituent pas une preuve de gain FPS. Les fichiers
`native_transform_math_*` contiennent les vérifications et résultats bruts.
Les mesures en jeu doivent encore établir le gain et ses limites.

Les accès aux groupes et le changement de section possèdent déjà des chemins
rapides dans GD 2.2081 : ajouter un cache supplémentaire à ces endroits n'est
pas une optimisation justifiée. Aucune simplification des effets Area n'est
activée sans avoir établi son équivalence avec le calcul natif.

## Critères de validation

Comparer le même jeu et le même module de mesure avec le paquet normal présent
ou absent. Séparer vérification exacte et mesure de performance. Rapporter les
frametimes par pourcentage et tentative, les dépassements de 16,67 ms, les morts,
la couverture et les images sombres effectivement présentées. Les passages
sombres observés aussi sans mod ne prouvent pas une régression du mod.

Niveaux autorisés : Orbit auto 133260800, Heliopolis auto 129598490 et Grief
auto 88952972. Arrêt à la fin normale ou après deux morts. Aucun noclip.
