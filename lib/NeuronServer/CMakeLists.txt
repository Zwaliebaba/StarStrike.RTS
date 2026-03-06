# ── NeuronServer (server-specific: simulation, persistence, transport, observability) ──
# Currently a placeholder. Will become a static library as server files are added.
add_library(NeuronServer INTERFACE)
target_link_libraries(NeuronServer INTERFACE NeuronCore GameLogic)
