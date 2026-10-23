# Audit Chlorium - 20 octobre 2026, revise le 21 octobre

Base : IliasHDZ/Bismuth, commit `76fd0ec`. Cible locale : GD 2.2081 x64,
Geode 5.11.2, Intel UHD Graphics, pilote 31.0.101.2141, OpenGL 4.6.

## Objectif et methode

Bismuth remplace une partie du rendu de GD : geometrie statique, couleurs et
transformations de groupes, elimination des sprites hors champ, puis commandes
de dessin indirectes sur GPU. L'objectif de Chlorium conserve cette architecture.
Un repli natif est une mesure de compatibilite, pas une preuve d'optimisation.
L'exigence precise est maintenant generale et sans perte visuelle : voir
`OBJECTIF.md`. Le remplacement statique incomplet est reserve aux builds
experimentaux ; l'optimisation compatible travaille dans le pipeline natif.

Passage 1 : journal de crash, compilation, cycle de vie du moteur, ressources GPU,
groupes, sprites, shaders et profiler. Passage 2 : dependances de compilation,
limites et entrees invalides, chargements repetes, atlas, couleurs et checkpoints.
Les tests reels du jeu peuvent faire apparaitre de nouveaux defauts : ils ne
permettent pas de conclure qu'aucun bug ne subsiste.

## Defauts identifies et corrections

