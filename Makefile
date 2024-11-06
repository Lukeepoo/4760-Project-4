all: oss user

oss: oss.c pcb.h
	gcc -o oss oss.c -lrt

user: user.c pcb.h
	gcc -o user user.c -lrt

clean:
	rm -f oss user *.o