# Calculs natifs : verification et prototype combine — 24 octobre 2026

Les tests ont repris apres autorisation. Le paquet normal alpha.6 installe
reste conserve ; les versions decrites ici sont des experiences isolees.
L'objectif de 60 FPS stables sans perte visuelle reste non atteint.

## Verifications effectuees

- Le prototype Area V3 et le module de mesure independant compilent.
- Le cache mathematique passe ses cas de bits, controles, valeurs
  exceptionnelles, collisions de table, epoques et politique de reutilisation.
- Un test executable du relais E8 passe 576 comparaisons : arguments, resultat,
  etat SSE, quatre adresses de retour natives et protection lecture/execution.
- Les matrices passent 853 560 comparaisons de bits et d'etat contre Cocos.
  Les indicateurs flottants different dans 244 447 cas : limite connue du
  contrat de ce cache, pas une equivalence generale errno/fenv.
- Le bilan CPU cible conserve maintenant ses moyennes. Sur Grief 22-26 %,
  les valeurs du bilan correspondent aux lignes CSV retenues.

## Resultats Area V3 puis V3b

Sur Grief 22-26 %, aucun appel mathematique eligible n'est rencontre. Cette
piste ne peut donc pas accelerer ce passage. Le parcours termine sans mort
ni crash ; ce n'est pas une comparaison de performances sans/avec.

Orbit 83-89.5 % termine avec V3 sans mort ni crash : dernier releve,
13 109 verifications numeriques echantillonnees, aucun ecart. Le cache a deja
renonce a reutiliser des resultats sur plus de 24 millions d'appels cumules.

V3b deplace les controles de propriete apres les branches qui restent natives.
Un cache desactive par sa politique ou des controles flottants non pris en
charge retourne directement par le relais natif courant du jeu. Ces appels
ne paient plus les comparaisons d'entrees du cache qu'ils n'utilisent pas.
Les controles restent obligatoires avant toute reutilisation.

V3b atteint 88.5 % dans Orbit, sans mort ni crash : dernier releve,
12 938 verifications echantillonnees, aucun ecart. Il ne s'agit pas d'une
verification exhaustive de toutes les valeurs ou de chaque image.

Un temoin natif visant 89.5 % meurt vers 89.93 %. Il est arrete par l'agent,
conserve comme parcours incomplet et exclu de la comparaison propre. Le
message generique du harnais indique « Stopped by user » ; agent-note.json
dans le dossier brut documente l'origine reelle de l'arret.

La paire suivante utilise 83-88.5 %, les memes mods personnels, donnees de
niveau, reglages et module de mesure ; LDM OFF / SHAKE OFF, collisions natives.
Les compilations sont terminees avant ces deux mesures. Verification et
compteurs par appel sont coupes pour mesurer le prototype.

| Mesure | Sans Chlorium | Area V3b |
| --- | ---: | ---: |
| Images retenues | 24 | 35 |
| FPS moyens | 3.323 | 4.778 |
| CPU moyen par image | 300.808 ms | 209.176 ms |
| p95 intervalle | 757.945 ms | 825.414 ms |
| p99 intervalle | 915.282 ms | 1114.190 ms |
| Pire intervalle | 1105.383 ms | 1304.967 ms |
| Images au-dessus de 16.67 ms | 100 % | 100 % |

La moyenne augmente, mais les pics empirent. Les deux parcours restent tres
lents ; ce resultat ne valide pas la regularite demandee. Une seule paire
avec peu d'images et des temps tres variables ne prouve pas un gain causal
reproductible. La surveillance visuelle etant coupee pour cette mesure,
aucun verdict sur les noirs n'en est tire.

## Nouveau prototype : matrices et decomposition

L'option --math-pipeline-candidate produit une experience alpha.606 qui
associe les deux appels de matrices aux quatre appels de decomposition.
Les autres combinaisons restent refusees. Les caches conservent leur contrat
numerique, leurs portees et leurs gardes ; aucun remplacement du rendu GPU,
LDM, noclip ou automatisation d'input n'est introduit.

Les deux composants modifient des instructions distinctes dans les memes
corps natifs. Pour conserver les empreintes quel que soit l'ordre
d'installation, le calcul d'empreinte restitue les octets d'origine uniquement
aux six sites connus, si un patch actif appartient a ce mod et si ses octets
publies correspondent encore exactement a la memoire. Une modification
etrangere reste dans l'empreinte et doit faire refuser le corps.