| # | Gravite | Defaut | Correction / verification |
|---|---|---|---|
| 1 | Critique | Ninja enregistrait 0 dependance d'en-tete avec le compilateur francais. Des objets binaires pouvaient utiliser des dispositions de classe differentes. | Nouveau dossier de build, normalisation du prefixe include, reconstruction complete. |
| 2 | Critique | DifferenceMode interceptait les scenes et pouvait utiliser un moteur invalide, correspondant a la pile du crash utilisateur. | Ancien outil archive hors compilation, hook F9 retire. |
| 3 | Critique | Tous les enfants d'un objet etaient convertis sans controle en CCSprite. | Parcours de CCNode avec verification du type avant acces aux champs de sprite. |
| 4 | Haute | La couleur noire d'un detail pouvait faire classer un glow dans le mauvais atlas. | Separation de la classification glow et de la coloration noire. |
| 5 | Haute | Pointeurs vers les textes d'include GLSL invalides apres reallocation. | Conteneurs proprietaires stables, liberation explicite de chaque include. |
| 6 | Haute | Ressources glslang incompletement initialisees. | Limites officielles par defaut. |
| 7 | Haute | Un pointeur mappe par OpenGL etait conserve pendant le rendu et libere comme une allocation CPU. | Zone CPU proprietaire, map temporaire seulement pendant l'envoi puis unmap. |
| 8 | Haute | Le resultat du calcul GPU pouvait ne pas etre visible aux commandes indirectes et aux index. | Barrieres SSBO, commandes et index. |
| 9 | Haute | Restauration incorrecte des textures, melanges, VAO et buffers. | Captures locales empilables, reconciliation ciblee du cache Cocos, restauration des plages SSBO/UBO et des equations de melange. |
| 10 | Haute | Nettoyage conditionne au pointeur global, fuites de buffers et shader compute. | Nettoyage propre a chaque proprietaire, rollback d'initialisation, references des batches natifs conservees. |
| 11 | Haute | FinishNode utilisait le moteur global et n'etait pas correctement autorelease. | Reference au proprietaire et cycle de vie de scene. |
| 12 | Haute | Compilation de shader ou batch echouee apres remplacement partiel. | Verification avant masquage, repli natif, restauration des visibilites. |
| 13 | Haute | Texte capture comme geometrie statique alors que ses enfants et sa visibilite changent. | Texte conserve dans ses batches natifs ; mises a jour natives actives sur ces niveaux. |
| 14 | Haute | Rotation reduite de moitie et retour premature apres la premiere combinaison de groupes. | Angle entier, toutes les combinaisons, composition et rotation autour d'un centre testees. |
| 15 | Haute | Fusion des rotations utilisait plusieurs fois le delta de la meme commande. | Delta de chaque autre commande, sans auto-addition. |
| 16 | Haute | Reset GPU apres les actions initiales annulait les toggles de debut. | Reset avant l'execution native. |
| 17 | Haute | Etats GPU non associes aux checkpoints de practice. | Sauvegarde/restauration des transformations, groupes desactives et etats de scale par checkpoint. |
| 18 | Haute | Acces a des groupes natifs nuls et interceptions appliquees a un autre layer. | Gardes et verification du PlayLayer proprietaire. |
| 19 | Haute | Hue 300 degres debordait l'encodage signe sur 9 bits. | Normalisation de l'angle et tests reels de la couleur magenta sur GPU. |
| 20 | Moyenne | Flag No Audio Scale ignore. | Respect de m_hasNoAudioScale. |
| 21 | Moyenne | Flags du glow LBG et objets pouvant tourner incorrects. | Flags emis selon les proprietes de l'objet. |
| 22 | Haute | Culling base sur le centre supprimait des sprites visibles dont l'origine etait hors champ. | Rayon conservateur calcule a partir des quatre coins et adapte aux transformations. |
| 23 | Haute | Index d'objet, de groupe, de couleur ou d'index buffer non controles dans le compute shader. | Verification des bornes avant lecture/ecriture. |
| 24 | Moyenne | Tri instable des sprites ayant le meme Z. | Tri stable, ordre de depart conserve. |
| 25 | Moyenne | Additif appliquait une nouvelle fois l'alpha a des couleurs deja premultipliees. | GL_ONE / GL_ONE. |
| 26 | Haute | Version OpenGL et points d'entree non verifies. | Verifications 4.6, fonctions et limites SSBO avant activation. |
| 27 | Haute | Lecture/ecriture CPU d'un buffer protegee seulement par assert, avec addition debordable. | Bornes verifiees en execution par soustraction, exceptions sur les acces invalides. |
| 28 | Moyenne | Echec de map ou contenu invalide apres unmap sans repli. | Envoi par SubData depuis la copie CPU. |
| 29 | Moyenne | Shader/programme fuitait sur certains echecs de compilation/lien. | Suppression sur tous les chemins d'echec. |
| 30 | Moyenne | Uniformes de textures recevaient des IDs OpenGL plutot que des numeros d'unite. | Envoi des indices d'unite. |
| 31 | Moyenne | Une mesure GPU pouvait bloquer la boucle ou mesurer un etat non initialise. | Queries timestamp asynchrones, activation uniquement pour le diagnostic. |
| 32 | Moyenne | Profiler et compteurs actifs dans les chemins critiques sans demande. | Instrumentation intrusive exclue par defaut, autres mesures dormantes. |
| 33 | Moyenne | Recompilation des memes shaders a chaque niveau. | Macros inutiles retirees, caches bornes de sources et programmes. |
| 34 | Moyenne | Copie des groupes natifs de rotation et parcours de tous les IDs alpha a chaque image. | References constantes et parcours des groupes alpha utiles. |
| 35 | Moyenne | Tableau debug de culling inutile accumule au chargement. | Chemin inutilise exclu. |
| 36 | Moyenne | Cache de maillage conservait les anciens frames apres changements d'animation ou rechargement. | Suivi de setDisplayFrame et reconstruction du cache de frames. |
| 37 | Moyenne | Maillages degeneres/non finis acceptes. | Validation du nombre, des coordonnees et de la convexite ; quad complet sinon. |
| 38 | Moyenne | Insertion d'une contrainte parallele plus stricte au mauvais indice. | Indice d'insertion renseigne aussi dans le cas de normales identiques. |
| 39 | Moyenne | Taille de fichier negative convertie en allocation geante, lecture tronquee acceptee. | Lecture binaire, taille bornee, resultat de lecture verifie. |
| 40 | Moyenne | Particules natives non mises a jour dans le chemin optimise. | Conservation de updateParticles. |
| 41 | Moyenne | Combinaisons de groupes negatives, nulles, dupliquees ou trop longues. | Canonisation bornee avant indexation. |
| 42 | Faible | Horloge de types melanges et donnees de temps non initialisees. | steady_clock et durees explicites en nanosecondes. |
| 43 | Critique | ccGLInvalidateStateCache detruisait aussi les piles de matrices via kmGLFreeAll ; menus bleus/noirs ou en deux couleurs. | Appel supprime ; cache reconcilie par liaisons ciblees. Reproduction et correction en jeu avant restriction des tests. |
| 44 | Haute | onExit etait traite comme une destruction definitive alors qu'un parent peut etre rattache au graphe. | onEnter restaure l'etat propre du moteur ; noeuds de dessin associes a leur proprietaire. Validation live encore requise pour ce scenario. |
| 45 | Haute | Le glow pouvait etre parcouru deux fois s'il descendait de l'objet ou du detail. | Detection du sous-arbre, composition des transformations jusqu'a l'ancetre commun. |
| 46 | Moyenne | Les tableaux de geometrie CPU gardaient leur capacite apres upload et la triangulation etait repetee par instance. | Liberation des quatre tableaux ; cache de triangulation indexee par frame, triangles identiques conserves. |
| 47 | Haute | La compression HSV modifiait meme le multiplicateur identite 1.0. | HSV natif flottant, ABI CPU/GPU 80 octets et tests de couleurs fractionnaires. Les arrondis finaux doivent encore etre compares au jeu. |
| 48 | Haute | La suppression de toutes les decorations des groupes natifs incluait texte et autres objets non remplaces. | Classification selective, ancres preservees, deux partitions natives conservees pour les objets qui en ont besoin. |
| 49 | Moyenne | Etats de groupes et produits alpha regeneres/envoyes sans changement. | Plages sales, recalcul des produits seulement en cas de modification, invalidation reset/checkpoint. |
| 50 | Haute | Toggles indirects/remappes absents des seules cibles litterales d'alpha. | Opacite calculee pour les groupes effectivement portes par les objets. |
| 51 | Moyenne | Chaque batch de dessin reattachait les entrees du compute et sauvegardait leurs etats inutilement. | Dessin limite explicitement a SSBO4/UBO3 ; scope de restauration adapte et contrat verifie par reflexion GPU. |
| 52 | Critique | Le packager pouvait donner un manifeste normal a une DLL contenant le harnais de test. | Verification de configuration et identite exportee dans les octets de la DLL ; aucune DLL chargee pour ce controle. |
| 53 | Haute | Le clone du test perdait version, type et parametres de compatibilite du niveau. | Copie native des informations, copies explicites des champs de jeu, ID prive et sauvegardes bloquees. |
| 54 | Haute | Le test pouvait relancer le reset initial et appeler drawScene manuellement, perturbant potentiellement les actions/scheduler. | Attente setupHasCompleted, reset et dessin artificiels retires. Parcours natif ensuite observe jusqu'a 89,98 %. |

