TARGET=../bin/uc
CPPFLAGS+=-Wall -O2
LDLIBS+=-pthread
#LDLIBS+=-lws2_32 # for mingw

all: $(TARGET)

$(TARGET): uc.o ../mingw_net.o
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@
ifdef ANDROID_NDK_ROOT
	termux-elf-cleaner --api-level 23 $(TARGET)
endif	

clean:
	rm -f $(TARGET) *.o ../mingw_net.o
