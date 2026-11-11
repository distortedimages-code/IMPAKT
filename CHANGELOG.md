# Versions d'IMPAKT

## 0.88.0

- **RTUN** (rangée TRIG, page MORE, à côté de RDEC) : la hauteur de chaque coup tirée au hasard (RND) ou en boucle, la
  même suite à chaque tour du pattern (LOOP). De quelques cents près du milieu à une octave en butée ; sur une piste
  qui suit la gamme, les notes tombent dans la gamme. RVEL reste sur la page TRIG.
- **Les potards à la même taille** : une seule échelle sur toutes les pages. Les effets (FX et master) ont tous la même
  grande taille, la machine et la rangée TRIG la taille normale, TONE et MOD une taille plus petite, les réglages fins
  de WEST la plus petite.
- **Nettoyage** : les commutateurs ne recouvrent plus leur nom (BAND, TYPE, MODE…), les inscriptions des potards ne
  débordent plus sur leurs voisins, les quatre modules de TONE ont la même largeur, l'écran de MOD est plus grand, WEST
  répartit ses réglages sur toute la largeur.
- **Mises à jour** : le panneau s'ouvre au milieu d'IMPAKT, en rouge, et le bandeau aussi. Sur Mac, une fois l'image
  disque téléchargée, un bouton copie la commande du Terminal.

## 0.87.2

- **Installation sur Mac** : glissez chaque élément sur le raccourci de son dossier, puis collez une commande dans le
  Terminal (Cmd + Espace, « Terminal »). Elle lève le blocage de macOS sur les fichiers téléchargés, enlève les
  anciennes copies d'IMPAKT de votre dossier personnel et fait valider l'Audio Unit, sans passer par les Réglages
  Système. Elle est dans le LISEZ-MOI, désormais visible dans la fenêtre de l'image disque, et sur la page Installer.
  L'installeur reste là pour qui le préfère.

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
