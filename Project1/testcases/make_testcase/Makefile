all: make4061
	echo 'make4061'

make4061: util.a main.o
	gcc -o make4061 main.o util.a

main.o: main.c
	gcc -c main.c

util.a: parse.o cal.o
	ar rcs util.a parse.o cal.o

cal.o: cal.c
	gcc -c cal.c

parse.o: parse.c
	gcc -c parse.c

clean:
	rm -rf main.o cal.o parse.o util.a make4061
