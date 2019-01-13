/*
   gcc -o minimal_clk minimal_clk.c
   sudo ./minimal_clk x.xx [options]
*/

/*
minimal_clock.c
2014-05-20
Public Domain
*/

/*

This allows the setting of the three general purpose clocks.

The clocks are named GPCLK0, GPCLK1, and GPCLK2.

The clocks are accessible from the following gpios.

gpio4  GPCLK0 ALT0
gpio5  GPCLK1 ALT0 B+ and compute module only (reserved for system use)
gpio6  GPCLK2 ALT0 B+ and compute module only
gpio20 GPCLK0 ALT5 B+ and compute module only
gpio21 GPCLK1 ALT5 Not available on Rev.2 B (reserved for system use)

gpio32 GPCLK0 ALT0 Compute module only
gpio34 GPCLK0 ALT0 Compute module only
gpio42 GPCLK1 ALT0 Compute module only (reserved for system use)
gpio43 GPCLK2 ALT0 Compute module only
gpio44 GPCLK1 ALT0 Compute module only (reserved for system use)

Clock sources

0     0 Hz     Ground
1     19.2 MHz oscillator 
2     0 Hz     testdebug0
3     0 Hz     testdebug1
4     0 Hz     PLLA
5     1000 MHz PLLC (changes with overclock settings)
6     500 MHz  PLLD
7     216 MHz  HDMI auxiliary
8-15  0 Hz     Ground

The integer divider may be 2-4095.
The fractional divider may be 0-4095.

There is no 25MHz cap for using non-zero MASH
(multi-stage noise shaping) values.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static volatile uint32_t piModel = 1;

static volatile uint32_t piPeriphBase = 0x20000000;
static volatile uint32_t piBusAddr = 0x40000000;

#define SYST_BASE  (piPeriphBase + 0x003000)
#define DMA_BASE   (piPeriphBase + 0x007000)
#define CLK_BASE   (piPeriphBase + 0x101000)
#define GPIO_BASE  (piPeriphBase + 0x200000)
#define UART0_BASE (piPeriphBase + 0x201000)
#define PCM_BASE   (piPeriphBase + 0x203000)
#define SPI0_BASE  (piPeriphBase + 0x204000)
#define I2C0_BASE  (piPeriphBase + 0x205000)
#define PWM_BASE   (piPeriphBase + 0x20C000)
#define BSCS_BASE  (piPeriphBase + 0x214000)
#define UART1_BASE (piPeriphBase + 0x215000)
#define I2C1_BASE  (piPeriphBase + 0x804000)
#define I2C2_BASE  (piPeriphBase + 0x805000)
#define DMA15_BASE (piPeriphBase + 0xE05000)

#define DMA_LEN   0x1000 /* allow access to all channels */
#define CLK_LEN   0xA8
#define GPIO_LEN  0xB4
#define SYST_LEN  0x1C
#define PCM_LEN   0x24
#define PWM_LEN   0x28
#define I2C_LEN   0x1C

#define GPSET0 7
#define GPSET1 8

#define GPCLR0 10
#define GPCLR1 11

#define GPLEV0 13
#define GPLEV1 14

#define GPPUD     37
#define GPPUDCLK0 38
#define GPPUDCLK1 39

#define SYST_CS  0
#define SYST_CLO 1
#define SYST_CHI 2

#define CLK_PASSWD  (0x5A<<24)

#define CLK_CTL_MASH(x)((x)<<9)
#define CLK_CTL_BUSY    (1 <<7)
#define CLK_CTL_KILL    (1 <<5)
#define CLK_CTL_ENAB    (1 <<4)
#define CLK_CTL_SRC(x) ((x)<<0)

#define CLK_SRCS 4

#define CLK_CTL_SRC_OSC  1  /* 19.2 MHz */
#define CLK_CTL_SRC_PLLC 5  /* 1000 MHz */
#define CLK_CTL_SRC_PLLD 6  /*  500 MHz */
#define CLK_CTL_SRC_HDMI 7  /*  216 MHz */

#define CLK_DIV_DIVI(x) ((x)<<12)
#define CLK_DIV_DIVF(x) ((x)<< 0)

