<div align="center">
  <img src="logo.png" width="144" alt="Logo Chlorium" />
  <h1>Chlorium</h1>
  <p>Renderer expérimental CPU/GPU pour Geometry Dash.</p>
  <p><strong>Windows x64 · GD 2.2081 · Geode 5.11.2</strong></p>
</div>

Chlorium est un fork indépendant de [Bismuth, créé par IliasHDZ](https://github.com/iliasHDZ/Bismuth).
L'objectif est un rendu plus rapide avec tous les effets et le gameplay conservés,
en visant 120 FPS stables. Aucun code Sodium n'est intégré.

## État actuel

**La recherche active porte sur les programmes mixtes GPU R74/R75 et leur
couplage à la géométrie native, avec R67 comme référence en jeu. Aucun candidat
n'atteint encore les critères de promotion dans une version normale.**

- R57 dessine réellement les lots d'objets pendant la partie avec du stockage GPU
  persistant. Les menus et les cas refusés conservent le rendu natif.
- R58 réutilise les indices vérifiés et l'encodage, conserve des tables d'identités
  réutilisables et évite le second dessin GPU natif. Une fuite de rendu conditionnel
  après une erreur OpenGL préexistante a été reproduite puis corrigée.
- R59 diffère les transferts natifs de sommets redondants. Le drapeau natif de mise
  à jour reste armé pour que le retour natif rétablisse les données nécessaires.
- R60 mesure séparément la préparation, les sources, les transferts, les sauvegardes
  et les dessins. C'est un diagnostic, pas un résultat de performance comparable.
- R61 retire le programme graphique privé inutilisé du chemin à sommets GPU.
  Les contrôles isolés passent ; son diagnostic Heliopolis atteint 78 %, mais
  ne prouve aucun gain reproductible. Il reste séparé et non promu.
- R62 fusionne les déplacements ordonnés des groupes et la production des quads,
  puis dessine avec le programme Cocos. Les positions restent sur le GPU : seules
  les commandes de groupes sont transférées à chaque image. Prototype fermé hors
  GD, non intégré au mod ; effets Area, hiérarchies et lecteurs CPU non qualifiés.
- R63 observe deux images avec Chlorium actif, puis relit les objets encore
  présents à la fin de chacune. La capture Heliopolis complète vérifie la continuité
  des poses pour préparer l'adaptateur GPU ; elle ne désactive aucun calcul natif
  et ne constitue pas une comparaison de performances.
- R64 maintient les poses sur GPU avec des programmes de groupes partagés.
  Les mouvements enregistrés de 27 867 objets passent les contrôles natifs, mais
  ce composant n'enlève pas encore les lecteurs CPU dans GD.
- R65 fait dessiner Cocos directement depuis des pages de sommets cohérentes,
  avec ses shaders et callbacks d'origine. Il retire la reconstruction et les
  sauvegardes de framebuffer de R61. Le retour natif reste disponible.
- R66 adapte les pages aux lots, sous la même borne mémoire. Quatre passages
  Heliopolis ne montrent pas de gain convaincant et certains pics empirent :
  ce chemin reste isolé.
- R67 vérifie le contexte avant les appels de buffers et conserve le contenu
  canonique natif pour les autres dessins GPU. Ce dernier point corrige un défaut
  reproduit en fixture ; les transferts natifs restent payés. Partiels et cas
  refusés restent natifs. Les effets et le gameplay restent exécutés par GD.
- R68 regroupe les calculs CPU par programme. Son noyau isolé est rapide, mais
  republier les objets annule le gain. Aucun hook de jeu n'utilise ce prototype.

Les calculs CPU natifs des objets et triggers restent exécutés. Le transfert
complet de leur logique sur GPU, objectif de Bismuth, **n'est pas terminé**.
Les prototypes directs s'activent uniquement dans le profil de mods isolé validé.

## Validation et limites

Les contrôles CPU et 345 cas GPU isolés passent pour la chaîne récente, avec des
contrôles supplémentaires de transfert différé et de restauration après erreur.
Ils ne prouvent pas la fidélité de tous les effets GD 2.2 ni l'absence universelle
d'écrans noirs.

Référence historique, avant les corrections R67 : quatre passages Heliopolis
auto 74–78 %, ordre R65/R61/R61/R65 :

| Renderer Chlorium | FPS, deux passages | Frametime p95 | Pic le plus lent |
|---|---:|---:|---:|
| R61 sans timing détaillé | 16,76 / 15,29 | 96,76 / 98,34 ms | 200,01 ms |
| R65 direct | 29,10 / 33,21 | 44,84 / 40,99 ms | 52,56 ms |

Même auditeur, détails complets, 120 FPS demandés, sans shake ni noclip, mods isolés.
Le p95 baisse dans chacune des quatre tranches de pourcentage. Le contrôle ayant
oublié son shader est exclu. [Preuves et limites](research/R4-65/helio-comparison.json).
**Gain mesuré face à l'ancien Chlorium sur cette section uniquement : aucun test
sans mod dans cette campagne, aucune preuve de 120 FPS ni de fidélité visuelle
globale.** Les calculs CPU natifs restent coûteux. Les diagnostics visuels sont
séparés des passages de performance.

Les binaires, captures, journaux privés, sauvegardes et données de GD restent hors
Git. Aucun de ces essais ne remplace automatiquement l'installation principale.

## Construire

Prérequis : Windows x64, MSVC 2022 et SDK Geode 5.11.2. Les scripts locaux indiquent
les chemins du poste de développement. Ils compilent sans lancer ni installer GD.

```bat
research\R4-04\build-quad.cmd --direct-pages-r67
python research\R4-04\package_private_fixture.py native-direct-pages-r67
```

Le paquet privé est créé sous `build-research/native-direct-pages-r67/`.
`--resident-timed` construit le diagnostic R60 ; `--continuous-measure-r60`
construit son auditeur indépendant. `--resident-direct` construit le diagnostic
R61 (`native-stream-direct-r61`). Les essais en jeu utilisent le wrapper protégé
et un plan local explicite avec empreintes, sauvegarde et restauration vérifiée.

`build-local.cmd`, `package-local.ps1` et `--canonical` conservent le V10 historique
comme référence. Ils ne construisent pas le nouveau renderer continu.

Le moteur de programmes mixtes GPU R71/R74 reste un composant isolé. Sur une
capture réelle Heliopolis, 78 objets / deux images produisent 17 004 positions
exactes, avec une seule initialisation des positions. Les calculs natifs, les
lecteurs CPU et le rendu du jeu restent présents : aucun gain FPS n'en découle
encore. Le transfert R73 a régressé en jeu et n'est pas activé.

R75 capture aussi les quads natifs et prépare leur émission depuis les poses GPU.
Sur une capture Heliopolis, 18 quads visibles sont reproduits exactement en
rejeu GPU et 40 autres conservent leur sortie native. Les calculs CPU restent payés.
Sur Orbit, des transformations intercalées restent absentes du programme : les
objets concernés sont refusés et restent natifs. Le découpage des longues séquences
passe 1 540 états natifs/GPU en fixture, sans démontrer un gain en jeu.
[Résultats et limites R75](research/R4-75/DECISION.md).

R76 ajoute les mouvements de suivi natifs au programme GPU. Les échantillons
Orbit, Grief et Heliopolis passent les comparaisons de positions/quads en rejeu,
avec replis natifs pour les objets refusés. Le CPU du jeu reste actif : cela
ne démontre pas encore d'accélération. [Preuves R76](research/R4-76/DECISION.md).

R78 P0 relie le prototype de racines GPU au draw natif Cocos seulement sur une
atlas complet admissible. En cas d'échec GPU, il force la remise à jour du VBO
depuis les quads natifs avant le repli. Le fixture local Cocos passe 128 images
exactes et 8 échecs GPU simulés. Le profil reste expérimental : pas de validation
dans Geometry Dash ni de gain FPS mesuré. [Détails et limites R78](research/R4-78/DECISION.md).

## Suivi

[Journal des expériences](docs/EXPERIMENTS-V5.md) · [Versions](docs/VERSIONS.md) ·
[Référence Bismuth](docs/BISMUTH-REFERENCE-20261025.md).

Niveaux autorisés pour les mesures : Orbit auto 133260800, Grief auto 88952972,
Heliopolis auto 129598490 et Society auto 139648964. Deux morts maximum par passage.

## Licence et attribution

Lire [LICENSE.md](LICENSE.md), [COPYING](COPYING), [NOTICE.md](NOTICE.md) et
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Les notices originales Bismuth
sont conservées. Chlorium n'est pas une version officielle de Bismuth.
