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

clean:
	rm $(OBJECTS) illumetrics
