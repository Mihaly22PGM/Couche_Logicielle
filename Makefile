all: compile

compile:
	sudo g++ main.cpp Snif.cpp -o CoucheLogicielle -ltins -std=c++11 -pthread
	#g++ CoucheLogicielle.cpp -o CoucheLogicielle -ltins 

clean:
	rm -rf CoucheLogicielle 
