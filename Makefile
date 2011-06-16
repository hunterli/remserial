all: remserial

REMOBJ=remserial.o stty.o
remserial: $(REMOBJ)
	$(CC) $(LDFLAGS) -o remserial $(REMOBJ)

clean:
	rm -f remserial *.o
