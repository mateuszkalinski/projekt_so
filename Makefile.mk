# Kompilator
CXX = g++
CXXFLAGS = -Wall -std=c++11

# Domyslny cel
all: kapitanStatku kapitanPortu pasazer

kapitanStatku: kapitanStatku.o common.o
	$(CXX) $(CXXFLAGS) -o $@ kapitanStatku.o common.o

kapitanPortu: kapitanPortu.o common.o
	$(CXX) $(CXXFLAGS) -o $@ kapitanPortu.o common.o

pasazer: pasazer.o common.o
	$(CXX) $(CXXFLAGS) -o $@ pasazer.o common.o

kapitanStatku.o: kapitanStatku.cpp common.h
	$(CXX) $(CXXFLAGS) -c kapitanStatku.cpp

kapitanPortu.o: kapitanPortu.cpp common.h
	$(CXX) $(CXXFLAGS) -c kapitanPortu.cpp

pasazer.o: pasazer.cpp common.h
	$(CXX) $(CXXFLAGS) -c pasazer.cpp

common.o: common.cpp common.h
	$(CXX) $(CXXFLAGS) -c common.cpp

# Uruchomienie w jednym terminalu (kolejno w tle i na pierwszym planie)
run: all
	@echo "Uruchamiam kapitanStatku w tle..."
	./kapitanStatku &
	sleep 1
	@echo "Uruchamiam 5 pasazerow w tle..."
	./pasazer 5 &
	sleep 1
	@echo "Uruchamiam kapitanPortu (na pierwszym planie)..."
	./kapitanPortu
	@echo "Wszystko zakonczone."

clean:
	rm -f *.o kapitanStatku kapitanPortu pasazer
