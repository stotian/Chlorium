# Verification visuelle renforcee — 23 octobre 2026

Le rendu de la version normale alpha.6 n'est pas modifie pendant cette passe.
Les corrections portent sur les outils prives de mesure et leur protocole.
L'alpha.6 installee garde ses transferts GPU natifs ; aucun nouveau gain FPS
ni disparition universelle des ecrans noirs n'est revendique.

## Corrections concretes

- Une lecture GPU echouee ne devient plus une image noire : stockage, fence,
  mapping, unmapping et erreurs GL sont controles ; la couverture est invalidee.
- Le filtre par une seule ligne centrale est retire. Une grille dans toute
  la largeur de la zone centrale repere aussi les grandes zones noires partielles.
  Une suspicion de quasi-noir est confirmee sur tous les pixels de cette zone.
- La zone couvre 25-85 % de la hauteur depuis le bas, pour exclure les HUD.
  Il ne s'agit donc pas d'une inspection exhaustive de chaque pixel de l'ecran.
- Les signatures repetees sont signalees comme candidats, sans les appeler
  automatiquement images figees. L'echantillonnage et la quantification ont des limites.
- La fin attend de facon bornee les lectures restantes. Une lecture manquante,
  invalide ou sautee interdit un verdict visuel valide.
- Les inventaires reels des mods, reglages effectifs, empreintes et conditions
  sont compares. Des parcours repetes ambigus ne sont plus ecrases silencieusement.
- Les captures a des pourcentages demandes affichent aussi leur position et
  horloge reelles ; les particules et animations ne sont pas declarees identiques.
- Une reference peut correspondre a plusieurs fragments d'un clignotement :
  le partage est signale, les nombres, bornes et durees restent distincts.

## Protocole et controles

Installation de test distincte ; memes paquets personnels figes et chargement
verifie dans chaque paire. LDM OFF, SHAKE OFF, collisions natives, aucun noclip
ni input artificiel. Hitboxes, trajectoires, layout mode et suppression des
shaders d'Eclipse sont temporairement desactives, puis les octets des reglages
d'origine sont restaures et verifies. Les autres mods desactives restent tels quels.

Un premier parcours V2 de Grief a ete exclu des conclusions : les hitboxes
etaient actives et masquaient les passages noirs. Cette exclusion est conservee
dans l'historique local ; elle n'est pas remplacee par un resultat favorable.

Verification des outils :

- Cas CPU : entree invalide, HUD, quatre directions de noir partiel, luminosite,
  ligne centrale lumineuse et bornes du seuil de quasi-noir.
- 157 controles GPU : etat GL et tampon du demandeur preserves, fin de file
  recuperee, echec d'allocation, fence nulle, attente echouee, mapping nul,
  unmapping corrompu et erreurs preexistantes rejetes ; sondes enregistrees.
- Cinq contre-exemples du lecteur rejetes : pixels invalides, image dupliquee,
  fin manquante, LDM active et mauvaise presence du mod.
- Test des episodes : reference partagee explicite, durees preservees,
  absence de reference et borne de tolerance.

## Quatre parcours de comparaison V2

Chaque paire utilise le meme module de mesure et les memes conditions verifiees.
Aucune mort ni crash. 31 213 lectures valides, aucune invalide, sautee ou en
attente a la fin ; aucune exclusion pour perte de focus dans ces comparaisons.

| Niveau | Couverture sans / avec | Episodes quasi noirs sans / avec | Partiels gauches sans / avec | Partiels droits sans / avec |
|---|---|---:|---:|---:|
| Grief auto 88952972 | 0-40 % / 0-40 % | 18 / 18 | 4 / 4 | 0 / 1 |
| Heliopolis auto 129598490 | Fin / fin | 1 / 1 | 1 / 1 | 0 / 1 |

Les deux candidats droits ne sont pas effaces : un sur Grief vers 12.7143 %
(une image, environ 9.3 ms), l'autre sur Heliopolis vers 76.9814 % (deux images,
environ 56 ms). L'absence de reference dans ce tableau est celle du detecteur,
pas une preuve de causalite du mod. Aucun candidat de signature repetee n'a
ete releve ; cela ne prouve pas l'absence de tout gel ou de toute image lente.

