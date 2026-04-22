CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS  = -lstdc++fs
TARGET   = vehicle_diagnostic
SRCDIR   = src

SRCS = $(SRCDIR)/main.cpp \
       $(SRCDIR)/simulator.cpp \
       $(SRCDIR)/serial_port.cpp \
       $(SRCDIR)/decoder.cpp \
       $(SRCDIR)/diagnostic_tool.cpp

TEST_DECODER   = tests/test_decoder
TEST_SIMULATOR = tests/test_simulator

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

$(TEST_DECODER): tests/test_decoder.cpp $(SRCDIR)/decoder.cpp
	$(CXX) $(CXXFLAGS) -o $(TEST_DECODER) tests/test_decoder.cpp $(SRCDIR)/decoder.cpp $(LDFLAGS)

$(TEST_SIMULATOR): tests/test_simulator.cpp $(SRCDIR)/simulator.cpp $(SRCDIR)/decoder.cpp
	$(CXX) $(CXXFLAGS) -o $(TEST_SIMULATOR) tests/test_simulator.cpp \
		$(SRCDIR)/simulator.cpp $(SRCDIR)/decoder.cpp $(LDFLAGS)

test: $(TEST_DECODER) $(TEST_SIMULATOR)
	@echo "\n=== Decoder Tests ==="; ./$(TEST_DECODER)
	@echo "\n=== Simulator Tests ==="; ./$(TEST_SIMULATOR)

clean:
	rm -f $(TARGET) $(TEST_DECODER) $(TEST_SIMULATOR)

run: $(TARGET)
	./$(TARGET)

.PHONY: all test clean run
