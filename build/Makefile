SRC=$(PWD)/../src
CFLAGS= -m64 -W -Wall
CINC= -I /opt/libslablist/include -I /opt/libgraph/include
LDFLAGS= #TODO finish this
LIBS= -lslablist -lgraph -lumem
CWEBSRC=$(SRC)/illumetrics.w
#The outputs of ctangle and cweave should be in build directory
CSRC=$(PWD)/illumetrics.c
TEXSRC=$(PWD)/illumetrics.tex
PDF=$(PWD)/illumetrics.pdf
#aux files created and used by pdftex
TEXRESIDUE=	$(PWD)/illumetrics.log\
		$(PWD)/illumetrics.toc\
		$(PWD)/illumetrics.idx\
		$(PWD)/illumetrics.scn

$(CSRC): $(CWEBSRC)
	ctangle $(CWEBSRC) $(CSRC)

$(TEXSRC): $(CWEBSRC)
	cweave $(CWEBSRC) $(TEXSRC)

$(PDF): $(TEXSRC)
	pdftex $(TEXSRC)

pdf: $(PDF)
	echo "Made PDF"

code: $(CSRC)
	echo "Made PDF"

# TODO: implement this
#exec: $(CSRC)
#	gcc $(CSRC) -o illumetrics $(CINC) $(LDFLAGS) $(LIBS)


# We remove everything that isn't this Makefile
clean:
	rm $(CSRC) $(TEXSRC) $(PDF) $(TEXRESIDUE)
