#include "spi.h"

//variables for a standerd connection. mode0,0 8 bits wide. transfer speed 500Khz
//static const char *spi_device = "/dev/spidev0.0";
uint8_t spi_mode = 0;
uint8_t spi_bits = 8;
uint32_t spi_speed = 500000;
uint16_t spi_delay = 0;


//prints error message and aborts. (exits)
void pabort(const char *s)
{
	perror(s);
	abort();
}

//this function sets spi mode
int spi_set_mode(int fd, int mode)
{
	int ret;
	/*
	 * spi mode
	 */
	spi_mode = mode;
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");
	return ret;
}
//this function sets word length (bits per word)
int spi_set_word(int fd, int bits)
{
	int ret;
	/*
	 * bits per word
	 */
	spi_bits = bits;
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");
	return ret;

}
//this function sets communication speed.
int spi_set_speed(int fd, int speed)
{
	int ret = 0;
	/*
	 * max speed hz
	 */
	spi_speed = speed;
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");
	return ret;
}
/*
 * how long to delay after the last bit transfer
 * before optionally deselecting the device before the next transfer.
 * */
void spi_set_delay(int delay)
{
	spi_delay = delay;
}

int spi_transfer32(int fd, volatile __u32 *tx, volatile __u32 *rx){
	struct spi_ioc_transfer tr = 
	{
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = SPI_BUFFER_SIZE,
		.delay_usecs = spi_delay,
		.speed_hz = spi_speed,
		.bits_per_word = spi_bits,
	};
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if(ret < 1)
		pabort("can't send spi message");
	return ret;
}
//closes device with descriptor fd.
int spiClose(int fd)
{
	//error checking?
	int ret = close(fd);
	return ret;
}
