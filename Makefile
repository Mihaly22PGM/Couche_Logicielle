#Executive file
EXEC = CoucheLogicielle

#Source files
SRC_FILES = main.cpp c_Socket.c

COURSE_DIR = /usr/include/postgresql

#Compiler
CXX = g++

#Additional flags
CXXFLAGS        = -g -Wall

CPPFLAGS        = -I. \
                  -I$(COURSE_DIR)

LDFLAGS         = -L. \
                  -L$(COURSE_DIR) \

# What libraries should be linked with.
# For example, -lm links with libm.so, the math library.
# If you make a library of your own, say, libscandir.a, you have to link it
# in by adding -lscandir here.
LDLIBS          = -lpq -pthread -std=c++11

###########################################################################
# Additional rules make should know about in order to compile our files
###########################################################################
# all is the default rule
all: $(EXEC)


# exec depends on the object files
# It is made automagically using the LDFLAGS and LOADLIBES variables.
# The .o files are made automagically using the CXXFLAGS variable.
$(EXEC):
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(SRC_FILES) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)

#all:	compile hexconverter
#
#compile:
#	g++ main.cpp c_Socket.c -pthread -std=c++11 -o CoucheLogicielle -g
#	touch Request.log
hexconverter:
	gcc Functionals/converterHEX.c -o HEXConverter
clean:
	rm -rf Request.log
	rm -rf CoucheLogicielle
	rm -rf HEXConverter
