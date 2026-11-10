# Include after the repository Makefile: make -f Makefile -f tools/manual.mk manual-screens
build/manual-render.o: tools/manual_render.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/manual-render: build/manual-render.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: manual-screens
manual-screens: build/manual-render
	UBSAN_OPTIONS=halt_on_error=1 ./build/manual-render
	@echo "Current renderer frames and controls are in build/manual-*; archived v386 manual is unchanged."

build/ui-visual-capture.o: tools/ui_visual_capture.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/ui-visual-capture: build/ui-visual-capture.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
