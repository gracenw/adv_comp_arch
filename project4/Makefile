SRCDIRS = simulator coherence coherence/directory coherence/agents
CFLAGS = -g -MMD -Wall -pedantic -Werror -std=c11 $(foreach srcdir,$(SRCDIRS),-I$(srcdir))
CXXFLAGS = -g -MMD -Wall -pedantic -Werror -std=c++17 $(foreach srcdir,$(SRCDIRS),-I$(srcdir))
LIBS = -lm
CC = gcc
CXX = g++
OFILES = $(patsubst %.c,%.o,$(wildcard $(foreach srcdir,$(SRCDIRS),$(srcdir)/*.c))) \
		 $(patsubst %.cpp,%.o,$(wildcard $(foreach srcdir,$(SRCDIRS),$(srcdir)/*.cpp)))
DFILES = $(patsubst %.c,%.d,$(wildcard $(foreach srcdir,$(SRCDIRS),$(srcdir)/*.c))) \
		 $(patsubst %.cpp,%.d,$(wildcard $(foreach srcdir,$(SRCDIRS),$(srcdir)/*.cpp)))
HFILES = $(wildcard *.h *.hpp)
PROG = dirsim
TARBALL = $(if $(USER),$(USER),gburdell3)-proj4.tar.gz

ifdef ASAN
undefine DEBUG
CFLAGS += -fsanitize=address
CXXFLAGS += -fsanitize=address
LIBS += -fsanitize=address
endif

ifdef PROFILE
FAST=1
undefine DEBUG
CFLAGS += -pg
CXXFLAGS += -pg
LIBS += -pg
endif

ifdef DEBUG
CFLAGS += -DDEBUG
CXXFLAGS += -DDEBUG
endif

ifdef FAST
CFLAGS += -O2
CXXFLAGS += -O2
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
	@bash validate.sh

submit: clean
	tar --exclude=project4_v*.pdf -czhvf $(TARBALL) Makefile $(wildcard *.pdf) simulator coherence experiments
	@echo
	@echo 'submission tarball written to' $(TARBALL)
	@echo 'please decompress it yourself and make sure it looks right!'

clean:
	rm -f $(TARBALL) $(PROG) $(OFILES) $(DFILES)

-include $(DFILES)

# if you're a student, ignore this
-include ta-rules.mk
