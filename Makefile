CXX := g++
CXXFLAGS := -std=gnu++20 -O3 -Wall -Wextra -pthread -march=native -MMD -MP
INCLUDES := -Iinclude

SRC_DIR := src
TEST_DIR := tests
BUILD_DIR := build

BENCH_SRC := $(SRC_DIR)/main_bench.cpp
BENCH_OBJ := $(BUILD_DIR)/main_bench.o
BENCH_EXE := $(BUILD_DIR)/bench

SINGLE_SRC := $(SRC_DIR)/main_single.cpp
SINGLE_OBJ := $(BUILD_DIR)/main_single.o
SINGLE_EXE := $(BUILD_DIR)/run_one

$(shell mkdir -p $(BUILD_DIR))

# -----------------------------------------------------------
# Default: build both bench and run_one
# -----------------------------------------------------------
all: $(BENCH_EXE) $(SINGLE_EXE)

# -------------------- bench (multi-algo) -------------------
$(BENCH_EXE): $(BENCH_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "[OK] Built benchmark -> $@"

$(BUILD_DIR)/main_bench.o: $(BENCH_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# -------------------- run_one (single-algo) ----------------
$(SINGLE_EXE): $(SINGLE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "[OK] Built single-run -> $@"

$(BUILD_DIR)/main_single.o: $(SINGLE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# -----------------------------------------------------------
# Parametric single-test build: make test TEST=stacks
# -----------------------------------------------------------
TEST ?=
TEST_SRC := $(TEST_DIR)/test_$(TEST).cpp
TEST_OBJ := $(BUILD_DIR)/test_$(TEST).o
TEST_EXE := $(BUILD_DIR)/test_$(TEST)

test: $(TEST_EXE)

$(TEST_EXE): $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "[OK] Built -> $@"

$(BUILD_DIR)/test_%.o: $(TEST_DIR)/test_%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# -----------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR)/*

rebuild: clean all

-include $(BUILD_DIR)/*.d
