main: main.c
	gcc -o main main.c -lutf8proc
clean:
	rm -v ./main ./output.txt
