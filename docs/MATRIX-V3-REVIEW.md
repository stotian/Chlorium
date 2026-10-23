# Matrix.3 : revue du code et limites

Travail realise le 22 octobre 2026 sans tests. L'analyse complete celle de
matrix.2 ; les deux points de redirection et le calcul scalaire sont conserves.

## Changements motives par la lecture du code

- **Effacement inutile** : matrix.2 abandonnait toutes ses valeurs a chaque
  groupe. Les valeurs reutilisees sont des fonctions des angles, echelles et
  controles SSE. Leur conservation ne depend pas de l'objet ni de son groupe.
  Les tableaux gardent une taille fixe ; les translations ne sont jamais reprises
  d'un objet precedent.
- **Cout de recherche** : quatre multiplications entieres etaient effectuees
  pour indexer chaque matrice, meme si elle etait identique a la precedente.
  Un pointeur vers la derniere entree permet de verifier la cle directement.
  Il est annule lors d'une invalidation ; les tableaux ne sont jamais deplaces.
  RotationCache est non copiable pour ne pas transporter un pointeur interne
  vers un autre exemplaire.
- **Petits groupes non proteges** : le compteur de 256 essais repartait a zero
  a chaque groupe. La nouvelle politique garde sa fenetre entre groupes et
  attend jusqu'a 32 images natives avant une nouvelle tentative peu rentable.
  Les phases en repli passent avant les interrogations de controles flottants.
- **Reentrance** : apres un appel imbrique, revalider seulement au debut du
  prochain groupe. Le reste de la boucle externe utilise le chemin natif.
- **Diagnostic incomplet** : une API de statistiques versionnee, de taille
  explicite, distingue absence du candidat, redirections installees et travail
  effectivement reutilise. Les deux anciennes API gardent leur disposition.

## Invariants relus

1. Une entree n'est utilisee que si ses valeurs, controles et generation
   correspondent ; les signed zeros ne sont pas confondus dans les cles.
2. Une case remplacee ne rend pas le pointeur recent invalide, mais sa cle est
   comparee a nouveau avant toute reutilisation.
3. Les generations annulent aussi les pointeurs rapides. Leur debordement
   efface les anciens tags avant de reutiliser la generation un.
4. Le delai de repli est borne et utilise une difference modulaire de compteur
   d'images ; changement de contexte ou retour en arriere reinitialise la politique.
5. Aucune simulation n'est sautee pendant le repli. Le pont appelle toujours
   la cible virtuelle actuelle sur les cas non pris en charge.
6. Les caches ne peuvent grandir avec le nombre d'objets, groupes ou images.

## Ce qui reste inconnu

Le taux de partage reel, le cout des branches de cache, la duree des sections
chargees, la fidelite numerique et visuelle, l'activation du candidat dans Geode
et ses interactions avec les autres mods n'ont pas ete testes. Les limites
sur les drapeaux flottants/errno de matrix.2 restent applicables. La conservation
entre images et le repli adaptatif ajoutent des transitions a verifier.

Priorites apres autorisation : comparaison au calcul natif, invalidations et
limites de compteur, puis comparaison avec/sans sur les sections autorisees.
Un gain de FPS ou une absence de bug ne peut pas etre deduit de la compilation.

Verdict : prototype compile separement, non valide pour remplacer l'alpha.5.

## Verification autorisee du 23 octobre 2026

853560 comparaisons numeriques et d'etat contre le CCNode natif passent.
244447 cas ont des indicateurs d'exception flottante differents : cette limite
reste explicite. Les bornes de repli et l'allocation du pointeur proche passent.

Le premier lancement Geode a revele une version de paquet invalide : Geode
n'accepte pas le suffixe alpha.6-matrix.3. Le paquet utilise maintenant
v0.2.0-alpha.603, avec le nom de l'experience dans le champ name. Le script
de packaging refuse les suffixes non pris en charge pour eviter la recurrence.

Grief auto 22-40 % : les deux sites sont installes, les phases sont actives,
mais aucun appel ne passe par les sites cibles sur cette section. Il n'y a
donc aucun gain possible de ce cache sur ce passage. Moyenne sans/avec :
70.33/66.67 FPS ; p99 25.24/25.46 ms ; pire image 51.67/440.02 ms. Une seule
paire ne permet pas d'attribuer le pic au candidat. Aucun remplacement du
paquet principal sur la base de cette mesure. Donnees :
audit-artifacts/matrix-v3-20261023-grief40/comparaison.json.

Ces mesures precedent la demande de desactiver les secousses. Elles ne doivent
pas etre melangees aux nouveaux essais full detail / no shake.
