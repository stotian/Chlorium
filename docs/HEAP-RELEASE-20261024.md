# Liberation de caches memoire inutilises — 24 octobre 2026

Experience alpha.607 isolee, avec calculs et rendu natifs. Elle ne contient
aucun des caches mathematiques precedents et ne remplace pas l'alpha.6.

## Motif et methode

Les releves Orbit montrent parfois moins de 400 Mio de RAM physique libre.
Ce constat ne suffit pas a attribuer les pics au fichier d'echange. Une petite
experience mesure donc la memoire rendue par les caches libres de l'allocateur.

Une seule operation est mise en file apres la construction du niveau. Une
reference faible verifie que le PlayLayer existe encore et est actif. Le
reglage enabled est respecte. Aucun travail periodique n'est ajoute au rendu.

L'appel documente [HeapSetInformation / HeapOptimizeResources](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapsetinformation)
avec un heap nul optimise les caches LFH du processus courant et peut rendre
de la memoire inutilisee. Sa [structure de parametres](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-heap_optimize_resources_information)
utilise la version 1 et des flags a zero. Aucun asset n'est purge ; ni
EmptyWorkingSet, ni reduction forcee du working set, ni modification d'un
autre processus n'est effectuee.

## Verifications et resultat

- Compilation du prototype reussie.
- Fixture autonome : 16 384 allocations variees, liberation partielle,
  optimisation, puis verification de chaque octet des 2 048 blocs encore
  vivants. Aucun changement detecte. Appel reussi ; environ 0.65 Mio rendus.
- Dans Orbit auto avec les mods personnels, parcours de 0 a 1 % termine sans
  mort ni crash. Appel reussi, erreur zero, deux mesures memoire valides.
- Memoire privee avant : 2 682 195 968 octets ; apres : 2 668 232 704 octets.
  Difference : 13 963 264 octets, soit 13.316 Mio.
- Working set avant : 1 527 894 016 octets ; apres : 1 514 827 776 octets.
- Duree de l'operation : 1.8037 ms, au chargement.

Cette economie est modeste. Elle ne justifie pas de multiplier les longs
parcours pour esperer une acceleration importante sur cette seule piste.
Aucun gain FPS, verdict sur la fin d'Orbit ou validation visuelle complete
n'est deduit de cet essai de chargement. Le prototype reste experimental.

L'API ChloriumReadHeapReleaseStats expose une structure versionnee de 64
octets : nombre d'appels, succes/erreur, validite des mesures, memoire avant
et apres, duree. Le journal du jeu contient aussi une seule ligne numerique
par operation, conservee dans les donnees locales du diagnostic.
