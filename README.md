<div align="center">
  <img src="logo.png" width="144" alt="Logo Chlorium" />
  <h1>Chlorium</h1>
  <p>Optimiser Geometry Dash en conservant ses effets et son gameplay.</p>
  <p><strong>Windows x64 · GD 2.2081 · Geode 5.11.2 · alpha.5</strong></p>
</div>

Chlorium est une adaptation indépendante de
[Bismuth, créé par IliasHDZ](https://github.com/iliasHDZ/Bismuth).
Son ambition s'inspire de Sodium : réduire le travail inutile du moteur pour
obtenir des images plus régulières, sans retirer les décorations ou les effets.

**Le projet est expérimental. Les 60 FPS stables et l'absence de défauts visuels
sur tous les niveaux ne sont pas encore démontrés.** Les résultats variables et
les régressions restent documentés ; une bonne moyenne ne suffit pas à valider
la fluidité.

[Télécharger une prerelease](https://github.com/stotian/Chlorium/releases) · [Versions](docs/VERSIONS.md) ·
[Changements](changelog.md) · [Mesures récentes](docs/VALIDATION-20261023.md) ·
[Signaler un problème](https://github.com/stotian/Chlorium/issues/new/choose)

## Ce que fait la candidate actuelle

La version normale **v0.2.0-alpha.5** conserve les transformations, la visibilité,
les collisions, les shaders, le tri et les dessins natifs de GD. Elle tente de
réduire les transferts GPU redondants en comparant exactement les données des
atlas et de réutiliser certains résultats mathématiques natifs identiques.

Le cache est borné : 32 Mio au total, 512 Kio par atlas et un budget de travail
supplémentaire de 2 Mio / 64 opérations par image. Les changements denses
provoquent un retour progressif aux transferts natifs. Les données dont la
présence sur le GPU n'est pas établie conservent le chemin natif.

Le remplacement complet du rendu hérité de Bismuth reste dans les sources,
**désactivé dans les paquets normaux** : sa couverture des effets GD 2.2 est
incomplète. Les alternatives Area, streaming et matrices sont des expériences
distinctes, détaillées dans [le journal technique](docs/EXPERIMENTS-20261022.md).

## Installer

1. Utiliser **Geometry Dash 2.2081, Windows x64**, avec **Geode 5.11.2**.
2. Télécharger le fichier `.geode` de la [prerelease](https://github.com/stotian/Chlorium/releases), puis
   l'importer avec Geode ou le placer dans `Geometry Dash/geode/mods/`.
3. Activer Chlorium dans Geode et redémarrer le jeu.

L'identifiant du mod est `local.chlorium`. Installer un seul paquet Chlorium
à la fois. Les expériences utilisent ce même identifiant et ne se cumulent pas.
Le paquet normal ne contient ni noclip, ni automatisation des inputs, ni
module de benchmark. La version 2.2081 est la version exacte vérifiée sur ce PC ;
les autres plateformes et binaires ne sont pas validés.

## Versions et état réel

| Branche de travail | Version Geode | État |
|---|---|---|
| Candidate normale | `v0.2.0-alpha.5` | Référence actuelle ; optimisations encore en validation |
| Streaming GPU | `v0.2.0-alpha.601` | Expérience isolée ; gain synthétique, pas de gain général prouvé |
| Arithmétique Area v2 | `v0.2.0-alpha.602` | Équivalence numérique vérifiée ; intérêt variable selon les données |
| Matrices v3 | `v0.2.0-alpha.603` | Aucun appel éligible sur Grief 22–40 %, donc aucune accélération possible de ce passage |

Les anciennes alpha.1 à alpha.4 sont des étapes du développement local,
décrites dans [l'historique](changelog.md). Le dépôt ne leur attribue pas de
fausses archives ou de faux tags rétroactifs.

## Vérifications récentes

Le 23 octobre 2026, des comparaisons visuelles avec et sans alpha.5 ont été
effectuées sur **Grief auto jusqu'à 40 %** et **Heliopolis auto jusqu'à la fin**.
Elles utilisent **LDM désactivé, secousses désactivées**, un compteur FPS,
les libellés AVEC/SANS Chlorium et les collisions natives.

Chaque épisode quasi noir détecté avec Chlorium possède un équivalent observé
sans le mod au même passage ou à proximité. Cela ne signifie pas que tous les
écrans noirs signalés sont corrigés. Ces essais isolés ne couvrent pas toutes
les interactions avec les autres mods. Heliopolis a eu une mort au tout début
avec Chlorium, puis a atteint la fin ; la référence sans Chlorium n'a pas eu
de mort.

Les FPS relevés pendant la surveillance visuelle incluent son coût. Ils ne
constituent pas une preuve de gain en jeu normal. Les limites, les compteurs
et les comparaisons numériques sont dans
[le bilan de validation](docs/VALIDATION-20261023.md).

## Compiler

Prévoir Visual Studio 2022 avec les outils C++ x64, CMake, Ninja, Python 3,
Git et le [SDK Geode 5.11.2](https://github.com/geode-sdk/geode/tree/v5.11.2).
Dans un terminal de développement **x64** Visual Studio, avec `GEODE_SDK`
pointant vers le SDK :

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DGEODE_SDK="$env:GEODE_SDK" `
  -DGEODE_TARGET_PLATFORM=Win64 `
  -DGEODE_DISABLE_CLI_CALLS=ON `
  -DGEODE_DISABLE_PRECOMPILED_HEADERS=ON `
  -DGEODE_DONT_INSTALL_MODS=ON
cmake --build build --target Chlorium --parallel 3
./package-local.ps1
```

Le paquet est créé dans `dist/` sans installation automatique. CMake récupère
GLM et glslang aux révisions fixées dans `CMakeLists.txt`. Le script
`build-local.cmd` est un raccourci propre au PC de développement actuel ; ses
chemins locaux ne remplacent pas les prérequis ci-dessus.

## Tests et diagnostic

Les vérifications CPU des plages d'atlas et du cache mathématique sont lancées
par la CI Windows sans redistribuer le jeu. Les scripts `test-*.cmd` servent
aux contrôles locaux ; certains exigent le SDK, OpenGL ou les DLL du jeu
installé. Les tests GPU et les parcours en jeu ne s'exécutent pas dans la CI.

Le harnais privé se compile avec `build-local.cmd --comparison-audit` et se
package séparément avec `package-local.ps1 -Audit`. Il mesure un paquet normal
réellement présent ou absent, avec FPS et mode visibles. Les seuls niveaux
autorisés par cet outil sont :

- Orbit auto : **133260800** ;
- Heliopolis auto : **129598490** ;
- Grief auto : **88952972**.

Il atteint les sections par le gameplay natif, sans saut de position ni noclip,
et s'arrête à la cible, à la fin ou après deux morts. Exemple local :

```powershell
./tools/run-runtime-audit.ps1 -Native -Level 88952972 `
  -FromPercent 22 -ToPercent 40 `
  -Package ./measurement-noshake-dist/local.chlorium.audit.geode `
  -ReportDirectory ./audit-artifacts/grief
```

`-VisualWatch` inspecte l'image finale présentée ; ces parcours servent au
diagnostic visuel et doivent être séparés des mesures de performances normales.
`tools/diagnostic-report.py` produit les tableaux par pourcentage, les épisodes
sombres et leurs captures. Peaceful et LIMBO sont exclus des essais actuels.

## Licence et crédits

Le code original, les shaders concernés et les données de maillage sont
attribués à **IliasHDZ / Bismuth**. Les contributions Chlorium sont identifiées
dans [NOTICE.md](NOTICE.md) et l'historique des changements.

La notice Bismuth déclare GPLv3 ou ultérieure **avec une restriction
supplémentaire**. Son texte exact est conservé dans
[LICENSES/Bismuth-LICENSE.md](LICENSES/Bismuth-LICENSE.md), accompagné du texte
GPL complet dans [COPYING](COPYING). Voir [LICENSE.md](LICENSE.md) et les
[notices des dépendances](THIRD_PARTY_NOTICES.md). Aucune licence MIT globale
n'est substituée aux conditions héritées.

Ce projet n'est affilié ni à Bismuth, ni à Sodium, ni à Geode ou RobTop.
Le logo Chlorium a été généré par IA. Les binaires du jeu, sauvegardes,
ressources propriétaires et autres mods restent hors du dépôt.