Ces entrees regroupent revue du code et corrections ; leur presence dans ce
tableau ne signifie pas qu'elles ont toutes ete reproduites dans GD. Les preuves
de tests et leurs limites sont indiquees ci-dessous et dans les journaux locaux.

## Limites restantes, a ne pas presenter comme corrigees

- Parite complete des triggers scale et des effets de zone/entree/sortie de la 2.2.
- Geometrie et UV dynamiques des sprites animes et de certains objets speciaux.
- Disparition exacte des pieces ramassees, hitboxes, particules, ecran de fin.
- Checkpoints, StartPos et rechargements sur des niveaux complexes reels.
- Regularite de chaque image et consommation memoire sur le seul niveau auto
  autorise (133260800), avec comparaison native et optimisee.
- Interoperabilite live des bindings SSBO/UBO avec les autres mods.
- Les outils historiques `legacy/`, `src/unused/` et `src/culling/` ne font pas
  partie du moteur compile et ne sont pas declares utilisables.

## Verification

- CPU : 18 controles d'etat graphique/rotation, 36 de buffers, 7 de geometrie
  elementaire et 2195 de maillage (47 frames / 148 polygones reels).
- GPU reel : 66 controles ; shaders fournis, calcul, commandes indirectes,
  framebuffer, couleur attendue, masquage alpha, mouvement, culling, bornes,
  HSV flottant, reflexion des ressources et scopes de restauration.
- `runtime-audit.jsonl` : resultats du test GD ; un echec ou un moteur non actif
  n'est pas un succes. Le harnais est exclu de la distribution normale.
- `auto-133260800-native.jsonl` : parcours natif jusqu'a 89,98 %, premiere mort,
  puis redemarrage et pause ; session arretee a la demande utilisateur. Ce
  releve ne constitue ni une completion ni une validation des FPS de Chlorium.

## Correctifs et mesures du 21 octobre : chemin natif

- `CCDirector::purgeCachedData` pointe vers le stub vide `libcocos2d+0x1fb0`,
  partage par **96 exports** du moteur Windows installe. Son interception
  vidait les caches durant des appels sans rapport. Interception supprimee.
- Le cache reste maintenant actif. Verification dans le niveau autorise :
  12 lectures du VBO reel, comparees octet par octet aux donnees natives,
  sans difference. Cette mesure ne prouve pas l'equivalence de tous les effets.
