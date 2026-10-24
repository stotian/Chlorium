# Relecture des sources Area V3 — 24 octobre 2026

Statut : code prepare et relu, sans compilation, test CPU ou lancement du jeu,
conformement a la consigne utilisateur. Cette note ne vaut pas validation.
Le paquet installe et la prerelease alpha.6 ne sont pas remplaces.

## Problemes traites dans les sources

| Constat | Modification | Limite restante |
| --- | --- | --- |
| L'interception historique traverse tous les appels a l'import atan2f avant de filtrer deux appelants | V3 redirige uniquement les quatre appels Transform/Area connus | Gain net non mesure ; les gardes de compatibilite ont un cout |
| Les compteurs sont mis a jour par objet, meme hors diagnostic | Deux specialisations, avec ou sans compteurs/verification par appel | Politique de reutilisation et compteurs de phases toujours actifs |
| Des compteurs desactives pourraient etre lus comme une optimisation inactive | API versionnee : revision, sites actifs, cache actif, compteurs, verification et propriete | Le module independant doit etre reconstruit pour lire cette API |
| Un repli natif pourrait etre presente comme une mesure V3 active | Le lanceur exige l'API et quatre sites actifs ; le bilan est invalide en cas de desactivation ou de perte de propriete | Cette nouvelle garde reste non executee |
| La desactivation Geode ecrit les octets d'origine sans comparer la memoire courante | Comparaison avant retrait ou reactivation, arret definitif du cache si changement de proprietaire | Une ecriture concurrente ou le retrait propre a Geode depasse ce contrat |
| Un pointeur CRT capture ne suit pas un hook ulterieur du relais natif du jeu | Les replis prennent le chemin du relais natif courant du jeu | Les gardes ne couvrent pas toute modification arbitraire du corps CRT |
| Un retrait partiel suivi d'une liberation du relais laisserait un appel vers de la memoire liberee | Le relais reste alloue tant que des patches Geode peuvent le referencer ; le cache reste inactif apres echec | Une page conservee jusqu'a la fin du processus dans ces cas |
| Le bilan CPU cible appelait setEnabled a chaque image, effacant ses propres moyennes | Separation entre debut de session et collecte d'une image ; accumulation selon l'eligibilite CSV avant sortie | Correction preparee, pas de nouvelle mesure disponible |
| Les comparaisons ne distinguaient pas tous les modes de verification/compteurs | Provenance explicite et refus des conditions mathematiques differentes | Deux traces anciennes restent soumises aux seules gardes qu'elles possedent |

## Relecture du chemin natif

Les empreintes de l'executable et des corps Transform/Area, les octets des
quatre appels et le relais d'import doivent correspondre. Les definitions de
sites sont communes au filtrage des appelants et au controle des instructions.
Un deplacement relatif hors de portee ou une allocation/protection impossible
refuse l'installation.

Le relais est un saut indirect sans modification de pile ou de registres ;
l'adresse de retour vue par le cache reste celle de l'appel E8 du jeu. La page
est ecrite puis protegee en lecture/execution avant publication. Une installation
partielle reste sur le calcul natif tant que les quatre patches ne sont pas
prets. La reactivation controle aussi l'import et les entrees natives.

Les caches Transform et Area restent distincts et invalides a chaque phase.
Aucun angle approche ni changement de groupes, d'ordre, de shader, de dessin
ou de collision n'est introduit. Le contrat numerique anterieur reste limite
aux resultats flottants exacts sous les controles pris en charge : les effets
errno/fenv d'appels elides ne sont pas garantis identiques. Une verification
numerique echouee coupe les deux flux de reutilisation.

## Ce qui reste a etablir quand les tests seront autorises

La compilation de V3 et du module de mesure, le comportement du relais/du
retrait avec les autres mods, la protection de pause, l'equivalence numerique,
les noirs et les temps d'image restent a verifier. Aucune correction visuelle,
absence de crash, amelioration FPS ou stabilite a 60 FPS n'est deduite de cette
relecture. Le precedent essai Area a crashe avant la premiere phase ; il reste
un essai incomplet, pas une preuve de fonctionnement.
