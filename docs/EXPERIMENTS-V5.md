# Journal synthétique V5 — 25 octobre 2026

Branche locale `research/v5-g0`, base `2312559`. Travail individuel ; tests
techniques isolés autorisés. Aucun jeu, capture, changement d'installation,
push ou publication. La version normale reste séparée. Les sources préparées
avant V5 ne sont pas toutes validées par les tests suivants.

## R4-01 — oracle, puis première expérience CPU

`python research/R4-01/fixture_mutations.py` : témoin accepté, **12/12** mutations
privées rejetées, code retour 0. Cela valide le comparateur sur sa fixture
déclarative, pas la collecte native, l'état d'un vrai pilote ou les pixels.
G1 est acquis pour cette couche seulement ; les gates du renderer restent ouverts.

## R4-06 — Alpha : graphe inverse à deux étages

**Hypothèse avant prototype.** Dans `GroupManager::prepareGroupStateBuffer`,
un changement Alpha déclenche tous les produits de groupes, puis tous les états.
Un index `groupe → combinaisons → états` doit réduire le travail à la région
affectée quand les changements sont rares, tout en préservant l'ordre des
multiplications. Ce code du renderer statique est inactif dans l'alpha.6 :
aucun gain direct de la version installée n'est déduit de cette expérience.

**Code.** `research/R4-06/IncrementalAlpha.hpp`, noyau CPU sans dépendance GD/GL.
Deux index CSR immuables, déduplication par révision, rejet d'epoch obsolète,
restauration/invalidations complètes, recomposition canonique sans division.
Trois variantes comparées : transcription indépendante du balayage intégral,
CSR pur, CSR avec parcours contigu lorsque le fanout est dense. Le jeu et ses
triggers ne sont ni appelés ni remplacés. Les objets hors écran restent dans le graphe.

**Défauts rencontrés et conservés.** Le premier test exigeait à tort le même
nombre d'écritures privées : le balayage réécrit les NaN inchangés. Cette assertion
a été remplacée par égalité bit à bit, absence d'écritures supplémentaires et
couverture du dirty set. Un vrai écart de résultats a ensuite été détecté après
changement d'arrondi : invalidation au prochain point où le natif recomposerait.
Une fixture a aussi démontré des flags flottants manquants après leur effacement
par un consommateur. Un retour au parcours complet régénère les flags si nécessaire,
sans les fabriquer. Les contrôles SSE (arrondi, FTZ/DAZ, masques) sont suivis ;
les traps non masqués sélectionnent le parcours complet. Aucun test de trap effectif
n'a été lancé. Les logs négatifs sont conservés, pas effacés du verdict.

**Vérifications finales.** Compilation MSVC x64 `/O2 /fp:strict /W4`, sans
avertissement dans la dernière sortie. **42 147 assertions**, dont 6 000 ticks
différentiels sur les deux variantes : résultats IEEE754 bit à bit, NaN/Inf,
zéros signés, sous-normaux, désactivation de groupes, checkpoints synthétiques,
remap/reconstruction d'epoch, snapshots périmés/courts, arrondi et FTZ/DAZ,
flags d'inexact/underflow/overflow et statut SSE brut. La variante privée qui
supprime une dépendance échoue bien sur `differential output bits` (code 1
attendu). Aucun mutant n'est compilé dans un paquet Geode.

**Mesure finale, CPU synthétique exclusivement.** 1 024 groupes, 8 192 combinaisons,
16 384 états, 768 appels par variante et densité, six répétitions avec ordre
rotatif. Peuplement initial exclu et déclaré, compteurs inclus dans les deux
noyaux. CSV brut de 6 912 durées conservé. RAM/VRAM de processus, fréquences,
températures et overhead du timer non mesurés ; le test n'est pas une mesure
de frametime de GD. Allocation dynamique candidate calculée : **818 904 octets**,
hors allocateur, struct et fixtures témoins.

| Groupes modifiés | Balayage p50 / p99 (µs) | CSR pur p50 / p99 | Adaptatif p50 / p99 |
|---|---:|---:|---:|
| 1 | 137,2 / 514,5 | 2,5 / 4,3 | 2,5 / 49,1 |
| 8 | 130,1 / 478,5 | 4,9 / 18,5 | 4,9 / 22,3 |
| 1 024 | 185,0 / 620,6 | 345,8 / 1 455,7 | 191,4 / 687,9 |

Sur 768 mises à jour à un groupe, les combinaisons recomposées passent de
**6 291 456 à 16 128**, et les états visités de **12 582 912 à 32 256**.
Ce travail évité est établi par compteurs ; le speedup du petit noyau n'est
pas un multiplicateur de FPS. Le polling de tous les groupes utilisés reste
nécessaire et n'est pas supprimé. Les valeurs temporelles varient nettement
entre révisions/runs ; tous les relevés intermédiaires restent disponibles.

