include ../Makefile.master

LIBS+=		-lumem

CFLAGS+=	-D UMEM

OBJECTS=	$(C_OBJECTS)

$(C_SRCS): %.c: $(C_HDRS)

$(C_OBJECTS): %.o: %.c $(C_HDRS)
	$(CC) $(CFLAGS) $(CINC) -o $@ -c $<

objs: $(OBJECTS)

illumetrics: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

# We copy the default config files into the prefix, and illumetrics copies them
# into the home directory on first run.
install:
	-pfexec rm -r $(PREFIX) 2> /dev/null
	pfexec mkdir $(PREFIX)
	pfexec mkdir $(PREFIX)/bin
	pfexec cp -r $(CONFIG) $(PREFIX)/config
	pfexec cp illumetrics $(PREFIX)/bin


clean:
	rm $(OBJECTS) illumetrics
