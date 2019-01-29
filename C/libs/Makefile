CC = gcc
LINK = $(CC)

PREFIX = /usr/local/morph
PATH_DICTS = $(PREFIX)/dicts

PATH_LIBS = /usr/local/lib
PATH_HEAD = /usr/local/include/morph

BIN_PREFIX = ""

SRC = ./src
SRC_MOR = ./src/morphology
SRC_COM = ./src/common
SRC_TEXT = ./src/textprocessor
SRC_DICTS = ./src/dicts


BUILD = ./objs
MISC  = ./misc

CFLAGS      = -g3 -O3 -DPATH_DICTS==\"$(PATH_DICTS)/\" 
CSAHREDLIBS = -shared

INCS = -I$(SRC) \
    -I$(SRC_MOR) \
    -I$(SRC_COM) \
	-I/usr/local/include 

LIBS = -L/usr/local/lib 


DEPS = $(SRC_MOR)/automat.h \
    $(SRC_MOR)/dictinfo.h \
    $(SRC_MOR)/helpers.h \
    $(SRC_MOR)/miniautomat.h \
    $(SRC_MOR)/multilang.h \
    $(SRC_MOR)/wordforms.h \
    $(SRC_COM)/datastruct.h \
    $(SRC_COM)/errors.h \
    $(SRC_COM)/hashtable.h \
    $(SRC_COM)/strict_alloc.h \
    $(SRC_COM)/strtools.h \
    $(SRC_COM)/timer.h \
    $(SRC_TEXT)/document.h \
    $(SRC_TEXT)/suffix.h \
    $(SRC_TEXT)/tokenizer.h \
    $(SRC)/morph.h 

OBJSLIB = $(BUILD)/automat.o \
    $(BUILD)/dictinfo.o \
    $(BUILD)/helpers.o \
    $(BUILD)/miniautomat.o \
    $(BUILD)/multilang.o \
    $(BUILD)/wordforms.o \
    $(BUILD)/datastruct.o \
    $(BUILD)/hashtable.o \
    $(BUILD)/strict_alloc.o \
    $(BUILD)/strtools.o \
    $(BUILD)/timer.o \
    $(BUILD)/document.o \
    $(BUILD)/suffix.o \
    $(BUILD)/tokenizer.o \
    $(BUILD)/morph.o 

BINS = $(BUILD)/libmorph.so

BINS2 = libmorph.so

all: prebuild \
	$(BINS)

$(BUILD)/morph.o: $(DEPS) \
	$(SRC)/morph.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/morph.o $(SRC)/morph.c

$(BUILD)/automat.o: $(DEPS) \
	$(SRC_MOR)/automat.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/automat.o $(SRC_MOR)/automat.c
	
$(BUILD)/dictinfo.o: $(DEPS) \
	$(SRC_MOR)/dictinfo.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/dictinfo.o $(SRC_MOR)/dictinfo.c

$(BUILD)/helpers.o: $(DEPS) \
	$(SRC_MOR)/helpers.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/helpers.o $(SRC_MOR)/helpers.c

$(BUILD)/miniautomat.o: $(DEPS) \
	$(SRC_MOR)/miniautomat.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/miniautomat.o $(SRC_MOR)/miniautomat.c

$(BUILD)/multilang.o: $(DEPS) \
	$(SRC_MOR)/multilang.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/multilang.o $(SRC_MOR)/multilang.c

$(BUILD)/wordforms.o: $(DEPS) \
	$(SRC_MOR)/wordforms.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/wordforms.o $(SRC_MOR)/wordforms.c
	
$(BUILD)/datastruct.o: $(DEPS) \
	$(SRC_COM)/datastruct.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/datastruct.o $(SRC_COM)/datastruct.c

$(BUILD)/hashtable.o: $(DEPS) \
	$(SRC_COM)/hashtable.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/hashtable.o $(SRC_COM)/hashtable.c

$(BUILD)/strict_alloc.o: $(DEPS) \
	$(SRC_COM)/strict_alloc.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/strict_alloc.o $(SRC_COM)/strict_alloc.c

$(BUILD)/strtools.o: $(DEPS) \
	$(SRC_COM)/strtools.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/strtools.o $(SRC_COM)/strtools.c

$(BUILD)/timer.o: $(DEPS) \
	$(SRC_COM)/timer.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/timer.o $(SRC_COM)/timer.c

$(BUILD)/document.o: $(DEPS) \
	$(SRC_TEXT)/document.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/document.o $(SRC_TEXT)/document.c

$(BUILD)/suffix.o: $(DEPS) \
	$(SRC_TEXT)/suffix.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/suffix.o $(SRC_TEXT)/suffix.c

$(BUILD)/tokenizer.o: $(DEPS) \
	$(SRC_TEXT)/tokenizer.c
	$(CC) -c $(CFLAGS) -fPIC $(INCS) -o $(BUILD)/tokenizer.o $(SRC_TEXT)/tokenizer.c
	
$(BUILD)/libmorph.so: \
	$(OBJSLIB)
	$(CC) $(CSAHREDLIBS) -o $(BUILD)/libmorph.so $(OBJSLIB)	


clean:
	rm -rf $(BUILD)

prebuild:
	test -d $(BUILD) || mkdir -p $(BUILD)

install: 
	test -d $(PREFIX) || mkdir -p $(PREFIX)
	test -d $(PATH_DICTS) || mkdir -p $(PATH_DICTS)
	test -d $(PATH_HEAD) || mkdir -p $(PATH_HEAD)
	test -d $(PATH_HEAD)/morphology/ || mkdir -p $(PATH_HEAD)/morphology
	test -d $(PATH_HEAD)/document/ || mkdir -p $(PATH_HEAD)/document
	test -d $(PATH_HEAD)/remote/ || mkdir -p $(PATH_HEAD)/remote
	test -d $(PATH_HEAD)/common/ || mkdir -p $(PATH_HEAD)/common
	test -d $(PATH_HEAD)/textprocessor/ || mkdir -p $(PATH_HEAD)/textprocessor
	
	chmod 777 $(PATH_DICTS)
	chmod 755 $(PATH_HEAD)
	chmod 755 $(PATH_HEAD)/morphology
	chmod 755 $(PATH_HEAD)/document
	chmod 755 $(PATH_HEAD)/remote
	chmod 755 $(PATH_HEAD)/common
	chmod 755 $(PATH_HEAD)/textprocessor
	
	cp -rf $(SRC_DICTS)/ $(PREFIX)

	cp -rf $(BUILD)/libmorph.so $(PATH_LIBS)
	cp -rf $(SRC)/morph.h $(PATH_HEAD)

	cp -rf $(SRC_MOR)/*.h $(PATH_HEAD)/morphology/
	
	cp -rf $(SRC_COM)/*.h $(PATH_HEAD)/common/
	
	cp -rf $(SRC_TEXT)/*.h $(PATH_HEAD)/textprocessor/

        #@list='$(BINS2)'; for p in $$list; do \
        #	echo "cp $(BUILD)/$$p $(PATH_BIN)/$$p"; \
        #	`cp $(BUILD)/$$p $(PATH_BIN)/$$p`; \
        #done

	ldconfig

