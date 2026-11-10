# Include after Makefile. The invariant baseline uses the maintained host flags.
build/optimization-audit.o: tools/dsp/optimization_audit.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/optimization-audit: build/optimization-audit.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
build/optimization-io.o: tools/dsp/optimization_io.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/optimization-io: build/optimization-io.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
build/optimization-leaf: tools/dsp/optimization_leaf.cpp $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/optimization-states: tools/dsp/optimization_states.cpp $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/optimization-content: tools/dsp/optimization_content.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< -o $@ $(UI_BASE_ARCHIVE)
build/optimization-factory.o: tools/content/factory.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/optimization-factory: build/optimization-factory.o $(ENGINE_TOOL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O2 $^ -o $@ $(UI_BASE_ARCHIVE)
