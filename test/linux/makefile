#
# make test code
#
#PLATFORM32=1
TARGET=amtchk amtpoll amtsink amtstream amtfeeder
CSOURCE_DIR=..

#
# lib source
#
LIBAPI_DIR=../../include
TARGET_LIB=../../lib/linux

#external code
EX_CODE  =../../ex_sock
CSRC_SOCK=$(wildcard $(EX_CODE)/*.c)
HSRC_SOCK=$(wildcard $(EX_CODE)/*.h)  

HEADERS =$(LIBAPI_DIR)/amt.h $(HSRC_SOCK) 

all: $(TARGET)

ADD_INCLUDE_DIR=-I$(LIBAPI_DIR) -I$(EX_CODE)

# standard options
CC=gcc
WFLAGS=-Wall -Wstrict-prototypes -Wpointer-arith  

ifdef PLATFORM32
AMTLIB=amt
CFLAGS=-m32 -march=i686 -DP32BITS -g -c  $(ADD_INCLUDE_DIR)
LIBS=-m32 -march=i686 -L$(TARGET_LIB) -static-libgcc -l$(AMTLIB) -lpthread 
else
AMTLIB=amt64
CFLAGS=-g -c  $(ADD_INCLUDE_DIR)
LIBS=-L$(TARGET_LIB) -static-libgcc -l$(AMTLIB) -lpthread 
endif

#CFLAGS=-g -c -O3 $(ADD_INCLUDE_DIR)

EX_OBJECTS=$(addsuffix .o,$(basename $(notdir $(CSRC_SOCK))))
OBJECTS=$(addsuffix .o,$(TARGET)) $(EX_OBJECTS)


# Main Targets
all: $(TARGET)

# make objects
%.o: $(CSOURCE_DIR)/%.c $(HEADERS)
	$(CC) -o $@ $(CFLAGS) $(WFLAGS)  -r $<

$(EX_OBJECTS): $(CSRC_SOCK) $(HEADERS)
	$(CC) -o $@ $(CFLAGS) $(WFLAGS)  -r $<

# show
show:
	@echo $(TARGET)
	@echo $(HEADERS)
	@echo $(OBJECTS)
	@echo $(EX_OBJECTS)

# make the lib
$(TARGET): $(OBJECTS) $(TARGET_LIB)/lib$(AMTLIB).a
	$(CC) -o $@ $@.o $(EX_OBJECTS) $(LIBS)

clean:
	rm -rf $(TARGET) $(OBJECTS)

