# Chlorium v0.3

Renderer expérimental CPU/GPU pour GD 2.2081 Windows et Geode 5.11.2.
Fork indépendant de Bismuth, créé par IliasHDZ.

Le renderer expérimental est désormais la base du projet. L'ancienne alpha.6
mathématique est archivée sous legacy-chlorium.
Le nouveau chemin conserve le programme graphique, ses uniformes, les sommets
complets et les indices natifs. Les effets et collisions restent natifs.

32 scénarios GPU contrôlés reproduisent les pixels de référence. La comparaison
Heliopolis 74–78 % avec les mods personnels n'a pas détecté de noir supplémentaire.
Cela ne prouve pas l'absence de défaut sur tous les niveaux, ni un gain FPS global.
Les prochains gains ciblent les parcours CPU répétés et les traitements GPU par groupes.

Voir README.md, docs/EXPERIMENTS-V5.md et les licences/notices du projet.
Chlorium n'est pas une version officielle de Bismuth et ne contient pas de code Sodium.
