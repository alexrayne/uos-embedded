/*
 * Driver for MELT MT-12864J LCD (Samsung KS0108B).
 */
#include <runtime/lib.h>
#include <stream/stream.h>
#include <gpanel/gpanel.h>

/* Флаги состояния LCD */
enum {
	BUSY	= 7,
	ONOFF	= 5,
	RESET	= 4,
};
#ifdef ARM_1986BE9
#define PORTC_WE		2
#define PORTC_CLOCK		7
#define PORTC_LCD_RST	9

#define PORTE_E1		4	/* E1 */
#define PORTE_E2		5	/* E2 */
#define PORTE_A0		11	/* A0 */

#define LCD_CMD1		0x10100000
#define LCD_CMD2		0x10200000

#define LCD_DATA1		0x18100000
#define LCD_DATA2		0x18200000

#endif

#ifdef ARM_1986BE1
#define PORTC_WE		0
#define PORTC_CLOCK		2

#define PORTE_E1		13	/* PE13 - E1 */
#define PORTE_E2		14	/* PE14 - E2 */
#define PORTE_A0		12	/* PE12 - A0 */

#define LCD_CMD1		0x68000000
#define LCD_CMD2		0x70000000

#define LCD_DATA1		0x6C000000
#define LCD_DATA2		0x74000000
#endif

static void set_crystal(gpanel_t *gp, int num) {
	if (num & 64) {
		/* Кристалл #2. */
		gp->DATA = (unsigned*) LCD_DATA2;
		gp->CMD = (unsigned*) LCD_CMD2;
#ifdef ARM_1986BE9
		ARM_GPIOE->DATA &= ~(1 << PORTE_E1 | 1 << PORTE_A0);
		ARM_GPIOE->DATA |= 1 << PORTE_E2;
#else
		ARM_GPIOE->CLRTX = (1 << PORTE_E1 | 1 << PORTE_A0);
		ARM_GPIOE->SETTX = 1 << PORTE_E2;
#endif
	} else {
		/* Кристалл #1. */
		gp->DATA = (unsigned*) LCD_DATA1;
		gp->CMD = (unsigned*) LCD_CMD1;
#ifdef ARM_1986BE9
		ARM_GPIOE->DATA &= ~(1 << PORTE_E2 | 1 << PORTE_A0);
		ARM_GPIOE->DATA |= 1 << PORTE_E1;
#else
		ARM_GPIOE->CLRTX = (1 << PORTE_E2 | 1 << PORTE_A0);
		ARM_GPIOE->SETTX = 1 << PORTE_E1;
#endif
	}
	udelay(8);
}

static void wait_status(gpanel_t *gp, int status) {
	unsigned stat;

	for (;;) {
		udelay(8);
		stat = *gp->CMD;
		udelay(8);
		if (stat != (1 << status))
			return;
	}
}

/*
 * No backlight control.
 */
void gpanel_backlight(gpanel_t *gp, int on) {
}

/*
 * No contrast control.
 */
void gpanel_contrast(gpanel_t *gp, int contrast) {
}

/*
 * Set up hardware for communication to Nokia 6100 LCD Display.
 * Internally, the controller has 132x132 pixels.
 * But visible area is only 130x130.
 */
