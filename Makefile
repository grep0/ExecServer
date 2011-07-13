SRC = ExecServer.cpp sha1.cpp $(wildcard xmlrpcpp/*.cpp)
OBJ = $(SRC:.cpp=.o)

SRC += unix.cpp

CXXFLAGS = -g -O -W -Wall -I xmlrpcpp

all: ExecServer.exe test_tools

ExecServer.exe: $(OBJ)
	$(CXX) -o $@ $^ $(LDLIBS)

test_tools:
	$(MAKE) -C ./t

clean:
	-rm -f *.o xmlrpcpp/*.o ExecServer.exe
	$(MAKE) -C ./t clean
