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

## 2026-10-29 - R4-27 : cumul dans le meme stamp, hors version normale

Sources/echelles cumulees compilees par phase, source remise a1 seulement au
nouveau stamp natif. Domaine dyadique borne, gardes/copiedescommandes/callbacks
conserves. 454314 assertions dont benchmark, mutant emprunt rejete a111057 ;
regressions R4-20 327427 / R4-22 26054 / R4-26 110202 passent. Archive exacte
des25 entrees figee avant build puis revalidee. Inspecteur ACCEPT fonctionnel ;
P2 chronos avec clear meme aux stamps egaux, pas de cout separe sans clear.
Dense16/64 phases : ratios medians apparies 0,790/0,730 mais une paire regresse
dans chaque cas ; une phase dense perd9/9. REFUSER activation normale : aucun
frametime GD ni controle visuel/ecran noir/crash pour ce prototype non integre.

Derniere consigne : aucune installation/publication/nouveau push. HEAD5678dcc
conserve ; travaux suivants locaux. Tests futurs isoles des mods personnels :
allowlist dans le helper de profils existant, preflight avant staging, campagnes
canoniques separees -isolated, empreintes dependances/reglages comparees. Fixtures
et vrais PrepareOnly natif/candidat passent, aucun jeu/dossierdepassage cree.
Ancienne installation et entrees/DLL du candidat canonique inchangees.

## 2026-10-29 - R4-28 : preuves reelles isolees et changement de cible

Auditeur PE borne r2 compile/lie/package. Selection du plan respectee, hashes
des deux paquets verifies avant staging ; fixtures preflight/alteration passent.
257 controles GPU passent. Grief natif froid sort1 avant auditeur, restauration92
fichiers ; reprise apres initialisation Steam atteint40. La cause semble liee au
demarrage client, pas une preuve de crash du renderer. Reference visuelle24-40 :
2215 images sans /2329 avec V10, couverture complete, zero mort/inactif/erreur.
11 images tres sombres dans chaque passage, debut commun30,3931%. Deux sondes
aux memes pourcentages ont geometrie similaire mais animations/particules et
pixels differents ; aucun verdict de fidelite universelle ou de FPS issu de cela.