void gpanel_init(gpanel_t *gp, const gpanel_font_t *font) {
	extern stream_interface_t gpanel_interface;
	int x;

	gp->interface = &gpanel_interface;
	gp->nrow = 63;
	gp->ncol = 127;
	gp->font = font;
	gp->foreground = GPANEL_WHITE;
	gp->background = GPANEL_BLACK;
	gp->contrast = 0x38;
	gp->row = 0;
	gp->col = 0;
	gp->c1 = 0;
	gp->c2 = 0;

	/* Enable clock for PORTA, PORTC, PORTE, external bus. */
	ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_GPIOA |
	ARM_PER_CLOCK_GPIOC | ARM_PER_CLOCK_GPIOE |
	ARM_PER_CLOCK_EXT_BUS;

	/* Инициализация портов внешней шины и выводов для работы с экраном */
	/* Main Function для DATA[7:0] */
	ARM_GPIOA->FUNC = (ARM_GPIOA->FUNC & ~0xFFFF)
			| ARM_FUNC_MAIN(0) | ARM_FUNC_MAIN(1) |
			ARM_FUNC_MAIN(2) | ARM_FUNC_MAIN(3) |
			ARM_FUNC_MAIN(4) | ARM_FUNC_MAIN(5) |
			ARM_FUNC_MAIN(6) | ARM_FUNC_MAIN(7);
	ARM_GPIOA->ANALOG |= 0xFF; /* Digital */
	/* Fast */
	ARM_GPIOA->PWR = (ARM_GPIOA->PWR & ~0xFFFF)
			| ARM_PWR_FAST(0) | ARM_PWR_FAST(1) |
			ARM_PWR_FAST(2) | ARM_PWR_FAST(3) |
			ARM_PWR_FAST(4) | ARM_PWR_FAST(5) |
			ARM_PWR_FAST(6) | ARM_PWR_FAST(7);

	/* Alternate Function для E1, E2, A0 */
	ARM_GPIOE->FUNC = (ARM_GPIOE->FUNC
			& ~(ARM_FUNC_MASK(PORTE_A0) | ARM_FUNC_MASK(PORTE_E1)
					| ARM_FUNC_MASK(PORTE_E2))) | ARM_FUNC_ALT(PORTE_A0) |
	ARM_FUNC_PORT (PORTE_E1) |
	ARM_FUNC_PORT (PORTE_E2);
	/* Digital */
	ARM_GPIOE->ANALOG |= (1 << PORTE_A0) | (1 << PORTE_E1) | (1 << PORTE_E2);
	/* Fast */
	ARM_GPIOE->PWR = (ARM_GPIOE->PWR
			& ~(ARM_PWR_MASK(PORTE_A0) | ARM_PWR_MASK(PORTE_E1)
					| ARM_PWR_MASK(PORTE_E2))) | ARM_PWR_FAST(PORTE_A0) |
	ARM_PWR_FAST (PORTE_E1) | ARM_PWR_FAST (PORTE_E2);

	ARM_GPIOC->FUNC = (ARM_GPIOC->FUNC & /* Main Function для WE, CLOCK */
	~(ARM_FUNC_MASK (PORTC_WE) | ARM_FUNC_MASK(PORTC_CLOCK)
#ifdef ARM_1986BE9
			| ARM_FUNC_MASK (PORTC_LCD_RST)
#endif
			)) | ARM_FUNC_MAIN(PORTC_WE) | ARM_FUNC_ALT(PORTC_CLOCK)
#ifdef ARM_1986BE9
			| ARM_FUNC_PORT (PORTC_LCD_RST)
#endif
			;
	ARM_GPIOC->ANALOG |= (1 << PORTC_WE) | /* Digital */
	(1 << PORTC_CLOCK)
#ifdef ARM_1986BE9
			| (1 << PORTC_LCD_RST)
#endif
			;
	ARM_GPIOC->PWR = (ARM_GPIOC->PWR & /* Fast */
	~(ARM_PWR_MASK (PORTC_WE) | ARM_PWR_MASK(PORTC_CLOCK)
#ifdef ARM_1986BE9
			| ARM_PWR_MASK(PORTC_LCD_RST)
#endif
			)) | ARM_PWR_SLOW(PORTC_WE) | ARM_PWR_FASTEST(PORTC_CLOCK)
#ifdef ARM_1986BE9
			| ARM_PWR_FASTEST(PORTC_LCD_RST)
#endif
			;

	/* Включение внешней шины адрес/данные в режиме ROM.
	 * Длительность цикла на шине равна 18 тактам (15 wait states). */
	if (ARM_EXTBUS->CONTROL & ARM_EXTBUS_RAM)
		ARM_EXTBUS->CONTROL |= ARM_EXTBUS_WS(15);
	else
		ARM_EXTBUS->CONTROL = ARM_EXTBUS_ROM | ARM_EXTBUS_WS(15);
#ifdef ARM_1986BE1
	ARM_EXTBUS->RAM_Cycles3 =
			ARM_EXTBUS_CYCLES_ENABLE_TUNE
					| ARM_EXTBUS_CYCLES_WS_ACTIVE(
							255) | ARM_EXTBUS_CYCLES_WS_SETUP(7) | ARM_EXTBUS_CYCLES_WS_HOLD(7);
	ARM_EXTBUS->RAM_Cycles4 =
			ARM_EXTBUS_CYCLES_ENABLE_TUNE
					| ARM_EXTBUS_CYCLES_WS_ACTIVE(
							255) | ARM_EXTBUS_CYCLES_WS_SETUP(7) | ARM_EXTBUS_CYCLES_WS_HOLD(7);
#endif
#ifdef ARM_1986BE9
	/* Программный сброс экрана. */
	ARM_GPIOC->DATA |= 1 << PORTC_LCD_RST;
	ARM_GPIOC->OE |= 1 << PORTC_LCD_RST;
	for (x = 0; x < 255; x++)
	ARM_GPIOC->DATA &= ~(1 << PORTC_LCD_RST);
	ARM_GPIOC->DATA |= 1 << PORTC_LCD_RST;
#endif

	/* Инициализация всех кристаллов. */
	ARM_GPIOE->OE |= 1 << PORTE_E1 | 1 << PORTE_E2;
	for (x = 0; x < gp->ncol; x += 64) {
		set_crystal(gp, x);
		wait_status(gp, BUSY);
		//for (j = 0; j < TEMP; j++) {
		*gp->CMD = 0x3F;		// LCD on
		udelay(8);
		//}
		wait_status(gp, ONOFF);

		//for (j = 0; j < TEMP; j++) {
		*gp->CMD = 0xC0;		// start line 0
		udelay(8);
		//}
	}
#ifdef ARM_1986BE9
    ARM_GPIOE->DATA |= (1 << PORTE_E2) | (1 << PORTE_E1);
#else
	ARM_GPIOE->SETTX = (1 << PORTE_E2) | (1 << PORTE_E1);
#endif
	/*	gpanel_clear(gp, 1);*/
}

