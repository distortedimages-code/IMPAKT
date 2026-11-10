<picture>
  <source media="(prefers-color-scheme: dark)" srcset="brand/impakt-logo-dark.svg">
  <img src="brand/impakt-logo-light.svg" alt="IMPAKT, music synthesiser" width="520">
</picture>

Une boîte à rythme de synthèse : huit pistes, 47 machines, un séquenceur à p-locks, des patterns enchaînés, des effets
séquencés et une section master. En plugin VST3, en Audio Unit (Mac), en application autonome et dans le navigateur.

**[Télécharger](https://github.com/distortedimages-code/IMPAKT/releases/latest)** ·
**[Installer](https://distortedimages-code.github.io/IMPAKT/install.html)** ·
**[Manuel](https://distortedimages-code.github.io/IMPAKT/manuel.html)** ·
**[Jouer dans le navigateur](https://distortedimages-code.github.io/IMPAKT/app/)**

| Système | Fichier | Contenu |
| --- | --- | --- |
| macOS 11+ (Apple Silicon, Intel) | `IMPAKT-macOS.dmg` | installeur, VST3, Audio Unit, application, raccourcis vers les dossiers |
| Windows 10 / 11 (x64) | `IMPAKT-Windows-Setup.exe` | VST3 et application |
| Linux (x64) | `IMPAKT-Linux.tar.gz` | VST3 et application, `install.sh` |

Le plugin et l'application se mettent à jour d'eux-mêmes : **OPTION › Updates**.

## Dans ce dépôt

```
core/        le moteur audio, en C++ sans dépendance (le même pour le plugin et le navigateur)
plugin/      le plugin et l'application (JUCE 9) : CMakeLists.txt, Source/
web/         l'interface (HTML, CSS, JS), commune au navigateur et au plugin ; web/*.wasm, le moteur compilé
bank/        la banque d'usine de CHIMERA
brand/       le logo et l'icône
docs/        le manuel complet (manuel.md)
site/        le site (GitHub Pages) : build_site.py
packaging/   l'image disque Mac, l'installeur Windows, l'archive Linux
tools/       compilations, tests
VERSION      la version (le plugin, les installeurs, le site et les mises à jour la lisent)
CHANGELOG.md les nouveautés de chaque version
```

## Compiler

```sh
cmake -S plugin -B build -DCMAKE_BUILD_TYPE=Release      # JUCE 9.0.3 est téléchargé (ou -DJUCE_PATH=…)
cmake --build build --config Release
```

Sur Mac, `build_mac.command` fait tout d'un double-clic (Xcode requis). Les détails, Windows et Linux compris, sont
dans le [manuel](docs/manuel.md#compiler-le-plugin). L'interface se reconstruit avec `python3 tools/build_web.py`.

## Publier une version

1. Mettre la nouvelle version dans `VERSION` et ses nouveautés dans `CHANGELOG.md` (une section `## x.y.z`).
2. `git tag vx.y.z && git push origin main vx.y.z`

Le CI compile Mac (universel), Windows et Linux, fabrique les trois installeurs et publie la version sur GitHub ; le
site se met à jour, et les plugins installés la proposent au prochain lancement.

## Licence

IMPAKT est un logiciel libre, sous licence **GNU GPL v3** ([LICENSE](LICENSE)). Il est construit avec
[JUCE](https://juce.com), utilisé sous AGPLv3. Il reprend du code ouvert, chacun sous sa licence : Mutable Instruments
(MIT, `core/mi`), le ressort de BYOD de ChowDSP (GPL-3, `core/LICENSE-BYOD.txt`), EaseLFO (BSD,
`core/LICENSE-EaseLFO.txt`), minimp3 (CC0, `core/ext`). Les polices de l'interface sont sous SIL Open Font License. Le
détail est dans le [manuel](docs/manuel.md#licences).
