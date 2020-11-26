CXX      := g++
MDULNAME := fim
DEFFLAGS := -DMODULE_NAME=$(MDULNAME) -DWITH_SIG_TERM -DNDEBUG
CXXFLAGS := -std=c++17 -fPIC -shared $(DEFFLAGS) -fopenmp
WRNFLAGS := -Wall -pedantic -Wextra -Weffc++ -Wunused-result
LDFLAGS  :=
TARGET   := $(MDULNAME).so
INCLUDE  := -I/usr/include/python3.6m -Iinclude
SRC      := $(wildcard src/*.cpp)

OBJECTS  := $(SRC:%.cpp=%.o)

all: $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@ $(LDFLAGS)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $^ $(LDFLAGS)

.PHONY: all clean

clean:
	-@rm -rvf src/*.o
	-@rm -rvf $(TARGET)
