# Paquet Bismuth fourni le 25 octobre 2026

Référence locale : iliashdz.bismuth.geode, SHA256
`af319b8d2e45ff0689708f05cbad079fe0b87039af776d6aa1d95a3397158926`.
Archive CRC valide, 25 entrées. Manifeste : Bismuth v0.1.0-alpha,
GD Windows 2.2081, Geode 5.10.1, auteur IliasHDZ. Chlorium utilise le SDK 5.11.2.
Le paquet est fourni comme fonctionnel par l'utilisateur ; il n'a pas été chargé
ou installé lors de cette analyse statique et n'est pas redistribué dans le dépôt.

Ses shaders de dessin lisent les sommets et les indices de triangles visibles
sur GPU à partir de gl_VertexID. Cette représentation diffère du chemin à
attributs de l'ancien port Chlorium et du pont natif V5. Les buffers de sprites
et de groupes permettent d'éviter une partie du travail par objet ; c'est une
référence architecturale utile, pas une preuve de couverture intégrale GD 2.2.

Pistes à comparer après les noirs : sélection de triangles/tranches visibles,
représentation GPU des groupes/poses, réduction des mises à jour CPU redondantes.
Aucun binaire externe n'est importé dans Chlorium et aucun gain de ce paquet
n'est attribué à Chlorium sans essai apparié. Les licences/notices Bismuth du
projet restent en vigueur.
