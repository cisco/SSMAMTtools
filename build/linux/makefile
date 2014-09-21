#
PLATFORM32=1
TARGET=libamt.a

TARGET_DIR=obj
SOURCE_DIR=../../src
LIBAPI_DIR=../../include
TARGET_BIN=../../bin/linux
TARGET_LIB=../../lib/linux

CSRC=$(wildcard $(SOURCE_DIR)/*.c)
HSRC=$(wildcard $(SOURCE_DIR)/*.h) $(LIBAPI_DIR)/amt.h

#external code
EX_CODE   =../../ex_sock
SRC_SOCK=$(wildcard $(EX_CODE)/*.c)
HSRC_SOCK=$(wildcard $(EX_CODE)/*.h)  

CSOURCES=$(CSRC) $(SRC_SOCK)
HEADERS =$(HSRC) $(HSRC_SOCK) 

ADD_INCLUDE_DIR=-I$(LIBAPI_DIR) -I$(SOURCE_DIR) -I$(EX_CODE) 

# standard options
CC=gcc
LIBS=-static-libgcc -lpthread
WFLAGS=-Wpadded -Wall -Wstrict-prototypes -Wpointer-arith -Werror

ifdef PLATFORM32
CFLAGS=-m32 -march=i686 -DP32BITS -g -c  $(ADD_INCLUDE_DIR)
else
CFLAGS=-g -c  $(ADD_INCLUDE_DIR)
endif

#CFLAGS=-g -c -O3 $(ADD_INCLUDE_DIR)

OBJECTS=$(addprefix $(TARGET_DIR)/, $(addsuffix .o,$(basename $(notdir $(CSOURCES)))))

# Main Targets
all:  $(TARGET_DIR) $(TARGET) 

# make objects
$(TARGET_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	$(CC) -o $@ $(CFLAGS) $(WFLAGS)  -r $<

obj/ex_sock.o: $(EX_CODE)/ex_sock.c $(HEADERS)
	$(CC) -o $@ $(CFLAGS) $(WFLAGS)  -r $<

# show
show:
	#echo "SRC_SOCK=" $(SRC_SOCK)
	@echo $(OBJECTS)
	@echo $(CSOURCES)
	@echo $(HEADERS)

# make the lib
$(TARGET): $(OBJECTS)
	ar -rs $(TARGET_LIB)/$@  $(OBJECTS)

clean:
	rm -rf $(TARGET_LIB)/$(TARGET) $(TARGET_DIR)

cleand:
	@echo do nothing
depend:
	@echo do explicitly


$(TARGET_DIR):
	@mkdir $@
	@chmod a+rwx $@