**Décision : GARDER le principe incrémental, RETRAVAILLER le cas dense.**
Le CSR pur est refusé pour le régime dense ; même l'adaptatif n'y bat pas
la référence sur la dernière série. Aucun remplacement normal recommandé.
Absence de preuve visuelle, de trigger natif, d'Area réel ou de gain GD.

**Suite discriminante.** Une trace passive de `processAreaActions` et
`processAreaEffects` doit établir les dépendants et barrières réellement actifs
avant d'appliquer ce principe au goulet CPU d'Orbit. Les signatures existent
dans les bindings locaux (`0x228fc0`, `0x2283e0`, GD 2.2081) ; aucune interception
de ces adresses n'est ajoutée ici. La collecte native attend l'autorisation de jeu.
Le prototype pur peut encore être éprouvé hors jeu sur membership dynamique
et statuts flottants ; aucune simulation Area n'est prétendue implémentée.

Preuves détaillées, commandes et empreintes :
[`research/R4-06/evidence.json`](../research/R4-06/evidence.json).
Rollback : ne pas sélectionner ce noyau, qui n'est lié à aucune DLL du mod.

## 2026-10-25 — V4 : régression signalée, périmètre GL réduit

**Constat.** Sur la paire exploratoire Grief AUTO 88952972 (0–40 %),
les deux passages ont atteint la cible. La V4 comptabilise environ 2,532 ms
par image dans son chemin capture/soumission, sans isoler le coût natif remplacé.
La référence est restée sans focus : aucune baisse chiffrée des FPS n'est validée.
La comparaison stricte est également refusée faute de `mod_profile_sha256`.
Les compteurs de captures complets ne prouvent pas l'absence d'écran noir.

**Hypothèse ciblée.** Le backend conserve la préparation native puis ajoute
comparaisons/copies et appels au pilote. Son ancien périmètre `SpriteDraw`
sauvegardait/restaurait aussi des états qu'il ne modifie pas. Le nouveau
`NativeQuad` conserve programme, VAO, texture 0/unité active et SSBO 4/générique.
Les autres périmètres restent disponibles. Les appels bruts restaurent les
bindings réels sans modifier le cache Cocos ; ce contrat reste à valider en jeu.

**Preuves hors jeu.** `research/R4-04/run-shaders-gpu.cmd` : 116 vérifications
GPU réussies sur Intel UHD, OpenGL 4.6.0, pilote 31.0.101.2141. Les nouvelles
sentinelles comparent tous les états capturés, dont plage SSBO et binding nul,
blend, UBO, EBO, unités de texture et buffers non modifiés. Neuf lots alternés
de 512 captures/restaurations après échauffement : médiane CPU par appel
`SpriteDraw=26,9402 µs`, `NativeQuad=6,4623 µs`. Micro-mesure sans dessin :
ni gain FPS ni fidélité GD démontrés. Sortie brute privée :
`build-research/shader-scope-check.local.txt`.

`research/R4-04/build-quad.cmd --scope-r2` : compilation et liaison réussies
avec avertissements SDK. DLL séparée dans `build-research/quad-v4-scope-r2`,
SHA256 `7300d43c2d67fb78cb03741e63a37d9a611b728048dd5520f67c47efd1367907`.
La V4 déjà testée et ses empreintes sont conservées ; aucun paquet normal
remplacé, aucun nouveau lancement de GD. Le changement reste expérimental.

**Décision : RETRAVAILLER.** Garder la réduction de périmètre en recherche,
sans annoncer la régression résolue. Prochaine preuve : comparaison appariée
avec focus contrôlé, puis coût des copies de quads si la régression persiste.


## 2026-10-25 — R4-08 : lots Area et programme causal

Nouveau noyau de quatre distances partageant une recette Area : divisions SSE
natives, sans FMA/réciproque approché ni quantification. Oracle extrait en mémoire
privée de la routine exacte de l'exécutable local, corps et constante vérifiés.
**460 672 contrôles**, 349 600 sorties comparées bit à bit, dont 198 700 voies
vectorisées ; statuts/contrôles SSE, 16 modes d'arrondi/denormaux, NaN/Inf et
recours natifs. Programme de vagues testé sur **1 200 séquences** de dépendances.
Deux mutations privées rejetées : valeur corrompue et barrière causale supprimée.
Dernière série de noyau : médianes natives 287,2 / 266,0 / 239,6 µs contre
39,5 / 53,4 / 47,7 µs, respectivement zone 0 / 0,2 / 0,5, 65 536 entrées.
Neuf lots par variante, ordre alterné. **Pas un gain FPS**, gather/commit GD absents.