#define CLK_GP0_CTL 28
#define CLK_GP0_DIV 29
#define CLK_GP1_CTL 30
#define CLK_GP1_DIV 31
#define CLK_GP2_CTL 32
#define CLK_GP2_DIV 33

#define CLK_PCM_CTL 38
#define CLK_PCM_DIV 39

#define CLK_PWM_CTL 40
#define CLK_PWM_DIV 41


static volatile uint32_t  *gpioReg = MAP_FAILED;
static volatile uint32_t  *systReg = MAP_FAILED;
static volatile uint32_t  *clkReg  = MAP_FAILED;

#define PI_BANK (gpio>>5)
#define PI_BIT  (1<<(gpio&0x1F))

/* gpio modes. */

#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

void gpioSetMode(unsigned gpio, unsigned mode)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);
}

int gpioGetMode(unsigned gpio)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   return (*(gpioReg + reg) >> shift) & 7;
}

/* Values for pull-ups/downs off, pull-down and pull-up. */

#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

void gpioSetPullUpDown(unsigned gpio, unsigned pud)
{
   *(gpioReg + GPPUD) = pud;

   usleep(20);

   *(gpioReg + GPPUDCLK0 + PI_BANK) = PI_BIT;

   usleep(20);
  
   *(gpioReg + GPPUD) = 0;

   *(gpioReg + GPPUDCLK0 + PI_BANK) = 0;
}

int gpioRead(unsigned gpio)
{
   if ((*(gpioReg + GPLEV0 + PI_BANK) & PI_BIT) != 0) return 1;
   else                                         return 0;
}

void gpioWrite(unsigned gpio, unsigned level)
{
   if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}

