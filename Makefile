
TARGET = rpi_ice40_config

$(TARGET): $(TARGET).c
	$(CC) -o $@ $^ -std=gnu99 -lbcm2835 -g
