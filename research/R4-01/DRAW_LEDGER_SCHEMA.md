# R4-01 — oracle de soumission, état et événements

**Code et fixture préparés ; aucune exécution.** Ce contrat ne décrit pas encore
une trace capturée dans GD et ne valide ni les shaders ni un framebuffer réel.

## Question falsifiable

Une substitution peut-elle conserver la multiplicité, le painter order, les
quads complets, les identités de ressources et les événements observables ?
Le témoin doit être accepté et 12 mutations privées doivent être rejetées.
Si un sabotage passe, l'oracle doit être retravaillé avant tout usage du moteur.

## Unité canonique

`frames[]` est ordonné. Chaque frame contient `scene_epoch`, `frame_id`,
`context_generation`, `draws[]`, `triggers[]`, `state_before`, `state_after`.
Un élément `draws` est une **primitive logique** native, pas un appel GL :
un appel instancié peut produire plusieurs entrées. Une implémentation doit
développer sa liste de primitives observées dans l'ordre réel de soumission.
Le découpage en appels physiques est une métrique séparée ; un dictionnaire,
set ou simple hash commutatif ne peut pas prouver l'ordre/multiplicité.

Chaque primitive conserve `native_draw_ordinal`, `primitive_ordinal`,
`resource_generation`, `texture_key`, `shader_key`, `quad_sha256`, `blend`,
`target`, `viewport`, `scissor`, `stencil`, `depth`.
`quad_sha256` couvre les **96 octets natifs complets**, avec leurs quatre XYZ,
RGBA et UV. Ce digest doit être calculé indépendamment sur la sortie candidate,
pas recopié d'une liste attendue avant le shader. Il ne remplace pas une preuve
des pixels, de la transformation MVP ou des interpolations.

`resource_generation`/`context_generation` sont des générations attribuées
par un registre, jamais déduites d'un nom GL ou pointeur réutilisable. La fixture
utilise des identités symboliques ; aucun registre GL réel n'est implémenté ici.

Un trigger conserve `sequence`, `event`, `target`, `value_bits` (chaîne hexadécimale
IEEE754 32/64 bits). L'ordre est strict ; des effets hors écran ne sont pas supprimés.
Ce contrat initial ne capture pas encore physique, checkpoint ou flags flottants :
ils doivent être ajoutés avant une revendication d'équivalence de gameplay.

Les sentinelles `state_before`/`state_after` couvrent au minimum programme,
VAO, draw/read FBO, texture et sampler, blend enable/facteurs/équations,
scissor, stencil, depth, ainsi que l'état du cache Cocos concerné. Les valeurs
sont comparées entre référence et candidate. Ce n'est pas une règle artificielle
« before == after » : le chemin natif peut laisser des effets de bord attendus.

## Séparation des preuves

- `ledger_oracle.py` vérifie le schéma puis compare sans réordonner, sans dédupliquer
  ni tolérer de valeurs numériques approximatives. Il refuse JSON dupliqué,
  NaN/Infinity, champs obligatoires manquants et capture déclarée incomplète.
- `fixture.json` est un exemple privé synthétique de deux primitives transparentes
  et deux événements liés. Les hashes sont des **valeurs de fixture**, pas des
  empreintes de captures réelles ni une mesure de performance.
- `fixture_mutations.py` prépare le témoin et les sabotages ; seul son futur
  lancement pourra établir s'ils sont détectés. Les mutations GL changent des
  enregistrements de sentinelle : elles ne simulent pas un vrai pilote GL.
- Une future fixture GPU devra aussi dessiner les couleurs sentinelles avant/après
  et vérifier leurs pixels sur le contexte cible. Une collecte native en shadow
  mode reste à instrumenter et à mesurer pour éviter son propre surcoût.

## Commandes préparées, non exécutées

Depuis la racine de ce worktree :

```text
python research/R4-01/fixture_mutations.py
python research/R4-01/ledger_oracle.py reference.json candidate.json
```

Le premier ne lance ni GD ni GPU et ne crée pas de variante de mod. Le second
consomme seulement des fichiers explicitement fournis. Leur exécution reste
différée conformément à la consigne actuelle « pas de test ».
