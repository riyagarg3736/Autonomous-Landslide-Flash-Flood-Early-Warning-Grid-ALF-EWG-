CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -Iinclude

alf_ewg: src/main.cpp include/DisasterCore/*.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o alf_ewg

run: alf_ewg
	./alf_ewg

clean:
	rm -f alf_ewg incident_log.bin stations.txt
