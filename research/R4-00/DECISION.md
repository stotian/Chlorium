# R4-00 — état réel et permissions

Hypothèse falsifiable : les sources locales, le cache de build et le paquet
installé peuvent désigner trois états différents. Inspecter les trois avant
d'attribuer une mesure au nouveau moteur.

Résultat de l'inventaire : HEAD conforme au mandat, workspace sale, cache natif
quads ON, paquet installé alpha.6 identique à dist. L'hypothèse d'un seul état
homogène est réfutée par cet inventaire ; aucune conclusion de FPS n'en découle.

Commandes effectuées : `git rev-parse HEAD`, `git status --short`,
`git branch --show-current`, lecture de mod.json/CMakeCache/VERSION, inventaire
WMI/processus, lecture ZIP du manifeste installé, `Get-FileHash -Algorithm SHA256`.
Les sorties locales sont conservées dans `../mandate-v5/` : environnement,
cache préexistant et manifeste SHA-256 des sources copiées. Ces fichiers
contiennent des chemins locaux et restent ignorés de Git.

Isolation : branche `research/v5-g0`, base 2312559. Les modifications
antérieures sont conservées, sans promotion ni exécution. Le premier essai de
copie avait refusé d'écraser les fichiers du checkout neuf ; la copie a été
refaite avec écrasement explicite de ce seul nouveau worktree et comparaison
des 36 empreintes. Aucun fichier de la copie principale n'a été supprimé.

Tests : build/CPU/GPU/runtime/A-B `not_run`, pas `passed`. Aucun événement GL
réel n'est enregistré. L'oracle de R4-01 est du code préparé, pas G1 validé.

Décision : **CONTINUER** audit et préparation offline. G0 documenté, G1–G5 ouverts.
Rollback : cesser d'utiliser ce worktree suffit à exclure la préparation V5 ;
le mod installé n'a pas changé. Ne pas effacer les sources non commitées.

Prochain résultat discriminant : le témoin de fixture R4-01 doit être accepté,
et chaque mutation doit être rejetée, après autorisation de tests. Puis choisir
la mesure qui distingue couverture/GL/ressource/harness avant toute attribution
des noirs. Aucune nouvelle autorisation intrusive n'est sollicitée pour cet audit.
