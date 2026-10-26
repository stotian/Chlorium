<div align="center">
  <img src="logo.png" width="144" alt="Logo Chlorium" />
  <h1>Chlorium</h1>
  <p>Renderer expérimental CPU/GPU pour Geometry Dash.</p>
  <p><strong>Windows x64 · GD 2.2081 · Geode 5.11.2 · v0.3.0-alpha.1</strong></p>
</div>

Chlorium est un fork indépendant de [Bismuth, créé par IliasHDZ](https://github.com/iliasHDZ/Bismuth).
L'objectif est de réduire fortement le travail CPU/GPU, avec tous les effets
visuels et le gameplay conservés. Sodium inspire l'objectif ; aucun code Sodium
n'est intégré.

**Le renderer expérimental est désormais la base du projet.** L'ancien chemin
alpha.6, centré sur des caches mathématiques, est arrêté et rangé dans
[legacy](legacy/README.md). Les anciennes releases alpha.6 ne correspondent pas
à cette nouvelle base.

## État réel

Le renderer V5 garde la préparation native, le programme graphique actif et
ses uniformes. Il soumet les sommets natifs complets depuis un stockage propre
et réutilise les indices natifs sans les modifier. Il vise � conserver le contexte,
la texture, le blending, la profondeur et les sous-plages.
Un autre hook d'atlas ou un contrat moteur inconnu impose le chemin natif.

Les tests GPU reproduisent les pixels de référence dans 32 scénarios contrôlés.
Une comparaison ciblée sur Heliopolis auto, 74–78 %, avec les mods utilisateur,
a fourni 189/183 captures valides, sans noir complet ni anomalie supplémentaire
détectée avec V5. Le renderer était actif, sans repli natif. Ces diagnostics
ne mesurent pas un gain FPS et ne couvrent pas tous les effets/niveaux/mods. Les écrans noirs
restent la priorité de validation. **Ni l'absence universelle de défauts visuels,
ni les 60 FPS stables, ni un ×3 FPS ne sont encore démontrés.**

Le renderer ne réduit pas encore les grandes boucles Area/Transform du moteur.
Les prochains gains doivent venir de programmes de groupes/poses, de traitements
par lots et d'une sélection spatiale du travail GPU, sans supprimer de logique.
Les noyaux expérimentaux ne constituent pas des gains FPS tant qu'ils ne sont
pas intégrés et mesurés sur le même passage.

## Construire

Prérequis : Windows x64, MSVC 2022, CMake/Ninja et SDK Geode 5.11.2.
Les scripts locaux indiquent les chemins du poste de développement.

- `build-local.cmd` construit la nouvelle base V5 par défaut.
- `package-local.ps1` vérifie l'identité V5 et crée le paquet dans `dist/`.
- `research/R4-04/build-quad.cmd --native-vertex` utilise les dépendances locales
  déjà compilées, dans une sortie isolée avec empreintes et archive des sources.
- `test-local.cmd` et les fixtures GPU vérifient les ressources et le rendu.

La compilation CMake neuve active `CHLORIUM_EXPERIMENTAL_NATIVE_QUADS` et
`CHLORIUM_NATIVE_VERTEX_PARITY`. Reconfigurer les anciens caches CMake : un cache
conservant explicitement OFF ne bascule pas automatiquement. Les scripts ne
lancent pas GD et n'installent pas de mod lors d'une compilation.

## Résultats et recherche

[Journal unique](docs/EXPERIMENTS-V5.md) · [Audit et priorités](research/R4-09/AUDIT.md) ·
[Versions](docs/VERSIONS.md) · [Référence Bismuth fournie](docs/BISMUTH-REFERENCE-20261025.md).
Les mesures distinguent résultat observé, estimation, test hors jeu et parcours
invalide. Les fichiers de jeu, sauvegardes, configurations privées et builds
locaux ne doivent pas être envoyés au dépôt.

## Licence et attribution

Lire [LICENSE.md](LICENSE.md), [COPYING](COPYING), [NOTICE.md](NOTICE.md) et
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Les notices originales Bismuth
sont conservées. Chlorium n'est pas une version officielle de Bismuth.