Trois diagnostics natifs distincts sur Orbit 96–97 %, sans LDM/shake/noclip
ni capture périodique : quatre phases initiales, quatre phases visibles, puis
agrégats par image. Les deux premiers échantillonnent le début léger et ne
représentent pas les pics. Le dernier compte **328 phases Area / 16 compartiments
d'image**, 15 phases visibles. À 96,70 % : 51 phases, 91,54 ms Area dont
58,19 ms dans transformAreaObjects, 197 421 membres proposés. Ces cardinalités
ne sont pas des comptes d'objets réellement visités (sauts/nulls possibles).
Les scopes sont imbriqués et leurs temps ne s'additionnent pas. Les premières
et dernières fenêtres peuvent être partielles. Une mort invalide ce dernier
parcours comme référence A/B, malgré la section atteinte ; 15 lignes de frames
sont avec focus. Aucune absence de blackscreen ne peut être déduite sans lecture
visuelle. Les 90 fichiers protégés ont été restaurés et vérifiés après chacun
des trois diagnostics ; paquet normal inchangé.

**Décision : RETRAVAILLER.** Garder les noyaux/ordonnanceur comme composants de
recherche, sans les brancher aux objets GD. La normalisation seule n'est pas la
cible dominante ; réduire les parcours et reconstructions de matrices devient
prioritaire. L'hypothèse de surcharge par rattrapage physique doit être distinguée
d'une hausse du coût par phase ; aucun plafond de physique n'est introduit.
Preuves : research/R4-08/evidence.json, census-frames.json et timings.csv.

## 2026-10-25 — R4-09 : audit transversal et corrections

Revue des chemins normal/V4/statique, ressources GL, groupes/Area et protocole
A/B, complétée par l'analyse MSVC du chemin normal. Corrections : résolution
WGL par contexte dans common.cpp, timer V4 incluant la restauration GL,
motif d'échec explicite après mort, profils de build mutuellement exclusifs,
empreintes des paquets/configurations communs du profil isolé. Les anciens
rapports restent inchangés et ne deviennent pas valides rétroactivement.

Compilation/liaison réussies dans trois sorties distinctes (audit-r3,
measurement-audit-r2, native-static-analysis). Analyse MSVC : 50 avertissements,
dont trois localisés dans les sources du projet (getenv et masquage de variable),
pas de prétention « zéro avertissement ». **36 contrôles GPU de contexte,
18 rendu CPU, 36 buffers, 7 géométrie, 6 invariants de profils réussis** ; mélange
de profils rejeté avant compilation. Sources de chaque nouveau build archivées
et empreintes ; SDK/libs externes réutilisés séparément, pas d'installation.

Risques ouverts : VAO/programmes du moteur statique insuffisamment gardés lors
d'un changement de contexte ; couverture causale GD 2.2/rollback des groupes
incomplète ; scan compute global et recomposition alpha globale ; détour atan2f
partagé du chemin normal. Aucun de ces risques non reproduits n'est présenté
comme la cause démontrée des noirs. **Pas de promotion, pas de ×3 FPS démontré.**
Le tableau d'audit reste dans research/R4-09/AUDIT.md ; preuves techniques dans
evidence.json du même dossier. L'audit ne prouve pas l'absence exhaustive de bugs.

## 2026-10-25 — R4-10 : fidélité native V5 et nouvelle base

Hypothèse : reconstruire le shader/MVP dans le pont V4 peut casser le contrat
graphique. V5 conserve le programme actif et tous ses uniformes, les sommets
natifs complets et les indices natifs en lecture seule. Stockage VBO/VAO propre,
sous-plages exactes, époques de contexte et restauration des bindings ; repli
natif si le moteur ou un autre hook ne respecte pas le contrat reconnu.

**218 contrôles GPU, dont 32 scénarios pixels identiques ; 257 contrôles de
lecture/restauration.** Le témoin MVP nul est détecté. Comparaison Heliopolis
auto 129598490, 74–78 %, tous les mods utilisateur, détails complets, sans shake
ni noclip : 189/183 lectures valides, zéro mort, couverture visuelle complète.
Aucun noir complet ni anomalie classée supplémentaire ; un événement gauche
commun aux deux parcours. V5 réellement actif : dernier compteur 5 837 images
de soumission, zéro repli. Les mesures intrusives ne prouvent **aucun gain FPS**.
Un essai antérieur avec erreur GL préexistante est exclu, sa cause reste inconnue.

**Décision : GARDER comme nouvelle base expérimentale, à la demande de l'utilisateur.**
Version v0.3.0-alpha.1 ; alpha.6 gelée dans legacy et sauvegardée localement avec
ses sources et son paquet. Le Bismuth 2.2081 fourni est inspecté sans exécution.
Le dépôt est privé. Les grandes boucles Area/Transform restent natives ; aucune
absence globale de noirs, aucun 60 FPS stable ni ×3 ne sont démontrés.
Le paquet livré garde exactement le DLL et les ressources du parcours validé ;
seuls about/changelog/support sont réécrits. Empreintes et limites : R4-10/evidence.json.

