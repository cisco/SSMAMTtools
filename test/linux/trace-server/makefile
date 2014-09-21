#
# make test code
#
PLATFORM32=1
TARGET=traceServer
CSOURCE_DIR=.


#external code
EX_CODE  =../../../ex_sock
CSRC_SOCK=$(wildcard $(EX_CODE)/*.c)
HSRC_SOCK=$(wildcard $(EX_CODE)/*.h)  

HEADERS =$(HSRC_SOCK) 

all: $(TARGET)

ADD_INCLUDE_DIR=-I$(EX_CODE)

# standard options
CC=gcc
WFLAGS=-Wall -Wstrict-prototypes -Wpointer-arith -Werror

ifdef PLATFORM32
CFLAGS=-m32 -march=i686 -DP32BITS -g -c  $(ADD_INCLUDE_DIR)
LIBS=-m32 -march=i686 -L$(TARGET_LIB) -static-libgcc -lpthread 
else
CFLAGS=-g -c  $(ADD_INCLUDE_DIR)
LIBS=-L$(TARGET_LIB) -static-libgcc -lpthread 
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
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $@.o $(EX_OBJECTS) $(LIBS)

clean:
	rm -rf $(TARGET) $(OBJECTS)

