# R4-01 — contrat préparé, exécution différée

Mise à jour après autorisation des tests techniques : commande exécutée,
témoin accepté, 12/12 mutations rejetées, code retour 0. Le corps ci-dessous
conserve le préflight antérieur ; l'état courant figure dans `evidence.json`
et le journal unique `docs/EXPERIMENTS-V5.md`. Aucun test GL/runtime de cet oracle.

Hypothèse : l'oracle de ledger détecte perte/duplication/ordre de primitives,
quad différent, génération obsolète, contexte, blending/shader, événement perdu
ou réordonné, et contamination d'état GL/cache Cocos.

Préparation : contrat, comparateur offline, fixture symbolique et 12 mutations
privées. Aucun hook GD nouveau ; aucun dessin natif supprimé. Le témoin est
une copie indépendante de la fixture, pas une baseline mesurée dans le jeu.

Commande préparée : `python research/R4-01/fixture_mutations.py`.
Statut : **not_run** à la demande de l'utilisateur, aucun stdout de test.
Build/GPU/jeu/A-B : **not_run**. G1 reste ouvert.

L'oracle compare des enregistrements déclarés. Il ne prouve pas que le producteur
les a réellement soumis ni que le shader produit les pixels attendus. Il faut
une capture indépendante, une fixture de dessin sentinelle GL, puis des preuves
d'événements/physique/checkpoint avant de valider un remplacement.

Décision : **RETRAVAILLER / validation différée**. Aucun gain mesuré, aucun paquet.
Rollback : ces seuls fichiers offline peuvent rester inutilisés sans effet sur GD.
Prochain test discriminant : accepter le témoin et rejeter les 12 sabotages ;
puis compléter les cas lifecycle/GL réel, sans confondre comparateur JSON et GPU.
