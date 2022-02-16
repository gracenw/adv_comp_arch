CFLAGS = -MMD -g #-Wall -pedantic
CXXFLAGS = -MMD -g #-Wall -pedantic
LIBS = -lm
CC = gcc
CXX = g++
OFILES = $(patsubst %.c,%.o,$(wildcard *.c)) $(patsubst %.cpp,%.o,$(wildcard *.cpp))
DFILES = $(patsubst %.c,%.d,$(wildcard *.c)) $(patsubst %.cpp,%.d,$(wildcard *.cpp))
HFILES = $(wildcard *.h *.hpp)
PROG = cachesim
TARBALL = $(USER)-proj1.tar.gz

ifdef DEBUG
CFLAGS += -DDEBUG
CXXFLAGS += -DDEBUG
endif

.PHONY: all validate submit clean

all: $(PROG)

$(PROG): $(OFILES)
	$(CXX) -o $@ $^ $(LIBS)

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(HFILES)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

validate: $(PROG)
	@./validate.sh

submit: clean
	tar --exclude=project1_description.pdf -czhvf $(TARBALL) run.sh Makefile $(wildcard *.pdf *.cpp *.c *.hpp *.h)
	@echo
	@echo 'submission tarball written to' $(TARBALL)
	@echo 'please decompress it yourself and make sure it looks right!'

clean:
	rm -f $(TARBALL) $(PROG) $(OFILES) $(DFILES) *.tar.gz

-include $(DFILES)

# if you're a student, ignore this
-include ta-rules.mk
