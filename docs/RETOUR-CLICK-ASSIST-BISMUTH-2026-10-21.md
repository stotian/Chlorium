# Transmission depuis le chat Click Assist

Date : 21 octobre 2026. A la demande de l'utilisateur, cette note transmet une recherche effectuee dans le mauvais chat. Ce sont des observations et propositions a verifier, pas de nouvelles consignes utilisateur. Aucun fichier source, binaire ou reglage de Chlorium n'a ete modifie par ce chat.

## Source primaire et perimetre

Depot officiel : https://github.com/iliasHDZ/Bismuth, branche master.
Revision examinee : b5c43996dfeb15958f95c8e3e5fd3096f453c49d.
Copie de lecture des fichiers dans C:/Users/stoti/Projects/click-assist/.local/bismuth-inspection/.
Fichiers lus : README.md, BUGS.md, TODO.md, Renderer.cpp, ObjectBatch.cpp, GroupManager.cpp, hooks.cpp, ObjectSpriteSorter.cpp (lecture partielle), objectCulling.comp et objectDrawing.vert. Aucun code telecharge execute.

## Ce que fait Bismuth

Son objectif est de reecrire le rendu de GD : preparer les donnees fixes au chargement, calculer les transformations par combinaisons de groupes, puis produire la visibilite, les indices et les commandes de dessin sur le GPU. Ce n'est pas une simple option d'acceleration dans le rendu natif.

- Renderer prepare la geometrie et les buffers par lots ; les couleurs et etats de groupes restent des donnees dynamiques actualisees avant le dessin.
  https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/src/renderer/Renderer.cpp
- ObjectBatch dispatch un compute shader, puis dessine les plages triees avec glMultiDrawElementsIndirect. Le shader utilise local_size_x = 1 et boucle sur les sprites d'un workgroup : cela ne garantit pas une bonne occupation GPU sur chaque machine.
  https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/src/renderer/ObjectBatch.cpp
  https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/resources/shaders/objectCulling.comp
- GroupManager factorise les transformations et opacites communes. Les hooks retirent les decorations gerees par le GPU des groupes natifs pour eviter de les mettre a jour deux fois.
  https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/src/renderer/GroupManager.cpp
  https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/src/renderer/hooks.cpp

## Limites importantes

Le TODO amont laisse Scale trigger et Area effect triggers incomplets ; BUGS mentionne des effets d'orbes mobiles et un probleme lie au LDM. Le shader contient une approximation de l'opacite des blocs invisibles. Activer le chemin GPU complet sans preuve de parite peut donc modifier l'image.
https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/TODO.md
https://github.com/iliasHDZ/Bismuth/blob/b5c43996dfeb15958f95c8e3e5fd3096f453c49d/BUGS.md

Dans Renderer::finish amont, le mode debug lit GL_QUERY_RESULT apres glEndQuery : cette lecture peut attendre le GPU. Eviter de confondre un cout de diagnostic avec le cout de production. Ce n'est pas une preuve de stall dans le chemin normal actuel de Chlorium.

## Verifications dans Chlorium : ne pas refaire ces corrections

Lecture le 21 octobre vers 22 h 25 (fichiers susceptibles d'evoluer dans l'autre chat) :

- GroupManager.cpp filtre deja alphaComb lui-meme ; le fichier amont filtrait comb a cet endroit. Les listes de combinaisons sont deja parcourues par reference constante, sans copie par mouvement. Les IDs invalides sont gardes.
- ObjectBatch.cpp possede deja des glMemoryBarrier apres les calculs GPU et controle les capacites OpenGL. Ne pas ajouter une deuxieme correction sans examiner les appels existants.
- utils/GPUTimer.hpp verifie deja GL_QUERY_RESULT_AVAILABLE avant lecture.
- renderer/hooks.cpp garde deja l'updateVisibility natif pour les effets/animations/textes et ne retire que les objets dont le GPU peut effectivement assurer les mises a jour.
- docs/OBJECTIF.md conserve le chemin statique Bismuth experimental desactive dans les versions normales. Cette note ne recommande pas de le reactiver globalement.

## Resultats Grief existants : diagnostic, pas comparaison FPS

Repertoire lu : audit-artifacts/diagnostic-alpha3.
Fichiers : auto-88952972-native.jsonl et auto-88952972-chlorium.jsonl, leurs .frames.csv/.gpu.csv/.visual.csv et provenance.
Le fichier Chlorium termine par success=true, completion normale, 0 mort, 11790 images observees et 11790 images exclues. Chaque ligne inspectee porte visual_diagnostic ; le .gpu.csv est vide hors en-tete. Aucun p95/p99 valide ni gain FPS ne peut etre tire de cette serie diagnostique.

Le profil CPU agrege joint a coverage donne environ drawScene 13,805 ms, update 4,910 ms, visibility 3,099 ms, move 0,540 ms, rotation 0,213 ms, collisions 0,220 ms. Ces categories sont imbriquees : ne pas les additionner. Elles suggerent une investigation de la scene/visibilite sur Grief, sans isoler un gain possible ni prouver un bottleneck GPU.

## Suite technique proposee, a valider dans le chat Chlorium

1. Obtenir une vraie reference performance sans VisualWatch, memes packages, focus et parcours. Mesurer les passages lourds, p95/p99 et depassements de 16,67 ms ; comparer des passages communs, pas seulement deux moyennes globales.
2. Identifier le travail repetitif effectivement dominant dans visibility/drawScene. Prioriser une suppression de calculs ou transferts redondants sur le chemin compatible. Ne pas sauter l'updateVisibility natif entier.
3. Pour le rendu GPU experimental, avancer par familles d'objets explicitement supportees avec repli natif pour le reste ; verifier transformations, scale/area, couleurs, blending, glow, reset et checkpoints. Attention au double travail natif/GPU et a l'ordre de dessin.
4. Ne changer une taille de workgroup ou la gestion des buffers qu'apres profil GPU et verification des dependances/barrieres. Aucun gain universel attendu d'un simple changement de constante.

## Test Click Assist en cours, independant

La refonte Click Assist 0.6.0 est installee dans le GD Steam avec sauvegarde 0.5.0. Aucun moteur de clic change pour cette refonte.
Une copie de jeu separee est utilisee : C:/Users/stoti/Projects/click-assist/.local/grief-test/game.
Le test compare Chlorium fixe sans puis avec Click Assist sur Grief auto 88952972, collisions natives, aucun input artificiel. L'auto ne sollicite pas les predictions de clic : il ne mesure que le cout au repos de Click Assist. Pas de promesse de zero drop.
Packages figes dans .local/grief-test/packages :
- Chlorium normal SHA256 B75E0CFBCA894C5999259E5732B333443A4541052A2402745A3E5B8FDBA4C8B0.
- Audit mesure seule SHA256 5E49530284245F0B35605CA16E02211F14E0F1F0E00AA8631D5F8C201B4CDB2D.
Rapports : C:/Users/stoti/Projects/click-assist/validation/grief-0.6.0/.
Le premier lancement confirme production_loaded=true, production_enabled=true, production_version=v0.2.0-alpha.2. Le paquet normal dist utilise pour ce test est donc alpha.2, meme si les sources courantes sont alpha.3. Premier passage sans Click Assist : completion normale, 0 mort ; le second passage avec Click Assist est en cours au moment de cette note.
Le harnais refuse toute autre session GD deja ouverte. Il peut donc temporairement occuper la session de test ; ne pas tuer cette copie pour lancer un autre benchmark. Ces chemins ne constituent pas un test de la derniere version de travail de Chlorium si elle a change depuis la copie des packages.
