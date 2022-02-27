all: 
	cc -o restclient restclient.c -DUNITTEST -g

clean:
	rm -f restclient restclient.o
