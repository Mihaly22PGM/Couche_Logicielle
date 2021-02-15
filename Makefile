all: compile

compile:
	g++ CoucheLogicielle.cpp -o CoucheLogicielle -ltins 

clean:
	rm -rf CoucheLogicielle 