void gpioTrigger(unsigned gpio, unsigned pulseLen, unsigned level)
{
   if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;

   usleep(pulseLen);

   if (level != 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}

/* Bit (1<<x) will be set if gpio x is high. */

uint32_t gpioReadBank1(void) { return (*(gpioReg + GPLEV0)); }
uint32_t gpioReadBank2(void) { return (*(gpioReg + GPLEV1)); }

/* To clear gpio x bit or in (1<<x). */

void gpioClearBank1(uint32_t bits) { *(gpioReg + GPCLR0) = bits; }
void gpioClearBank2(uint32_t bits) { *(gpioReg + GPCLR1) = bits; }

/* To set gpio x bit or in (1<<x). */

void gpioSetBank1(uint32_t bits) { *(gpioReg + GPSET0) = bits; }
void gpioSetBank2(uint32_t bits) { *(gpioReg + GPSET1) = bits; }

unsigned gpioHardwareRevision(void)
{
   static unsigned rev = 0;

   FILE * filp;
   char buf[512];
   char term;
   int chars=4; /* number of chars in revision string */

   if (rev) return rev;

   piModel = 0;

   filp = fopen ("/proc/cpuinfo", "r");

   if (filp != NULL)
   {
      while (fgets(buf, sizeof(buf), filp) != NULL)
      {
         if (piModel == 0)
         {
            if (!strncasecmp("model name", buf, 10))
            {
               if (strstr (buf, "ARMv6") != NULL)
               {
                  piModel = 1;
                  chars = 4;
                  piPeriphBase = 0x20000000;
                  piBusAddr = 0x40000000;
               }
               else if (strstr (buf, "ARMv7") != NULL)
               {
                  piModel = 2;
                  chars = 6;
                  piPeriphBase = 0x3F000000;
                  piBusAddr = 0xC0000000;
               }
            }
         }

         if (!strncasecmp("revision", buf, 8))
         {
            if (sscanf(buf+strlen(buf)-(chars+1),
               "%x%c", &rev, &term) == 2)
            {
               if (term != '\n') rev = 0;
            }
         }
      }

      fclose(filp);
   }
   return rev;
}

/* Returns the number of microseconds after system boot. Wraps around
   after 1 hour 11 minutes 35 seconds.
*/

uint32_t gpioTick(void) { return systReg[SYST_CLO]; }

static int initClock(int clock, int source, int divI, int divF, int MASH)
{
   int ctl[] = {CLK_GP0_CTL, CLK_GP2_CTL};
   int div[] = {CLK_GP0_DIV, CLK_GP2_DIV};
   int src[CLK_SRCS] =
      {CLK_CTL_SRC_PLLD,
       CLK_CTL_SRC_OSC,
       CLK_CTL_SRC_HDMI,
       CLK_CTL_SRC_PLLC};

   int clkCtl, clkDiv, clkSrc;
   uint32_t setting;

   if ((clock  < 0) || (clock  > 1))    return -1;
   if ((source < 0) || (source > 3 ))   return -2;
   if ((divI   < 2) || (divI   > 4095)) return -3;
   if ((divF   < 0) || (divF   > 4095)) return -4;
   if ((MASH   < 0) || (MASH   > 3))    return -5;

   clkCtl = ctl[clock];
   clkDiv = div[clock];
   clkSrc = src[source];

   clkReg[clkCtl] = CLK_PASSWD | CLK_CTL_KILL;

   /* wait for clock to stop */

   while (clkReg[clkCtl] & CLK_CTL_BUSY)
   {
      usleep(10);
   }

   clkReg[clkDiv] =
      (CLK_PASSWD | CLK_DIV_DIVI(divI) | CLK_DIV_DIVF(divF));

   usleep(10);

   clkReg[clkCtl] =
      (CLK_PASSWD | CLK_CTL_MASH(MASH) | CLK_CTL_SRC(clkSrc));

   usleep(10);

   clkReg[clkCtl] |= (CLK_PASSWD | CLK_CTL_ENAB);
}

static int termClock(int clock)
{
   int ctl[] = {CLK_GP0_CTL, CLK_GP2_CTL};

   int clkCtl;

   if ((clock  < 0) || (clock  > 1))    return -1;

   clkCtl = ctl[clock];

   clkReg[clkCtl] = CLK_PASSWD | CLK_CTL_KILL;

   /* wait for clock to stop */

   while (clkReg[clkCtl] & CLK_CTL_BUSY)
   {
      usleep(10);
   }
}


/* Map in registers. */

static uint32_t * initMapMem(int fd, uint32_t addr, uint32_t len)
{
    return (uint32_t *) mmap(0, len,
       PROT_READ|PROT_WRITE|PROT_EXEC,
       MAP_SHARED|MAP_LOCKED,
       fd, addr);
}

int gpioInitialise(void)
{
   int fd;

   gpioHardwareRevision(); /* sets piModel, needed for peripherals address */

   fd = open("/dev/mem", O_RDWR | O_SYNC) ;

   if (fd < 0)
   {
      fprintf(stderr,
         "This program needs root privileges.  Try using sudo\n");
      return -1;
   }

   gpioReg  = initMapMem(fd, GPIO_BASE, GPIO_LEN);
   systReg  = initMapMem(fd, SYST_BASE, SYST_LEN);
   clkReg   = initMapMem(fd, CLK_BASE,  CLK_LEN);

   close(fd);

   if ((gpioReg == MAP_FAILED) ||
       (systReg == MAP_FAILED) ||
       (clkReg == MAP_FAILED))
   {
      fprintf(stderr,
         "Bad, mmap failed\n");
      return -1;
   }
   return 0;
}

/* ------------------------------------------------------------------------ */

#define PLLD_FREQ 500e6

int set_clk(double freq){//xx.ffMhz
   double freq, div, f;
//   double cfreq[CLK_SRCS]={500e6, 19.2e6, 216e6, 1000e6};
   char *clocks[CLK_SRCS]={"PLLD", " OSC", "HDMI", "PLLC"};
   int divI, divF;

   /* command line parameters */

  freq *= 1000.0;
  if ((freq < 4687.5) || (freq > PLLD_FREQ)) {return(-1);}
  
  div = PLLD_FREQ / freq;
  divI = div;
  divF = (div-divI) * 4096;

//  for (i=0; i<CLK_SRCS; i++){
  printf("%s: %4d %4d ", clocks[0], divI, divF);
  if ((divI<2) || (divI>4095)) 
    printf("ILLEGAL\n");
    return -1;
  else{
    div = divI + (divF / 4096);
    f = PLLD_FREQ/div;
    printf("%7.2f MHz\n", f/1000000.0);
  }

  if (gpioInitialise() < 0) return 1;
  initClock(opt_c, opt_s, divI[opt_s], divF[opt_s], opt_m);
  gpioSetMode(4, PI_ALT0);
  return 0;
}

