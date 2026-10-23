# Alpha.6 : verification des ecrans noirs, 23 octobre 2026

Quatre parcours en alternance Grief / Heliopolis apres autorisation de reprise.
Windows x64, GD 2.2081, Geode 5.11.2, Intel UHD. Meme executable, donnees du
niveau, module de mesure, empreintes des mods personnels et reglages verifies
pour chaque paire. Chlorium absent pour les references, charge et active en
alpha.6 pour les autres. LDM OFF, SHAKE OFF, collisions natives, aucun noclip
ni input artificiel. Les autres mods desactives restent desactives ; les
assistances aux inputs et l'AutoLDM sont temporairement desactives puis leurs
reglages d'origine sont restaures.

## Resultat visuel

| Niveau | Couverture sans / avec | Morts sans / avec | Episodes sombres sans / avec | Images sombres sans / avec |
|---|---|---:|---:|---:|
| Grief auto 88952972 | 0-40 % / 0-40 % | 0 / 0 | 15 / 15 | 80 / 91 |
| Heliopolis auto 129598490 | Fin / fin | 0 / 0 | 1 / 1 | 61 / 60 |

Aucun crash, aucun episode quasi noir supplementaire detecte avec alpha.6.
Les episodes de Grief se trouvent vers 8.53, 9.37, 9.71, 9.85, 9.93, 11.92,
12.11, 12.27, 12.38, 12.53, 12.63, 12.78, 13.00, 13.64 et 30.45 % dans
les deux modes. Les captures vers 8.53 % ont aussi ete inspectees : la scene
est presque noire dans les deux modes, avec les overlays de test visibles.
Heliopolis devient presque noir a 99.075 % dans chaque mode, sur environ
1.279 s sans / 1.269 s avec.

Les nombres d'images varient avec la cadence. Le rapprochement automatique
utilise les intervalles de pourcentage avec une marge de 0.15 point ; cela
n'est pas une comparaison pixel a pixel. Le detecteur vise la quasi-obscurite,
pas toutes les textures manquantes, flashs partiels ou couleurs incorrectes.
Cette serie ne prouve pas l'absence universelle d'ecrans noirs ni que les
episodes natifs sont tous voulus par les auteurs des niveaux.

## Hypotheses et limites

1. Ancienne geometrie GPU a cause d'un envoi supprime : ce chemin est retire
   de la DLL normale, verifie par compilation et identites du paquet.
2. Episode sombre deja present sans Chlorium : reproduit aux passages listes,
   avec les memes mods et reglages. Ces episodes ne sont pas propres a alpha.6.
3. Interaction intermittente, autre passage ou defaut partiel : non exclu par
   ces quatre parcours. Aucune nouvelle correction speculative n'est ajoutee.

Le cache mathematique restant est installe, mais les appels cibles et les
reutilisations sont nuls dans ces parcours. Son compteur d'echecs a zero ne
constitue donc pas une verification de reutilisations en jeu. Il ne peut pas
non plus expliquer un gain dans ces passages.

## Regularite des images

| Niveau | FPS diagnostic sans / avec | p99 sans / avec (ms) | Pire image sans / avec (ms) |
|---|---:|---:|---:|
| Grief 0-40 % | 76.40 / 77.74 | 25.73 / 25.80 | 41.21 / 58.98 |
| Heliopolis entier | 59.01 / 58.45 | 31.97 / 28.63 | 197.12 / 189.44 |

La surveillance visuelle a un cout, et les captures, pauses, demarrage et
pertes de focus sont exclus. Une paire unique ne prouve pas un gain FPS en
jeu normal. Les 60 FPS stables ne sont pas atteints et aucune amelioration
generale de performances n'est revendiquee.

## Livraison et preuves

L'alpha.6 testee est installee ; les reglages du mod sont conserves et le
paquet alpha.5 d'origine est sauvegarde. Les empreintes figurent dans
alpha6-package-provenance.json et alpha6-installation.json.

Le bilan numerique est dans alpha6-runtime-verdict.json. Les donnees brutes,
captures et empreintes des mods restent locales, hors Git. Rapport lisible :
Documents/chlorium/diagnostic-alpha6/DIAGNOSTIC.html, avec FPS par pourcentage,
episodes sombres et captures. Les nouveaux essais ne valident pas Orbit.
