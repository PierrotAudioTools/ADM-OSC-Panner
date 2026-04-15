# ADM-OSC Panner — UI V2 (Proposition)

Version 100% séparée de la prod actuelle.
Aucun impact sur `Source/PluginEditor.*`.

## Direction visuelle
- Ambiance: console live premium, lisible en environnement sombre.
- Contraste élevé sur infos critiques (ID objet, statut OSC, format).
- Structure en cartes avec hiérarchie forte (OSC en haut, mapping au centre, gestuelle en bas).

## Design tokens
- `bg-0`: `#05070A`
- `bg-1`: `#0B1117`
- `panel-0`: `#0F1821`
- `panel-1`: `#13202C`
- `line-soft`: `#2A394A`
- `line-strong`: `#3B5066`
- `text-main`: `#EAF2FA`
- `text-sub`: `#8EA3B8`
- `accent-cyan`: `#39D8FF`
- `accent-orange`: `#FFB14A`
- `accent-mint`: `#34D8AF`
- `ok`: `#4DFFB1`
- `warn`: `#FFC56D`

## Typo
- Titre: 18-20 px, extra bold
- Labels: 11-12 px, medium
- Valeurs: 13-14 px, semibold
- Monospace recommandé pour `IP:PORT` et `OBJECT ID`

## Layout (600x750)
1. Header (52 px)
- Logo/titre gauche
- `OBJECT ID` badge au centre
- Statuts RX/TX à droite

2. OSC Control Card (170 px)
- Ligne 1: `OSC IN` toggle + port
- Ligne 2: `OSC OUT` toggle + IP + port
- Ligne 3: `FORMAT` segmented control (`CARTESIAN | POLAR`)
- Ligne 4: activité réseau (barre animée discrète)

3. Space Card (320 px)
- Gauche: XY pad principal (plus grand)
- Droite: XZ pad
- En bas de la carte: mini readouts `X Y Z` en temps réel

4. Motion/FX Card (170 px)
- `CIRCLE` on/off
- `RADIUS`
- `TIME` (1/1, 1/2, 1/4, 1/8)
- Lock visuel pour indiquer la priorité OSC si active

## Interactions
- Hover: liseré clair + lift de carte (`+2 px` visuel)
- Drag sur pads: halo directionnel + crosshair
- Feedback RX/TX: pulse doux, jamais agressif
- Focus champs texte: bord cyan net, fond inchangé

## Règles UX live
- Pas de popups
- Pas de transitions longues (>120 ms)
- Toujours afficher `OBJECT ID` et `FORMAT` en permanence
- Désactiver visuellement ce qui est inactif (opacity), sans reflow

## Implémentation proposée (étape suivante)
- Créer `Source/PluginEditorV2.h/.cpp`
- Garder `Source/PluginEditor.*` intact
- Ajouter une option CMake (`ADM_UI_V2=OFF`) pour basculer sans risque
