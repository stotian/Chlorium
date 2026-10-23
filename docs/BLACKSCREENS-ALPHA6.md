# Alpha.6 : retirer les transferts graphiques du chemin optimise

## Modification

La compilation normale contient maintenant NativeTransformCache,
NativeAtlasStatsEmpty et BuildIdentity. Elle n'inclut plus NativeAtlasUploads.
Aucun de ses hooks drawNumberOfQuads, mapBuffers, initWithTexture, setQuads,
resizeCapacity, retour au premier plan ou destruction d'atlas n'est installe.
Le rendu statique experimental reste exclu.

Le moteur natif garde la responsabilite de la copie des quads, du marqueur
dirty et du contenu de ses VBO. Chlorium ne supprime plus un transfert sur la
base d'une copie CPU anterieure. La reutilisation mathematique exacte reste
presente ; collisions et declencheurs restent natifs.

## Pourquoi

Le cache precedent prouvait l'egalite entre les octets CPU actuels et sa copie
des donnees envoyees auparavant. Ses gardes de cycle de vie ne prouvent pas
que personne n'a remplace ou modifie le stockage GPU hors de ces chemins.
Le nom du VBO, sa taille, le contexte et le pointeur CPU peuvent rester egaux.
Cette limite etait deja mentionnee dans NATIVE-QUADS-DESIGN.md.

L'invalidation de mapBuffers avait corrige un cas reproduit dans l'alpha.3.
Elle ne constitue pas une preuve couvrant toute intervention externe sur GL.
L'alpha.6 retire cette hypothese de la candidate normale pour eviter qu'une
ancienne copie de geometrie puisse faire supprimer un transfert natif.

C'est une mitigation conservative. La cause exacte de tous les ecrans noirs
rapportes n'est pas etablie ; aucune disparition universelle du probleme ni
aucun gain de FPS n'est revendique sans nouvelle verification.

## Protection du packaging

- Une identite exportee exige le chemin de transferts GPU natifs.
- Le packager normal refuse l'identite de l'ancienne implementation d'atlas.
- L'option CHLORIUM_NATIVE_ATLAS_CACHE est reservee aux audits et ne peut pas
  etre distribuee comme un paquet normal.
- Les anciens paquets et les experiences restent distincts de l'alpha.6.

## Traces disponibles avant l'arret des tests

Avec les mods personnels, Grief a atteint 40 % sans mort dans les deux modes,
avec 32 modules charges sans Chlorium et 33 avec. Meme module de mesure,
memes empreintes de paquets personnels et memes reglages verifies. Les marques
de LDM et de secousses dans PlayLayer correspondent a la consigne.

Heliopolis avec les mods et Chlorium a atteint la fin sans mort. Sa reference
avec ces memes mods et sans Chlorium n'a pas ete lancee apres l'arret demande.
Il ne faut pas lui substituer la reference isolee comme si les profils etaient
identiques. Les donnees des episodes sombres et les captures sont conservees
localement dans Documents/chlorium/diagnostic-mes-mods.

Quelques episodes autour de 8.5-9.94 % de Grief ont une couverture differente
entre les deux parcours avec mods. Ces episodes existent aussi dans la reference
native isolee. Les longues pauses et les differences de cadence limitent une
attribution causale a Chlorium ; ils ne sont pas declares corriges.

## Verification apres reprise autorisee

Apres la demande « commence », quatre parcours ont ete effectues avec les
mods personnels : Grief 0-40 % et Heliopolis entier, chacun sans et avec alpha.6.
Meme executable, donnees de niveau, module de mesure, paquets personnels et
reglages verifies pour chaque paire. Aucune mort ni crash.

Grief : 15 episodes sombres dans chaque mode, tous rapproches aux memes
passages ; 80 images sombres sans et 91 avec, avec des cadences differentes.
Heliopolis : un episode dans chaque mode a 99.075 %, 61 images sans et 60 avec.
Aucun episode quasi noir supplementaire n'a ete detecte avec alpha.6 dans
cette serie. Cela ne prouve ni l'identite de chaque pixel, ni la correction
de tous les episodes signales ailleurs. Les captures de Grief vers 8.53 % ont
aussi ete inspectees : la quasi-obscurite est presente dans les deux modes.

Le cache mathematique est installe, mais ses appels cibles sont nuls dans ces
parcours ; il ne peut donc pas y expliquer un gain de FPS. Les 60 FPS stables
ne sont pas atteints. L'alpha.6 est installee avec sauvegarde de l'alpha.5.
Voir VALIDATION-ALPHA6-20261023.md et alpha6-runtime-verdict.json.