La reconstruction CMake a révélé un générateur de bindings téléchargé plus
récent que les sources sélectionnées. Le générateur est désormais construit
depuis ces mêmes sources, sans faux en-têtes de compatibilité ni modification
du SDK. La vérification de cette chaîne reste distincte du binaire testé en jeu.
Reconstruction CMake et empaquetage standard réussis (34 entrées, identité V5).
Un avertissement C4996 getenv subsiste ; pas de prétention zéro avertissement.
Le paquet de livraison conserve le DLL manuel testé, distinct de ce rebuild.

## 2026-10-25 — R4-11 : programmes de poses et fusion des nœuds temporaires

Deux prototypes CPU isolés : compilation des recettes linéaires communes à une
phase, puis pipeline fusionné sans cache pour les poses toutes différentes.
**1 048 611 contrôles natifs réussis**, dont 65 536 poses fusionnées comparées
au constructeur Cocos et au bloc réel de décomposition GD. Opérations float
natives, aucun angle quantifié. Une corruption volontaire est rejetée.
Le cas unique a motivé un arrêt du compilateur après 1 024 entrées sans assez
de partage. Timings de snapshots seulement, pas de gain FPS démontré.

Un lancement tous mods échoue au chargement, cause inconnue. Un second diagnostic
en profil isolé atteint Orbit auto 97 %, zéro mort, détails complets sans shake
ni noclip : 1 084 638 compositions, 23 830 appels transformAreaObjects dans
14 compartiments d'image. Classification plafonnée à 4 096 appels/image :
13 579 répétitions sur 44 344 appels classés (30,62 %), sans extrapoler aux
1 040 294 appels hors budget. Les deux restaurations des 90 fichiers sont
vérifiées ; installation V5 inchangée. Aucune conclusion sur les noirs.

La fusion sans cache seule donne 2 544,2 contre 2 708,1 µs pour 16 384 entrées,
en fixture sans publication d'état ni callbacks objets : gain de noyau d'environ
6 %, insuffisant pour une refonte dominante. **Décision : RETRAVAILLER.** Garder
les noyaux comme références, sans installer le cache ou la fusion mathématique
comme réponse au goulot. Cibler un programme causal de groupes qui supprime des
parcours d'objets, avec lectures, publication et callbacks équivalents.
L'objectif ≥60 FPS à chaque
image, sans noirs sur les trois niveaux, reste non atteint. Preuves R4-11.

## 2026-10-25 — R4-12 : programme causal de groupes et dette explicite

Premier modèle de représentation différée : une commande pure par groupe,
fusion des flux par ordre global à chaque lecture, matérialisation avant
changement d'appartenance, handles générationnels, checkpoints, stockage borné
et barrière complète avant lecteur inconnu. Aucun branchement aux objets GD.
**105 394 contrôles** contre le modèle immédiat ; deux mutations rejetées
(ordre intergroupes et barrière supprimée). Allocation par liste de slots libres.

Exemple CPU unique, pas un benchmark FPS : 32 768 objets, 64 événements,
256 lecteurs. 6 340,3 µs immédiats, 340,2 µs pour les lectures différées, mais
**24 868,3 µs à la barrière finale**. Les 16 384 opérations immédiates deviennent
2 097 152 après règlement de la dette, comme la référence. **RETRAVAILLER** :
ne pas présenter le déplacement du travail comme une accélération. Étape
suivante : règlement/publication bornés et barrières natives prouvées. Les effets
Area complets, collisions, champs observables et rendu restent à intégrer.
L'objectif global n'est pas satisfait ; aucune nouvelle installation ni run GD.

## 2026-10-26 — R4-13 : règlement groupé de la dette

Compilation bornée par signatures exactes groupes/curseurs, programme partagé
par cohorte, exécution scalaire ou SSE quatre positions. Ordre conservé par
objet, aucun produit de matrices précomposé. Repli de référence après 65 536
références de commandes ; les cohortes déjà exécutées ne sont pas rejouées.
Barrière vide en temps constant. **106 212 contrôles** : appartenance, durée de
vie, checkpoints, repli borné, queues SIMD, NaN/Inf, zéros et 16 contrôles SSE.
Trois mutations (ordre, barrière, SIMD) rejetées.

Comparaison appariée, neuf lots : dette 22 115,9 µs en fusion par objet, contre
5 644,8 µs en cohortes SIMD pour 32 768 objets. Total SIMD 5 834,0 µs, maximum
6 918,4 µs ; référence immédiate 5 411,7 µs. **GARDER EN RECHERCHE** : la dette
est améliorée mais le règlement complet reste plus lent que le modèle immédiat.
Aucun gain FPS ni couverture native GD ne sont déduits de ces noyaux.

