<div align="center">
  <img src="logo.png" width="144" alt="Logo Chlorium" />
  <h1>Chlorium</h1>
  <p>Renderer expérimental CPU/GPU pour Geometry Dash.</p>
  <p><strong>Windows x64 · GD 2.2081 · Geode 5.11.2 · v0.3.0-alpha.6</strong></p>
</div>

Chlorium est un fork indépendant de [Bismuth, créé par IliasHDZ](https://github.com/iliasHDZ/Bismuth).
L'objectif est de réduire fortement le travail CPU/GPU, avec tous les effets
visuels et le gameplay conservés. Sodium inspire l'objectif ; aucun code Sodium
n'est intégré.

**La base actuelle est le coeur canonique V10.** Les anciennes soumissions
privees V4-V9 restent disponibles dans les experiences, hors du build courant.
L'ancien cache mathematique `v0.2.0-alpha.6` reste archive dans
[legacy](legacy/README.md) ; il est distinct de `v0.3.0-alpha.6`.

## Etat reel

Cocos conserve integralement ses dessins, shaders, uniformes, indices et buffers.
Chlorium n'intercepte plus les dessins : aucun stockage de sommets prive, scan
ou copie par atlas, remplacement de VAO ou restauration d'un ancien etat GL.
Cela retire les mecanismes V5 dont les tests isoles reproduisent la divergence
des etats GL et la perte de donnees GPU absentes de la copie CPU.

L'optimisation active reutilise les quotients identiques dans une routine Area
native dont le corps complet doit correspondre a GD 2.2081. Elle ne remplace
aucun trigger, callback de gameplay, effet, collision ou pas de simulation.
Un corps inconnu est refuse. Les trois substitutions forment un seul patch
contigu ; toute erreur d'installation ou de bascule est signalee.
Le garde de pause sur PlayLayer detache est conserve.

Le candidat est compile et verifie hors jeu. Les comparaisons numeriques
portent sur les bits et flags SSE avec exceptions masquees ; les traps non
masques ne sont pas valides. Aucun gain FPS, absence universelle de noirs ou
compatibilite avec tous les mods n'est declare sans validation en jeu.
Les prototypes de groupes/poses restent separes du paquet faute de preuve de
gain integre et de contrat complet pour les callbacks natifs.

## Construire

Prérequis : Windows x64, MSVC 2022, CMake/Ninja et SDK Geode 5.11.2.
Les scripts locaux indiquent les chemins du poste de développement.

- `build-local.cmd` construit le coeur canonique V10 par defaut.
- `package-local.ps1` verifie l'identite canonique V10 et crée le paquet dans `dist/`.
- `research/R4-04/build-quad.cmd --native-vertex` utilise les dépendances locales
  deja compilees pour le V5 historique, dans une sortie isolee avec empreintes. Ce script ne construit pas V10.
- `research/R4-04/build-quad.cmd --canonical` reutilise les dependances locales
  avec leurs empreintes et construit V10 dans une sortie separee.
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
