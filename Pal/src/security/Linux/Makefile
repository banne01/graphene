MAKEFLAGS += --check-symlink-times

CC	= gcc
LD	= ld

CFLAGS	= -Wall -fPIC -O2 -std=gnu99 -fgnu89-inline -Wall -U_FORTIFY_SOURCE \
	  -fno-stack-protector -fno-builtin \
	  -DPAL_LOADER_PATH="$(shell readlink -f ../../pal)" -DIN_PAL \
	  -I. -I../.. -I../../../include -I../../host/Linux \
	  -I../../../lib -I../../../linux-3.5/graphene
LDFLAGS	= -shared -nostdlib -z combreloc -z defs --version-script pal-sec.map -e start

objs	= bpf_helper filter main manifest wrapper slab printf
graphene_lib = ../../../lib/graphene-lib.a

all: libpal_sec.so

debug: debug = debug
debug: CC = gcc -g
debug: CFLAGS += -DDEBUG
debug: libpal_sec.so

$(graphene_lib):
	make -C ../../../lib $(debug)

libpal_sec.so: $(addsuffix .o,$(objs)) $(graphene_lib)
	@echo [ security/Linux/$@ ]
	@[ ! -f $@ ] || mv -f $@ $@.backup
	@$(LD) $(LDFLAGS) -o $@ $^ -soname=libpal_sec.so

%.o: %.c
	@echo [ security/Linux/$@ ]
	@$(CC) $(CFLAGS) -c $< -o $@

%.e: %.c
	@echo [ security/Linux/$@ ]
	@$(CC) $(CFLAGS) -E $< -o $@

clean:
	rm -rf *.o libpal_sec.so
