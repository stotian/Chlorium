# Revue locale de l'experience affine v2 — 24 octobre 2026

## Defauts rencontres et decisions

- Propagation de NaN differente dans le premier calcul groupe : reproduction
  contre la DLL installee, puis repli natif pour les huit entrees non finies.
- Initialisation native de la sortie et lectures successives en cas d'alias :
  helper general protege, huit cas de chevauchement verifies. L'appel cible
  utilise des plages de pile disjointes confirmees dans le corps natif valide.
- Exceptions non masquees : repli natif ; le chemin groupe ne promet pas
  l'ordre des traps. Arrondi et FTZ/DAZ ne sont pas modifies.
- Bypass potentiel d'un autre mod : import attendu et corps verifies avant
  installation, puis import et premiers octets de l'entree controles a chaque
  appel. Un autre proprietaire retrouve le chemin courant. Une modification
  tardive uniquement au milieu du corps reste hors detection de cette garde.
- Surcharge du diagnostic : premiere paire sans gain FPS, rotation plus lente ;
  retrait des compteurs/TLS du chemin mesure en v2. Les compteurs facultatifs
  ont une variante distincte selectionnee au chargement.
- Confusion de paquet : option isolee, marqueurs de type et revision, sortie
  experimentale separee, refus du packager normal. Le paquet installe alpha.6
  conserve son empreinte initiale.

## Verification et limite

Compilation candidate et module de mesure reussies. Comparaison directe a
la DLL installee : 192 000 resultats/flags exacts, huit chevauchements et trois
gardes de compatibilite. Trois essais Grief 22-28 %, aucune mort ni crash,
provenance appairee controlee. Aucun gain FPS convaincant ; pas de preuve
visuelle en jeu dans ces essais de performance sans surveillance d'image.

Jugement dans ce perimetre : correction 4/5, securite 4/5, maintenance 4/5,
performance 2/5, couverture 3/5. Ce sont des notes de revue, pas une mesure de
gain. Verdict : experience conservee, refus de promotion en version normale.
