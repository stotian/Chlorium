# R4-06 — décision CPU uniquement

Garder la réduction de travail par dépendances inverses en régime épars.
Retravailler la sélection du régime dense, qui reste régressif dans la série
finale. Aucune intégration normale ou équivalence native GD recommandée.

Hypothèse, chronologie des échecs, architecture, chiffres et limites sont dans
le [journal unique](../../docs/EXPERIMENTS-V5.md). Les sorties et commandes
exactes sont liées dans `evidence.json`. Les tests runtime/GPU de ce noyau
sont `not_run`, indépendamment des 42 147 assertions CPU réussies.

Rollback : prototype non lié au mod, donc aucun effet à retirer du jeu.
Prochaines preuves : événements/membership natifs et poids réel du goulet ;
shadow mode avant toute suppression d'un calcul ou dessin natif.