Heliopolis natif74-78 sans readback : 214 images incluses,38,6566 FPS,p99 34,4752ms,
pire37,0026ms ;212 images depassent16,6667ms. Visibilite9,6975ms/deplacement5,6914ms
contre Area0,00409ms. Ce passage unique ne compare pas de gain ; il invalide
Area seule comme reponse suffisante au deficit. Quatre restaurations92 fichiers
(dont l'echec de demarrage initial),
installation principale inchangee. Orbit non relance. Objectif60 partout non atteint.

## 2026-10-29 - R4-29 : code de diagnostic cible, tests arretes

Nouvelle consigne «plus de test» : aucun nouveau test ni lancement GD ensuite.
La partie principale deja ouverte (chemin Steam) n'est pas un processus de test
Chlorium et reste intacte. Developpement local seulement, aucun commit/push.
Source d'echantillonnage PSS bornee et resolution PE/empreintes preparees pour
localiser le cout visibilite/deplacement, sans hook par objet. Revision initiale
auto-verifiee avant arret ; modifications SHA256/receipts/resolution suivantes
non compilees/non executees. Compilation sampling engagee avant arret terminee,
sans lancement ; ancien binaire non conforme au nouveau handshake V2. Ses
donnees sont marquees/exclues des comparaisons FPS. Tout reste isole.
Inspecteur lecture seule REWORK P2 : un PID de fixture seul ne prouve pas le
mode diagnostic. Correction source : export de session immuable du vrai auditeur
mappe + hash DLL impose avant PSS, controle reitere, recu lie au PID/images/CSV.
Correction non compilee/non testee ; aucune validation supplementaire apres arret.
Suivi inspecteur ACCEPT pour le handshake en source ; note mineure fermeture du
jeu pendant relecture de l'export corrigee dans le meme gestionnaire de sortie.
Cette derniere modification reste non compilee/non executee. Navigation statique :
updateVisibility0x3af3a0 et moveObjects0x22dd50 dans les bindings natifs generes ;
pas de reprise aveugle de l'ancienne reecriture Bismuth qui omet des comportements.
Correction de navigation PE : moveObjects repartit son corps sur cinq entrees
de fonction ; l'ancienne lecture du seul prologue masquait ses trois appels.
Resolution commune des chaines/indirections dans les deux outils hors jeu,
bounds explicites des codes unwind et handlers. Cas de regression ecrits mais
non executes ; derniere revision non executee/non compilee. Candidat vtable
0x498 vers stub 0x3be80 : observation statique insuffisante pour supprimer le
callback ou reordonner les mouvements. Aucun chemin de rendu active/modifie.

## 2026-10-29 - R4-30 : programme de prefixes d'opacite, source seulement

Lecture statique native : groupOpacityMod0x19ccb0 multiplie les opacites dans
l'ordre des groupes, avec arret <=0 ; opacityModForGroup0x260b80 refait un hash
et une recherche de map par groupe/objet. Prototype de trie de prefixes ordonnes
avec snapshot unique par groupe, produits SSE et publication par objet. Aucun
tri, reassociation, cache de frame precedente ni remplacement des couleurs.
COMISS conserve pour ses effets MXCSR (denormaux), exceptions masquees requises.
Cas de regression ecrits, NON compiles/executés : ancien arret des tests respecte.
Garder isole : equivalence native, vrais partages, snapshot, invalidation/barrieres,
hooks et cout total non prouves. Aucun gain en jeu revendique, aucun hook ajoute.
Inspecteur V6 lecture seule ACCEPT source isole sans P0/P1 ; garde NDEBUG ajoutee
aux assertions. Bornes d'allocation requises avant tout adaptateur GD, qui
n'existe pas ici. La revue ne remplace ni compilation ni preuve native/en jeu.

### R4-30 - Adaptateur d'observation native, source non executee

Profil manuel --opacity-shadow separe, hooks d'observation qui deleguent chaque
appel natif une seule fois. Une phase ciblee, maximum512 appels/16384 lectures,
stockage fixe, aucune lecture supplementaire des champs objets/getters moteur.
Trace des prefixes reellement executes, pas des appartenances completes : ne
certifie ni cache persistant ni equivalence future. Controle valeurs changeantes,
reentrance, gestionnaires multiples et environnement FP ; replay natif observe
et comparaison du programme prevus, etat FP du jeu restaure apres diagnostic.
Tous les frametimes du profil exclus, provenance marquee, comparateur FPS refuse.
Lanceurs exigent un plan/package distinct et le switch explicite ; plan local
autorise non modifie. Bornes precedant allocations du programme via la collecte.
Ni compilation, tests, lancement GD, installation, commit ou push ; aucun resultat
de parite ou gain FPS encore disponible. Garder strictement isole/desactive.
Contrat registres : XMM1/2 vivants au getter, XMM3 vivant dans visibility. Le
generateur Tulip3.1.15 local sauvegarde/restaure bien XMM1..3 ; le P1 initial de
la revue est retire sur cette preuve source. Correspondance avec loader/handler
reel encore non prouvee : construction --opacity-shadow refusee avant tout build,
et #error dans le header observateur. Aucun contournement, compilation ou test.
Le noyau standalone reste distinct ; aucune activation moteur ni promotion.

## 2026-10-29 - Reprise autorisee, Grief jusqu'a 65 pour cent

Autorisation explicite des tests renouvelee. Limite Grief88952972 etendue de40 a65,
autres niveaux/reglages/profil isole inchanges. Revision de campagne grief65-r1
distincte pour conserver les anciennes preuves. PrepareOnly reste sans effet,
y compris quand un repertoire de campagne existe deja. L'observateur d'opacite
non verifie reste bloque ; valider d'abord le noyau CPU et les gardes de diagnostic.

## 2026-11-03 — Cible 120 FPS et R4-31, reprise explicitement autorisee

Branche research/area-pose-v6, HEAD5678dcc ; changements locaux conserves.
Autorisation autonome des essais renouvelee, cible120/plafond120 uniquement dans
la fixture isolee. Version GD2.2081, Geode5.11.2, candidat normal V10 conserve.
Auditeur r4 distinct compile/lie/package : DLLfaa9ee705fcedaf7e69a69d89c31e27ce6b146593fb8602feba1cefb6447e800,
paquet60c6d63b1ead9b0ab5822d3fdd9a448bb5d5d45c1618f5d2583dc1493f04628e.
Plafond natif via GameManager::updateCustomFPS et variable0116 ; intervalle
0,008333333767950535s et VSync0 effectivement verifies dans les recus en jeu.
Statistiques de stabilite sur raw_frame_ms, sans soustraire le cout auditeur ;
budget8,333333ms. Comparateur refuse plafonds differents/non verifies, readbacks
visuels et diagnostics shadow/sampling. Cinq controles de comparaison passent,
ainsi que les preflights reels natif/candidat et rejet de paquet altere.

Orbit natif : premier lancement sort1 avant auditeur, profil92 fichiers restaure.
Unique reprise de demarrage permise atteint la section83-100, puis deux morts
vers88,44 et blocages importants ; reference REJETEE, aucun couple revendique.
Statistiques partielles ne representent ni une run reussie ni une preuve de gain.
Grief24-65 : sans5213 images incluses,76,3586FPS,p9923,6321ms,pire29,2586ms ;
avec5281 images,77,3739FPS,p9925,6422ms,pire40,1842ms. Zero mort des deux cotes,
meme niveau/auditeur/profil/reglages ;89,16%/87,35% images >8,333333ms.
Un seul couple : moyenne legerement superieure, pics p99/pire moins bons,
aucune acceleration causale et aucune promotion. Pas de collecte visuelle dans
ces essais performance : ils ne prouvent PAS l'absence d'ecrans noirs.
Chaque passage termine a restaure/verifie92 fichiers ; installation inchangee.
Preuves privees : build-research/runtime-canonical-v10-isolated-fps120-r4-performance.

R4-31 : plan de reconstruction de paquets persistants par dependances, inspire
des regions Sodium et du cache de batches Bismuth, sources primaires epinglees
dans research/R4-31/DECISION.md. Aucune copie Sodium, aucun changement de licence.
Revue contradictoire propre : corriges reutilisation d'adresse de scene, echec
de transfert laissant un paquet valide, allocations par frame, visibilite perimee.
Ordre natif conserve ; decision passage natif entier avant tout dessin prive.
Deux classes de ressources separees : geometrie residente et parametres GPU ;
un changement de parametre ne reconstruit pas les sommets dans le modele.
Controles CPU compiles/passes, y compris compte synthetique1000 paquets affectes
sur100000 pour une entree changee. Ce compte n'est PAS un gain FPS en jeu.
Prototype sans adaptateur GD/GL, reste isole/desactive. Invalidations completes,
pixels, effets2.2, buffers/fences, cout total et ecrans noirs restent a prouver.
Ni installation principale, archive Documents, commit, push ou publication.

Heliopolis74-78 : references120 toutes deux completes, zero mort. Sans165 images,
29,8933FPS,p9957,4431ms,pire61,7805ms ; avec194 images,34,9382FPS,p9941,99ms,
pire45,8064ms. Toutes les images des deux passages depassent8,333333ms. Ce couple
unique est descriptif : le candidat ne change que le calcul Area, dont le cout
est minuscule ici ; l'ecart observe n'est donc pas attribue sans repetition au
mod. Aucun objectif120 ni validation visuelle globale atteint.

Diagnostic R4-29 recompile : auto-controle20 snapshots PSS passe, handles equilibres,
worker actif, processus non-fixture rejete. Auditeur samplingV2/120 compile et
package16d9e48613be34306a9f44a0dc365b0ddb5845f5bb0f26cca104c72424f8abae
(DLL40635fe4c26e39e6f471cc9d80b96e1abfd28a309c012f7d041cf89b3c67ffa5).
Heliopolis diagnostic isole distinct : garde session/hash verifiee,18 adresses,
zero manquante/erreur nettoyage,64,0112ms de snapshots en256,957ms. Limite de
cout observateur25% declenchee : aucune conclusion fine sur18 observations.
Une seule reprise justifiee avec intervalle50ms au lieu de10ms est engagee.
Tous les frametimes sampling exclus par l'auditeur/comparateur. Donnees privees
separees dans runtime-canonical-v10-isolated-fps120-leaf-r1/r2-sampling.
Lecture statique updateObjectSection0x227f50 confirme deja un chemin rapide
quand les indices de section ne changent pas ; cela ne justifie PAS de supprimer
les callbacks. Aucune reecriture native issue d'un nom de fonction seulement.

Reprise sampling50ms terminee :53 observations,0 manquante/erreur de nettoyage,
168,411ms de capture sur3059,73ms (~5,50%), arret normal a la duree3000ms.
Heliopolis diagnostic atteint78 sans mort, restauration92 fichiers verifiee.
Adresses resolues sur images SHA : moveObjects6/53, preUpdateVisibility5/53,
updateVisibility3/53, GameObject::getObjectTextureRect0x1978f0 :3/53 (binding verifie), setDirtyRecursively2/53 et updateAtlasIndex2/53. Ce sont des feuilles
observees dans le temps mural, avec attentes possibles, sans pile inclusive :
pas des pourcentages CPU prouves. Aucun getter d'opacite ne domine cette petite
capture ; ne pas prioriser sa reecriture comme si son cout etait deja mesure.
La prochaine integration doit viser le suivi spatial/mouvements et la preparation
persistante de geometrie/parametres, avec preuve des callbacks natifs et de toutes
les dependances. Le prototype R4-31 ne retire encore aucun de ces calculs en jeu.
Au total de cette reprise : huit sessions de fixture (dont demarrage Orbit en
echec), restaurations92 fichiers a chaque fin ; aucun jeu laisse ouvert par nous.
Preuves de gain120, de fidelite et d'absence d'ecrans noirs toujours insuffisantes.

## 2026-11-03 — Session autonome de cinq heures, R4-32

Demarrage11:48:47UTC, echeance demandee16:48:47UTC. Cible120, tests autorises,
profil isole/full detail/sans shake/sans noclip, aucun sous-agent, publication,
installation principale ou archive Documents remplaces. Travaux locaux preserves.
Capture native bornee de deux images, sans modifier de dessin ni callback :
premier lancement refuse avant niveau par la garde atlas de l'auditeur.
Exception limitee au profil capture compile, en mode natif sans production ;
table de verite controlee, interdiction intacte pour les profils performances.
Reprise Heliopolis74-78 complete sans mort ;92 fichiers restaures/verifies a
chaque tentative. V1 DLLdc1a8404a95a1c46a7924ea08b66173a23bb915350fab3a91347a1c0efabe03b,
paquet5fa09c856489238be4ee0fc987ba993799d19b9ba4fd4ff367939463e968882c.
Capture22 draws/image,10755/10919 quads, tous associes, zero erreur de capture.
Alignement par identite :10690 conserves,229 ajoutes,65 retires, ordre relatif
conserve ;zero changement geometrie locale/UV,7965 matrices et1711 couleurs
modifiees. Pas un gain FPS. Appariement strict par taille ratait dix lots.
V2 a capture un mauvais champ de controle ; correction V3 ci-dessous.
Visibilite, FP, parent UID et groupes/racines sont observes sans getters natifs. Aucun de ces constats ne prouve qu'un callback peut etre retire.
Details/preuves utiles seulement dans research/R4-32/DECISION.md et captures
privees build-research/runtime-canonical-v10-isolated-pass-capture-r2-pass-capture.
Headroom essaye sur le log : proxy local indisponible, compression0,3% seulement ;
ne pas repeter les gros logs, utiliser filtres/lectures ciblees. Aucun service
annexe redemarre pendant les essais.

### R4-32 correction V3 et R4-33 — 3 novembre, session cinq heures

Le controle natif0x149 est _realOpacity, PAS m_bUnkBool2 (0x139). Les assertions
ABI ont bloque la compilation fautive ; V3 lit le bon octet. Oracle Cocos reel,
calcul GPU puis geometrie residente/parametres animes et rejeu de deux images
implementes. Sources/preuves syntheses dans research/R4-33/DECISION.md.
Heliopolis21306 quads, Grief32:5099, Grief64:4129, Orbit3596 et Society2730
pris en charge correspondent bit a bit ; cas non geres gardes natifs. Corpus
adversarial4097, quatre arrondis x quatre DAZ/FTZ, borne atteignant FLT_MIN,
24 scenarios pixels exacts et mutants opacite/MVP noir rejetes. Aucun gain FPS.
Revue : initialisation manquante au passage vers un quad natif nul corrigee.
Resultat NEGATIF du noyau resident, travail CPU conserve : mediane0,694806ms
contre0,584625ms direct (9 lots alternes x16). Ne pas activer. Il faut retirer
du vrai travail natif avec preuve, pas ajouter des uploads/shaders par-dessus.
Society auto139648964 ajoute sur demande ; diagnostic0-15 atteint apres une
mort initiale, donc comparaison invalide. Orbit deux morts, invalide egalement.
Grief32-65 et64-65 atteints sans mort.92 fichiers restaures/verifies par passage,
installation principale inchangee, aucune capture d'ecran ni publication.

R4-34 : diagnostic materiel des lectures/ecritures de quatre X d'un sprite,
sur un seul intervalle main-thread. Premier essai contexte logiciel sans hits
rejete ; helper Get/SetThreadContext suspendu/verifie fonctionne. Tests nettoyage,
debordement, proprietaire et LastError passent. Deux passages Helio74-78 sans
mort,92 restaurations chacune.15 acces resolus :4 ecritures Cocos,4 lectures
updateQuad,7 lectures runtime C++ (candidats de pile de l'auditeur, pas unwind).
Octets verifies a la fin, pas snapshot atomique ; aucune fermeture globale des
dependances prouvee. Certains atlas sont deja propres/residents : volume natif
complet du R4-33 reste theorique. Details bornes dans research/R4-34/DECISION.md.

R4-35 : oracle ABI sur deux fonctions possedees par l'auditeur, aucun getter GD
accroche. Appelant Win64 remplit XMM1/2/3, detours les ecrasent volontairement ;
Geode reel conserve les12 mots pour les deux signatures Thiscall float (1/2 args),
retours detour2/8 observes, hooks retires et retours natifs1/7 retrouves. CPU
controle positif/poison passe. Cela ne prouve PAS les call-sites natifs ; R4-30
reste bloque. Paquet0198307657a0cfc0d19724460a3871cd003eae796b5107f31497191c3e64595f.
Society0-15 atteint avec1 mort initiale : passage invalide FPS/visuel,2071 images
exclues.17 atlas/18 deja propres sur chacune des2 images capturees.92 fichiers
restaures/verifies, installation principale et candidat normal inchanges.
Details/prerequis encore ouverts : research/R4-35/DECISION.md. Aucun push/export.

R4-36 : hash entier natif remplace dans un prototype prive, valeurs toujours
lues dans la table native vivante (aucun cache d'opacite). Identite exhaustive
des10000 hashes,42 438 evaluations controlees puis56 584 avec variante factorisee.
Essai reel Helio74-78 N/A/B/B/A/N :6 cibles atteintes sans mort,92 restaurations
verifiees chacune. N1 sans focus sur195 images -> bras sans Chlorium REJETE.
Bloc reference A/B/B/A valide : medianes28,916 contre27,811FPS ;
p9952,129 contre51,453ms ; pire53,338 contre58,056ms. Aucune activation :
moyenne/pic regressent, causalite incertaine et120FPS non atteints. Donnees exactes
dans research/R4-36/DECISION.md et heliopolis-reference-abba.local.json prive.
Variante CPU sans table : exacte mais pas meilleure de facon convaincante ;
pas de test GD supplementaire de cette variante. Prochain levier : mesurer les
appels reels plutot que supposer que GD ne partage pas deja certains calculs.

R4-37 : comptage natif sans patch par deux points d'arret d'execution, un seul
intervalle entre captures Helio74-78.92 produits de groupes/92 recherches,
184 evenements complets :70 retours3af8aa,22 retours3af9fe,92 retours19ccfb.
Zero mort, cible78 atteinte,92 fichiers restaures/verifies, installation intacte.
Tests isoles1200 traps + regression des lectures/ecritures passent. RF, debordement,
registres XMM, retour natif et propriete des slots controles. Ce n'est pas une
mesure FPS. Piste opacite non prioritaire ; viser le parcours memoire/mouvements.
Details et empreintes : research/R4-37/DECISION.md. Aucun changement promu.

R4-38 : prechargement memoire dans une copie de la boucle moveObjects native,
calculs et callbacks conserves.6720 cas compares octet/FP/callback exacts.
Trampoline puis version inline : toutes les36 paires de chaque variante perdent.
Ratios medians1,104-1,419 puis1,168-1,287. ABANDON, aucun test/patch GD.
Details research/R4-38/DECISION.md ; mesures de noyau uniquement, aucun gain jeu.

R4-39 : SSE2 groupe les4 sommets d'un meme sprite.1296384 comparaisons natives
sur Helio/Orbit83/Grief64 passent,29031 quads captures pris en charge exacts.
Noyau Helio : ratio median0,9541,6/9 gains,plage0,9003-1,1237 -> preuve
insuffisante pour activer. Ne remplace aucun callback ; voir R4-39/DECISION.md.

## 2026-11-04 — Reprise autonome de cinq heures

Debut06:14:44UTC, echeance11:14:44UTC. Branche et modifications locales preservees,
profil isole/full detail/120FPS/noshake/noclipfalse, sans sous-agent ni publication.
R4-40 : NaN revele un ordre d'operandes inverse par le compilateur -> echec
avant activation. Instructions SSE2 explicites :1754528 cas arithmetiques passent.
Insertion dans le Cocos du helper uniquement :1750592 methodes completes
comparees sur Helio/Orbit83/Grief64 passent. Gardes fallback observees, contexte
unwind/RBX/RDI/XMM6..15 verifie, corps natif restaure apres chaque processus.
Copies CCSize conservees, callbacks natifs conserves. Gain CPU de noyau faible :
ratio0,98378,7/9 paires,2 regressions -> PAS d'integration/test jeu justifie.
Methode/details/limites : research/R4-40/DECISION.md. Reference normale inchangee.

R4-41 : occlusion opaque ordonnee, prototype GPU isole.129 cas RGBA8 exacts,
68 fallbacks, mutants noir/alpha/early-depth detectes. Cas synthetique favorable,
mais ZERO quad entierement opaque dans les lots Helio observes. Variante alpha :
2005 quads natifs captures avec VBO/IBO/programme GPU lies verifies. Reproduction
native imparfaite :204 octets/138 pixels differents, candidat=baseline helper.
Noyau non valide en jeu : GPU0,726->1,931ms, wall1,408->2,716ms,0/9 gains.
ABANDON de cette variante ; aucun gain jeu/fidelite revendique.92 fichiers
restaures apres chaque diagnostic, reference normale inchangee. R4-41/DECISION.md.

R4-42 : programme de mouvements GPU partage par combinaison, additions FP64
ordonnees.2163720 etats/prefixes natifs compares exacts,36 rejets arithmetiques,
4 mutants observateurs detectes. Gros noyau64 commandes favorable (ratios wall
0,174/0,272), petits lots perdants (17x/49x). AUCUN gain FPS ni callbacks GPU prouves.
Census reel Helio74-78 :696/986 appels,32849/34816 visites,467/678 groupes vides,
plus gros groupe3290 visite une fois, repetitionmax7/10. Le noyau64 commandes
ne represente PAS ces images ; presque toutes les visites ont des indices de
section nonnegatifs, les effets de leurs callbacks ne sont pas clos. Zero mort,92 fichiers restaures, aucun chemin normal modifie. R4-42/DECISION.md.

R4-43 : comparaison dans le framebuffer natif : copies avant/apres, redessin
natif,977 quads points retires sur2009 et restauration finale :ZERO octet different.
Helio74-78 atteint sans mort,92 fichiers restaures. Noyau points defavorable :
GPU ratio1,0032, CPU1,5701, wall1,0197 ; pas d'activation. Outil conserve pour
verifier les prochaines variantes GPU au meme endroit, sans accepter le decalage
194/204 octets du helper hors jeu. Tests isoles14 cas,5 fallbacks,mutant detecte.
R4-44 : shader ignore uniquement RGBA exactement nul, lookup avant branche.
Reproduction isolee : candidat=baseline helper ; ecart natif194 octets toujours
explicite.9 paires : span GPU ratio0,9280,wall0,9667,CPU1,2198. Ces chiffres NE
sont PAS des frametimes de jeu ni un gain global. Diagnostic natif distinct en
cours ; aucune modification d'installation/reference, aucun commit/push/export.

R4-44 natif termine :2009 quads, shader clone execute (`drawn`),ZERO octet
different pour baseline/points/zero-fragment/copies/restauration. Helio78 atteint
sans mort ;92 fichiers restaures et SHA verifies. Preuve locale d'image seulement,
pas de gain FPS global. Prototype conserve isole, reference normale inchangee.
Session06:14:44UTC -> cloture apres11:14:44UTC (plus de cinq heures). Aucun
sous-agent, capture periodique, commit, push, publication ou export Documents.


Society auto139648964, test demande apres session : selection diagnostic>=256
pour atteindre le lot420 (ancien seuil1024 ne testait rien au debut). Shader
inchange.0-15% : baseline/candidat zero-fragment/copies/restauration, ZERO octet
different ;5 quads points retires. Replay exporte natif exact aussi.9 paires de
noyau : GPU1,35854->1,18391ms (ratio0,8732), wall1,9465->1,8102ms (0,9298),
CPU0,083->0,1047ms (1,2447). PAS des FPS en jeu ni un gain global.
Mort au depart0,0061235% dans le diagnostic ET dans une reference native sans
ces captures/prototype ; les deux atteignent15% apres reset, comparaisons FPS
rejetees. Cause de la mort non etablie.92 fichiers restaures apres chaque passe,
installation/reference inchangees, aucune promotion. DetailsR4-44/DECISION.md.

## 2026-11-04 — Continuation autonome d'une heure

Debut16:58:37UTC,echeance17:58:37UTC. Branche/diffs preexistants preserves.
R4-45 : observation native sans hook de mort/collision, R8 capture/teste. Cause
immediate Society : objet8,type2,position/start(0,105), appel natif depuis214950,
mort0,0061235%. Hypothese ID du clone0 : garder139648964 ne change pas la mort.
Pas de contournement ni de relaxation des criteres ; passages FPS rejetes.
R4-46 : shader zero lie temporairement autour de l'appel atlas NATIF complet,
avec garde corps/texture/hooks. Uniformes natifs, uploads/dirty/callbacks conserves.
MXCSR/LastError entrants et effets sortants du callback conserves, meme exception.
Rejeu Cocos complet natif exact :9 paires,ratio spanGPU0,8613,wall0,9189,CPU1,1731.
Mesures de lot isole UNIQUEMENT,pas FPS. Pixel-identique en cible native : Society
0%420 quads,Helio74%585,Society10%474 ; un seul appel natif par variante.
Validation renforcee : pas de succes sans shader demande execute/couleur exacte/
restauration ; requetes d'occlusion actives rejetees.14 cas,6 fallbacks,mutant teste.
Helio78 atteint sans mort ; Society15 atteint apres mort initiale, preuve FPS
invalide. Chaque passe restaure92 fichiers/SHA et laisse le paquet principal intact.
Details techniques : R4-45/DECISION.md et R4-46/DECISION.md. Rien promu/publie.

Verification finaleR46 : Helio74-78,lot580,un appel natif,zero difference pixels,
renderer_comparison_passed=true,sortie succes,zero mort.281 images/281 exclues FPS.
Paquet793466ee285b27570ac1a4298a1c6258328a096d94666cd916a49d5a8af8a834.
Sources compilees verifiees/archivees pour cette heure ; reference107b945c... et
installation principale non remplacee (paquet local.chlorium absent au controle final).
La reference475191c7... du compte rendu ne constitue pas une verification finale.
Toujours aucun gain FPS global/120 stable prouve,
ni activation normale. Prochaine etape : module experimental persistant separe
avec cycle de vie des programmes et auditeur independant avant A/B complets.

Correction utilisateur2026-11-04 : Society tue normalement au premier essai.
La mort initiale n'est donc pas un bug Chlorium a corriger. Pour les prochains
A/B, meme premier essai de preparation puis mesure de la tentative suivante.
Les captures diagnostiques existantes restent exclues des preuves FPS ; aucun
changement collision/gameplay ni requalification retroactive des mesures.

## 2026-11-04 — R4-47, fondation du renderer persistant

Priorite utilisateur : reecriture complete du rendu. Aucun test pendant sa partie.
Scene sur notifications, handles generationnels, lanes geometrie/parametres/ordre/
fallback ; stockage et dessin GPU propres, shader R4-33 reutilise. Plus de scan
complet par image dans cette nouvelle scene ; aucun chemin natif retire.
Revue : corrige couleurs/UV du fallback, changements de scene/consommateur,
ordre invalide apres destruction, et distinction echec avant/apres dessin.
Sources de controles CPU/GPU preparees, NON compilees/NON executees. Installation
et version normale inchangees ; aucun gain declare. R4-47/DECISION.md detaille
les contrats manquants : producteurs natifs, lecteurs CPU, effets, groupes/Area
et validation. Fondation isolee, pas encore un remplacement complet de GD.

## 2026-11-05 — R4-47, raccord aux notifications de sprites natifs

Encodeur partage R4-33 ; registre par adresse/ID natif, sans dereferencement.
Scene.update compare seulement le sprite notifie et salit les lanes modifiees.
Adaptateur SHADOW : comparaison CPU96 octets avec le quad observe, MXCSR restaure,
fallback complet si divergence/source non concordante. Il ajoute du travail de
validation et ne supprime aucune preparation native. Aucun hook natif installe.
Revue corrige la creation apres destruction avant reparation de l'ordre.
Controles CPU/GPU prepares pour ces transitions ; toujours NON compiles et NON
executes selon la restriction utilisateur. Aucun gain FPS/activation declares.

Suite statiqueR4-47 : jeton unique de creation exige meme si adresse+ID natif
sont reutilises ; cas de controle prepare. Pont GLSL pour UN programme Cocos
standard epingle : graphe de positions resident, varyings natifs, expression
fragment couleur*texture conservee sans facteur de demonstration. Source GLSL
convertie n'est pas preuve du programme natif lie ni de parite pixels. Sources
deja capturees extraites en fichiers prives ; aucun test/compilation/lancement.
Autres effets/programmes et raccord des hooks restent a etablir ; rien active.

## 2026-11-06 — R4-47/48 qualifies, R4-49 premier lot natif GPU exact

Tests reautorises explicitement. Graphify interroge via NetworkX ; graphe ancien
utilise comme carte puis sources actuelles verifiees. R47 CPU passe ;15 images
GPU possedees exactes avec plages SSBO et vrai shader Cocos. R33 regression :
25503 quads eligibles/21676 captures natifs,zero divergence GPU,24 images exactes.
R48 modele updateQuad :480 cas natifs passes ; inventaire statique NON exhaustif.
R49 : nouveau dessin resident branche a l'auditeur prive, positions reconstruites
depuis les sources natives, fallback complet sinon, restoration R43 conservee.
Demarrage froid sans auditeur : aucune preuve. Helio74-78 distinct a chaud :
580/580 quads reconstruits,zero octet divergent sur comparaison/restauration,
zero mort,success=true,92 fichiers restaures. Paquet5b1d359f... ; voirR49/DECISION.
Preparation CPU native toujours executee ; aucun gain FPS ni activation normale.


## 2026-11-06 — R50 cache persistant, divergence Society, R51 hybride natif

R50 : stockage conserve entre deux captures, ordre identique sans revision ni
upload, publication complete des temoins natifs, garde anti-image perimee. Helio
580 vers587 quads : cache reutilise,587 reconstruits,zero octet divergent ; transferts
prives de mise a jour4096/30720/2348/0 octets. Premier refus sans dessin : aliases
mono BACK/BACK_LEFT corriges en conservant rejet HDR/MRT/erreurs GL. R43 passe
14 images possedees,9 refus,1 mutant,2 controles de format par defaut.
Society R50 :420 quads,zero uploads,mais4 octets d'image divergents. Rejet conserve.
Oracle R33 :2730 quads captures Society exacts (6557 eligibles avec synthetiques),
zero divergence GPU,24 cas pixels. Probleme situe apres la production des quads.

R51 : nouveau flux GPU de quads vers un VBO prive, puis dessin par le programme
natif reel ; stockage persistant et restauration TF/VAO/SSBO/texture preservees.
17 images possedees exactes,CPU/8 gardes comparaison/gardes lancement passes.
Society R51 :420/420,zero divergence ; donnees VBO/IBO/images natives/binaire natif,
textures,MVP et etat identiques au cas R50 rejet. Mort initiale attendue = passage
entier NON qualifie. Helio R51 :587/587 exacts,zero mort,cible78 atteinte. Tous les
92 fichiers partages restaures,installation principale inchangee. Preuves/binaire
exact conserves sous build-research/resident-native-replay ; details R49/DECISION.
Census corrige : ces diagnostics peuvent consommer les erreurs GL. Pas de gain FPS
prouve : preparation CPU native toujours payee,passage GPU supplementaire,lectures
pixels. Aucune activation normale ; autres effets/Grief/Orbit restent a qualifier.

Cloture de cette campagne bornee : binaire teste natif4cfd0df6...,conserve avec
ses sources et r51-proof.local.json. Dernier rebuild c1f9592e... compile/package,
non relance dans GD : comparaison des archives confirme une UNIQUE modification
du drapeau de diagnostic des erreurs GL ; tous les autres fichiers/chemins de
rendu sont identiques. Gardes de lancement finaux,8 tests de comparaison et diff
check passes. Pas de commit/push/publication/installation principale. Etape
suivante : conserver les quads GPU entre dessins et etablir les lecteurs CPU
avant de supprimer la preparation native ; aucun raccourci sur groupes/Area.


## Reprise 7h — R52/R53, donnees persistantes et mises a jour exactes

Branche conservee research/area-pose-v6 ; Graphify interroge (graphe ancien,
relations non inventees), puis verification des sources actuelles. R52 : quads
GPU maintenus par emplacement, indices produits sur GPU ; modifications d'ordre
sans recalcul de geometrie.24 images possedees exactes,foreign SSBO4/TF preserves,
initialisation refusee si compteur natif de primitives actif. Dans GD : Society
420 objets,zero divergence,aucun recalcul apres seed ; Helio587 objets,zero
 divergence,640 emplacements recalcules.92 fichiers restaures pour chaque passage.
Mort initiale Society attendue : passage entier exclu des comparaisons.

R53 : masque CPU exact des quads modifies, reconnu seulement au commit reussi,
independant des plages d'upload elargies. Nouveau producteur compute GPU issu du
graphe arithmetique epingle ; donnees natives96 octets par quad,programme natif
pour le dessin.24 images possedees exactes ; CPU/commit refuse/masques passes.
Oracle Cocos reel :6827 et25773 quads compares,zero divergence ; notification
manquante volontaire detectee puis reparee.198 triplets d'images chronometres
R51/R52/R53 strictement egaux. Noyau8192 objets :82 changements disperses,
7872 recalculs R52 contre82 R53 ; mediane GPU0.3152 vers0.1939ms,p950.3379 vers
0.1960ms. Negatif : petits blocs contigus R53 env0.01ms plus lent queR52 ; aucune
promotion sur ces chronometrages isoles,aucun gain FPS global deduit.
GD R53 : Society420 exact/cache reutilise ; Helio587 exact/587 recalcules. Native
restore exact. Analyse du lot Helio :580 matrices retenues changent uniquement
TX,quads579 modifies ; hypothese de faux dirty flags rejetee (aucun flag change).
Couts CPU natifs non retires. Binaires/sources R51/R52/R53 conserves avec empreintes,
preuves build-research/resident-native-replay/r52-r53-proof.local.json.

R54 en cours : observateur opt-in de producteurs natifs (pose,couleur,updateQuad),
original appele une fois,FP/LastError post-appel preserves,pas de pointeurs natifs
suivis apres stockage. Champs d'admission SourcePayload compares apres seed1,
contre CPU natif2. Budget borne et cas ecritures/notifications manquantes testes.
Hooks supplementaires desactives par defaut. Compiler/serialiser passe ; validation
GD du mode inactif/actif encore requise. Aucune suppression de callback/calcul,
aucune installation principale/publication,fermeture des lecteurs CPU toujours
NON prouvee. Recherche de la vraie alimentation incrementale du renderer.

R54 : mode inactif verifie Society (pas de receipt producteurs, hook atlas inactif).
Premier Helio actif : donnees sprites exactes mais suivi atlas incomplet ; mort
a50.82%,passage entier exclu. Observateurs depuis limites a deux frames, inactifs
au demarrage/desactives a la fin. Helio r54-windowed-producers-helio-r3 : cible78,
zero mort,585 quads GPU reconstruits exactement et restauration image exacte.
11240 paquets sprite/indices exacts ; suivi atlas9138 exacts,11 manquants,2091
divergents : NON qualifie.92 fichiers restaures,package principal inchange.
Binaire/sources R54 geles sous resident-native-replay/r54-windowed-qualified-helio*.
Pas de gain FPS deduit. Census RGBA zero : Helio/Society,aucun quad zero-couleur
non ponctuel ; abandon du culling supplementaire sur cette hypothese sans cible.

R55 statique : inventaire des exports instance Cocos etendu aux retours non-void
et methodes const (84 contre55). Nouveau lecteur initWithTexture a8bcf0 identifie.
118 echappements restent ouverts ; fermeture CPU toujours NON prouvee. Tests du
classificateur passes. Aucun calcul natif supprime.
R55 prototype : alimentation GPU depuis paquets producteurs, donnee oracle lue
uniquement pour refuser les divergences, pas pour fournir le payload. Mode reste
diagnostique deux frames, seed initial natif. Tests manque/source modifiee/index
change/quad divergent/identite dupliquee passent ; compilation SDK en cours.

R55 compile/link/package passes. Helio :588 quads issus des paquets producteurs,
Grief :288 ; images et restauration exactes,zero erreur GL,fenetres atteintes
(Helio78,Grief65).92 fichiers restaures pour chaque passage. Tous les calculs et
callbacks natifs restent executes ; aucun gain CPU ou FPS n'est prouve par ce
raccordement. Grief24% contient3004/3021 quads,lot le plus grand607/611 : les288
quads controles ne representent ni tous les lots ni toute la fenetre24-65.

R56 revue GPU : les barrieres apres compute couvrent les fetchs vertex/index,
mais pas une prochaine reecriture SSBO apres leurs lectures. Ajout d'une barriere
SHADER_STORAGE avant reecriture d'un stockage deja initialise ; seed/reuse sans
travail conserves. Reference : Khronos OpenGL-Refpages/gl4/glMemoryBarrier.xml,
section GL_SHADER_STORAGE_BARRIER_BIT (ordre des ecritures apres acces anterieurs).
Test instrumente echoue avant correction,puis passe :24 images possedees exactes,
barriere requise pour pose/ordre/style/remplacement et absente pour seed/reuse.
Ce risque de course est corrige dans le prototype ; aucun lien causal avec les
anciens blackscreens n'est encore prouve. Verification SDK/natif encore requise.

R56 verification : compilation/link/package passent. Orbit capture a89%,cible100
atteinte,zero mort.444 quads depuis paquets producteurs,444 reconstruits sur GPU,
zero octet divergent et restauration exacte,zero erreur GL detectee. Les autres
lots/frames restent natifs ; ni run entiere remplacee ni gain FPS global prouves.
Overlay corrige en DIAG CHLORIUM / NATIF + GPU pour ce mode hybride, plutot que
SANS CHLORIUM. Gardes de lancement,24 cas GPU,contrats CPU passent.92 fichiers
partages restaures ; baseline SHA inchangee,package principal toujours absent.
Sources/binaire et preuve scopes : resident-native-replay/r56-qualified-orbit-batch*
et r56-proof.local.json. Decision : garder le prototype isole,ne pas activer la
version normale. Suite architecturale : etablir fermeture des lecteurs/lifetimes
et alimentation incrementale avant de retirer la preparation native ; qualifier
les autres lots/programmes/effets et mesurer frametimes en paires equilibrees.

## R57 — nouveau mandat : continu d'abord, arret des essais sans Chlorium

Le renderer R51-R56 etait un replay ponctuel. R57 introduit un mode continu
separe,cache par atlas,eviction des enregistrements hors ordre courant sans
inferer une destruction native,source du shader epinglee embarquee. Gardes de
programme/ownership/contexte/topologie et bornes conservees ; repli natif pour
les cas non admis. Nouveau paquet native-resident-continuous-r57,ancien coeur
Area V10 absent. Installation principale inchangee.

Chaque lot accepte est dessine par le renderer GPU,avec copie GPU de sauvegarde
du target pour annuler un echec apres soumission. Le corps atlas natif s'execute
ensuite UNE fois avec rasterisation desactivee : conserve uploads,calculs,
compteurs/etat natifs et callbacks,sans redessiner les pixels. Couts natifs et
vertex work demeurent ; cette premiere integration peut degrader les FPS. Pas
de gain annonce. Les lots incompatibles utilisent integralement le chemin natif.

344 controles GPU possedes passent,dont320 dessins consecutifs avec churn
d'identites au-dela de la capacite,poses,couleurs,ordre et disparitions,blend
normal/additif,annulation d'une image volontairement endommagee. Ancien mode
ponctuel toujours verifie. Compilation/link du nouveau renderer passent ; son
integration GD et sa couverture reelle restent a verifier. Auditeur independant
R57 en compilation ; futur essai avec Chlorium seulement,pas de nouvelle reference.

Stockage :270 objets de compilation regenerables retires.387 copies volumineuses
de sauvegardes DEJA RESTAUREES et verifiees mutualisees par liens physiques NTFS,
contenus SHA256 controles avant/apres,chaque chemin de restauration conserve.
Espace libre observe12.64 vers18.56Gio (valeurs ponctuelles),7.79Gio logiques de
doublons evites. Aucun code,binaire de reference,preuve ou sauvegarde unique
supprime. Receipts cleanup-compiler-outputs.local.json et backup-compaction-*.

R57 reel dans GD : Helio0-78%,3234 frames,67768 lots effectivement soumis puis
conserves a l'ecran,14618194 quads (100% des lots/quads observes),zero repli,
zero rollback/fatal signale,zero mort. Ce sont des soumissions continues du
nouveau paquet,pas un replay de deux snapshots.344 cas GPU isoles conserves.
Section74-78 :16.26FPS,p95 image119.87ms,90 images mesurees ; objectif120 NON
atteint. Aucune nouvelle reference sans Chlorium lancee. Rendu complet pixel
par pixel et absence de tous blackscreens NON prouves. Preparation CPU et
vertex work natifs restent payes,plus sauvegarde du framebuffer et lecture
synchrone des indices a chaque lot : couts a traiter avant promotion.
92 fichiers restaures,installation principale inchangee. Preuve/binaire/source
testes preserves sous build-research/resident-continuous-r57. L'auditeur teste
affichait encore gpu_active=false dans les progress generiques ; compteurs
resident_continuous_r57 attestent les vrais commits. Correction de ce seul
indicateur en compilation,aucun changement du renderer natif teste20fc32c2.

## Lanceur visible R57

Raccourci Bureau Chlorium - tests GPU et dossier Documents/chlorium/Tests-GPU-R57
avec lancement/choix des quatre autos,arret gracieux et instructions. Controleur
separe cache,jeu explicitement WindowStyle Normal,revision de campagne unique
en memoire sans ecraser le plan,mode candidate seulement,donnees partagees
sauvegardees/restaurees par le wrapper existant. Aucun nouveau paquet/source du
jeu copie dans Documents. Compteurs visibles : FPS,AVEC CHLORIUM GPU CONTINU,
part de lots effectivement commites (ou REPLI NATIF),details/shake conserves.
Les quatre choix passent le preflight sans lancer GD. Compilation/link passent
pour l'overlay ; renderer20fc32c2 inchange,paquet e00b8d89 conserve.
Premier lancement visible ferme avant le harness (exit1),92 fichiers restaures.
Lancement suivant apres WindowStyle Normal explicite : processus/fenetre valides,
compteurs R57 actifs,10473/10473 lots commites a49.1%,zero repli/fatal. Le controle
de mise au premier plan est retourne false ; Alt+Tab reste necessaire si Windows
garde Codex devant. Inspection visuelle automatique indisponible (outils CUA/node
retournent kernel assets/path not found),aucune capture repetee. Pas de validation
visuelle complete ni de gain FPS annonce. Arret utilisateur via .stop-request,
fin automatique aux bornes existantes ou deux morts,restauration par controleur.
Le test visible s'est ensuite termine avec morts (validation performance rejetee),
92 fichiers verifies restaures. Un GD principal distinct est maintenant ouvert,
conserve intact. Raccourci pointe vers le CMD pour conserver le message explicatif
si un jeu est deja ouvert ; notice precise copie de test et Alt+Tab. Pas de nouvelle
installation dans les mods principaux ni de fermeture forcee du jeu de l'utilisateur.


## R58 - supprimer le travail redondant du renderer continu (code uniquement)

Retour utilisateur : performances R57 mauvaises. Grief88952972 demande0-100,
mais quitte par utilisateur a36.1084%,pas termine. Dernier echantillon4 images
seulement vers35.88-36.11 :6.93FPS,aucune moyenne de run extrapolee. Compteurs :
34936 lots,34552 commits,384 replis budget,zero rollback/fatal signale. Pas de
nouvelle comparaison avec reference ; sources/binaires R57 conserves.

Hypothese : supprimer les synchronisations GPU->CPU par lot et le second travail
vertex natif coute moins que reproduire l'etat natif avec rasterizer discard.
Profil separe --resident-fast / native-resident-fast-r58,v0.4.0-alpha.58 :
- Topologie native telechargee/verifiee une fois par identite atlas/UID/IBO/
  pointeur CPU/capacite/contexte. Chaque prefixe CPU est encore compare bit a bit.
  Changement ou incoherence : repli natif. Writer mapBuffers156 octets epingle
  FNV e348dc857131a9f0 (SDK2.2081,m_pIndices0x38). Hors profil de mods isole ou
  writer non reconnu : lecture synchrone R57 conservee. Limite connue : les
  ecritures GPU seules ne sont pas observees ; aucune qualification generale.
- Cache d'encodage exact source192+quad96+controle FP : reutilise seulement le
  calcul pur,appelle TOUJOURS Scene::update et ses controles de vie/etat/upload.
  Evenements de style ne contournent pas cette validation.
- Lots de points degeneres entierement rendus par le natif : aucun rendu GPU
  prive,aucune sauvegarde cible pour ces lots. Aucun objet natif supprime.
- Suppression du second draw natif par predicat occlusion nul (QUERY_WAIT),sans
  glGetQueryObject ni lecture CPU. L'appel natif et ses uploads/etats/compteurs
  restent executes exactement une fois. Si indisponible/refuse : ancien discard.
  Fermeture conditionnelle en erreur : quarantaine du renderer. Sauvegarde et
  restauration framebuffer conservees. Cette suppression ne couvre QUE le corps
  natif epingle et le profil de mods isole,pas des callbacks tiers arbitraires.
  Source primaire : https://registry.khronos.org/OpenGL/extensions/NV/NV_conditional_render.txt

Relecture statique : taille du snapshot indices corrigee en capacity*6 (uint16),
controle FP inclus dans la cle,Scene::update conserve meme sur hit,mode NO_WAIT
exclu car pourrait redessiner,scope conditionnel ferme avant toute commande
ulterieure. Invalidation et fallback conserves. ABI StatsR57 conserve ; export
separe ChloriumReadFastStatsR58 pour lectures/reutilisations/contournements.

Controles ECRITS mais NON executes : R4-58/checks.cpp (anciens contrats scene +
cache topologie/identites/capacite/rejets + mutation de chaque octet source/quad
et controle FP),gpu_checks.cpp (ancienne suite GPU + preuve upload/callback/
image preserves et zero primitive du draw supprime). Aucune compilation,aucun
test CPU/GPU/GD,aucune installation ni modification du plan runtime. Pas de gain
mesure,120FPS non atteint,parite et blackscreens non qualifies. Cache additionnel
~400 octets par sprite actif : cout memoire/benefice reel a mesurer. R58 conserve
ISOLE,NON PROMU. R57 continue de payer preparation CPU/upload natifs et copie
framebuffer par lot : ces couts restants ne sont pas declares resolus.

Derniere relecture R58 : separation des flags CPU de maintenance (dirty,
recursiveDirty,flip,rectRotated,opacityModifyRGB) des parametres GPU. Preuve
source : encodeParameters copie matrice/rectangle/z/couleurs/UV autoritaires ;
corners::domain/materialize et resident.vert ne lisent que1|64|128|512|1024.
Seul le paquet de parametres prive est masque,sous macro R58 ; aucun flag natif,
callback ou controle known()/Scene::update retire. Matrice/quad natifs toujours
compares,donc UV/couleurs/poses resultant d'un flip ou opacityModifyRGB restent
pris en compte. Cela evite de salir le GPU pour un changement de maintenance
sans changement visuel. Controle source ajoute pour revision/uploads inchanges,
flag geometrie64 et rejet de flags inconnus ; NON execute. Diff whitespace des
fichiers suivis propre (relecture de code uniquement,pas un test fonctionnel).


### R58 suite - allocations par lot et checkpoint tardif (non execute)

Hypothese CPU : deux unordered_set temporaires allouent un noeud par sprite et
par preparation ; le vecteur d'ordre est aussi recree. R58 utilise deux tables
plates reutilisables AddressSet (identites exactes,zero interdit,charge<=1/2,
capacite<=65536,sondage lineaire) et un ordre persistant. Les DEUX controles de
doublons sont conserves,ainsi que les controles de vie/generation de Scene.
Croissance seule alloue ; reset efface la table,donc ne conserve aucune ancienne
identite. Le cout de remise a zero reste O(capacite),pas un cache magique O(1).

Hypothese GPU : les sauvegardes de cible avant un refus de pre-soumission sont
inutiles. Point de checkpoint prive ajoute APRES validations/synchronisation/
materialisation GPU et JUSTE AVANT glDrawElements. R58 y appelle TargetBackup.begin,
qui conserve tous ses controles et restaure ses bindings. Refus : aucun draw,
appel natif normal. Echec apres soumission : restauration toujours obligatoire.
Un refus sans soumission autorise une nouvelle tentative avec controles complets
pour ne pas condamner un atlas apres un etat transitoire. R57 et les replays
ponctuels gardent leur checkpoint externe (arguments optionnels nuls).

Relecture dependances : les seuls appelants sont NativeReplay,ContinuousRenderer,
gpu_scene_checks et NativeInputCensus ; les appels draw() sans argument restent
valides. ContextEpoch::capture restaure bien le VAO ; TargetBackup restaure ses
FBO/texture/element-buffer avant le draw. Nouveau callback interne doit conserver
l'etat GL. Compteurs distincts checkpointsSaved/preSubmitDeclines ajoutes,ABI R57
inchangee. Controles ecrits pour table pleine/collisions/churn/zero/doublons et
comparaison unordered_set ; suite GPU ecrite pour refus avant checkpoint,checkpoint
refuse sans modification de pixels,reprise puis rollback sur320 images.

Aucune compilation,aucun test execute,aucun jeu lance,aucun paquet installe.
Gain reel inconnu. La copie reste PLEIN ECRAN pour tout lot effectivement soumis ;
le cout principal des copies n'est donc PAS resolu. Reduire son rectangle sans
borne de couverture prouvee risquerait de laisser des pixels errones lors d'un
rollback : cette modification n'est pas faite. Variante R58 toujours isolee.


## R59 - validation R58, transfert GPU natif differe et fuite de predicat

R58 compile/link passent (DLL a9569527,paquet90b5f516). Contrats CPU passent,
345 cas GPU possedes passent avec shaders natifs captures. Premier lancement GPU
avant main refuse faute de glew32.dll dans le dossier isole ; build.cmd copie
maintenant la dependance existante. Preuves R58 figees resident-fast-r58-proof.
Aucun resultat FPS en jeu,aucune installation. Ces cas ne couvraient pas encore
une erreur GL laissee en attente avant la reutilisation du predicat.

Nouveau chantier R59 : enlever le transfert natif de sommets lorsque le draw
natif est supprime par le predicat zero et que Chlorium a deja soumis ses pixels.
Analyse native2.2081 : drawNumberOfQuads0xa6350 teste m_bDirty0x48,puis envoie
count*96 via BufferSubData (IAT0xe4d48),puis efface ce flag. SDK confirme que ce
flag concerne la mise a jour du VBO. CPU/preparation/callback/compteur natifs
restent executes. Scope prive suspend seulement ce transfert en conservant le
flag dirty a la sortie. Repli natif complet : upload natif normal. Repli partiel :
flag conserve pour les plages encore en attente,jusqu'a un upload complet.
Controles conserves : corps natif/texture/driver/import connus,profil isole,
predicat actif,buffer vivant/assez grand/non mappe,contexte courant et bornes CPU.
Bindings COPY_READ restaures ; exceptions restaurent dirty. Aucun skip en simple
rasterizer-discard. Aucune patch IAT dans la variante retenue.

Critique : premier prototype interceptait un import pour reporter le transfert ;
compile/link et controles isoles passaient,mais le drapeau natif rend cet import
inutile. Remplace par scope local plus court,sans changement de fonction globale.
Premiere compilation du scope echoue sur GL_COPY_READ_BUFFER_BINDING absent du
vieux GLEW ; alias GL_COPY_READ_BUFFER deja utilise par le projet repris.

Panne supplementaire REPRODUITE : predicat reutilise + INVALID_ENUM preexistant.
BeginConditionalRender reussissait,glGetError lisait l'ancienne erreur,active_
restait false et le predicat zero n'etait jamais ferme. Le clear vert suivant
restait masque (controle rouge exit1). Correction : refuser l'erreur precedente
AVANT le begin. Controle vert passe ; reproduction sous predicate-error-r59.
C'est une fuite prouvee du nouveau chemin R58,pas la preuve que tous les noirs
historiques de GD avaient cette cause.

R59 final : compile/link passent (DLL d8fb472b,paquetbcf50a7f). 345 cas GPU
herites et contrats du scope dirty passent : transfert reellement differe,
callback une fois,repli complet/partiel,buffer mappe et erreur native preservee,
bornes/bindings,scopes imbriques et exception. Controle predicat rouge puis vert.
Sources exactes,paquet et logs figes sous resident-deferred-r59-proof/d8fb472bf8d7.
Jeu NON lance,installation principale inchangee,plan runtime R57 inchange.
Aucun gain FPS mesure/annonce. Cout CPU natif et copies plein ecran toujours payes.
R59 reste EXPERIMENTAL : couverture des lecteurs GPU tiers/effets2.2 et mesures
appariees Orbit/Grief/Helio restent indispensables avant promotion normale.


## R60 — attribution CPU et reprise du 8 novembre 2026

R59 réellement mesuré sur Helio74–78 :97 images retenues,17.797FPS,p95=105.367ms,
p99=119.3529ms,pire=454.6984ms ; toutes dépassent8.333ms. Référence R57 avec même
modèle/auditeur/réglages :82 images,15.080FPS,p95=124.5505ms. Une paire seulement,
pas de gain causal annoncé. R59 confirme61287 transferts différés/1.443Go et137
lectures d'indices pour61150 réutilisations sur0–78,zero rollback/fatal signalé.
Les92 fichiers protégés sont restaurés et vérifiés à chaque sortie.

Deux lancements R59 n'ont pas atteint le harness. Le contrôle R57 et un diagnostic
R59 avec logs détaillés ont atteint la cible. Cause du démarrage intermittent non
établie. Inventaire des mods maintenant limité à une partie active et au loader
Done ; aucune prétention que ce garde résout à lui seul tous les démarrages.

R60 ajoute des scopes QPC sans allocation :préparation native hors atlas,capture,
indices,préparation GPU,programme,sources,uploads,matérialisation,backup,dessin et
corps natif. Sommes imbriquées et LastError vérifiés,345 cas GPU hérités et contrats
R59 passent ; compile/link candidat et auditeur passent. Détection de diagnostic
corrigée dans le script :le marqueur doit être lu avant de libérer la chaîne DLL.
Provenance R60/startup marquée non comparable pour ne pas transformer une mesure
instrumentée en preuve de gain.

Passage R60 interrompu à51.633%,cible74–78 NON atteinte. Attribution disponible sur
40.066–51.633 :11.347s cumulées dans les batches,dont10.737 dans les atlas ;
prepare4.123s(inclut sources2.379s et programme0.408s),backup1.146s,upload0.498s,
matérialisation0.512s,capture0.615s,draw0.505s,natif0.487s. Ces temps inclusifs ne
s'additionnent pas tous. Les compilations/réinitialisations et les sources méritent
la prochaine expérimentation ; cette section ne décrit pas les effets finaux.

Reprise utilisateur du8novembre :mise à jour du dépôt explicitement demandée.
Visibilité origin constatée publique,puis corrigée en privée selon la demande
initiale. Checkpoint de recherche seulement,pas de merge/release ni installation.
Jeu fermé et profils restaurés lors de cette reprise. Cible120FPS toujours ouverte.


### R61 — retirer le programme graphique qui ne dessine jamais (8 novembre)

Hypothèse : le chemin NativeVertexStream compile aussi un programme vertex/fragment
privé, prépare son VAO/indices et écrit ses uniforms, puis dessine avec le programme
natif et le VAO du stream. Cette préparation peut être retirée sans changer le dessin.
Initialisation stream-only explicite, réservée au profil --resident-direct ; sources
revues, contexte, domaine, états, callbacks, checkpoint et retour natif conservés.
L'endpoint direct refuse un stockage stream-only, même avec zéro objet.

Contrôle avant correction : le compteur de créations observe des shaders graphiques
et échoue. Après correction : zéro shader graphique et deux compute pour le premier
stream ; le chemin direct normal crée toujours deux shaders graphiques, y compris
après fermeture d'un stockage stream-only. Sources inconnues refusées. Les345 cas
GPU hérités et les contrats de transfert R59 passent. Ce ne sont pas des FPS en jeu.
Build GD2.2081/Geode5.11.2 compile/link et paquet privé passent. DLL effectivement
exécutée71a8ac8294ae32c6bf99ec4e5ff45cce0fa2ea06e225dbe02530aac697718a30,
paquet5337200df9711fe4778bb8d1423ddb39ebaa8619696be0be03cc821bed86270d.

Premier lancement isolé : sortie1 avant harness, aucune donnée de niveau. Diagnostic
avec logs loader : Heliopolis0–78.013%, cible74–78 atteinte, zéro mort ; 56933 lots
GPU engagés,14152116 quads, zéro rollback/fatal et zéro refus hors lots ponctuels.
Les deux sorties restaurent et vérifient les92 fichiers ; installation inchangée.
81 frames incluses toutes au premier plan :14.600FPS, moyenne68.492ms,
p95124.495ms, pire179.337ms. Une frame exclue à la frontière de couverture.
Mise à jour native18.695ms en moyenne. Aucun contrôle sans mod, aucune paire
R60/R61 équilibrée : diagnostic instrumenté et logs détaillés, PAS preuve de gain.
L'auditeur gelé affichait R60/DIAGNOSTIC ; le hash du candidat identifie R61.
Le prochain build de l'auditeur distingue maintenant R61 dans l'overlay.
Pas de contrôle visuel intégral en jeu : zéro rollback ne prouve pas zéro écran noir.

Décision : garder le prototype isolé, NE PAS promouvoir et NE PAS considérer cette
économie comme une voie de gain massif. Les sources par objet, calculs natifs et
sauvegardes framebuffer dominent toujours ; objectif120FPS non atteint. Éviter les
nouveaux ajustements marginaux et les campagnes sans hypothèse discriminante.
Prochaine voie architecturale : séparation simulation/décoration avec données GPU
résidentes, effets par groupes et visibilité spatiale. Les lecteurs CPU, callbacks,
sections et ancres restent une frontière obligatoire : R42 montre déjà que leur
suppression aveugle casserait le contrat. Le speedup du noyau R42 à64 commandes ne
représente pas les7–10 répétitions observées en jeu. Aucun multiplicateur annoncé.

Contre-vérification après modification des headers partagés : chemin R60 sans R61,345 cas GPU et contrats R59 passent ; bookkeeping CPU R60, gardes de profils/plans et syntaxe Python passent. Aucun nouvel essai de FPS requis pour ces contrôles. Nouveau libellé R61 de l'auditeur modifié mais pas encore exécuté.


### R62 — chaîne groupes → quads → dessin GPU, hors GD (8 novembre)

Hypothèse : le GPU doit conserver les positions des décors et recevoir des
commandes de groupes, plutôt qu'une copie des positions calculées par le CPU.
Le compute R62 fusionne le programme ordonné R42 et l'arithmétique de coins R33 :
état FP64 résident, estampilles/verrous/lastPosition, sortie native96 octets et
VAO consommé par le même programme Cocos que la référence. Un dispatch par image,
aucun download de position/quad avant dessin, aucune publication de position par
objet à chaque image. Transfert variable limité à7/10 commandes,224/320 octets,
indépendamment des65/4096 objets. Huit combinaisons ; huit images consécutives
sans réensemencer les états. Ce n'est pas la somme des deltas en float de Bismuth.

Contrôles :266304 états et quads comparés bit à bit à moveObjects GD2.2081 et
updateTransform Cocos exacts,128 comparaisons d'images non vides en1280×720,
quatre contrôles DAZ/FTZ masqués/arrondi nearest, signed-zero, verrous et invisibilité.
112 refus de paquets/commandes invalides ;16 callbacks natifs modifiant un futur
objet produisent bien des différences. Ces contre-exemples interdisent d'inférer
la propriété GPU depuis la seule équivalence arithmétique. Deux shaders mutants
privés sont détectés : déplacement faux par image, état non retenu par comparaison
d'état. Les téléchargements diagnostiques ont lieu après le dessin, jamais entre
le compute et ce dessin. Aucun processus GD/Geode lancé, aucune installation changée.

Intervalle GPU isolé (28 échantillons chauds par configuration, pas des frametimes
GD ni du temps GPU actif pur) :4096 objets/10 commandes, médiane0.599177ms,
p950.631879ms,pire0.670225ms. Avec7 commandes :médiane0.596521ms,
p950.628227ms,pire0.652546ms. Les intervalles comprennent les bulles de soumission.
La scène, ses petites textures et son overdraw sont synthétiques ; aucun ratio
FPS ni coût de niveau complet ne peut en être déduit. Dernier log local :
build-research/fused-group-quads-r62/checks.local.txt ; entrées et binaire dans le
reçu local associé. Reproduction :R4-62/build.cmd, puis gpu_checks.exe avec les
images GD/Cocos possédées,group_quads.comp et les deux shaders natifs capturés.

Lecture des captures V3 existantes uniquement, premier de deux échantillons :
Helio74.001%10246/10597 quads rattachés à une décoration ; Grief64.004%1745/2633 ;
Orbit83.030%1513/1741 ; Society0%1352/1365. Ce sont des proportions de quads,
pas des coûts CPU ou des admissibilités. Les sprites racines eux-mêmes ne couvrent
respectivement que8128,691,920,603 quads ; hiérarchies importantes. Les captures
Orbit83% et Society0% ne décrivent pas leurs sections lourdes ultérieures.

Critique/décision : garder cette architecture isolée ; aucune activation native,
aucun .geode R62. Racines fermées seulement : transformation locale statique,
translation issue de l'état d'objet, UV/couleurs statiques. Pas de rotation dynamique,
Area, follow, parent dynamique, mutation de topologie ou lecteur CPU arbitraire
qualifié. Le domaine des poses finales est vérifié par l'oracle avant dispatch dans
cette fixture, pas résolu pour des commandes futures arbitraires. L'adaptateur réel
doit préserver sections, ancres, callbacks et lecteurs avant de retirer les boucles
CPU. Le résultat montre une chaîne sans copie CPU intermédiaire, pas un gain en jeu.
