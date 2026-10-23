# Verification du 23 octobre 2026

Installation isolee : Windows x64, GD 2.2081, Geode 5.11.2, Intel UHD.
Paquet normal mesure : alpha.5, SHA256
`EBEDE54A1456CECC38E11B51A401A5D36B08134AB49A2386F2432806D8B3AF1E`.
Le reconditionnement avec les notices de licence ne change pas sa DLL.

## Comparaisons visuelles

Les quatre parcours verifient dans PlayLayer : LDM desactive, secousses
desactivees. Compteur FPS et libelles AVEC/SANS Chlorium affiches. Collisions
natives, aucun noclip ni input artificiel. Meme executable, module de mesure
et donnees du niveau pour chaque paire.

| Niveau | Couverture sans / avec | Morts sans / avec | Images tres sombres sans / avec |
|---|---|---:|---:|
| Grief auto `88952972` | 0-40 % / 0-40 % | 0 / 0 | 95 / 93 |
| Heliopolis auto `129598490` | Niveau termine / niveau termine | 0 / 1 | 60 / 60 |

Chaque episode sombre detecte avec Chlorium correspond a un episode observe
sans le mod a proximite du meme pourcentage. Sur Heliopolis, le seul episode
commence vers 99.07 %. Cela ne prouve pas que tous les ecrans noirs rapportes
sont corriges : aucune anomalie sombre supplementaire n'a ete reproduite sur
ces parcours. L'installation isolee ne couvre pas tous les autres mods du jeu
principal. Le detecteur vise les images quasi noires et ne valide pas chaque
texture ou chaque pixel.

Les FPS avec surveillance visuelle incluent son cout. Grief sans/avec :
76.44/78.32 FPS, p99 25.25/25.56 ms. Heliopolis : 53.86/58.51 FPS,
p99 39.16/30.18 ms. Ces paires uniques, avec conditions du PC variables et une
mort supplementaire sur Heliopolis, ne prouvent pas un gain en jeu normal.
Les 60 FPS stables ne sont pas atteints sur l'ensemble des passages.

## Matrices experimentales

853560 comparaisons numeriques/d'etat contre CCNode natif passent ; les
indicateurs d'exception flottante different dans 244447 cas. Bornes de repli,
debordement du compteur et allocation du pointeur proche verifies.

Grief 22-40 % avec matrices v3 : deux sites installes mais aucun appel cible.
Ce prototype ne peut pas accelerer ce passage et n'a pas ete installe comme
remplacement de la candidate normale. Cette paire precede la consigne sans
secousses et ne doit pas etre melangee aux quatre parcours ci-dessus.

Area : 804840 comparaisons bit a bit et d'indicateurs flottants passent.
Les temps des microbenchmarks sont variables selon les donnees ; ils ne
constituent pas une preuve de gain FPS general.

## Corrections des outils

- Formats de version des experiences acceptes par Geode, avec garde au packaging.
- Qualite complete et absence de secousses appliquees et verifiees en jeu.
- Un arret a 40 % est distingue d'une fin de niveau.
- Une fin atteinte avec une mort reste distinguee d'un parcours sans mort.
- Les episodes sombres ne sont rapproches qu'entre configurations verifiees.
- Les rapports de plusieurs niveaux peuvent etre regroupes sans nouvelle partie.

Les traces brutes, captures et donnees de niveaux restent hors du depot. Le
rapport detaille local est conserve dans `Documents/chlorium/diagnostic/`.

