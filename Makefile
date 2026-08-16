.PHONY: all clean run

BUILD_DIR = build
BINARY    = $(BUILD_DIR)/server

all: $(BINARY)

$(BINARY):
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

run: $(BINARY)
	$(BINARY)

clean:
	cmake -E remove_directory $(BUILD_DIR)