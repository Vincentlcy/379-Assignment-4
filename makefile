all: clean compile

clean:
	rm -rf a3sdn submit.tar
	
tar:
	tar -czf submit.tar a4tasks.c makefile report.pdf 
		 
compile:
	gcc a4tasks.c -o a4tasks

