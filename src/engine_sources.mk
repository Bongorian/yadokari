# One maintained translation-unit inventory for host/tools/firmware.
ENGINE_NAMES := state_defaults performance_transport performance_sequence performance_navigation performance_files performance_parameters performance_history performance_composition ui_renderer_shell ui_renderer_sequence ui_renderer_sound software_canvas software_widgets engine engine_parameters engine_sequence engine_voice engine_recording engine_launch engine_command engine_synth engine_render
ENGINE_SOURCES := $(addprefix src/,$(addsuffix .cpp,$(ENGINE_NAMES)))
