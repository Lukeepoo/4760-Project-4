all: oss user

oss: oss.o queue.o utility.o
	gcc -o oss oss.o queue.o utility.o -lrt

user: user.o utility.o
	gcc -o user user.o utility.o -lrt

clean:
	rm -f *.o oss user log.txt
