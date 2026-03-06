# ── NeuronClient (client-specific: DX12 rendering, input, client networking, audio, UI) ──
# Currently a placeholder. Will become a static library as client files are added.
add_library(NeuronClient INTERFACE)
target_link_libraries(NeuronClient INTERFACE NeuronCore GameLogic)