/*
 * Fill the LCD screen with a given color: black or white.
 */
void gpanel_clear(gpanel_t *gp, unsigned color) {
	unsigned x, i, j;

	if (color)
		color = 0xFF;
	else
		color = 0;

	/* Очистка данных для всех кристаллов */
	for (x = 0; x < gp->ncol; x += 64) {
		set_crystal(gp, x);
		wait_status(gp, BUSY);
		udelay(8);

		*gp->CMD = 0x3E;		// LCD off
		udelay(8);

		for (i = 0; i < 8; i++) {
			*gp->CMD = 0xB8 + i;	// set page
			udelay(8);

			*gp->CMD = 0x40;	// set address 0
			udelay(8);

			for (j = 0; j < 64; j++) {
				*gp->DATA = color;
				udelay(8);
			}
		}
		*gp->CMD = 0x3F;		// LCD on
		udelay(8);
		wait_status(gp, ONOFF);
	}
#ifdef ARM_1986BE9
	ARM_GPIOE->DATA |= (1 << PORTE_E2) | (1 << PORTE_E1);
#else
    ARM_GPIOE->SETTX = (1 << PORTE_E2) | (1 << PORTE_E1);
#endif
	gp->row = 0;
	gp->col = 0;
}

/*
 * Lights a single pixel in the specified color
 * at the specified x and y addresses
 */
void gpanel_pixel(gpanel_t *gp, int x, int y, int color) {
	unsigned data;
	if (x >= gp->ncol || y >= gp->nrow)
		return;

	set_crystal(gp, x);
	x &= 63;

	*gp->CMD = 0xB8 + (y >> 3);	// set page
	udelay(8);

	*gp->CMD = 0x40 + x;		// set address
	udelay(8);

	/* Первое чтение - необходимо для получения корректных данных */
	*(gp->DATA);
	udelay(8);
	data = *gp->DATA;
	udelay(8);

	if (color)
		data |= 1 << (y & 7);
	else
		data &= ~(1 << (y & 7));

	*gp->CMD = 0x40 + x;		// set address
	udelay(8);

	*gp->DATA = (unsigned char) data;
	udelay(8);
#ifdef ARM_1986BE9
    ARM_GPIOE->DATA |= (1 << PORTE_E2) | (1 << PORTE_E1);
#else
	ARM_GPIOE->SETTX = (1 << PORTE_E2) | (1 << PORTE_E1);
#endif
}

/*
 * Draw a part of glyph, up to 8 pixels in height.
 */
