PROG = quantum_console

OS = $(shell uname -s)
CXXFLAGS = -std=c++11 -O0 -Wall -g

ifeq ($(OS), Linux)
TARGET = $(PROG)
LIBS =
else ifeq ($(OS), Darwin)
TARGET = $(PROG)
LIBS = -framework IOKit -framework CoreFoundation
else ifeq ($(OS), cygwin)
TARGET = $(PROG).exe
LIBS = -lhid -lsetupapi
endif

MAKEFLAGS = --jobs=2
OBJS = quantum_console.o rawhid-apple.o rawhid-linux.o rawhid-win.o

all: $(TARGET)

linux: $(TARGET)
	cp $(TARGET) /tmp/q && sudo env ASAN_OPTIONS='check_initialization_order=1:strict_init_order=1:detect_odr_violation=1:detect_stack_use_after_return=1' /tmp/q

$(PROG): $(OBJS)
	$(CXX) -o $(PROG) -g $(OBJS) $(LIBS) $(CXXFLAGS)

$(PROG).exe: $(OBJS)
	$(CXX) $(OBJS) -g -o $(PROG).exe $(LIBS)

clean:
	rm -f *.o $(PROG) $(PROG).exe

