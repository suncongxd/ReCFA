
LIBS =  -L$(DYNINST_LIB) -ldyninstAPI  -lparseAPI -linstructionAPI -lsymtabAPI  -ldw  -lcommon -ldynElf -lboost_system
HEADERS = -I$(DYNINST_INCLUDE)
CC = g++
CFLAGS = -std=c++0x -no-pie
src = preCFG.cc
tar = preCFG

$(tar):$(src)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(HEADERS)

.PRONY:
	clean
clean:
	rm -rf $(tar)

install:
	cp $(tar) ../../bin