- Prise en charge des plages partielles avec couverture explicite des quads
  connus, seuil d'admission de 4 Kio et stockage du seul prefixe observe.
  Les appels avec `start != 0` restent sans couverture live sur ce niveau.
- Une mise a jour dense ne provoque plus deux controles successifs du meme
  stockage graphique. Les controles restent presents avant une interception.
- Les hooks du remplacement statique ne sont plus enregistres dans le chemin
  natif. Le raccourci P et la boucle de capture du profiler historique sont
  reserves a `CHLORIUM_ENABLE_PROFILER` ; ils restaient actifs sans cette option.
- Deux parcours de diagnostic ont atteint 100 %, sans mort, sans input et avec
  collisions natives. Les chutes 83–89 % et les blocages 96–100 % subsistent.
  Le profil cible les actions de mouvement, rotation, transformation et zone.
- `tools/analyze-auto-run.py` conserve les mesures par pourcentage, le nombre
  d'images, les exclusions, les intervalles bruts et les etapes CPU. Une mort
  a 89,98 % lors d'une ancienne passe ne prouve pas sa cause ; les nouvelles
  passes peuvent franchir ce passage sans mourir.

Verdict de revue : **Needs rework** tant que les limites de rendu 2.2 et les tests
dans GD ne sont pas resolus. Priorites : activation reelle et cycle de vie,
fidelite du rendu dynamique, puis mesures natives/GPU comparables.
# 21 octobre 2026 — cache de stockage et diagnostic actuel

Bug reproduit dans le paquet normal alpha.2 : un atlas de 64 quads etablit
un cache avec l'octet couleur 77. mapBuffers recrée le stockage GPU avec 102,
sans changer le pointeur CPU, les identifiants VBO ni la capacité. Le CPU
revient à 77 puis demande un rafraîchissement ; le cache compare son ancien
77, saute l'envoi et laisse 102 sur le GPU. Le test réel observe bien 102.

Correction alpha.3 : invalider l'atlas avant mapBuffers (export natif unique
libcocos2d+0xA69C0, corps vérifié ; aucune interception du stub partagé).
Le même test réel observe 77 après correction. Invalidations[0] désigne cette
recréation. Le hook appelle l'export via la distribution normale Geode vers
le hook suivant/original, car la déclaration SDK est privée.

Limiter le plan à un seul envoi partiel contigu évite également de multiplier
les appels pilote. 2022 contrôles CPU de reconstruction passent. Les mesures
en niveau doivent encore établir son intérêt en FPS.

Harnais V4 : liste active 133260800 / 129598490 / 88952972. Peaceful et LIMBO
sont exclus des nouveaux essais. Affichage explicite du mode et des FPS ;
inspection visuelle du tampon réellement présenté avant swapBuffers, avec
prélèvements asynchrones. Les FPS de cette inspection restent indicatifs.
Les épisodes noirs attendus existent aussi sans Chlorium, notamment au
début de Grief ; ne pas les « corriger » en retirant des effets du niveau.

Les sections suivantes sont historiques.

# 22 octobre 2026 — surcout de cache dans les scenes chargees

Candidate alpha.5 : sortie native immediate pour les dessins propres/vides,
atlas caches plafonnes a 512 Kio, budget supplementaire de 2 Mio et
64 operations par image. Un transfert natif non capture rend la couverture
inconnue ; aucun ancien shadow ne peut alors supprimer un futur transfert.
Les invalidations mapBuffers restent en place. La pause apres cycles denses
progresse de 120 a 1920 dessins admissibles, sans perdre son historique lors
d'une capture native.

Cache mathematique : reset par generation O(1), nettoyage au debordement
uniquement, controle FP differe et reevaluation de reutilisation tous les
256 calculs. Les bits des entrees et resultats restent exacts.

Verifications ciblees : 2030 controles atlas passent ; controles de bits,
exceptions, generations et transitions de reutilisation mathematique passent.
Paquet normal alpha.5 compile :
EBEDE54A1456CECC38E11B51A401A5D36B08134AB49A2386F2432806D8B3AF1E.

Le harnais cible Grief 22–28 %, avec atteinte de la section par le gameplay
natif, collecte allegee avant celle-ci et arret a sa fin. Le lancement de
controle sans paquet normal quitte avec code 1 avant toute ligne du harnais.
Une seconde tentative apres ouverture de Steam reproduit ce symptome. La
cause du demarrage n'est pas etablie ; aucune nouvelle mesure FPS ou validation
visuelle alpha.5 ne peut etre deduite de ces tentatives.

