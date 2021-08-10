# DYNINST_INCLUDE and DYNINST_LIB should be set to locations
# where Dyninst header and library files were installed, respectively

T=mutator

SOURCES=mutator.cc

OBJECTS=$(SOURCES:.c=.o)

CC=g++

LIBS =  -ldyninstAPI -lboost_system -linstructionAPI -lparseAPI -lcommon -lsymtabAPI -ldynElf -L$(DYNINST_LIB) #-L/usr/local/lib

HEADERS = -I$(DYNINST_INCLUDE) #-I/usr/local/include

FLAGS = $(LIBS) $(HEADERS) -std=c++11 -no-pie

$(T): $(OBJECTS)
	$(CC) -o $(T) $(OBJECTS) $(FLAGS)
	
%.o: %.c
	$(CC) -c -o $@ $< $(FLAGS)

clean:
	rm -rf $(T) *.o

install:
	cp $(T) ../../bin





