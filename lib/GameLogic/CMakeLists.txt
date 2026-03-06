# ── GameLogic (shared game logic: entities, voxels, sectors, combat, mining) ──
# Currently a placeholder. Will become a static library as game logic files are added.
add_library(GameLogic INTERFACE)
target_link_libraries(GameLogic INTERFACE NeuronCore)
