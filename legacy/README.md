# Legacy Chlorium

L'alpha.6 et son cache mathématique ne sont plus la base du projet.
Le développement actif porte sur le nouveau renderer CPU/GPU.

Le fichier `chlorium/NativeTransformCache.cpp` est conservé uniquement pour
les anciens essais et le retour arrière. Il n'est pas compilé dans le nouveau
profil par défaut. Les en-têtes mathématiques partagés restent dans src/renderer.

Référence complète de l'ancien projet : commit
`2312559c47fdaea2c03ee2934c3db8cbd77e45ac`, à conserver sur la branche
`legacy/chlorium-alpha6`. Le paquet local alpha.6 et son instantané de sources
sont archivés dans Documents/chlorium/legacy-chlorium.

La conservation historique ne signifie pas qu'un gain FPS était démontré.
Les notices Bismuth et licences du projet continuent à s'appliquer.
