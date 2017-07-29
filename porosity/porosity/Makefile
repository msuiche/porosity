SOURCES=$(shell find . -name "*.cpp")
OBJECTS=$(SOURCES:%.cpp=%.o)
TARGET=porosity
CXX=g++  # or c++, clang works too
CXXFLAGS=-std=c++14

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
