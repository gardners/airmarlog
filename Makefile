CC=g++
# For OSX:
COPT=	-g -Wall -Iyocto/sources -framework CoreFoundation -framework IOKit
# For linux:
#COPT= -g -Wall -lm, -lpthread, -lusb1.0, -lstdc++

all:	wx150logger

wx150logger:	main.c yocto/Binaries/osx/libyocto-static.a
	$(CC) $(COPT) -o wx150logger main.c yocto/Binaries/osx/libyocto-static.a $(COPT) 