## 2026-10-26 — R4-14 : transport adaptatif des quads natifs

Candidate v0.3.0-alpha.2 isolée : en animation dense, envoi du préfixe natif
complet sans recopier tous les quads dans un shadow CPU. Transition stable avec
réinitialisation complète ; cas sparse comparé intégralement. Échantillonnage
uniquement pour choisir le transport, jamais pour supprimer une mise à jour.
Repli natif préservé, taille du stockage vérifiée, invalidation après échec.
**2 530 contrôles CPU et 219 GPU**, dont 32 scénarios pixels natifs identiques,
et corruption d'un objet non échantillonné rejetée. Installation V5 inchangée.
Comparaison visuelle appariée Grief auto 0–40 %, détails complets, sans shake
ni noclip : preuves consignées après clôture des deux passages.


## 2026-10-26 — R4-15/16 : contrat Cocos réel et autorité GPU

L’oracle appelle le véritable dessin d’atlas Cocos supporté. Il reproduit
un écart V5/V6 dans l’état GL final malgré des pixels courants identiques.
V7 remet le dessin à Cocos, mais un second témoin démontre que son envoi CPU
inconditionnel écrase des données GPU valides d’un atlas propre. Grief V7
0–40 % atteint avec les deux modes : 3 247/3 426 captures, zéro mort ; deux
épisodes noirs non appariés restent non résolus. Aucune promotion ni gain FPS.

V8 reconstruit un stockage complet dans un spare distinct : intervalle CPU
sale exact, tête/queue conservées par copies GPU, puis transfert de propriété.
Les dessins propres gardent le stockage canonique. Dessin Cocos original et
repli natif sans dette de réhydratation. **250 contrôles GPU**, dont les deux
témoins négatifs précédents et la conservation des données GPU seules ;
**41 contrôles buffers**, y compris propriété, rejet mappé et destruction unique.
V8 compilée, isolée ; vérifications étendues à **636 contrôles GPU**.
Grief 0–40 % avec mods : 3 182/2 971 échantillons, six épisodes noirs non
appariés tous également observés dans des références natives précédentes ;
attribution non résolue. Heliopolis 74–78 % isolé : 212/202 échantillons,
aucun noir supplémentaire détecté. Le premier essai Heliopolis avec mods
est invalide (pause 11,43 s et sondes manquantes), jamais compté comme réussi.
Installation V5 inchangée. Mesures de fidélité distinctes des performances.


R4-16, verdict apres mesure : **REJETER LE TRANSPORT V8 POUR INSTALLATION**.
Grief 0-40 % avec mods, mesures sans captures : 59,118 -> 43,911 FPS ; p99
21,973 -> 39,114 ms ; pire image 70,878 -> 1 783,909 ms. Un seul appariement,
charge de fond variable, mais aucune base pour promouvoir cette version.
3926/2792 images acceptees, zero mort ; donnees de chaque pourcentage conservees.
Orbit isole 83-100 % : 292/234 captures, un noir non apparie a 97,3372 %,
reference native trop espacee pour attribuer la cause. L'objectif n'est pas atteint.
Conserver les oracles de contrat et les preuves negatives ; comparer un ring
de transfert persistant sans remplacement du stockage canonique avant integration.


## 2026-10-26 - R4-17 : ring persistant sans remplacement du VBO natif

Nouveau concurrent V9 : trois pages coherentes, copie de la seule plage CPU
sale dans le stockage natif canonique, fences et repli natif si une page reste
occupee (poll timeout zero). Aucun VAO/programme de remplacement, aucune copie
de tete/queue inchangee, aucune reallocation par dessin. Les dessins natifs et
leurs postconditions restent originaux. 1162 controles GPU, dont 256 transferts
aleatoires et le dessin Cocos reel ; corruption volontaire de reutilisation
refusee par le test. Candidate alpha.5 et audit independant compiles, versions
et sources figees. Mesure en jeu distincte ; aucun gain ni installation annonces.


Premier passage V9 Grief atteint 40 %, zero mort, 80 357 transferts et
18 replis natifs pour pages occupees, aucune image avec moteur inactif. Le
validateur historique a rejete les replis : ce passage reste invalide pour une
conclusion de performances. Audit R2 distinct, politique hybride typee et
13 controles CPU ; aucun resultat brut reecrit. Nouvel appariement en cours.


