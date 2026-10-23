# Revue statique de matrix.2 — 22 octobre 2026

Portee : code source, instructions des binaires installes et compilation.
Aucun lancement du jeu, test unitaire ou microbenchmark pour cette revision.

## Problemes traites

1. **Performance / elevee** : matrix.1 detournait tous les appels CCNode,
   y compris hors des boucles visees. Remplacement par deux redirections locales
   de six octets, avec repli sur la cible virtuelle reelle. Les autres appels
   ne traversent plus le code du candidat.
2. **Performance / elevee** : un changement d'echelle invalidait toute possibilite
   de reutiliser la trigonometrie. Table separee de rotations, indexee par les
   bits exacts de l'angle et les controles SSE. Aucune quantification des angles.
3. **Couverture / moyenne** : la boucle `transformAreaObjects` a le meme cout
   de reconstruction, mais matrix.1 ne la couvrait pas. Ajout d'une portee
   specifique autour de cette boucle ; aucun changement de la liste d'objets.
4. **Correction / elevee** : une fonction C++ libre retournant une structure
   n'aurait pas le meme ordre d'arguments que cette methode MSVC. Le pont utilise
   explicitement RCX=noeud, RDX=buffer, RAX=buffer retourne, comme le code natif.
5. **Correction / moyenne** : la premiere garde d'empreinte de
   CCAffineTransformMake ne couvrait pas les trois derniers octets. Etendue
   aux 0x33 octets jusqu'au RET inclus.
6. **Correction / moyenne** : les contextes imbriques pourraient emprunter le
   cache externe. Suspension puis invalidation au retour ; pas de resultat
   conserve au travers d'un appel imbrique.
7. **Livraison / moyenne** : un ancien DLL matrix.1 ne doit pas recevoir le nom
   matrix.2. Marqueur de revision distinct exige par le script de packaging.

## Correspondance avec le code natif

GD 2.2081, timestamp 0x69702da0, image 0xa3a000 ; Cocos timestamp 0x69702d99,
image 0x1c5000. Ces gardes limitent explicitement la compatibilite.

- CCNode::nodeToParentTransform : Cocos+0x37f0, longueur 0x468.
- CCAffineTransformMake : Cocos+0x6560, longueur 0x33.
- processTransformActions : GD+0x22c680, longueur 0xf30.
- transformAreaObjects : GD+0x229720, longueur 0x662.
- Deux appels `FF 90 08 03 00 00`, slot virtuel 0x308 : remplacement par
  `FF 15 <rel32>` vers une page contenant l'adresse du pont C++.
- Constante degres/radians 0x3c8efa35 ; negation par bit de signe ; appels natifs
  cosf puis sinf. Rotations nulles et rotations X/Y egales suivent les branches
  correspondantes du code d'origine.
- a=scaleX*cosY ; b=scaleX*sinY ; c=(-sinX)*scaleY ; d=scaleY*cosX.
  Chaque operation reste une multiplication float scalaire explicite.
- tx/ty : copie de la position pour ce chemin sans ancre/skew ; m_fTransformX/Y
  recopies ; transformDirty et positionDirty effaces. Position seule : original.

## Limites de cette revue

- L'ABI a ete deduite des instructions natives ; le pont n'a pas ete execute.
- La fidelite numerique doit etre comparee aux matrices natives, y compris
  zeros signes, echelles negatives, changements SSE et reinitialisations.
- Les flags flottants/errno des appels supprimes ne sont pas integralement
  reproduits. Les hooks tiers connus sur les fonctions concernees inhibent
  le cache ; cette garde ne detecte pas tous les patches externes possibles.
- Le temps gagne depend du partage des rotations. Un faible taux de partage
  peut rendre le cache plus cher ; la politique de repli reste a mesurer.
- L'activation effective dans Geode, le cout des redirections et l'absence
  de defauts visuels ne sont pas etablis par la compilation.

## Evaluation provisoire

| Dimension | Score / 5 | Motif |
|---|---:|---|
| Correction | 3 | Chemin natif et ABI analyses, validation dynamique suspendue |
| Securite | 4 | Pas d'entree distante, gardes binaires, page de pointeurs non executable |
| Performance | 3 | Travail redondant supprime dans le code, gain non mesure |
| Maintenabilite | 3 | Piste bornee, mais dependance a des instructions/version exactes |
| Completude | 3 | Candidat compile separement, validation de fonctionnement encore absente |

Verdict : **candidat experimental ; pas de remplacement de l'alpha.5 installee**.
Les trois priorites a la reprise : equivalence des matrices et ABI, activation
effective sans erreurs visuelles, puis comparaison des frametimes dans une
section autorisee. Aucun resultat de test n'est revendique dans cette revue.
