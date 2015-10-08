include ../Makefile.master

LIBS+=		-lumem

CFLAGS+=	-D UMEM

OBJECTS=	$(C_OBJECTS) $(D_OBJECTS)

ILLUMETRICS_PROV=	illumetrics_provider.d

$(D_HDRS):
	$(DTRACEH) $(SRCDIR)/$(D_SCR) -o $(SRCDIR)/$(D_HDRS)

$(C_SRCS): %.c: $(C_HDRS) $(D_HDRS)


$(C_OBJECTS): %.o: %.c $(C_HDRS)
	$(CC) $(CFLAGS) $(CINC) -o $@ -c $<

$(D_OBJECTS): $(C_OBJECTS)
	$(DTRACEG) $(SRCDIR)/$(ILLUMETRICS_PROV) $(C_OBJECTS)

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
