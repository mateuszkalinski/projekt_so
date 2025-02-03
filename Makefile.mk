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
	@echo "Uruchamiam kapitanStatku w tle..."
	./kapitanStatku &
	sleep 1
	@echo "Uruchamiam pasazer (generator) w tle..."
	./pasazer &
	sleep 1
	@echo "Uruchamiam kapitanPortu (na pierwszym planie)..."
	./kapitanPortu
	@echo "Wszystko zakonczone."

clean:
	rm -f *.o kapitanStatku pasazer kapitanPortu