R4-17 R2 : paire Grief avec mods valide, 3805/3749 images acceptees, zero mort,
15 replis occupes comptes et zero image avec moteur inactif. 57,539 -> 56,626
FPS ; p99 23,288 -> 28,913 ms. Pire natif 999,632 ms contre 36,778 ms, mais
incident natif ponctuel : aucun gain de stabilite prouve. **NE PAS PROMOUVOIR**.
Conserver les preuves ; cesser les variantes de transport seules et supprimer
du travail de preparation dans la prochaine architecture.


## 2026-10-26 - R4-18 : elimination prouvee de phases de translation

Programme immuable, quantum dyadique et garde exacte 53 bits : supprimer les
reset/apply intermediaires seulement si chaque addition/soustraction est exacte.
Sinon rejeu ordonne. Barriere de lecture verifiee aussi sur le prefixe visible.
458659 controles, 16 modes SSE, oracle du vrai composant reset GD possede
(119 octets extraits seulement a l'execution) : positions, offsets, dirty et
flags masques. Mutation sans garde rejetee ; aucun code de jeu dans Git.

Modele CPU 32768 objets / 64 phases : grille exacte 11391,3 -> 283,9 us,
mixte 11171,6 -> 2302,4 us, precision pleine 11669,8 -> 4472,7 us (aucune
coalescence dans ce dernier cas). Aucun gain FPS GD deduit, aucune activation.
Lecteurs/callbacks natifs, formule de production, uniformite reelle et tous les
autres effets restent a prouver. Retour booleen du reset natif confirme et coherent avec le header SDK genere ;
l'appelant conditionne un callback virtuel sur AL. Cette observation interdit
de supprimer les callbacks sur la seule preuve arithmetique. Aucun hook actuel
sur cette fonction.

## 2026-10-26 - R4-19 : shadow du producteur Area natif complet

Trois fonctions natives completes executees dans une fixture isolee, empreintes
et relocalisations verifiees : moveAreaObject, resetAreaObjectValues et capture
des dernieres valeurs. Aucune instruction proprietaire distribuee. Le reset
depend du numero de commande : +2 puis +4 au meme stamp donne +6, un seul
enregistrement et deux callbacks de section. Le modele de remplacement R4-18
donne +4 : ses gains synthetiques ne permettent pas de supprimer ces appels GD.

376652 controles, 49185 appels natifs, 4145 sequences et 47351 callbacks compares
sur 16 modes SSE. Tous les octets objets, le layer normalise et les identites
des slots sont compares aussi aux callbacks. Reutilisation/extension des slots,
stamps decroissants, positions bloquees et observateur ecrivant un autre objet.
Trois mutations rejetees : dernier ecrivain, ordre par objet, corruption de slot
temporaire masquee a l'etat final. Inspecteur independant : ACCEPT pour fixture ;
deux lacunes P2 corrigees avec controles cibles, sans nouvelle passe d'agent.

GARDER l'oracle, RETRAVAILLER l'elision. Inventaire complet des branches directes
et appels de transformAreaObjects ; lecteurs indirects et effets reels de
updateObjectSection encore ouverts. Aucun lancement GD, aucune installation,
aucun benchmark/FPS revendique. Suite : producteur reel des operands par objet
et barrières du helper-transform avant suppression des parcours.

## 2026-10-26 - R4-20 : boucle native complete et helpers compiles

Oracle isole du corps complet transformAreaObjects et de getRealPosition,
21 relocalisations explicites, vrais CCNode Cocos et callbacks temoins. Candidat
ferme pour decorations a lineaire identite : materialisation des champs/caches
des helpers et decomposition constante par phase, vrais getters/reset/move et
ordre de publication conserves. Le recentrage flottant natif peut deplacer
X=.25 vers 0 autour du centre 16777216, meme a echelle 1. Mutation delta nul rejetee.

327427 controles : octets objets, ownership/slots, etats des callbacks,
caches helpers, flags/errno, bornes/subnormaux et resetVisual=false. Mutations
flags supprimes et callback supprime rejetees aussi. Inspecteur initial REWORK :
garde des tables de callbacks et normalisation des pointeurs etrangers corrigees ;
contre-verification ciblee ACCEPT. Aucun vrai handler collision/section prouve.

REJETER le candidat runtime : neuf lots alternes, cout complet de validation,
compilation et publication compris, 4096 objets/16 phases : medianes natives
15675,3 us contre 19767,2 ; maxima 19134,0 / 23885,3. Chaque paire dense des
variantes precedentes regressait aussi ; preuves conservees dans R4-20.
Ce sont des timings de fixture native relocalisee, pas des FPS GD. Aucun jeu
lance ni paquet installe. Garder l'oracle pour etudier un programme de groupes
qui retire les parcours de validation repetes avec invalidation reellement prouvee.

Diagnostic R4-20 separe, macro privee inactive par defaut : 327427 controles
natifs passent avec cinq chronos entiers par phase. Sur neuf lots denses,
validation = 42,12 % median du temps candidat ; medianes par etape : 5499,5 us
de validation, 7383,8 us de boucle objets, 19,2 us de compilation constante.
Ces medianes distinctes ne forment pas une paire additive ni un gain FPS.
Prochaine preuve discriminante : groupe possede, suite synchrone bornee et
empreinte d'ecriture des callbacks stable pour valider une seule fois.

## 2026-10-26 - R4-21 : isolation du chargeur des fixtures Cocos

La fenetre Geode provenait du banc GPU : dependance XInput du Cocos chargee
depuis le jeu. Garde commune : DLL Windows absolue, chemin/handle verifies,
refus du proxy deja charge, reference gardee apres dechargement Cocos. Tous
les lecteurs natifs Cocos sont couverts, sans modification du jeu.

Deux tests enfants passes (DLL factice et cycle Cocos), GPU 1162 controles,
Area 327427. Premiere paire dense isolee : 11965,9 / 12489,8 us, regression.
Mesures historiques sans preuve d'isolation qualifiees ; chronos ulterieurs
concurrents a la partie utilisateur exclus. GPU final annule, aucun lancement GD.
Inspecteur ACCEPT ; demandes P2 couvertes. GARDER uniquement le correctif de
fixture ; aucune preuve de gain FPS ou de correction des blackscreens.

## 2026-10-26 - R4-22 : programme synchrone de groupes possedes

Prototype borne a 64 phases : copie des commandes avant ecritures, une seule
validation des membres/lineaires/callbacks et decomposition constante partagee.
L'execution conserve les vrais getters/reset/move et publications. Aucun
certificat persistant ou handler GD reel suppose stable. Centre verifie comme
les membres ; ancienne boucle factorisee et toujours couverte.

26053 assertions natives passent : petits mondes compares a chaque callback,
16 phases sur 4096 objets compares a l'etat final, quatre modes DAZ/FTZ, bornes,
stamps et registre plein reutilise. Commandes aliasant des positions modifiees
par le natif couvertes ; mutation par emprunt rejetee a 25971, suppression de
validation rejetee a 1. R4-20 : 327427 controles sans regression ; variante de
chronos compilee seulement. Inspecteur ACCEPT pour fixture, P2 de capacite
corrige et teste. GARDER comme prototype fonctionnel, sans gain FPS revendique.
Aucun GD lance, aucune installation. Chronos differes pendant la partie Steam.

## 2026-10-26 - R4-23 : candidat canonique V10 installable

Default cible v0.3.0-alpha.6 : aucun hook de dessin, stockage GPU prive, scan ou
copie d'atlas. Cocos conserve toute la soumission et ses donnees canoniques ;
les mecanismes V5 aux divergences GL/autorite reproduites ne sont plus charges.
Optimisation active : quotients Area exacts avec signature complete, plan valide
avant ecriture et un seul patch contigu de 87 octets. Pause guard conserve.

1619360 comparaisons bits/flags SSE masquees passent, plans corrompus/courts
refuses, octets intermediaires conserves. Aucun benchmark. Packager normal
V10 accepte ; V5, mixte et incomplet refuses sans ecraser le paquet precedent.
Inspecteur REWORK puis ACCEPT cible apres correction P1 packaging/P2 patchs.
Compilation/liaison manuelles serie passees, archive des entrees et dependances
empreintees. DLL 82944 octets, contre 282112 installee : pas une mesure de RAM/FPS.
CRC, PE x64, manifeste, licences et hashes du paquet reel verifies.

Candidat versionne dans Documents/chlorium, sans remplacement de l'installation.
La premiere config CMake a echoue apres retrait d'une source temporaire ; essai
sur cache obsolete annule, nouvelle config sources figees reussie. Voir evidence
pour statut final. GARDER candidat de compatibilite, sans revendiquer gain FPS,
absence universelle de noirs ou validation GD. Aucun GD lance. Main inchangee.

### R4-23 - Autorisation et premiers essais reels V10

Autorisation GD renouvelee par l'utilisateur le 26 octobre. Controleur V10
independant compile, empreinte de la routine Area verifiee a chaque image,
compteur d'inactivite persistant. Ancien snapshot non declare actif. Inspecteur
ACCEPT pour moteur attendu ; borne PE initialement signalee P2, corrigee ensuite
par controle de taille ModuleInfo et en-tete PE, avec cas corrompus/courts.
Cet auditeur corrige compile mais ne constitue pas le binaire des essais ci-dessous.
Aucun mode de jeu, noclip ou reduction de detail active. Focus reel observe ;
controle de fenetres indisponible (assets kernel manquants), aucune injection UI.

Le premier diagnostic visuel sans Chlorium s'arrete a 39,6267 % Grief, code
0xcfffffff, sans rapport final. Erreur GL preexistante au debut et interruptions
longues autour des lectures/captures ; cause de l'arret non attribuee. Ce n'est
ni une reference complete ni une preuve d'ecrans noirs corrigees.

Paires sans lecture d'image, tous les mods utilisateur, details complets, sans
shake/noclip. Grief 0-40 : sans 51,144 FPS/p99 53,213 ms ; V10 58,856/p99 23,284.
Heliopolis 74-78, ordre inverse : V10 28,690/p99 152,826 ; sans 44,274/p99 34,792.
Quatre passages atteignent la cible, zero mort, zero image de code inactif,
90 fichiers partages restaures apres chaque passage, paquet principal inchange.
Ces paires ordonnees aux nombres d'echantillons differents ne prouvent pas une
causalite positive. Les couts Area observes de quelques microsecondes ne peuvent
expliquer seuls ces ecarts de scene. Aucun gain FPS global revendique.

REFUSER la promotion V10 comme gain de performances ; garder la correction
structurelle du rendu. 60 FPS stables et absence de noirs non acquis. Differer
Orbit sur cette version pour travailler une specialisation CPU sans caches ni
balayage de validation repete, puis revenir sur les passages lourds.

### 2026-10-29 - R4-23 : cloture des preuves et de l'export

Exporteur encore verrouille sur V5 corrige : vrai script V10 accepte, V5/mixte/
incomplet refuses avant remplacement, archive sources presente. Fixtures passees.
L'archive exacte des sources de l'auditeur des essais a ete ecrasee par sa
recompilation avec bornes PE. Son paquet/DLL et leurs hashes restent disponibles ;
ne pas assimiler le nouvel auditeur aux essais precedents. Les prochains builds
et packagings conservent leurs anciens fichiers et hashes dans un historique prive
avant ecriture ; fixture de preservation passe. Installation principale inchangee.
Inspecteur export REWORK P2 puis ACCEPT cible : sources assemblees/relues en
staging avant remplacement ; README manquant preserve les deux anciens hashes.
Les trois deplacements finaux ne constituent pas une transaction disque atomique.

## 2026-10-29 - R4-25 : specialisation directe du helper CCNode

189571 assertions natives passent (etat complet/FP/errno, alias compris), mutant
sans garde rejete a 11817. SIMD entier, aucun cache/hook de jeu. Mesure deja
faite : 4096 identites 2633,3 -> 1795,9 us, mais melange 25% 2843,8 -> 3173,7
et 75% 2667,0 -> 2779,3. REFUSER integration : distribution reelle inconnue.
Ancien recensement pose Orbit ne mesure pas cette eligibilite. Pas de gain FPS.

Graphify reutilise dans son environnement installe : graphe structurel des 90
fichiers src, 1464 noeuds/2659 liens, sans agents semantiques ni cout LLM. Deux
fichiers a macros Geode partiellement extraits : graphe de navigation, pas preuve
de graphe d'appels complet. Cache prive exclu de Git et exports. Headroom utilise
en compression locale (proxy indisponible), chiffres/preuves conserves intacts.

### 2026-10-29 - R4-22 : cout complet du programme de groupes

712179 assertions passent, Geode absent du processus Cocos isole. Neuf lots,
ordre natif/phase/programme tournant, tous couts du programme inclus. Dense
4096 objets/16 phases : medianes 26969,3 / 22022,5 / 12315,6 us ; ratio median
des paires programme/natif 0,609 (0,362-0,873), 9/9 favorables. A une phase,
9/9 defavorables (ratio median 1,490). Pas de multiplicateur stable de FPS.
Inspecteur ACCEPT, dispersion ajoutee ; GARDER composant de recherche uniquement.
Possession des objets/barrieres reelles GD non prouvees ; pas d'installation.

## 2026-10-29 - R4-26 : programme d'echelles Area possede

Copie/validation une fois, concat lineaire et decomposition une fois par phase.
Echelles dyadiques positives [1/16,16], membres uniques initialement identite,
stamps nouveaux stricts ; resets/mouvements/champs/callbacks natifs conserves.
110202 assertions finales, mutant d'emprunt rejete a3, regressions R4-20 327427
et R4-22 26054 passent. Inspecteur ACCEPT ; P2 reset du centre couvert ensuite.
Dense 4096/16 phases : ratio median des paires 0,759 (0,682-0,820), 9/9 gains ;
64 phases 0,660 (0,591-0,791), 9/9. Une phase perd 8/9. Chronos avant derniers
cas de couverture, algorithme inchange. GARDER prototype ferme, pas FPS/GD.
La plupart des barrières de jeu restent inconnues ; ne pas fusionner des ticks
physiques sur la foi du benchmark. Meme-stamp cumulatif encore rejete ici.