static void graw_glyph8(gpanel_t *gp, unsigned width, unsigned height,
		const unsigned short *bits, unsigned ypage, unsigned yoffset) {
	unsigned char data[32];
	unsigned i, k, x, crystal = -1;
	if (height > 8 - yoffset)
		height = 8 - yoffset;
	if (width > sizeof(data))
		width = sizeof(data);
	/*debug_printf ("glyph8 %ux%u at %u-%u offset %u-%u\n", width, height, gp->col, gp->row, ypage, yoffset);*/

	/* Read graphics memory. */
	if (yoffset != 0 || height != 8) {
		x = gp->col;
		for (i = 0; i < width; i++, x++) {
			if (crystal != (x & 64)) {
				crystal = (x & 64);
				set_crystal(gp, crystal);

				*gp->CMD = 0xB8 + ypage;	// set page
				udelay(8);

				*gp->CMD = 0x40 + (x & 63);	// set address
				udelay(8);

				/* Первое чтение - необходимо для получения корректных данных */
				*(gp->DATA);
				udelay(8);
			}
			data[i] = *gp->DATA;
			udelay(8);

		}

		/* Clear glyph background. */
		unsigned mask = ~(((1 << height) - 1) << yoffset);
		for (i = 0; i < width; i++)
			data[i] &= mask;
	} else
		memset(data, 0, width);

	/* Place glyph image. */
	unsigned words_per_row = (width + 15) / 16;
	for (i = 0; i < width; i++) {
		for (k = 0; k < height; k++) {
			if (bits[k * words_per_row + i / 16] & (0x8000 >> (i & 15))) {
				data[i] |= 1 << (k + yoffset);
			}
		}
	}

	/* Write graphics memory. */
	x = gp->col;
	for (i = 0; i < width; i++, x++) {
		if (crystal != (x & 64)) {
			crystal = (x & 64);
			set_crystal(gp, crystal);

			*gp->CMD = 0xB8 + ypage;	// set page
			udelay(8);

			*gp->CMD = 0x40 + (x & 63);	// set address
			udelay(8);

		} else if (i == 0) {
			*gp->CMD = 0x40 + (x & 63);	// set address
			udelay(8);
		}

		*gp->DATA = data[i];
		udelay(8);
	}
#ifdef ARM_1986BE9
    ARM_GPIOE->DATA |= (1 << PORTE_E2) | (1 << PORTE_E1);
#else
	ARM_GPIOE->SETTX = (1 << PORTE_E2) | (1 << PORTE_E1);
#endif
}

/*
 * Draw a glyph of one symbol.
 */
void gpanel_glyph(gpanel_t *gp, unsigned width, const unsigned short *bits) {
	unsigned ypage = gp->row >> 3;
	unsigned yoffset = gp->row & 7;
	unsigned words_per_row = (width + 15) / 16;
	int height = gp->font->height;

	/*debug_printf ("<glyph %d at %d-%d>", width, gp->col, gp->row);*/
	for (;;) {
		graw_glyph8(gp, width, height, bits, ypage, yoffset);
		height -= 8 - yoffset;
		if (height <= 0)
			break;
		ypage++;
		if (ypage >= 8)
			break;
		bits += (8 - yoffset) * words_per_row;
		yoffset = 0;
	}
}

/*
 * Draw a filled rectangle in the specified color from (x1,y1) to (x2,y2).
 *
 * The best way to fill a rectangle is to take advantage of the "wrap-around" featute
 * built into the Philips PCF8833 controller. By defining a drawing box, the memory can
 * be simply filled by successive memory writes until all pixels have been illuminated.
 */
void gpanel_rect_filled(gpanel_t *gp, int x0, int y0, int x1, int y1, int color) {
	/* Temporary solution */
	int xmin, xmax, ymin, ymax, x, y;

	/* calculate the min and max for x and y directions */
	if (x0 <= x1) {
		xmin = x0;
		xmax = x1;
	} else {
		xmin = x1;
		xmax = x0;
	}
	if (y0 <= y1) {
		ymin = y0;
		ymax = y1;
	} else {
		ymin = y1;
		ymax = y0;
	}
	for (y = ymin; y <= ymax; y++)
		for (x = xmin; x <= xmax; x++)
			gpanel_pixel(gp, x, y, color);
#if 1
#else
	/* specify the controller drawing box according to those limits */
	write_command (PHILIPS_PASET);
	write_data (ymin + 1);
	write_data (ymax + 1);
	write_command (PHILIPS_CASET);
	write_data (xmin + 1);
	write_data (xmax + 1);
	write_command (PHILIPS_RAMWR);

	/* loop on total number of pixels/2 */
	for (i = 0; i < ((((xmax - xmin + 1) * (ymax - ymin + 1)) / 2) + 1); i++) {
		/* use the color value to output three data bytes covering two pixels */
		write_data (color >> 4);
		write_data ((color << 4) | ((color >> 8) & 0xF));
		write_data (color);
	}
#endif
}

/*
 * Write an image to LCD screen.
 */
void gpanel_image(gpanel_t *gp, int x, int y, int width, int height,
		const unsigned short *data) {
#if 0
	unsigned i, pixels = height * width;
	unsigned long rgbrgb;

	write_command (PHILIPS_PASET);
	write_data (y + 1);
	write_data (y + height);
	write_command (PHILIPS_CASET);
	write_data (x + 1);
	write_data (x + width);
	write_command (PHILIPS_RAMWR);

	for (i=0; i<pixels; i+=2) {
		rgbrgb = (unsigned long) data [i] << 12;
		rgbrgb |= data [i+1];

		write_data (rgbrgb >> 16);
		write_data (rgbrgb >> 8);
		write_data (rgbrgb);
	}
#endif
}