## Reprises ciblees et examen des captures

Quatre parcours supplementaires sans/avec : Grief 12.6-40 %, Heliopolis
76.8-78 %. La simulation avant ces sections reste normale, sans saut ni
acceleration. 7 467 lectures valides, aucune perdue ou invalide, aucune mort ni
crash. Sept cibles de capture sur Grief et onze sur Heliopolis, dans chaque mode.

Les captures montrent les memes grandes zones noires a droite sans Chlorium :

- Grief, cible 12.70 % : capture sans a 12.70926 %, avec a 12.70175 %.
  Le decor rouge reste a gauche et la grande zone noire a droite existe dans
  les deux modes. Le detecteur releve ici un episode droit dans chacun.
- Heliopolis, cible 76.97 % : capture sans a 76.98732 %, avec a 76.99030 %.
  Les bandes verticales et la zone noire a droite sont visibles dans les deux
  modes. Le seuil droit n'est franchi dans aucun des deux parcours de reprise.

Ce constat visuel ne constitue pas une egalite pixel a pixel. Les positions,
horloges, particules et fines bandes lumineuses peuvent varier entre les prises.
Les deux alertes initiales ne demontrent donc pas un noir ajoute par alpha.6.

Sur la reprise Grief, le detecteur compte six episodes quasi noirs sans et
sept avec : une image a 30.5107 % suit de tres pres un intervalle sombre deja
present dans la reference, jusqu'a 30.5007 %. Le rapprochement est affiche comme
reference partagee, avec les durees originales ; cette difference d'une image
reste une limite de la comparaison, pas une correction du rendu demontree.

## Controle final cible a 30.5 % (termine le 24 octobre)

Deux reprises Grief 30.3-40 %, avec dix cibles de capture par mode :
918 lectures natives et 953 avec Chlorium, toutes valides, aucune mort ni crash.
Le detecteur releve trois episodes quasi noirs sans et deux avec.
A la cible 30.50 %, les captures reelles sont a 30.50822 % sans et
30.50322 % avec. L'examen montre dans les deux cas le decor quasi noir,
les traces rouges a droite et la ligne turquoise en bas. Le passage sombre
existe donc aussi sans Chlorium ; l'alerte precedente ne prouve pas un noir
exclusif au mod. Les particules et horloges ne sont pas identiques.

Bilan de cette passe : dix parcours valides, 40 551 lectures valides,
aucune lecture invalide, sautee ou en attente a la fin. Le premier parcours
avec hitboxes reste exclu. Aucune nouvelle execution n'est necessaire pour
rediger ce bilan ; ces mesures visuelles ne servent pas de preuve de gain FPS.

## Conclusion bornee et preuves

Ces essais ne fournissent pas de nouvelle cause confirmee de noir ajoute par
alpha.6. Les anomalies detectees restent consultables. Aucun correctif graphique
speculatif et aucune reduction visuelle ne sont ajoutes au paquet normal.
Les 60 FPS stables restent non atteints. Orbit et les autres passages ne sont
pas valides par cette passe ; les textures hors zone centrale ne sont pas couvertes.

Bilans numeriques : visual-v2-run-summary.json et visual-v2-comparisons.json.
Modules de mesure : A25D4A886DEF30B18CAEC0F0C685056D051ACC4C53502A4F7CDCA28026DE4BD2
pour la premiere serie, D589FBEE2F69FEF39685EE1B8AC9BFECB4FEDBDCBE14DBFCE216B9EB2CF13F86
pour les reprises avec sondes. Paquet normal identique :
7939B0339E33C2F0F8B8E2A9FEDA9B1793A5C36D6F2558BDC4BC4C2877533AEA.

Rapports, captures appairees et traces brutes restent dans
Documents/chlorium/diagnostic-alpha6-v2 et les audit-artifacts locaux, hors Git.
