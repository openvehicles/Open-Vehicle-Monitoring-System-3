CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g \
            -DVWEGOLF_NATIVE_TEST \
            -Imock \
            -I. \
            -I../src \
            -include mock/mock_ovms.hpp

SRC := mock/mock_ovms.cpp \
       ../src/vehicle_vwegolf.cpp \
       test_can_decode.cpp \
       test_crtd_replay.cpp \
       test_clima.cpp

all: test

test: $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@
	./$@

# DBC-decode regression suite. Asserts the cantools decode of vwegolf.dbc
# matches the per-capture .md ground-truth claims. Requires the project venv
# (.venv) with cantools+pytest installed — see analysis/README.md.
# Activate .venv first or pass PYTEST=/path/to/.venv/bin/pytest.
PYTEST ?= pytest
pytest:
	cd analysis && $(PYTEST) -q

clean:
	rm -f test

.PHONY: all test pytest clean
