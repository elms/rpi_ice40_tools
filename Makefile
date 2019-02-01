
TARGET = rpi_ice40_config

.PHONY: all clean mrproper

all: $(TARGET)

clean:
	$(RM) $(TARGET)

mrproper: clean
	$(RM) -r bcm2835-1.58 lib include

$(TARGET): $(TARGET).c lib/libbcm2835.a
	$(CC) -o $@ $^ -std=gnu99 -lbcm2835 -g -I include -L lib

bcm2835-1.58:
	curl http://www.airspayce.com/mikem/bcm2835/bcm2835-1.58.tar.gz | tar xz

lib/libbcm2835.a: bcm2835-1.58
	cd bcm2835-1.58 && ./configure --prefix=$(CURDIR) && make install
