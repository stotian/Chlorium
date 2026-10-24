# Versions de Chlorium

## Convention

Les versions normales utilisent `v0.2.0-alpha.N`. Une alpha est une version de
developpement : elle ne garantit ni 60 FPS stables, ni l'absence de bug.
Le manifeste Geode n'accepte qu'un suffixe de prerelease reconnu et numerique.

| Version | Nature | Etat |
|---|---|---|
| `v0.2.0-alpha.6` | Candidate normale | Transferts GPU natifs ; Grief 0-40 % et Heliopolis entier compares avec mods, aucun episode sombre supplementaire detecte |
| `v0.2.0-alpha.5` | Reference mesuree | Ancien cache borne pour scenes denses ; prerelease conservee |
| `v0.2.0-alpha.4` | Historique local | Reutilisation exacte des calculs `atan2f` natifs |
| `v0.2.0-alpha.3` | Historique local | Invalidation lors de la recreation du stockage GPU, correction reproduite |
| `v0.2.0-alpha.2` | Historique local | Retour au pipeline natif et cache de transferts |
| `v0.2.0-alpha.1` | Historique local | Premiere adaptation Chlorium |
| `v0.2.0-alpha.601` | Experience streaming | Transferts GPU ; non combinee a la candidate normale |
| `v0.2.0-alpha.602` | Experience Area v2 | Reutilisation de divisions natives ; comparaisons numeriques passees |
| `v0.2.0-alpha.603` | Experience matrices v3 | Appels actifs sur Orbit ; deux morts vers 89.88 %, cible 96-100 % non atteinte ; aucun appel eligible sur Grief 22-40 % |
| `v0.2.0-alpha.604` | Experience affine v2 | Comparaisons numeriques passees ; aucun gain FPS convaincant sur trois essais Grief |
| `v0.2.0-alpha.605` | Experience de decomposition Area | V3b verifiee numeriquement dans Orbit ; pics pires dans une paire de performances |
| `v0.2.0-alpha.606` | Pipeline mathematique combine | Deux composants actifs ; regression mesuree dans Orbit 83-88.5 % |
| `v0.2.0-alpha.607` | Liberation de caches heap | Essai de chargement Orbit : 13.316 Mio rendus en 1.8037 ms ; gain FPS non etabli |

Les numeros 601-607 identifient des experiences, pas une progression validee
de performances. Ne pas remplacer automatiquement la candidate normale par une experience.
Un seul paquet partageant l'identifiant `local.chlorium` peut etre charge.
L'alpha.6 a remplace l'installation locale apres ces essais ; l'alpha.5 est sauvegardee.

Les versions alpha.1 a alpha.4 documentent les etapes du travail local. Aucun
tag ni aucune archive de sources ancienne n'est invente pour ces versions.
La premiere prerelease du depot correspond a la candidate alpha.5.

## Tags et fichiers

Chaque version publiee associe un tag Git, des notes, le paquet `.geode` et
un fichier `SHA256SUMS.txt`. GitHub fournit egalement les sources du tag.
Les binaires du jeu et le module prive de mesure ne font pas partie du paquet
normal. Les experiences sont documentees dans `EXPERIMENTS-20261022.md`.

Voir [l'historique des changements](../changelog.md) pour les details et
[les mesures du 23 octobre](VALIDATION-20261023.md) pour les limites connues.

