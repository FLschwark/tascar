PREFIX = /usr/local

BINFILES = tascar_scene 

OBJECTS = jackclient.o coordinates.o speakerlayout.o multipan.o osc_helper.o async_file.o errorhandling.o

INSTBIN = $(patsubst %,$(PREFIX)/bin/%,$(BINFILES))

#GTKMMBIN = tascar_draw

CXXFLAGS += -Wall -O3 -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -fno-finite-math-only -L./

EXTERNALS = jack libxml++-2.6 liblo sndfile

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

all:
	mkdir -p build
	$(MAKE) -C build -f ../Makefile $(BINFILES)

install:
	$(MAKE) -C build -f ../Makefile $(INSTBIN)

uninstall:
	rm -f $(INSTBIN)

clean:
	rm -Rf *~ src/*~ build doc/html

VPATH = ../src ../src/hoafilt

.PHONY : doc

doc:
	cd doc && sed -e 's/PROJECT.NUMBER.*=.*/&'`cat ../version`'/1' doxygen.cfg > .temp.cfg && doxygen .temp.cfg
	rm -Rf doc/.temp.cfg

include $(wildcard *.mk)


$(JACKBIN): LDLIBS += -ljack

tascar_jackio: LDLIBS += -lsndfile

tascar_scene: $(OBJECTS)

$(GTKBIN): LDLIBS += `pkg-config --libs gtk+-2.0`
$(GTKBIN): CXXFLAGS += `pkg-config --cflags gtk+-2.0`

$(GTKMMBIN): LDLIBS += `pkg-config --libs gtkmm-2.4`
$(GTKMMBIN): CXXFLAGS += `pkg-config --cflags gtkmm-2.4`

$(PREFIX)/bin/%: %
	cp $< $@

%: %.o
	$(CXX) $(CXXFLAGS) $(LDLIBS) $^ -o $@

%.o: %.cc
	$(CPP) $(CPPFLAGS) -MM -MF $(@:.o=.mk) $<
	$(CXX) $(CXXFLAGS) -c $< -o $@

dist: clean doc
	svn commit -m "auto commit dist"
	$(MAKE) -C doc
	$(MAKE) DISTNAME=tascar-`cat version`-r`svnversion .|sed -e 's/.*://1'` bz2

disttest:
	$(MAKE) DISTNAME=tascar-`cat version`-r`svnversion .|sed -e 's/.*://1'` disttest2

disttest2:
	rm -Rf $(DISTNAME) && tar xjf $(DISTNAME).tar.bz2
	cd $(DISTNAME) && $(MAKE)

bz2:
	rm -Rf $(DISTNAME) $(DISTNAME).files
	(cat files; find doc/html -print) | sed 's/.*/$(DISTNAME)\/&/g' > $(DISTNAME).files
	ln -s . $(DISTNAME)
	tar cjf $(DISTNAME).tar.bz2 --no-recursion -T $(DISTNAME).files
	rm -Rf $(DISTNAME) $(DISTNAME).files


tascar_ambdecoder: LDLIBS += `pkg-config --libs gsl`


# Local Variables:
# compile-command: "make"
# End: