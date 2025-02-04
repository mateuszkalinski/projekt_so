CXX = g++
CXXFLAGS = -Wall -std=c++11

all: kapitanStatku pasazer kapitanPortu

kapitanStatku: kapitanStatku.o common.o
        $(CXX) $(CXXFLAGS) -o kapitanStatku kapitanStatku.o common.o

pasazer: pasazer.o common.o
        $(CXX) $(CXXFLAGS) -o pasazer pasazer.o common.o

kapitanPortu: kapitanPortu.o common.o
        $(CXX) $(CXXFLAGS) -o kapitanPortu kapitanPortu.o common.o

kapitanStatku.o: kapitanStatku.cpp common.h
        $(CXX) $(CXXFLAGS) -c kapitanStatku.cpp

pasazer.o: pasazer.cpp common.h
        $(CXX) $(CXXFLAGS) -c pasazer.cpp

kapitanPortu.o: kapitanPortu.cpp common.h
        $(CXX) $(CXXFLAGS) -c kapitanPortu.cpp

common.o: common.cpp common.h
        $(CXX) $(CXXFLAGS) -c common.cpp

run: all
ifeq ($(N),)
        @echo "Blad: Nie podano parametru N, np. N=5 K=3 R=3 T2=5"
        @exit 1
endif
ifeq ($(K),)
        @echo "Blad: Nie podano parametru K."
        @exit 1
endif
ifeq ($(R),)
        @echo "Blad: Nie podano parametru R."
        @exit 1
endif
ifeq ($(T2),)
        @echo "Blad: Nie podano parametru T2."
        @exit 1
endif

        @echo "Uruchamiam kapitanStatku w tle z parametrami: N=$(N), K=$(K), R=$(R), T2=$(T2)"
        ./kapitanStatku $(N) $(K) $(R) $(T2) &
        sleep 1
        @echo "Uruchamiam pasazer (generator) w tle..."
        ./pasazer &
        sleep 1
        @echo "Uruchamiam kapitanPortu (na pierwszym planie)..."
        ./kapitanPortu
        @echo "Wszystko zakonczone."

clean:
        rm -f *.o kapitanStatku pasazer kapitanPortu