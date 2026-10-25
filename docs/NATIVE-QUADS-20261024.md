# Nouveau moteur de lots natifs — 24 octobre 2026

> **Mise à jour de préflight V5, 25 octobre : ce texte conserve le compte rendu
> du prototype V2/alpha.608. Il ne décrit pas intégralement les sources actuelles.**
> Le retour utilisateur signale des erreurs de rendu et des noirs persistants.
> V2 est refusée à la promotion. Les sources suivantes conservent désormais les
> **96 octets natifs complets**, exécutent le draw natif jusqu'à la soumission
> d'atlas, et préparent des gardes de transparence/contexte/readback. Ces dernières
> corrections ont été compilées et la V4 a été essayée sur Grief 0–40 %.
> Le contrôle sans mod étant resté sans focus, aucun gain FPS apparié n'est
> démontré. La régression signalée interdit la promotion ; la restauration GL
> minimale est vérifiée hors jeu seulement. Le chiffre
> de 64 octets ci-dessous appartient uniquement au format V2 abandonné.
> Voir [RESEARCH_BASELINE.md](../RESEARCH_BASELINE.md) pour l'état vérifiable.

## Changement de structure

Experience `v0.2.0-alpha.608`, backend `CHLORIUM_NATIVE_QUAD_BACKEND_V2`.
La version normale installee reste alpha.6. Aucun gain FPS universel ni absence
universelle d'ecran noir n'est etabli.

Le premier pont reprenait encore la preparation statique de Bismuth :
273 000 sprites prepares dans Grief, controle des descendants incompatible
avec leur activation native, puis sprite natif additionnel non couvert.
Il repassait au rendu natif avant la premiere image. Cette implementation a
ete retiree des sources ; ses essais locaux ne prouvent aucun gain.

Le remplacement final intervient dans `CCSpriteBatchNode::draw`, apres le
parcours et le tri natifs. Il lit les descendants actuellement presents dans
chaque lot, y compris les sprites ordinaires ajoutes pendant la partie.
Il n'efface aucun objet du niveau et ne modifie ni collisions, ni simulation,
ni declencheurs, ni visibilite GD.

- Les matrices natives des noeuds fournissent positions, rotations, Scale,
  Area, animations et transformations des parents.
- Les couleurs quantifiees des quatre coins et les UV proviennent des quads
  natifs. Aucune nouvelle approximation HSV ou opacite n'est introduite.
- Un enregistrement GPU de **64 octets** remplace le quad natif de 96 octets.
  C'est une comparaison de formats, pas un pourcentage de gain FPS.
- Un seul dessin instancie par lot genere ses quatre sommets dans le shader.
  Aucun tampon de sommets, d'indices, de commandes indirectes ou compute
  n'est necessaire a ce backend.
- Le stockage GPU appartient entierement a Chlorium. Il est initialise en
  entier a chaque allocation, puis mis a jour par blocs modifies seulement.
  Cela ne reutilise pas le cache d'atlas natifs retire de l'alpha.5.
- L'ordre natif, la texture, la matrice de projection et les facteurs de
  melange du lot sont conserves. Les etats OpenGL touches sont restaures.

Le travail CPU des declencheurs Area/Transform reste natif. Cette version
acheve une passerelle de rendu vers leurs resultats ; elle ne deplace pas
encore ces lourdes boucles de simulation sur le GPU.

## Verification regroupee

`test-dirty-state-uploads.cmd` : **1 200** plans de transferts verifies octet par
octet, tailles de records 64/80/96, changements denses/epars/desordonnes,
dernier bloc incomplet et regroupement d'un nombre excessif de transferts.

Controles GPU sur Intel UHD/OpenGL 4.6 : compilation des shaders, dessin sans
geometrie CPU, adressage de deux instances, changements d'UV, miroir et
melange SRC_ALPHA. Le fichier de compte rendu local conserve le nombre exact
de controles de chaque revision.

Grief AUTO 88952972, **22–26 %**, avec les mods personnels, sans LDM et sans
shake : moteur actif a 26 %, **2 318 images GPU**, **zero repli natif**,
**zero mort**. A la fin, 2 872 sprites actifs dans les lots couverts.
Les sondes 24.90 % et 25.20 % ont ete sauvegardees et inspectees.

Le diagnostic visuel a mesure **341 images**, mais la premiere lecture a
rencontre une erreur GL preexistante **1282**. La couverture reste donc
**incomplete** et l'essai est marque en echec pour tout verdict sur les ecrans
noirs. L'origine de cette erreur n'est pas attribuee. Aucune lecture invalide
n'a ete effacee pour faire passer le test.

Paquet effectivement joue pendant cet essai :
`C0BAB7A1B2179C4023CB3907D5B11B64E5D8EB45413CB5F7D14B0C67F1FA63ED`.
Le nettoyage suivant retire le pont abandonne et deux reglages inactifs ;
la comparaison de performances utilise un nouveau paquet identifie dans son
propre fichier de provenance. Les mesures visuelles sont exclues des FPS.

## Limites et prochaine cible

Les shaders speciaux gardent le rendu natif. Les lots contenant des quads
manuels sans descendants correspondants, une hierarchie inattendue ou une
profondeur 3D passent egalement au rendu natif avec diagnostic. Les mods qui
dependent des effets secondaires de `CCSprite::updateTransform`, d'une
geometrie deformee directement ou d'UV non rectangulaires restent a auditer.
Le chemin GPU peut changer l'arrondi de calculs flottants : l'identite des
pixels n'est pas revendiquee sans comparaison.

Les petits tests de demarrage ne valident pas les sections lourdes. Orbit en
fin de parcours et Heliopolis restent a mesurer avec ce nouveau backend.
La pression memoire et les boucles natives Area/Transform ne sont pas
resolues par le seul remplacement du dessin.

Reference du fonctionnement des lots Cocos :
[CCTextureAtlas, documentation Cocos 2.2](https://docs.cocos2d-x.org/api-ref/cplusplus/V2.2/df/d3a/classcocos2d_1_1_c_c_texture_atlas.html).
Les structures et interfaces effectivement utilisees sont aussi verifiees
dans les en-tetes du SDK Geode local pour GD 2.2081.
