all: compile

compile:
	g++ main.cpp c_Socket.c -pthread -o CoucheLogicielle
clean:
	rm -rf CoucheLogicielle