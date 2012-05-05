
EXE := pcopy
SRC := pcopy.c pcopy-opts.c
HDR := pcopy-opts.h
GCC := $(CC) $(CFLAGS) -std=c99 -g -O0
CAG := $(shell autoopts-config cflags)
LAG := $(shell autoopts-config ldflags) -lpthread -lncurses
OBJ := $(SRC:.c=.o)
DOC := $(EXE).1 $(EXE).texi doxy-pcopy

default : $(EXE)
all     : default $(EXE).1

GEN     := pcopy-opts.h pcopy-opts.c
gen     : $(GEN)
docs    : $(DOC)
$(GEN)  : stamp-pcopy-opts
$(OBJ)  : $(GEN)
-include pcopy-opts.d

$(EXE)  : $(OBJ)
	gcc -g -o $@ $(OBJ) $(LAG)

.c.o :
	$(GCC) -c -o $@ $(CAG) $<

install : all doxy-install
	install -t /usr/local/bin $(EXE) ; \
	install -t /usr/local/man/man1 $(EXE).1
	install -t /usr/local/share/info $(EXE).info

stamp-pcopy-opts : pcopy-opts.def
	autogen -MT$@ -MFpcopy-opts.d -MP $<

$(EXE).1        : pcopy-opts.def
	autogen -Tagman-cmd.tpl $<

$(EXE).texi     : pcopy-opts.def
	autogen -DLEVEL=document -T agtexi-cmd pcopy-opts.def

doxy-pcopy      : $(SRC) $(HDR) pcopy.doxy
	autogen -T doxygen.tpl pcopy-opts.def

doxy-install    : doxy-pcopy
	rm -rf ~/public_html/pcopy ; \
	cp -frp $</html ~/public_html/pcopy

clean  :
	rm -f *~ $(OBJ) pcopy-opts.d-* doxy.err

clobber :
	git clean -f -x -d .

tarball : $(GEN) $(DOC) $(SRC) Makefile mk-tarball.sh
	EXE="$(EXE)" $(SHELL) mk-tarball.sh

.PHONY : gen all clean clobber tarball doxy-install

