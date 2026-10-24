# Revue du diagnostic visuel V2

Revue locale du code et de ses preuves, limitee aux outils prives de mesure.

[CORRECTNESS] HIGH — corrige
tests/FrameWatch.hpp:63 : une lecture echouee pouvait laisser analyser des zeros.
Correction : verifier les operations GL et rejeter les echantillons invalides.

[COMPLETENESS] HIGH — corrige
tests/VisualSignature.hpp:15 : la ligne centrale seule pouvait masquer un noir
partiel ou une scene presque noire avec une ligne lumineuse.
Correction : echantillonnage spatial et confirmation du quasi-noir dans la zone.

[CORRECTNESS] HIGH — corrige
tests/FrameWatch.hpp:118 : les dernieres lectures pouvaient manquer silencieusement.
Correction : drainage borne, compteurs et invalidation d'une couverture incomplete.

[CORRECTNESS] MEDIUM — corrige
tools/compare-visual.py:93 : imposer une correspondance unique transformait une
fragmentation de clignotement en fausse absence de reference native.
Correction : references partagees explicites et conservation des durees et bornes.

[COMPLETENESS] HIGH — corrige dans le protocole
tools/run-runtime-audit.ps1:141 : des hitboxes actives recouvraient la scene.
Correction : neutralisation temporaire des repères et restauration verifiee.

[MAINTAINABILITY] MEDIUM — corrige
tools/diagnostic-report.py:179 : plusieurs parcours pour un meme mode pouvaient
se remplacer silencieusement dans un rapport.
Correction : refuser l'ambiguite et demander une paire explicite dans l'outil.

Score dans ce perimetre : correction 4/5, securite 4/5, performance 3/5,
maintenabilite 4/5, couverture 4/5. Ces scores sont un jugement de revue,
pas des mesures des performances du jeu. Le diagnostic copie encore l'image
GPU et a un cout ; il n'est pas distribue dans le mod normal.

Verdict : utilisable comme diagnostic prive avec les limites documentees.
Priorites traitees : lectures invalides, couverture spatiale et attribution
aux references. Restent hors preuve : fidelite pixel a pixel, tous les niveaux,
zones hors echantillonnage et regularite a 60 FPS du mod normal.
