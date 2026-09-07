# Sibelius Hotel epilogue

After Nyra's Grok speech and the closing cards, G reveals the Sibelius Hotel IN GROK. The player, sky and landscape stay in place. An invisible architecture-only tile preloads during the speech; G makes it visible and collidable. This is additive streaming, not travel. O still returns to the office. Repeated G cannot create duplicates. `Grok.HotelGenerated` persists the reveal and is cleared with the normal new-game grants.

Purchased artwork: **Elven architecture**, DmitriyDryzhak, https://www.fab.com/listings/1cb2369a-f53b-43fe-9c67-14790e3a4e15 . The hotel uses the supplied castle environment, not newly furnished guest rooms.

Install the pack at `/Game/ElvenArchitecture`, then run `Tools/Scripts/make_hotel_tile.py` through Unreal Python with PIE stopped and edited maps saved. It creates `/Game/Maps/L_SibeliusHotel_Generated`, containing only architecture, plants, local lights, a sign and a shallow approach. No vendor sky, camera, distant rocks or PlayerStart is included. The vendor scene stays unchanged. The old `make_sibelius_hotel.py` and preview map are historical standalone previews, no longer packaged or reached by G.

The entrance is 40 m forward of the Grok arrival (yaw 294.6), with its floor at world Z -1000. The 29 m approach drops 5.25 m to meet the landscape. Translation is calculated from the tile's entrance (1900,500,275.36388). Terrain samples around the footprint ranged approximately from -1648 to -1139. Grok itself is not edited.

Both the pack and generated map are Git-ignored because the repository is public. The tile is explicitly included in packaging; referenced vendor materials and meshes are followed by the cooker. Reconstruct it before packaging a clean checkout. PreviewHotel() is PIE-only and exercises the reveal without saving a grant.

Development Editor build passed September 7, 2026 (`Saved/hotel-generation-build.log`). Full player and packaged validation remain required. No commits or pushes were made for this feature.
