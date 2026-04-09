CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g \
            -Imock \
            -I. \
            -I../src \
            -include mock/mock_ovms.hpp

SRC := mock/mock_ovms.cpp \
       ../src/vehicle_vwegolf.cpp \
       test_can_decode.cpp \
       test_crtd_replay.cpp

all: test

test: $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@
	./$@

clean:
	rm -f test

.PHONY: all test clean
