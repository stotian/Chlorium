# Objectif de Chlorium

Exigences utilisateur, mises a jour le 21 octobre 2026 :

- Optimiser Geometry Dash 2.2081 / Geode 5.11.2 sur Windows, sur tous les niveaux.
- Conserver les details, couleurs, textes, effets, animations, collisions et
  comportement du jeu. Aucune reduction de qualite ni suppression d'effets.
- Ameliorer la regularite des images : une moyenne elevee ne suffit pas si les
  images subissent des retards ou des variations importantes.
- Viser au moins 60 FPS stables sur le PC actuel. Orbit est un exemple charge,
  pas une exception codee dans le moteur.
- Le fork est independant et s'appelle Chlorium, avec attribution a Bismuth.

## Cadre actuel — mandat V5 reçu le 25 octobre 2026

Travail individuel, sans sous-agents ni captures périodiques. Après le préflight,
la mission d'ingénierie du 25 octobre autorise compilations et tests techniques
statiques/CPU/GPU isolés. Aucun lancement de GD ni changement de l'installation
sans autorisation explicite. Voir le journal `EXPERIMENTS-V5.md` pour les preuves.
Les autorisations historiques décrites plus bas sont conservées comme historique.
La liste suivante définit le périmètre des niveaux envisageables ; elle ne
constitue pas une autorisation actuelle de lancer le jeu.
Avant jeu, installation/remplacement de DLL, sauvegardes, push, PR ou publication,
appliquer le consentement explicite prévu par le mandat V5. Continuer l'audit et
la préparation hors jeu dans le worktree de recherche en attendant ces étapes.

## Niveaux dans le périmètre

Liste actuelle : **133260800** (Orbit auto), **129598490** (Heliopolis auto),
**88952972** (Grief auto). Peaceful et LIMBO ont ete retires a la demande
utilisateur. Les noms exacts proviennent du telechargement natif. Priorite :
mini ecrans noirs uniquement en niveau ; compteur FPS visible pendant les tests. Collisions natives,
aucun noclip, aucun input artificiel. Arret apres
deux morts au maximum, a la fin normale, ou sur demande utilisateur.

Les premiers tests sur d'autres niveaux et le parcours sans collision sont
historiques ; ils ne sont plus autorises par le harnais courant et ne prouvent
pas les performances de cette version.

Chaque resultat doit preciser les passages effectivement couverts, les pauses,
les captures exclues des mesures, les changements de focus, les frametimes et
les depassements du budget de 16,67 ms. Un test interrompu n'est pas une preuve
de parcours complet. Une amelioration de microbenchmark n'est pas un gain FPS.

## Direction technique

Le rendu statique herite de Bismuth ne reproduit pas encore tous les effets
dynamiques de GD 2.2. Il reste disponible dans les sources comme chemin
experimental, desactive dans les versions normales.

Le chemin compatible conserve les calculs, le tri, les shaders et le dessin
natifs. Les optimisations doivent enlever du travail redondant sans changer
les donnees dessinees. Les transferts d'atlas differencies utilisent les memes
VBO natifs ; tout cas non reconnu conserve le chemin d'origine.

L'ID du niveau de mesure appartient uniquement aux outils de test. Le code
d'optimisation de production ne doit contenir aucune exception propre a Orbit.

## Priorite du 22 octobre : regression sur les fortes densites

L'utilisateur constate un resultat pire que sans Chlorium a la fin d'Orbit
et vers 25 % de Grief. Le cache doit borner son propre travail : comparaison,
copies et interrogations du pilote ne doivent pas se multiplier avec tous les
objets visibles. La candidate alpha.5 limite les atlas caches a 512 Kio et
le travail supplementaire a 2 Mio / 64 operations par image ; le reste conserve
le dessin natif. Ce sont des limites conservatrices a mesurer, pas un gain FPS
deja prouve. Les pauses apres changements denses augmentent jusqu'a 1920 dessins.

Les tests sont desormais courts et cibles. Premiere comparaison : Grief auto
22–28 %. Pour Orbit, la fin 95–100 % est une cible ; les effets anterieurs
doivent etre executes normalement pour conserver leur etat. Aucun saut de
position, aucune acceleration de simulation, aucun noclip. Une section mesuree
ne valide pas le niveau entier. Les passages hors section evitent la collecte
lourde et les captures synchrones.

## Consignes de test du 23 octobre 2026

Tests de nouveau autorises. Grief doit atteindre 40 % avant l'arret.
Priorite aux ecrans noirs supplementaires avec Chlorium. Qualite complete :
LDM desactive, secousses desactivees dans la copie de niveau du harnais.
Ces options doivent etre identiques sans/avec, affichees et verifiees apres
initialisation du PlayLayer. Aucun changement des sauvegardes utilisateur.

Critere de reussite rappele par l'utilisateur : frames regulieres et plus de
60 FPS dans tous les passages, sans ecran noir cause par Chlorium. Une moyenne
superieure a 60 n'est pas une validation. Les effets sombres natifs doivent
rester visuellement fideles. Des essais avec les mods personnels ont ete demandes.

Derniere consigne : arreter les nouveaux tests et se concentrer sur les ecrans
noirs. Aucun nouveau niveau n'est lance apres cette consigne. La candidate
alpha.6 retire les hooks d'atlas du paquet normal ; compilation seulement,
verification en jeu encore a effectuer apres nouvelle autorisation.

Puis l'utilisateur a demande « commence » : reprise des comparaisons alpha.6
avec les mods personnels, Grief 0-40 % et Heliopolis entier, LDM desactive,
secousses desactivees. Quatre parcours termines sans mort ni crash ; aucun
episode quasi noir supplementaire detecte avec Chlorium dans cette serie.
L'alpha.6 a ete installee avec sauvegarde du paquet precedent. Les 60 FPS
stables restent non atteints. Voir VALIDATION-ALPHA6-20261023.md.