Le paquet possede une identite propre. Un module de mesure ancien est refuse.
Le diagnostic exige le cache de decomposition actif et quatre sites, ainsi que
le cache de matrices actif et deux sites ; un repli silencieux ne peut pas
etre presente comme un essai du pipeline combine.

La compilation du prototype combine et du module de mesure passe, ainsi que
la compilation de la configuration normale. Les parcours suivants terminent
leurs sections sans mort ni crash avec les mods personnels.

## Verdict du prototype combine

Nouvelle paire independante dans Orbit 83-88.5 %, meme module et memes
conditions entre ses deux parcours. Ne pas remplacer sa reference par celle
d'une autre paire : les temps du PC varient fortement.

| Mesure | Sans Chlorium | Pipeline combine |
| --- | ---: | ---: |
| Images retenues | 90 | 63 |
| FPS moyens | 12.447 | 8.683 |
| CPU moyen par image | 80.249 ms | 115.076 ms |
| p95 intervalle | 172.989 ms | 259.681 ms |
| p99 intervalle | 240.316 ms | 335.669 ms |
| Pire intervalle | 255.315 ms | 366.966 ms |

Les deux composants restent actifs. Dernier releve : 19.86 millions d'appels
eligibles de matrices, 9.97 millions de matrices et 8.36 millions de rotations
reutilisees, cumules depuis le debut. Ces compteurs ne justifient aucune
acceleration : cette paire montre une regression de moyenne et de pics.
Le prototype n'est pas installe comme mod normal.

La protection contre la pause d'un PlayLayer sans parent est aussi ajoutee
aux sources de la configuration normale future, avec une identite binaire
explicite. Elle etait jusque-la limitee a l'experience Area. Le crash de
transition precedent reste sa motivation ; aucun nouveau test force de ce
crash n'est revendique. Le paquet alpha.6 deja installe n'est pas remplace.

Une analyse supplementaire reutilise les CSV existants, sans nouvelle partie.
Elle ne relie que des images retenues consecutives et divise le temps CPU des
etapes par l'avancement du temps du jeu. Ce dernier est un indicateur de
travail, pas un compteur direct de pas physiques. Sur cette paire, Transform
passe de 0.191 a 0.224 ms CPU par ms de jeu, Area de 0.238 a 0.265. Cela
renforce la decision de ne pas promouvoir ces caches sur la seule foi des
hits. Les valeurs brutes par image restent les mesures de fluidite.

Une sonde du helper seul, mediane de trois passages alternes de deux millions
d'appels, donne 12.813 ms natifs contre 10.038 ms caches pour des arguments
alignes sur les axes, 32.391 contre 14.146 ms pour 16 couples non alignes
repetes. Cette sonde n'inclut ni les gardes ni l'integration au jeu. Elle ne
permet pas de promettre des FPS ; elle evite aussi de supposer a tort que le
cache arithmetique isole est toujours plus lent sur les axes.

## Controle visuel cible d'Heliopolis

Paire sans/avec entre 74 et 80 %, lectures GPU V2 valides et completes,
captures demandees a 76.97 et 77.1 %. 286 echantillons sans, 291 avec ; deux
images exclues de chaque cote pour les captures. Aucun episode entierement
noir detecte. Une alerte partielle droite existe sans mod vers 76.9933 % ;
une alerte gauche de deux images existe avec le prototype vers 77.1723 %.
Cette derniere reste sans reference appairee : elle n'est pas declaree corrigee.

Les captures de controle montrent des bandes tres sombres aussi sans mod,
mais a des positions et temps differents. Elles ne sont pas la capture exacte
de l'alerte gauche. Les deux caches ne rencontrent aucun appel eligible dans
ce parcours Heliopolis, bien que leurs six sites soient actifs. Ces elements
limitent l'attribution au calcul optimise ; ils ne prouvent pas l'identite de
chaque image ni l'absence de tout defaut visuel.

## Memoire et suite

Le minimum de RAM physique disponible releve dans les paires Orbit est faible :
environ 330-459 Mio suivant le parcours. Le rapport garde maintenant la RAM
disponible, la memoire privee, le working set et les defauts de page. Ces
derniers incluent les defauts sans lecture disque : aucune preuve d'acces au
fichier d'echange n'est deduite de ce compteur seul.

Le prototype suivant utilise une liberation des caches de heap inutilises,
sans modifier les objets vivants : voir HEAP-RELEASE-20261024.md. Les trois
pistes restent separees du paquet normal alpha.6. Les donnees selectionnees
figurent dans math-pipeline-20261024-results.json ; les traces brutes et images
restent locales, dans audit-artifacts et Documents/chlorium.
