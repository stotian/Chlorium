# Nouvelles experiences — 22 octobre 2026

Ces pistes sont compilees separement : aucun cumul ne masque leur cout. Le
paquet alpha.5 installe dans le jeu principal n'est pas remplace. Les nouveaux
paquets partagent son identifiant pour le harnais de comparaison : un seul
paquet Chlorium peut etre charge a la fois. Aucun noclip ni changement de niveau.

L'utilisateur a demande l'arret des tests pendant ce travail. Les mesures
ci-dessous ont ete faites **avant cette consigne**. Ensuite : code, lecture
statique et compilation uniquement. Aucun parcours de niveau n'a valide ces
experiences. La priorite reste Orbit fin de niveau et Grief autour de 25 %.

## 1. Transferts GPU sans shadow CPU

Nouveau backend independant de NativeAtlasUploads et NativeTransformCache.
Le dessin reste natif : shader, texture, indices, ordre, framebuffer et effets.

- Mappage ecriture + invalidation de plage, sans UNSYNCHRONIZED. Si le mappage
  echoue : transfert natif. Si unmap declare le stockage corrompu : reconstruction
  par le mapBuffers natif avant le dessin. Cette piste est lente sur ce PC.
- Remplacement du stockage avant l'envoi natif. Il n'est tente que si l'envoi
  reecrit l'integralite du VBO, sans queue a conserver, et depasse 1 Mio dans
  le candidat. Les petits lots et plages partielles restent natifs.
- Anneau de trois VBO avec fences, et retour natif si aucun n'est libre :
  prototype uniquement dans l'outil GPU. Seulement 19 utilisations contre
  318 retours natifs dans chaque taille du test ; pas integre dans le mod.

Microbenchmark sur Intel UHD Graphics, OpenGL 4.6, pilote 31.0.101.2141.
Surface 64 x 64, quatre envois/dessins par image, trois repetitions avec ordre
alterne. Donnees de quads natives de 96 octets, texture et transparence.

| Quads par lot | Methode | Temps total median par image (ms) | p99 soumission CPU (ms) |
|---:|---|---:|---:|
| 1 024 | natif | 0.137700 | 0.5943 |
| 1 024 | mappe | 1.443320 | 2.0501 |
| 1 024 | remplacement stockage | 0.132087 | 0.0923 |
| 1 024 | anneau avec retours natifs | 0.133808 | 0.1541 |
| 16 384 | natif | 1.341350 | 2.9483 |
| 16 384 | mappe | 3.163140 | 3.9462 |
| 16 384 | remplacement stockage | 1.208720 | 0.7689 |
| 16 384 | anneau avec retours natifs | 1.209260 | 0.9630 |

1412 controles passent, dont octets GPU, preservation de la queue et egalite
des pixels textures/transparents pour ces jeux de donnees. Ce n'est ni une
preuve de fidelite de tous les niveaux, ni une mesure de FPS de GD. Le gain
de la variante remplacement sur le gros lot est environ 10 % dans cet essai.

Compilation : `build-local.cmd --streaming-candidate`.
Paquet : `package-local.ps1 -Experimental -OutputDirectory experimental-streaming-dist`.
Mode par defaut : remplacement. La variable CHLORIUM_STREAM_MODE=map active
l'autre chemin pour une comparaison future. Aucun choix automatique par niveau.

Sources : [glMapBufferRange](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glMapBufferRange.xml),
[glUnmapBuffer](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glUnmapBuffer.xml),
[glBufferData](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glBufferData.xml).

## 2. Reutilisation directe des divisions Area

La routine native GD+0x227b60 calcule parfois plusieurs fois le meme quotient,
sans appel intermediaire ni modification de ses operandes. Trois corrections
locales reutilisent xmm6 : aucun hook par objet, aucune approximation, aucun
cache. Conversion initiale, comparaisons, ordre du reste du code et sauvegarde
du registre sont conserves. Empreinte exigee sur les 197 octets du corps natif.
Le reglage enabled active/desactive les patches ; un echec provoque un rollback.

