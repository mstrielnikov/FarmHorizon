# Builds the shared FarmHorizon static library (engine/src/**/*.cpp) for one platform.
# Requires: ENGINE_ROOT, BUILD_ROOT, PLATFORM, CXX, AR, CXXFLAGS.
# Provides: LIB (path to the static library), OBJ_DIR.

LIB_SRC_DIR := $(ENGINE_ROOT)/src
LIB_SRCS    := $(shell find $(LIB_SRC_DIR) -name '*.cpp')
OBJ_DIR     := $(BUILD_ROOT)/$(PLATFORM)
LIB_OBJS    := $(patsubst $(LIB_SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(LIB_SRCS))
LIB         := $(OBJ_DIR)/libfarmhorizon.a

$(OBJ_DIR)/%.o: $(LIB_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(LIB): $(LIB_OBJS)
	@rm -f $@
	$(AR) rcs $@ $^

-include $(LIB_OBJS:.o=.d)
