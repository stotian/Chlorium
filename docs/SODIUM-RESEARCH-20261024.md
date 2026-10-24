# Recherche et travail du 24 octobre 2026

Objectif : fluidite generale sans reduction visuelle. Un facteur x5 n'est pas
etabli sur ce PC et ne constitue pas un resultat de cette passe.

Chrome n'est pas connecte au controle disponible. La tentative d'ouverture
a retourne « Browser is not available: chrome ». Recherche web et consultation
des sources primaires ensuite ; aucune recherche n'est attribuee a Chrome.

## Ce qui est transferable

[Sodium, gestion des sections](https://github.com/CaffeineMC/sodium/blob/148151cf25c56f8b670986b348dcc589278c867e/common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/RenderSectionManager.java)
separe preparation des listes, reconstructions, visibilite et envois. Les listes
sont reutilisees quand leur contexte le permet et les reconstructions sont
planifiees. Le principe utile est de reduire le travail repete et ses couts
de soumission, avec des invalidations explicites. L'interpretation pour GD est
la notre : des groupes bouges par des triggers ne sont pas des sections de
terrain immobiles ; supprimer leur mise a jour casserait leur comportement.

[Contrat du moteur de dessin](https://github.com/CaffeineMC/sodium/blob/148151cf25c56f8b670986b348dcc589278c867e/common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/ChunkRenderer.java)
: preparation des listes, dessin, rotation des ressources et destruction.
La gestion de la duree de vie des donnees reste essentielle pour eviter les
images manquantes. Aucun cache d'anciennes geometries n'est reintroduit ici.

[Presentation de Bismuth](https://github.com/iliasHDZ/Bismuth/blob/0b4b71a84c48a5bff2fc80f210de8d6f93a05c3a/about.md)
: transformations/couleurs sur GPU et diminution de l'overdraw. L'auteur
avertit encore que les niveaux 2.2 s'affichent mal. Le
[TODO](https://github.com/iliasHDZ/Bismuth/blob/0b4b71a84c48a5bff2fc80f210de8d6f93a05c3a/TODO.md)
laisse Scale et Area inacheves. Reactiver ce remplacement incomplet serait
incompatible avec l'exigence de fidelite demandee.

Sodium est consulte comme reference architecturale ; aucun code Sodium n'est
copie dans Chlorium. Son
[fichier de licence](https://github.com/CaffeineMC/sodium/blob/148151cf25c56f8b670986b348dcc589278c867e/LICENSE.md)
est PolyForm Shield 1.0.0, distinct de la provenance de ce fork.

## Nouvelle piste effectivement implementee

Les profils existants montrent des boucles de mouvement/rotation couteuses.
La boucle native de rotation de GD 2.2081 appelle pour chaque objet avec pivot
un helper Cocos qui calcule X puis Y separement et initialise un temporaire.
La nouvelle experience calcule les deux composantes en parallele SSE2, en
double precision, avec l'ordre d'addition distinct du natif pour chaque axe.
Elle ne fusionne pas de ticks, ne saute pas d'objets et ne modifie pas les
couleurs, shaders, textures, particules, collisions ou visibilites.

Une seule instruction d'appel est remplacee, dans cette boucle. Les autres
appels du moteur restent natifs. Pas de hook global ni allocation par objet.
Les corps du jeu/helper et l'import doivent correspondre ; une modification
ulterieure de l'import ou de l'entree du helper reprend le chemin courant.
Les controles sont inclus dans le cout de l'experience.

La premiere verification a detecte une difference de propagation du signe
d'un NaN. Les entrees non finies passent desormais par le helper natif.
Les sorties partageant la memoire des entrees et les exceptions flottantes
non masquees passent egalement par le natif. Le calcul rapide conserve les
reglages d'arrondi et FTZ/DAZ du demandeur.

Verification autonome contre la DLL Cocos installee : 192 000 comparaisons
exactes des bits de resultat et drapeaux SSE, dans seize combinaisons de
reglages flottants ; huit cas d'aliasing compares au natif. Cette verification
ne prouve pas la fidelite de tous les niveaux ni un gain FPS.

Compilation : `build-local.cmd --affine-candidate`. Paquet isole :
`package-local.ps1 -Experimental -OutputDirectory experimental-affine-v1-dist`.
Version experimentale `v0.2.0-alpha.604`, interdite au paquet normal par les
gardes de configuration et de signature. Alpha.6 installee n'est pas remplacee.

## Organisation et budget

Deux skills communautaires installes depuis
[obra/superpowers](https://github.com/obra/superpowers/tree/2ce8aa15d05bd57213e6f746f953aafb7ded7a8b/skills)
: systematic-debugging et verification-before-completion. Revision epinglee,
304 236 etoiles selon l'API GitHub a la consultation. Seuls ces deux dossiers
sont installes ; aucun orchestrateur, hook de plugin ou lecteur de sessions.
Le contenu du skill et le script de diagnostic ont ete examines ; aucun de
leurs scripts n'a ete execute. Provenance : COMMUNITY-SKILLS.json.

Le paquet planning-with-files a ete examine mais ecarte de cette selection :
son ensemble de hooks et scripts serait excessif pour cette demande. Les
documents de recherche, preuves et prochaine decision servent de memoire
compacte. Aucun sous-agent n'est lance.

## Premiere paire mesuree : compteur actif

Grief auto 88952972, 22-28 %, memes modules de mesure, niveau, paquets
personnels et reglages verifies par empreintes non manquantes. LDM OFF,
SHAKE OFF, collisions natives. Les deux reprises terminent la section sans
mort ni crash. La surveillance visuelle est desactivee pour ne pas mesurer
son cout ; aucune conclusion sur les ecrans noirs n'est tiree de cette paire.

| Mesure | Sans mod | Prototype avec compteurs |
|---|---:|---:|
| Images retenues | 542 | 544 |
| FPS moyens | 56.12 | 56.54 |
| CPU moyen par image | 10.187 ms | 10.407 ms |
| Rotation moyenne | 0.5846 ms | 0.6571 ms |
| GPU moyen mesure | 9.707 ms | 9.671 ms |
| p95 intervalle | 18.682 ms | 18.147 ms |

2 596 754 appels ont ete executes jusqu'a 28 %, compteur cumulatif incluant
la simulation avant 22 %. Ce nombre n'est pas celui de la seule section.
La premiere integration n'apporte pas de gain FPS convaincant et sa rotation
est plus lente. Les pics longs se produisent a des pourcentages differents
entre reprises : ne pas les presenter comme des corrections de scene.

## Revision 2 et limite de la piste

Le chemin mesure ne touche plus aux compteurs par objet ni au stockage local
de thread. Le comptage peut etre active separement au demarrage avec
CHLORIUM_AFFINE_COUNT ; des compteurs nuls par defaut ne signifient donc pas
une absence d'appels eligibles. Une seule variante est selectionnee a
l'installation de l'appel, sans branche de diagnostic par objet.

L'appel natif valide utilise trois plages de pile disjointes : sortie
[rsp+0x68,+0x70), point [+0x78,+0x80), matrice [+0x88,+0xa0). La verification
d'aliasing par objet est donc retiree a cet endroit precis. Le helper general
la conserve. Les gardes de valeurs non finies, exceptions non masquees et
proprietaire de l'import/entree restent dans le cout mesure.

Benchmark final avec ces gardes : mediane de quatre passages alternes de
2 097 152 appels, environ 16.87 ms natifs contre 10.25 ms pour le prototype.
Les temps varient entre passages ; cela decrit uniquement ce helper, pas
un facteur d'acceleration du jeu. Details et donnees de la premiere paire :
affine-v1-grief-results.json ; traces brutes locales hors Git.

## Verdict apres le court essai de revision 2

Le meme passage 22-28 % termine sans mort ni crash. Jeu, module de mesure,
profil de mods, reglages, niveau et bornes ont les memes empreintes que la
reference native precedente. 508 images retenues : 52.63 FPS moyens,
11.338 ms CPU, 0.6230 ms de rotation, p95 intervalle 19.029 ms.
Des pics longs de nouveau decales entre reprises empechent une attribution
fine. Le compteur est volontairement inactif ; la presence du patch est
verifiee et les zero appels rapportes ne sont pas interpretes comme zero usage.

Ce resultat n'etablit aucun gain FPS. La revision sans compteur ne suffit pas
a faire de ce helper le goulot dominant ; les timings ne justifient pas une
nouvelle version normale. Trois courts essais en jeu au total pour cette
piste, puis arret des tests. Donnees : affine-v2-grief-results.json.

Decision : conserver le code et ses preuves comme experience isolee, sans
remplacer le paquet installe. Ne pas multiplier les microcaches autour de ce
helper. Pour viser un gain important, la prochaine mesure discriminante doit
porter sur le travail de groupes/effets Area dans Orbit en fin de niveau et
la preparation native de visibilite, plutot que prolonger cette micro-piste.
Cette mesure n'a pas ete executee dans cette passe. Les 60 FPS stables et
l'absence de tout ecran noir restent des objectifs non atteints.
