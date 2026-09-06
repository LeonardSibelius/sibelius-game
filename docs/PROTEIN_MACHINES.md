# Protein Machines Inc.

L_City now includes a walk-in enhancement office beside the deli plaza, initially
placed at (2000, -400, 10), yaw -90. The open entrance faces the deli, and the
footprint clears the existing plaza characters. It is the `Protein Machines Inc.` actor and can
be moved as one unit. No new map or engine plugin is required.

## Player sequence

1. Buy/eat the burger and drink the coffee in L_Cafe using E on each item.
2. Return to the city and enter Protein Machines Inc.
3. Inspect the rotating molecular exhibit and press E to accept the enhancement.
4. Follow the existing spaceport generation, uFoods supplies, boarding and launch sequence.

The office is free to use. `City.Burger`, `City.Coffee`, and
`City.ProteinEnhanced` use the existing persistent progression grant registry.
Both food milestones are required in either purchase order. Repeated enhancement
is harmless. Food still restocks on a later cafe visit. Older saves have no meal
record, so those players must buy the two items once under the new version.

Boarding and its nearby hint require the enhancement. Already launched ships and
the post-launch portal keep their existing behavior. The HUD and Nyra direct the
player toward the office before the normal city instructions resume. After both
meal purchases, Nyra plays `dancer_protein_nyra`, the ElevenLabs recording about
space-travel enhancement and the blue dancers getting too much. Before both
purchases she repeats the original deli invitation. After enhancement she resumes
the existing guide recording for the current city stage. Voice and optional face
performance use the same asset base, so the old spaceport face clip cannot play
over the new invitation. No baked facial performance is included for this take;
the existing audio-driven mouth motion remains available.

Source audio: `Tools/Audio/dancer_protein_nyra.mp3`. Import just this take with
`Tools/Scripts/import_protein_guide_voice.py` in an editor-closed Python commandlet
using `-AllowCommandletAudio`. `/Game/Audio/Dancers` is already included for cooking.

## Display asset

The included glowing folded chain is a stylized placeholder, not a scientific
protein model. No Fab asset was acquired or imported. The suggested listing was verified in the browser as Paul Bourke's Plant Seed
Protein (Abyssinian cabbage), supplied in OBJ format:
https://www.fab.com/listings/a81221e5-f98a-4d27-a443-0e807e154715

To replace it, assign an imported, licensed static mesh to the office actor's
`ProteinMesh` component and adjust that component's relative scale. Its parent
`DisplayPivot` rotates it. At BeginPlay the bead model is hidden when a replacement
is assigned. Keep the original mesh's materials; the placement script preserves
the replacement component. Materials in `/Game/ProteinMachines` are referenced
directly by L_City for cooking.

## Authoring and checks

`Tools/Scripts/place_protein_office.py` authors the office and meal markers in an
editor-closed Python commandlet. Re-running preserves the office transform and
does not edit vendor content. Pre-change maps are backed up under
`Saved/ProteinOfficeBackup`.

The Development Editor build and ProgressionSmokeTest pass. The smoke test covers
each missing meal item, purchase order, persistence of all three grants, and
duplicate enhancement claims. This does not establish a packaged playthrough.

Manual playtest: eat both items, enter the office, inspect the rotating display,
press E, reload, and verify that the enhancement persists and boarding still
requires uFoods supplies. Try the office and spaceport before enhancement to
check the directions. Existing post-launch saves should retain portal access.
