#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "bcm2835.h"

#define ICE40_SS_B      RPI_BPLUS_GPIO_J8_24
#define ICE40_CRESET_B  RPI_BPLUS_GPIO_J8_11
#define ICE40_CDONE     RPI_BPLUS_GPIO_J8_07

#define CDONE_POLL      512
#define DUMMY_PAD       ((49 + 7)/8)

void ERR(char* msg) {
  fprintf(stderr," %s\n", msg);
  exit(1);
}

int main(int argc, char** argv) {
  int ret = 0;

  ret = bcm2835_init();
  if (1 != ret) {
    ERR("failed to init");
  }


  // Reset ICE into SPI peripheral mode
  bcm2835_gpio_fsel(ICE40_CRESET_B, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(ICE40_CDONE, BCM2835_GPIO_FSEL_INPT);
  bcm2835_gpio_fsel(ICE40_SS_B, BCM2835_GPIO_FSEL_OUTP);

  bcm2835_gpio_write(ICE40_CRESET_B, LOW);
  bcm2835_delay(10);

  for (int ii=0; ii< CDONE_POLL; ii++) {
    if (bcm2835_gpio_lev(ICE40_CDONE) == LOW) {
      break;
    }
  }
  if (bcm2835_gpio_lev(ICE40_CDONE) == HIGH) {
    ERR("timeout waiting for CDONE");
  }
    
  bcm2835_gpio_write(ICE40_SS_B, LOW);
  bcm2835_delay(10);
  bcm2835_gpio_write(ICE40_CRESET_B, HIGH);
  bcm2835_delay(200);
  
  ret  = bcm2835_spi_begin();
  if (1 != ret) {
    ERR("SPI failed to init");
  }

  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE2);
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

  if (argc<2)
    ERR("no file given");

  FILE* fbitmap = fopen(argv[1], "r");
  if (NULL == fbitmap) {
    printf("failed to open %s\n", argv[1]);
    return 1;
  }

  struct stat bit_stat;
  stat(argv[1], &bit_stat);
  size_t bit_size = bit_stat.st_size;
  printf("bitmap size: %d\n", bit_size);

  uint8_t* bitmap = calloc(bit_size + DUMMY_PAD, 1);
  fread(bitmap, bit_size, 1, fbitmap);
  
  bcm2835_spi_writenb(bitmap, bit_size + DUMMY_PAD);

  bcm2835_spi_end();

  for (int ii=0; ii< CDONE_POLL; ii++) {
    if (bcm2835_gpio_lev(ICE40_CDONE) == HIGH) {
      break;
    }
  }
  if (bcm2835_gpio_lev(ICE40_CDONE) == LOW) {
    ERR("Configuration error. Timeout waiting for CDONE");
  }

}
