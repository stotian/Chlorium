# Reprise — 21 octobre 2026

IDs autorises : 133260800, 86452082, 129598490, 59730461. Collisions normales,
aucun input, aucun noclip, arret a deux morts maximum. Tests en jeu isoles,
aucune compilation simultanee aux mesures.

Les trois nouveaux controles natifs sont complets, sans mort :

| ID | Nom telecharge | Objets | FPS moyens | p99 image | Pire image |
|---|---|---:|---:|---:|---:|
| 59730461 | Peaceful Auto | 224435 | 115,5 | 60,8 ms | 2273,2 ms |
| 86452082 | LIMBO auto | 394499 | 130,2 | 26,1 ms | 216,1 ms |
| 129598490 | heliopolis auto dec | 165779 | 78,2 | 23,0 ms | 325,8 ms |

Une moyenne haute ne valide pas la stabilite. Peaceful ralentit surtout vers
60–75 %, avec movement/rotation dominants ; son occupation memoire reste
autour de 0,8 Go. Les traces de processus couvrent aussi le chargement.

Prototype CPU : NativeObjectUpdates.cpp / NativeMovementLoop.hpp. Reproduit
les operations de moveObjects et leur ordre, precharge les donnees des objets
suivants, evite seulement le callback de section dont le corps natif revient
immediatement pour un index X negatif. Garde le callback final des objets
desactives, les positions precedentes, tous les drapeaux natifs, le compteur
32 bits et l'arret au premier pointeur nul. Ni regroupement de ticks ni omission
d'objets. Garde-fous : empreintes des fonctions installees, offsets compiles,
PlayLayer actif, vtable native et absence de hooks concurrents connus.

Les fixtures CPU passent. Le premier parcours prototype de Peaceful atteint
100 %, sans mort, avec 108 lectures graphiques reparties sur le niveau et
zero divergence. Aucun depassement d'indices 16 bits observe (12830 quads max).
Ces donnees de verification excluent les images avec readback et ne prouvent
pas encore un gain FPS. Les deux prefetchs supplementaires 0x240/0x3c0 sont
ajoutes ensuite dans les sources et restent a mesurer.

Les tests prototype de LIMBO/heliopolis n'ont pas demarre : le jeu principal
etait rouvert. Ne pas l'interrompre. Prochaine etape : refaire les trois
parcours prototype, puis mesurer sans CHLORIUM_VERIFY_NATIVE_UPLOADS, et
comparer par pourcentage aux controles. Conserver les donnees de verification
separees des mesures de performance.

Le prototype est compile uniquement si CHLORIUM_EXPERIMENTAL_NATIVE_MOVES=ON
avec audit actif ; il est force OFF dans le build normal et refuse par le
packager normal. Ne pas livrer ce prototype avant validation des trois niveaux
et d'Orbit. La version normale conserve uniquement le chemin natif et le
cache graphique deja teste ; aucun gain global n'est etabli.

Reprise technique : build-local.cmd --auto-audit, puis reconfigurer CMake
avec -DCHLORIUM_EXPERIMENTAL_NATIVE_MOVES=ON et build-local.cmd --incremental.
run-runtime-audit.ps1 -Level ID -BenchmarkSeconds 900. Ajouter -Native pour
un controle et CHLORIUM_VERIFY_NATIVE_UPLOADS=1 pour les lectures graphiques.
Le marqueur autorise est CHLORIUM_AUTO_COLLISIONS_ALLOWLIST_V3.

Donnees : audit-artifacts/multi-level (controles + Peaceful prototype), et
auto-ID-native.jsonl / .frames.csv / .gpu.csv / .process.csv. Le jeu ouvert
en dernier etait le jeu principal (PID 17168), pas une session de test.
