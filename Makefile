all:
	gcc -c structs.c
	gcc -c proxy.c
	gcc -O2 -s -o proxy structs.o proxy.o
test:
	gcc -c structs.c
	gcc -c compress.c
	gcc -c tests/tests.c
	gcc -O2 -s -o run_tests structs.o compress.o tests.o
	./run_tests
clean:
	rm -rf *.o proxy run_tests