La premiere variante a passe 804840 comparaisons bit a bit avec une copie de
la routine native, dont les flags d'exception SSE, quatre modes d'arrondi,
FTZ/DAZ, NaN/Inf, zeros signes et longueurs extremes.

Microbenchmark initial (2 097 152 appels, mediane de trois repetitions) :

| Deadzone | Natif (ms) | Premiere variante (ms) |
|---:|---:|---:|
| 0 | 8.5177 | 7.1421 |
| 0.2 | 8.5936 | 8.1764 |
| 0.5 | 7.7289 | 7.9748 |

Le resultat est mitige : aucune acceleration generale n'est etablie. La
variante area.2 remplace les sept NOP elementaires de chaque trou par un NOP
multioctet pour reduire le travail de decodage. Cette revision n'a pas ete
executee depuis la consigne d'arret des tests. Ses performances restent a mesurer.

Compilation : `build-local.cmd --area-candidate`.
Paquet : `package-local.ps1 -Experimental -OutputDirectory experimental-area-v2-dist`.

## 3. Matrices et rotations : revision matrix.2

**Aucun test ni execution de cette revision.** La compilation ne prouve ni
la fidelite visuelle ni un gain de FPS. L'ancien paquet matrix.1 est conserve
dans `experiences/matrices-v1`; matrix.2 est archive dans `experiences/matrices-v2`.
Le prototype courant matrix.3 est dans `experiences/matrices` (section suivante).

La lecture du binaire natif montre la meme reconstruction de matrice dans
`processTransformActions` et `transformAreaObjects`. Dans chacune de ces
boucles, les setters d'echelle et rotation salissent le noeud auxiliaire
`m_areaTransformNode2` avant son calcul de matrice. Des objets aux positions
et tailles differentes peuvent pourtant partager leurs rotations.

Changements concrets par rapport au premier prototype :

- Suppression du hook global CCNode. Deux appels seulement sont rediriges :
  GD+0x22d00d et GD+0x229a59. Les menus, textes, autres sprites et autres appels
  de matrice n'entrent plus dans ce filtre. Le cache reste limite au noeud
  auxiliaire pendant les deux phases autorisees.
- Ajout d'un cache de rotations : une rotation identique reutilise son sinus
  et cosinus meme quand les echelles changent. Sur echec du cache, appel des
  imports natifs `cosf`/`sinf`, sans approximation ni arrondi volontaire des
  angles. Les signes des zeros, constante radians, multiplications scalaires
  et ordre des operandes reprennent le chemin natif analyse.
- Conservation du cache des quatre coefficients pour les objets qui partagent
  aussi leurs echelles. Deux tables fixes de 256 entrees, pas d'allocation par
  objet. Invalidation par generation aux limites de phase.
- Desactivation du cache pour le reste de la phase si une fenetre de 256 objets
  presente moins de 25 % de reutilisation utile. Les rotations nulles ont un
  chemin direct equivalent aux operations du cas natif sans trigonometrie.
- Prise en charge de la boucle des transformations Area, absente de matrix.1.
  Les appels imbriques suspendent le cache et invalident son contexte au retour.

Le calcul accelere reste limite aux ancres nulles, sans skew ni transformation
additionnelle. Les valeurs non finies et angles minuscules reviennent au natif.
Le chemin de position seule reste natif car il accumule une difference de
position, dont l'arrondi differe d'une translation absolue. L'etat de matrice,
les dernieres positions et les deux indicateurs dirty sont mis a jour comme
dans le chemin complet analyse. Les changements de reglages retirent les
deux patches ; le mode de comparaison native les laisse egalement desactives.

Les corps natifs, constantes, imports et instructions attendus sont controles
avant installation. Les overrides de la fonction virtuelle sont conserves.
La redirection utilise une page de pointeurs en lecture seule a portee des
appels, sans allocation de code executable. Allocation impossible ou corps
inconnu : conservation des appels natifs. Les patches appartiennent a Geode.

