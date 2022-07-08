TARGET   = p6cache

BUILD    = dos
SYSTEM   = pmode
DEBUG    = all
DLEVEL   = 0
RASTER   = 0

INCLUDE  = coreboot

AS       = nasm.exe
CC       = wcc386.exe
CP       = wpp386.exe
LD       = wlink.exe
CFLAGS   = -5r -fp5 -fpi87 -zp16 -onhasbmir -s -d$(DLEVEL) -bt=$(BUILD) -I=$(INCLUDE)
LFLAGS   =
AFLAGS   = -f win32 -l $<.lst

OBJS     = $(TARGET).obj mycpuid.obj l2_cache.obj
OBJSTR   = file {$(OBJS)}

LIBS     = 
LIBSTR   = library {$(LIBS)}

all: $(TARGET).exe .symbolic

$(TARGET).exe : $(OBJS) .symbolic
	%write $(TARGET).lnk debug $(DEBUG)
	%write $(TARGET).lnk name $(TARGET)
	%write $(TARGET).lnk option map=$(TARGET).map
#	%write $(TARGET).lnk option eliminate
	%write $(TARGET).lnk $(OBJSTR) 
#	%write $(TARGET).lnk $(LIBSTR) 
	%write $(TARGET).lnk system $(SYSTEM)
	$(LD) @$(TARGET).lnk $(LFLAGS)
	del $(TARGET).lnk

.c.obj:
	$(CC) $< $(CFLAGS)

.cpp.obj:
	$(CP) $< $(CFLAGS)

.asm.obj:
	$(AS) $< $(AFLAGS)
	
# clean all
clean: .symbolic
	del *.$(O)
	del $(TARGET).exe
	del *.err