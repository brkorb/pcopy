
EXE := pcopy
SRC := pcopy.c pcopy-opts.c
HDR := pcopy-opts.h
GCC := $(CC) $(CFLAGS) -std=c99 -g -O4
CAG := $(shell autoopts-config cflags)
LAG := $(shell autoopts-config ldflags) -lpthread -lncurses
OBJ := $(SRC:.c=.o)
DOC := $(EXE).1 $(EXE).texi doxy-pcopy

default : $(EXE)
all     : default $(EXE).1

GEN     := pcopy-opts.h pcopy-opts.c
gen     : $(GEN)
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

stamp-pcopy-opts : pcopy-opts.def
	autogen -MT$@ -MFpcopy-opts.d -MP $<

$(EXE).1 : pcopy-opts.def
	autogen -Tagman-cmd.tpl $<

doxy-pcopy     : $(SRC) $(HDR) pcopy.doxy
	autogen -T doxygen.tpl pcopy-opts.def

doxy-install   : doxy-pcopy
	rm -rf ~/public_html/pcopy ; \
	cp -frp $</html ~/public_html/pcopy

clean  :
	rm -f *~ $(OBJ) pcopy-opts.d-* doxy.err

clobber : clean
	rm -f stamp-* $(EXE) $(EXE).1 $(EXE)-*.tar.xz \
	      $(AUTOGEN_stamp_pcopy_opts_TList) pcopy-opts.d
	rm -rf doxy-pcopy

tarball : $(GEN) $(EXE).1 $(SRC) Makefile pcopy-opts.def
	eval $$(sed -n "/^version/s/[ ;']//gp" pcopy-opts.def) ; \
	if test -d $(EXE)-$$version ; then rm -rf $(EXE)-$$version ; fi ; \
	mkdir $(EXE)-$$version ; \
	ln pcopy-opts.def *.[ch] $(EXE).1 $(EXE)-$$version/. ; \
	sed '/^stamp/,$$d;/ stamp/d;/^all /,/^-include.*opts/d' Makefile \
		> $(EXE)-$$version/Makefile ; \
	tar -cJf $(EXE)-$$version.tar.xz $(EXE)-$$version ; \
	rm -rf $(EXE)-$$version ; \
	tar -xJf $(EXE)-$$version.tar.xz ; \
	test -d $(EXE)-$$version ; \
	cd $(EXE)-$$version ; make ; cd - ; rm -rf $(EXE)-$$version

.PHONY : gen all clean clobber tarball doxy-install

