// cc -lgpio -lm sh1106.c -o sh1106 && ./sh1106
// Datasheet: https://cdn.velleman.eu/downloads/29/infosheets/sh1106_datasheet.pdf
// Helped out by: https://github.com/8TN/Raspberry-Pi-SH1106-oled-display

#include <sys/types.h>

#include <fcntl.h>
#include <libgpio.h>
#include <sys/spigenio.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DC_SELECT 25 // Selection between data transfer and command transfer.
#define RESET 24

#define SPI_FREQ_HZ 1000000 // 7629
#define SPI_MODE 0b00       // XXX Idk what this does.

#define PATH "car.png"

static gpio_handle_t handle;
static int spi_fd;

static void xfer(bool data_mode, size_t size, uint8_t data[size]) {
	if (data_mode) {
		gpio_pin_high(handle, DC_SELECT);
	}

	else {
		gpio_pin_low(handle, DC_SELECT);
	}

	// clang-format off
	struct spigen_transfer xfer = {
		.st_command = {
			.iov_base = data,
			.iov_len = size,
		},
	};
	// clang-format on

	if (ioctl(spi_fd, SPIGENIOC_TRANSFER, &xfer) < 0) {
		err(EXIT_FAILURE, "ioctl(SPIGENIOC_TRANSFER): %s", strerror(errno));
	}
}

static void display(bool fb[64][128]) {
	for (size_t i = 0; i < 8; i++) {
		xfer(false, 3, (uint8_t[]) {0xB0 + (7 - i), 0x02, 0x10}); // Set page number, lower column address (to 0), and higher column address (to 0).
		uint8_t data[128];

		for (size_t x = 0; x < 128; x++) {
			data[127 - x] =
				fb[i * 8 + 0][x] << 7 |
				fb[i * 8 + 1][x] << 6 |
				fb[i * 8 + 2][x] << 5 |
				fb[i * 8 + 3][x] << 4 |
				fb[i * 8 + 4][x] << 3 |
				fb[i * 8 + 5][x] << 2 |
				fb[i * 8 + 6][x] << 1 |
				fb[i * 8 + 7][x];
		}

		xfer(true, 128, data);
	}
}

int main(void) {
	handle = gpio_open(0);

	if (handle == GPIO_INVALID_HANDLE) {
		err(EXIT_FAILURE, "gpio_open failed");
	}

	spi_fd = open("/dev/spigen0.0", 0);

	if (spi_fd < 0) {
		err(EXIT_FAILURE, "open: %s", strerror(errno));
	}

	printf("Setting up.\n");

	uint32_t const freq = SPI_FREQ_HZ;
	uint32_t const mode = SPI_MODE;

	if (ioctl(spi_fd, SPIGENIOC_SET_CLOCK_SPEED, &freq) < 0) {
		err(EXIT_FAILURE, "ioctl(SPIGENIOC_SET_CLOCK_SPEED): %s", strerror(errno));
	}

	if (ioctl(spi_fd, SPIGENIOC_SET_SPI_MODE, &mode) < 0) {
		err(EXIT_FAILURE, "ioctl(SPIGENIOC_SET_SPI_MODE): %s", strerror(errno));
	}

	gpio_pin_output(handle, DC_SELECT);
	gpio_pin_output(handle, RESET);

	printf("Resetting.\n");

	gpio_pin_low(handle, RESET);
	usleep(100000);
	gpio_pin_high(handle, RESET);
	usleep(100000);

	printf("Displaying.\n");

	xfer(false, 1, (uint8_t[]) {0xAF}); // Display on.

	int x_res, y_res, channels;
	uint8_t* const buf = stbi_load(PATH, &x_res, &y_res, &channels, STBI_grey);

	printf("Loaded image, %dx%dx%d\n", x_res, y_res, channels);

	assert(buf != NULL);
	assert(x_res == 128);
	assert(y_res == 64);

	bool fb[64][128] = {0};
	float t = 0;

	for (;;) {
		for (size_t y = 1; y < 64 - 1; y++) {
			for (size_t x = 0; x < 128; x++) {
				fb[y][/*(int) (sin(t) * 40) + */x] = buf[y * 128 + x] > 0xFF / 2;
			}
		}

		display(fb);
		t += 0.05;
	}

	printf("Done.\n");

	free(buf);
	close(spi_fd);
	gpio_close(handle);

	return EXIT_SUCCESS;
}
