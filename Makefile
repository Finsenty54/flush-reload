#make -f Make.Linux 或 make --file Make.AIX

#% 的意思是匹配零或若干字符
ind=src/include
vpath %.c src
vpath %.o obj
OBJ=obj

#$(patsubst %.c,%.o,$(wildcard *.c))
objects :=probe.o args.o
src :=$(objects:.o=.c)
SRC=src
probe:obj/probe.o obj/args.o
	mkdir -p bin
	gcc -o bin/$@ $^ 

obj/probe.o:src/probe.c $(ind)/args.h
	gcc -I $(ind) -c -o $@ $<

obj/args.o:src/args.c $(ind)/args.h
	gcc -I $(ind) -c -o $@ $<

.PHONY : clean
clean :
	-rm  obj/$(objects)