Le cache exige des exceptions flottantes masquees et ne promet pas de reproduire
les drapeaux d'exception/errno de chaque appel supprime. Les dependances natives
analysees utilisent les resultats numeriques ; l'equivalence bit a bit et les
interactions avec d'autres mods restent a verifier a la reprise des tests.
Cette revision n'intervient pas sur les effets dessines, la visibilite,
les sections, les collisions, les inputs ou l'ordre des objets.

Les exports `ChloriumReadMatrixStats` et `ChloriumReadMatrixDetails` exposent les
hits de matrices, hits/misses de rotations, retours natifs, phases Area et nombre
de points de redirection actifs. Ils ne sont pas encore integres au rapport
automatique du harnais independant et ne constituent pas des mesures realisees.

Compilation : `build-local.cmd --matrix-candidate`.
Paquet : `package-local.ps1 -Experimental -OutputDirectory experimental-matrix-v2-dist`.
Revue de code et points restant a verifier : `MATRIX-V2-REVIEW.md`.

## 4. Conservation des calculs et maitrise du surcout : matrix.3

Cette revision prolonge matrix.2. **Aucun test ni lancement du jeu.** Compilation
uniquement ; le paquet principal alpha.5 n'est pas remplace.

Le code precedent effacait les deux tables a l'entree et a la sortie de chaque
groupe. Ce travail est retire : les coefficients lineaires et les rotations
peuvent etre reutilises entre groupes et entre images. Les tables ne memorisent
ni objet, ni position, ni visibilite, ni etat de trigger. Les cles comprennent
les bits des valeurs et les controles SSE. La translation est copiee depuis
le noeud a chaque appel. Une reutilisation ne supprime donc pas une mise a
jour de position ou d'effet ; elle remplace seulement le meme calcul.

Invalidation sur changement de contexte de jeu/noeud, reglage, conflit detecte,
environnement flottant non pris en charge ou appel imbrique. Apres un appel
imbrique, le reste de la boucle externe reste natif jusqu'a la prochaine
verification. Les tables restent bornees a 256 entrees chacune.

Deux acces rapides evitent le calcul d'un index pour une matrice ou une rotation
identique a celle qui vient d'etre utilisee. Ce cas est particulierement
interessant pour des objets consecutifs aux proprietes communes, mais son
importance dans les niveaux reels n'a pas encore ete mesuree.

La politique de repli accumule maintenant les observations entre petits groupes.
Auparavant, un groupe de moins de 256 objets ne pouvait jamais declencher
l'arret du cache. Apres 256 essais dont moins de 64 utiles, le nouveau chemin
revient au calcul natif pour le reste de l'image puis 1, 2, 4, 8, 16 ou 32 images
supplementaires en cas d'echecs repetes. Les transformations ordinaires et Area
ont chacune leur politique, afin que l'une ne desactive pas l'autre. Une fenetre
utile ramene le delai initial a une image. **Seul le cache fait une pause : les
objets, effets, collisions et etapes de simulation continuent normalement.**

Le code du harnais independant dispose maintenant de l'export versionne
`ChloriumReadMatrixSnapshotV1`. Lors des futurs tests, il enregistrera avec le
pourcentage courant : activation effective, hits/misses, retours natifs,
invalidations, acces rapides et phases sans cache. L'absence d'export est
signalee par `available:false`, sans presenter des zeros comme des mesures.
Ces compteurs sont cumulatifs ; leurs differences entre echantillons seront
necessaires pour attribuer le travail a une portion de niveau. Leur ajout
ne constitue pas une mesure deja realisee.

Compilation : `build-local.cmd --matrix-candidate`.
Paquet : `package-local.ps1 -Experimental -OutputDirectory experimental-matrix-v3-dist`.
Analyse statique : `MATRIX-V3-REVIEW.md`.

## Reprise des mesures quand elle sera autorisee

Une paire avec/sans sur une section courte connue, sans compiler en parallele.
Observer le backend reellement actif, les frametimes par pourcentage et les
images. Mesurer aussi les echecs/cas de repli. Ne pas fusionner les pistes ni
les installer normalement sur la seule base d'un microbenchmark.
