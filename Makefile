all:	compile hexconverter

compile:
	g++ main.cpp c_Socket.c -pthread -o CoucheLogicielle

hexconverter:
	gcc Functionals/converterHEX.c -o HEXConverter
clean:
	rm -rf CoucheLogicielle
	rm -rf HEXConverter
