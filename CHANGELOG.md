# Versions d'IMPAKT

## 0.87.1

- **Installeurs plus légers** : les sons de CHIMERA ne sont plus copiés dans chaque plugin. Un seul exemplaire sert au
  VST3, à l'AU et à l'application : `/Library/Application Support/IMPAKT` sur Mac, `C:\ProgramData\IMPAKT` sous
  Windows, `~/.local/share/IMPAKT` sous Linux.
  L'image disque Mac pèse beaucoup moins lourd.
- **Image disque Mac** : un quatrième raccourci, Application Support, pour y glisser le dossier des sons quand on
  installe à la main.

## 0.87.0

Première version publiée : IMPAKT s'installe sur Mac, Windows et Linux, et se met à jour tout seul.

- **47 machines**, dont RADIO (v87) : quatre récepteurs qui écoutent une station sur internet ou un récepteur ondes
  courtes partagé sur le réseau, joués en direct, mis en boucle ou fouillés dans leurs vingt dernières secondes.
- **Installeurs** : une image disque pour Mac (Apple Silicon et Intel) avec son installeur et des raccourcis vers les
  dossiers des plugins, un installeur pour Windows (VST3 et application), une archive pour Linux.
- **Mises à jour** : OPTION > Updates. Le plugin et l'application demandent une fois par session si une version plus
  récente existe, la téléchargent dans le dossier Téléchargements et l'ouvrent ; rien n'est remplacé tant que le
  logiciel de musique tourne.
- **Site** : le manuel complet, la procédure d'installation et la version navigateur, jouable sur téléphone.
- Le code d'IMPAKT est publié sous licence GNU GPL v3.
