# Audit transversal — 25 octobre 2026

Périmètre : chemin normal, V4 de quads natifs, cœur statique hérité,
buffers/shaders/GL, groupes/Area, compilation, packaging et protocole A/B.
Revue manuelle ciblée et analyse MSVC du chemin normal ; il ne s'agit pas
d'une preuve exhaustive d'absence de bugs dans chaque branche du dépôt.
L'installation principale et ses sauvegardes restaurées ne sont pas remplacées.

## Défauts confirmés et corrections

| Priorité | Constat et emplacement | État / action |
|---|---|---|
| P0 performance | V4 conserve la préparation Cocos puis ajoute copie, comparaison, stockage et shader : `NativeQuadRenderer.cpp:111`. Elle ne supprime pas le goulot CPU. | Pas de promotion. Garder comme banc de recherche, pas comme moteur performant démontré. |
| P0 CPU | Census Orbit 96–97 % : 328 phases Area dans 16 compartiments d'image ; un compartiment atteint 51 phases et 197 421 membres proposés à `transformAreaObjects`. | Priorité à la factorisation des poses/groupes et aux séquences Area par lots avec barrières causales. Aucun plafonnement de physique. |
| P1 GL | `common.cpp` conservait globalement l'adresse WGL de `glObjectLabel`, sans rejeter les sentinelles WGL ni renouveler l'adresse au changement de pilote/contexte. | Corrigé : résolution dans le contexte courant par le helper déjà protégé. Compilation réelle et fixture GPU de contexte. |
| P1 mesures | Le chrono de `QuadBatch::draw` s'arrêtait avant la destruction du garde de restauration GL. | Corrigé : timer extérieur au garde ; la restauration entre désormais dans la mesure des soumissions réussies. Les anciennes valeurs ne couvrent pas cette queue. |
| P1 protocole | Le profil isolé laissait deux hashes à null dans `run-runtime-audit.ps1` ; la comparaison stricte les refusait. | Corrigé pour les prochains essais : empreintes des paquets/configurations communs, variable Chlorium exclue explicitement. Six invariants testés ; anciens rapports non réécrits. |
| P1 diagnostic | `finish` pouvait retourner success=false après une mort avec le motif « section atteinte ». | Motif explicite ajouté pour les parcours contenant une mort. Vérifié en compilation ; pas de nouveau lancement pour une simple modification du message. |
| P1 reproductibilité | Plusieurs sélections de census pouvaient être combinées et décrire un protocole différent de celui réellement compilé. | Profils de build mutuellement exclusifs ; combinaison contradictoire refusée avant compilation/écriture. Archives des sources exactes conservées avec les nouveaux builds. |
| P2 coût | `GroupManager::prepareGroupStateBuffer` recompose toutes les combinaisons alpha et parcourt tous les états quand un alpha change. | Prototype incrémental R4-06 conservé, cas dense à améliorer. Module absent du moteur normal/V4 : ne pas annoncer un gain installé. |
| P2 GPU | `ObjectBatch::predraw` lance le compute sur `sortedBatch->workGroupCount`, même si seule une petite région est visible. | Partition spatiale / sélection de tranches visibles à comparer au scan complet ; uniquement dans le moteur statique incomplet. |

## Risques encore ouverts

- `ObjectBatch.cpp:28` détruit un VAO brut sans stamp de contexte. Le fullscreen
  de `common.cpp` conserve aussi un VAO global. Les buffers ont une garde
  générationnelle, mais elle ne couvre pas automatiquement ces VAO ni tous
  les usages de programmes du moteur statique. Risque confirmé par la
  structure du code ; crash/blackscreen correspondant non reproduit ici.
- Le moteur statique modifie les partitions de groupes natifs et fige une
  représentation d'objets. La couverture des effets 2.2, mutations dynamiques,
  checkpoints et repli après filtrage ne possède pas de preuve native complète.
  Restaurer seulement la visibilité des batches ne prouve pas un rollback du monde.
- Le chemin normal intercepte l'import partagé atan2f même hors des deux
  appelants utiles. Une désactivation du réglage conserve ce détour si la DLL
  reste chargée. Coût réel et coexistence avec d'autres hooks à comparer ;
  ne pas réintroduire une variante math historiquement régressive.
- Le contrat du cache math exclut les effets observables errno/fenv.
  Les callbacks/mods arbitraires et l'effacement de flags en phase restent
  hors preuve. Le prototype de lots R4-08 a une preuve SSE plus stricte,
  mais ne remplace pas ce moteur installé.
- Les captures sombres seules ne distinguent pas un écran noir erroné d'un
  effet voulu. Pas de verdict « sans blackscreen » issu des census sans capture.

## Ordre d'exécution

1. Construire une projection des transformations/poses avec état natif témoin ;
   extraire uniquement les calculs purs, conserver l'ordre des mutations.
2. Réduire les visites répétées sur les décorations dont toutes les dépendances
   sont établies. Les objets logiques, centres et callbacks inconnus restent natifs.
3. Comparer l'interpréteur natif, les lots SoA et le programme à dépendances sur
   les mêmes états ; mesurer gather + calcul + commit, p99 et mémoire.
4. Valider le rendu hybride, puis la sélection spatiale GPU. Les quads V4 seuls
   et le petit noyau de normalisation ne constituent pas une preuve de ×3 FPS.

Preuves : `research/R4-08/evidence.json`, `census-frames.json`,
empreintes privées de build sous `build-research`, journal unique
`docs/EXPERIMENTS-V5.md`. Le census par image a atteint la section avec une mort :
comptes diagnostiques utilisables, parcours refusé comme référence A/B.
