CFLAGS =

upload-to-rbn: upload-to-rbn.c
	gcc $(CFLAGS) -D_GNU_SOURCE -o $@ $^

clean:
	rm -rf *.o upload-to-rbn
