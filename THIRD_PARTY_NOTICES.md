# Composants tiers

Les dependances sont recuperees par le systeme de compilation ou fournies
par l'installation de developpement. Leurs sources et leurs binaires ne sont
pas copies dans ce depot. Les notices pertinentes sont conservees dans
`LICENSES/` ; celles des dependances transitives restent applicables.

| Composant | Revision utilisee | Conditions / notice |
|---|---|---|
| Bismuth | `76fd0ec0b1ceedd1fc618bfcc62f98dc937ab6fa` | Notice amont : GPLv3 ou ulterieure et restriction additionnelle ; `LICENSES/Bismuth-LICENSE.md` |
| Geode SDK | `v5.11.2` | Boost Software License 1.0 ; `LICENSES/Geode-BSL-1.0.txt` |
| GLM | `0af55ccecd98d4e5a8d1fad7de25ba429d60e863` | Choix MIT disponible dans la notice amont ; `LICENSES/GLM-copying.txt` |
| glslang | `vulkan-sdk-1.4.335` | Notices multiples BSD/MIT et autres parties decrites par l'amont ; `LICENSES/glslang-LICENSE.txt` |

Sources de reference :

- <https://github.com/geode-sdk/geode/tree/v5.11.2>
- <https://github.com/g-truc/glm/tree/0af55ccecd98d4e5a8d1fad7de25ba429d60e863>
- <https://github.com/KhronosGroup/glslang/tree/vulkan-sdk-1.4.335>

Geometry Dash et ses ressources appartiennent a leurs ayants droit. Ils sont
necessaires a certains tests locaux et ne sont jamais inclus dans le depot,
les archives de sources ou les fichiers d'une version.

