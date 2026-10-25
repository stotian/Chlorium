# R4-08 — Area par lots et barrières causales

**Verdict : RETRAVAILLER, aucune promotion.**

Le prototype ne remplace aucune fonction du jeu. Le noyau évalue quatre
distances indépendantes pour un même effet avec les divisions et arrondis
natifs. L'oracle exécute en mémoire privée la routine exacte de GD 2.2081
(corps et constante vérifiés), sans charger ni lancer le jeu. Aucun octet
de l'exécutable propriétaire n'est inclus dans les sources du dépôt.

Le domaine rapide est explicite : exceptions SSE masquées, longueur non nulle,
zone entre -1 inclus et 1 exclu, distances finies. Les autres entrées restent
natives. Aucun réciproque approché, FMA, arrondi de coordonnées ou cache
persistant de résultats. Les snapshots sont disjoints et de même taille.

Le second prototype compile des vagues contiguës de commandes indépendantes.
Une écriture suivie d'une lecture dépendante coupe la vague. Les callbacks,
alias ou effets non modélisés restent des barrières scalaires. L'ordre des
écritures est conservé ; génération périmée et changement de cardinalité sont
refusés avant exécution. Le producteur doit garantir l'invalidation sur
changement des commandes, recettes, groupes, objets et checkpoints.

Les tests du planificateur utilisent des champs explicites et des écritures
pures. **Ils ne prouvent pas que les callbacks de GD satisfont ce contrat.**
Le mode rapide ne doit pas être relié à des GameObject avant cette preuve.
La parallélisation ou le report de callbacks natifs n'est pas autorisé par
un simple drapeau du prototype.

## Preuves et limites

Commande : `research/R4-08/run-cpu.cmd`. Les deux mutations privées
`--mutant` et `--hazard-mutant` doivent échouer avec le code 1.
Les temps bruts du noyau sont dans `timings.csv` ; ce sont des microsecondes
CPU sur snapshots, pas des FPS. Le rassemblement des données natives,
les callbacks, la préparation GPU et les commits de GD restent hors mesure.
Les traps SSE réellement déclenchés et les checkpoints natifs ne sont pas testés.

La normalisation seule ne peut pas résoudre un goulot dominé par les matrices
et les visites répétées d'objets. Les premiers relevés de quatre phases, puis
quatre phases visibles, échantillonnaient le début léger de 96–97 %.
Ils sont conservés comme diagnostics limités, sans les présenter comme les pics.
Le relevé par image ajoute les nombres de phases physiques, la somme des dt,
les visites et temps inclusifs par type. Les scopes imbriqués ne s'additionnent
pas ; les durées sont perturbées par l'instrumentation.

**Prochaine décision discriminante :** utiliser ces comptes pour choisir entre
factorisation des poses/groupes et évaluation vectorisée de séquences Area.
Un plafonnement des pas physiques modifierait le jeu : il ne constitue pas
une optimisation équivalente et n'est pas implémenté.
