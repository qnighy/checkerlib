#!/usr/bin/gmake -f

CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -g -std=c++03
OBJS = sample1 sample2 sample3 sample4 sample4-gen sample5a sample5b

.PHONY: all clean test

all: $(OBJS)

clean:
	$(RM) $(OBJS)

test: all
	./sample1 < sample1-1.in
	./sample1 < sample1-2.in
	! ./sample1 < sample1-3.in
	! ./sample1 < sample1-4.in
	! ./sample2 < sample2-1.in
	! ./sample2 < sample2-2.in
	! ./sample2 < sample2-3.in
	./sample3 < sample3-1.in
	./sample3 < sample3-2.in
	./sample4 < sample4-1.in
	./sample4-gen | ./sample4

%: %.cpp checkerlib.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

