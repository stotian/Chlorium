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

**La recherche active porte sur le renderer GPU continu R57–R61 et la chaîne de
groupes GPU R62. Aucun candidat
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

Les calculs CPU natifs des objets et triggers restent exécutés. Le transfert
complet de leur logique sur GPU, objectif de Bismuth, **n'est pas terminé**.
Les sauvegardes de framebuffer par lot restent également payées.

## Validation et limites

Les contrôles CPU et 345 cas GPU isolés passent pour la chaîne récente, avec des
contrôles supplémentaires de transfert différé et de restauration après erreur.
Ils ne prouvent pas la fidélité de tous les effets GD 2.2 ni l'absence universelle
d'écrans noirs.

Une paire sur Heliopolis auto, section 74–78 %, a donné :

| Renderer | Images retenues | FPS de la section | Frametime p95 |
|---|---:|---:|---:|
| R57 | 82 | 15,1 | 124,6 ms |
| R59 | 97 | 17,8 | 105,4 ms |

Même auditeur, détails complets, 120 FPS demandés, sans shake ni noclip, mods isolés.
**Une seule paire ne démontre pas un gain reproductible. Le résultat reste très
insuffisant pour 120 FPS.** Les démarrages incomplets et le passage R60 interrompu
vers 51,6 % ne sont pas comptés comme des tests complets.

Les binaires, captures, journaux privés, sauvegardes et données de GD restent hors
Git. Aucun de ces essais ne remplace automatiquement l'installation principale.

## Construire

Prérequis : Windows x64, MSVC 2022 et SDK Geode 5.11.2. Les scripts locaux indiquent
les chemins du poste de développement. Ils compilent sans lancer ni installer GD.

```bat
research\R4-04\build-quad.cmd --resident-deferred
python research\R4-04\package_private_fixture.py native-resident-deferred-r59
```

Le paquet privé est créé sous `build-research/native-resident-deferred-r59/`.
`--resident-timed` construit le diagnostic R60 ; `--continuous-measure-r60`
construit son auditeur indépendant. `--resident-direct` construit le diagnostic
R61 (`native-stream-direct-r61`). Les essais en jeu utilisent le wrapper protégé
et un plan local explicite avec empreintes, sauvegarde et restauration vérifiée.

`build-local.cmd`, `package-local.ps1` et `--canonical` conservent le V10 historique
comme référence. Ils ne construisent pas le nouveau renderer continu.

## Suivi

[Journal des expériences](docs/EXPERIMENTS-V5.md) · [Versions](docs/VERSIONS.md) ·
[Référence Bismuth](docs/BISMUTH-REFERENCE-20261025.md).

Niveaux autorisés pour les mesures : Orbit auto 133260800, Grief auto 88952972,
Heliopolis auto 129598490 et Society auto 139648964. Deux morts maximum par passage.

## Licence et attribution

Lire [LICENSE.md](LICENSE.md), [COPYING](COPYING), [NOTICE.md](NOTICE.md) et
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Les notices originales Bismuth
sont conservées. Chlorium n'est pas une version officielle de Bismuth.
