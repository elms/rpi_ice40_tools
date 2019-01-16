#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bcm2835.h"

#define ICE40_SS_B_1      RPI_BPLUS_GPIO_J8_24
#define ICE40_CRESET_B_1  RPI_BPLUS_GPIO_J8_11
#define ICE40_CDONE_1     RPI_BPLUS_GPIO_J8_07

#define ICE40_SS_B_2      RPI_BPLUS_GPIO_J8_26
#define ICE40_CRESET_B_2  RPI_BPLUS_GPIO_J8_13
#define ICE40_CDONE_2     RPI_BPLUS_GPIO_J8_15

#define ICE40_EN_FLASH_POWER  RPI_BPLUS_GPIO_J8_31
#define ICE40_SEL_SPI_MUX     RPI_BPLUS_GPIO_J8_29

#define CDONE_POLL      100

// TN1248 - iCE40 Programming and Configuration specifies 49 SCLKs
// after full bin file. (n+7)/8 to round up in bytes.
#define DUMMY_PAD       ((49 + 7)/8)

void ERR(char* msg) {
  fprintf(stderr," %s\n", msg);
  exit(1);
}

/**
 * options:
 * bit bang (instead of native SPI mode)
 * speed (allow slower load)
 *
 * ICE1 SRAM - Hold ICE2 in reset, switch SPI_MUX low, flash power EN low
 * ICE2 SRAM - Hold ICE1 in reset, switch SPI_MUX X, flash power EN low,  use CS1
 * FLASH - hold ICE1 and ICE2 in reset, switch SPI_MUX high, flash power EN high and CS0
 * * hex and bin file inputs?
 */

typedef enum {
  MODE_SPI_NATIVE,
  MODE_SPI_BITBANG,
} spi_mode_t;

typedef enum {
  TARGET_NONE,
  TARGET_ICE1,
  TARGET_ICE2,
  TARGET_FLASH,
} config_target_t;

typedef struct {
  int speed;
  spi_mode_t mode;
  config_target_t target;
  char* bit_fname;
} options_t;

options_t parse_args(int argc, char* const* argv) {
  int opt;
  options_t option = {
    1,
    MODE_SPI_NATIVE,
    TARGET_NONE,
    "",
  };

  while (-1 != (opt = getopt(argc, argv, "bs:12f"))) {
    switch(opt) {
    case 'b':
      option.mode = MODE_SPI_BITBANG;
      break;
    case 's':
      option.speed = atoi(optarg);
      break;
    case '1':
      if (option.target != TARGET_NONE) ERR("Can only set 1,2, or f target options");
      option.target = TARGET_ICE1;
      break;
    case '2':
      if (option.target != TARGET_NONE) ERR("Can only set 1,2, or f target options");
      option.target = TARGET_ICE2;
      break;
    case 'f':
      if (option.target != TARGET_NONE) ERR("Can only set 1,2, or f target options");
      option.target = TARGET_FLASH;
      break;
    default:
      ERR("unknown option");
      break;
    }
  }

  option.bit_fname = argv[optind];

  return option;
}

int main(int argc, char* const* argv) {
  int ret = 0;

  options_t opts = parse_args(argc, argv);

  int cs = BCM2835_SPI_CS0;
  int ss = ICE40_SS_B_1;
  int reset = ICE40_CRESET_B_1;
  int done = ICE40_CDONE_1;

  if (opts.target == TARGET_ICE2) {
    cs = BCM2835_SPI_CS1;
    ss = ICE40_SS_B_2;
    reset = ICE40_CRESET_B_2;
    done = ICE40_CDONE_2;
  }

  FILE* fbitmap = fopen(opts.bit_fname, "r");
  if (NULL == fbitmap) {
    printf("failed to open %s\n", opts.bit_fname);
    return 1;
  }

  struct stat bit_stat;
  stat(opts.bit_fname, &bit_stat);
  size_t bit_size = bit_stat.st_size;
  printf("bitmap size: %d\n", bit_size);

  uint8_t* bitmap = calloc(bit_size + DUMMY_PAD, 1);
  fread(bitmap, bit_size, 1, fbitmap);

  ret = bcm2835_init();
  if (1 != ret) {
    ERR("failed to init");
  }

  // Reset ICE into SPI peripheral mode
  bcm2835_gpio_fsel(ICE40_CRESET_B_1, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(ICE40_CDONE_1, BCM2835_GPIO_FSEL_INPT);
  bcm2835_gpio_fsel(ICE40_SS_B_1, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(ICE40_CRESET_B_2, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(ICE40_CDONE_2, BCM2835_GPIO_FSEL_INPT);
  bcm2835_gpio_fsel(ICE40_SS_B_2, BCM2835_GPIO_FSEL_OUTP);

  bcm2835_gpio_fsel(ICE40_EN_FLASH_POWER, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(ICE40_SEL_SPI_MUX, BCM2835_GPIO_FSEL_OUTP);

  bcm2835_gpio_write(ICE40_CRESET_B_1, LOW);
  bcm2835_gpio_write(ICE40_CRESET_B_2, LOW);
  bcm2835_delay(10);

  if (opts.target == TARGET_ICE1 || opts.target == TARGET_ICE2) {
    bcm2835_gpio_write(ICE40_EN_FLASH_POWER, LOW);
  } else {
    bcm2835_gpio_write(ICE40_EN_FLASH_POWER, HIGH);
  }

  if (opts.target == TARGET_ICE1) {
    bcm2835_gpio_write(ICE40_SEL_SPI_MUX, LOW);
  } else {
    bcm2835_gpio_write(ICE40_SEL_SPI_MUX, HIGH);
  }

  if (opts.target == TARGET_ICE1 || opts.target == TARGET_ICE2) {
    for (int ii=0; ii< CDONE_POLL; ii++) {
      if (bcm2835_gpio_lev(done) == LOW) {
	break;
      }
    }

    if (bcm2835_gpio_lev(done) == HIGH) {
      ERR("timeout waiting for CDONE");
    }
  }

  bcm2835_gpio_write(ss, LOW);
  bcm2835_delay(10);
  bcm2835_gpio_write(reset, HIGH);
  bcm2835_delay(200);
  
  ret  = bcm2835_spi_begin();
  if (1 != ret) {
    ERR("SPI failed to init");
  }

  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE2);
  bcm2835_spi_chipSelect(cs);
  bcm2835_spi_setChipSelectPolarity(cs, LOW);

  
  bcm2835_spi_writenb(bitmap, bit_size + DUMMY_PAD);

  bcm2835_spi_end();

  for (int ii=0; ii< CDONE_POLL; ii++) {
    if (bcm2835_gpio_lev(done) == HIGH) {
      break;
    }
  }
  if (bcm2835_gpio_lev(done) == LOW) {
    ERR("Configuration error. Timeout waiting for CDONE");
  }

}
