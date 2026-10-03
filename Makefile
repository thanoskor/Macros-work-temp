HOST_GCC = g++
TARGET_GCC = gcc

PLUGIN_SOURCE_FILES = plugin.cc

GCCPLUGINS_DIR := $(shell $(TARGET_GCC) -print-file-name=plugin)

CXXFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -fno-rtti -O2

plugin.so: $(PLUGIN_SOURCE_FILES)
	$(HOST_GCC) -shared $(CXXFLAGS) $^ -o $@

clean:
	rm -f plugin.so