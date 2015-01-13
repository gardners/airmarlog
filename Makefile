COPT=	-g -Wall

all:	wx150logger

wx150logger:	main.c
	$(CC) $(COPT) -o wx150logger main.c $(COPT)
