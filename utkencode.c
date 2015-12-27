/*
** utkencode
** Encode wav to Maxis UTalk.
** Authors: Fatbag
** License: Public domain (no warranties)
** Compile: gcc -Wall -Wextra -ansi -pedantic -O2 -ffast-math -g0 -s
**	-o utkencode utkencode.c
*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define CLAMP(x,min,max) ((x)<(min)?(min):(x)>(max)?(max):(x))
#define ROUND(x) ((int)((x)>=0?((x)+0.5):((x)-0.5)))
#define ABS(x) ((x)>=0?(x):-(x))

#define READ16(x) ((x)[0]|((x)[1]<<8))
#define READ32(x) ((x)[0]|((x)[1]<<8)|((x)[2]<<16)|((x)[3]<<24))

#define WRITE16(d,s) (d)[0]=(uint8_t)(s),(d)[1]=(uint8_t)((s)>>8)
#define WRITE32(d,s) (d)[0]=(uint8_t)(s),(d)[1]=(uint8_t)((s)>>8),\
	(d)[2]=(uint8_t)((s)>>16),(d)[3]=(uint8_t)((s)>>24)

const float utk_rc_table[64] = {
	0,
	-.99677598476409912109375, -.99032700061798095703125, -.983879029750823974609375, -.977430999279022216796875,
	-.970982015132904052734375, -.964533984661102294921875, -.958085000514984130859375, -.9516370296478271484375,
	-.930754005908966064453125, -.904959976673126220703125, -.879167020320892333984375, -.853372991085052490234375,
	-.827579021453857421875, -.801786005496978759765625, -.775991976261138916015625, -.75019800662994384765625,
	-.724404990673065185546875, -.6986110210418701171875, -.6706349849700927734375, -.61904799938201904296875,
	-.567460000514984130859375, -.515873014926910400390625, -.4642859995365142822265625, -.4126980006694793701171875,
	-.361110985279083251953125, -.309523999691009521484375, -.257937014102935791015625, -.20634900033473968505859375,
	-.1547619998455047607421875, -.10317499935626983642578125, -.05158700048923492431640625,
	0,
	+.05158700048923492431640625, +.10317499935626983642578125, +.1547619998455047607421875, +.20634900033473968505859375,
	+.257937014102935791015625, +.309523999691009521484375, +.361110985279083251953125, +.4126980006694793701171875,
	+.4642859995365142822265625, +.515873014926910400390625, +.567460000514984130859375, +.61904799938201904296875,
	+.6706349849700927734375, +.6986110210418701171875, +.724404990673065185546875, +.75019800662994384765625,
	+.775991976261138916015625, +.801786005496978759765625, +.827579021453857421875, +.853372991085052490234375,
	+.879167020320892333984375, +.904959976673126220703125, +.930754005908966064453125, +.9516370296478271484375,
	+.958085000514984130859375, +.964533984661102294921875, +.970982015132904052734375, +.977430999279022216796875,
	+.983879029750823974609375, +.99032700061798095703125, +.99677598476409912109375
};

static const char *prog_name;

static void print_help(void)
{
	printf("Usage: %s [options] infile.wav outfile.utk\n", prog_name);
	printf("Encode wav to Maxis UTalk.\n");
	printf("\n");
	printf("General options:\n");
	printf("  -f, --force               overwrite without prompting\n");
	printf("  -q, --quiet               suppress normal output and do not prompt\n");
	printf("  -h, --help                display this help and exit\n");
	printf("  -V, --version             output version information and exit\n");
	printf("\n");
	printf("Encoding options:\n");
	printf("  -b, --bitrate=N           target bitrate in bits/sec (default 32000)\n");
	printf("  -H, --halved-inn          encode innovation using half bandwidth\n");
	printf("                            (default)\n");
	printf("  -F, --full-inn            encode innovation using full bandwidth\n");
	printf("  -T, --huff-threshold=N    use the Huffman codebook with threshold N where\n");
	printf("                            N is an integer between 16 and 32 (inclusive)\n");
	printf("                            (default 24)\n");
	printf("  -S, --inngain-sig=N       use innovation gain significand N where N is\n");
	printf("                            between 8 and 128 (inclusive) in steps of 8\n");
	printf("                            (default 64)\n");
	printf("  -B, --inngain-base=N      use innovation gain base N where N is between\n");
	printf("                            1.040 and 1.103 (inclusive) in steps of 0.001\n");
	printf("                            (default 1.068)\n");
	printf("\n");
	printf("If infile is \"-\", read from standard input.\n");
	printf("If outfile is \"-\", write to standard output.\n");
}

static void print_version(void)
{
	printf("utkencode 0.0\n");
}

static void print_usage_error(void)
{
	fprintf(stderr, "Usage: %s [options] infile.wav outfile.utk\n",
		prog_name);
	fprintf(stderr, "Try '%s --help' for more options.\n", prog_name);
}

static const char short_options[] = "fqhVb:HFT:S:B:";
static const struct option long_options[] = {
	{"force",          no_argument,       0, 'f'},
	{"quiet",          no_argument,       0, 'q'},
	{"help",           no_argument,       0, 'h'},
	{"version",        no_argument,       0, 'V'},
	{"bitrate",        required_argument, 0, 'b'},
	{"halved-inn",     no_argument,       0, 'H'},
	{"full-inn",       no_argument,       0, 'F'},
	{"huff-threshold", required_argument, 0, 'T'},
	{"inngain-sig",    required_argument, 0, 'S'},
	{"inngain-base",   required_argument, 0, 'B'},
	{0, 0, 0, 0}
};

static int bitrate = 32000;
static int force = 0;
static int quiet = 0;
static int halved_innovation = 1;
static int huffman_threshold = 24;
static int inngain_sig = 64;
static float inngain_base = 1.068f;
static const char *infile = "";
static const char *outfile = "";
static FILE *infp = NULL;
static FILE *outfp = NULL;

static uint8_t wav_buffer[432*2];
static float input_samples[12+432];
static float adaptive_codebook[324+432];
static uint8_t compressed_buffer[1024];
static uint8_t inn_buffers[2][256];
static float prev_rc[12];
static float innovation[5+108+5];
static float inn_gains[64];

struct bit_writer_context {
	uint8_t written_bits_count;
	size_t pos;
	uint8_t *buffer;
};

static void read_data(FILE *fp, uint8_t *buffer, size_t size)
{
	if (fread(buffer, 1, size, fp) != size) {
		fprintf(stderr, "%s: failed to read '%s': %s\n",
			prog_name, infile, ferror(fp)
			? strerror(errno) : "reached end of file");
		exit(EXIT_FAILURE);
	}
}

static void write_data(FILE *fp, const uint8_t *buffer, size_t size)
{
	if (fwrite(buffer, 1, size, fp) != size) {
		fprintf(stderr, "%s: failed to write to '%s': %s\n",
			prog_name, outfile, ferror(fp)
			? strerror(errno) : "reached end of file");
		exit(EXIT_FAILURE);
	}
}

static void flush_data(FILE *fp)
{
	if (fflush(fp) != 0) {
		fprintf(stderr, "%s: failed to flush '%s': %s\n",
			prog_name, outfile, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void bwc_init(struct bit_writer_context *bwc, uint8_t *buffer)
{
	bwc->written_bits_count = 0;
	bwc->pos = 0;
	bwc->buffer = buffer;
	bwc->buffer[0] = 0;
}

static void bwc_write_bits(struct bit_writer_context *bwc, unsigned value,
	uint8_t count)
{
	unsigned x = value << bwc->written_bits_count;

	bwc->buffer[bwc->pos] |= (uint8_t)x;
	bwc->written_bits_count += count;

	while (bwc->written_bits_count >= 8) {
		x >>= 8;
		bwc->buffer[++bwc->pos] = (uint8_t)x;
		bwc->written_bits_count -= 8;
	}
}

static void bwc_pad(struct bit_writer_context *bwc)
{
	if (bwc->written_bits_count != 0) {
		bwc->buffer[++bwc->pos] = 0;
		bwc->written_bits_count = 0;
	}
}

static void bwc_flush(struct bit_writer_context *bwc, FILE *fp)
{
	write_data(fp, bwc->buffer, bwc->pos);
	bwc->buffer[0] = bwc->buffer[bwc->pos];
	bwc->pos = 0;
}

static unsigned quantize(float value, const float *alphabet, size_t alphabet_size)
{
	unsigned i;
	unsigned min_idx = 0;
	float min_distance = ABS(value - alphabet[0]);

	for (i = 1; i < alphabet_size; i++) {
		float distance = ABS(value - alphabet[i]);

		if (distance < min_distance) {
			min_distance = distance;
			min_idx = i;
		}
	}

	return min_idx;
}

/* used in the parsing of some arguments */
static int read_dec_places(const char *string, int n)
{
	int i;
	int value = 0;
	int pows_10[10];

	pows_10[0] = 1;
	for (i = 1; i < n; i++)
		pows_10[i] = pows_10[i-1] * 10;

	for (i = 0; i < n && string[i] >= '0' && string[i] <= '9'; i++)
		value += pows_10[n-1-i] * (string[i]-'0');

	return (string[i] == '\0') ? value : -1;
}

static int file_exists(const char *filename)
{
	FILE *fp;

	fp = fopen(filename, "rb");
	if (fp) {
		fclose(fp);
		return 1;
	}

	return 0;
}

static void find_autocorrelations(float *r, const float *samples)
{
	int i, j;

	for (i = 0; i < 13; i++) {
		r[i] = 0;
		for (j = 0; j < 432 - i; j++)
			r[i] += samples[j]*samples[j+i];
	}
}

static void levinson_durbin_symmetric(float *x, float *k,
    const float *r, const float *y)
{
	float a[12]; /* the forward vector */
	float e; /* prediction error */
	int i;

	if (r[0] <= 1.0f/32768.0f && r[0] >= -1.0f/32768.0f)
		goto zero;

	a[0] = 1;
	e = r[0];
	x[0] = y[0]/r[0];

	for (i = 1; i < 12; i++) {
		float u, m;
		float a_temp[12];
		int j;

		u = 0.0f;
		for (j = 0; j < i; j++)
			u += a[j]*r[i-j];

		k[i-1] = -u/e; /* reflection coefficient i-1 */
		e += u*k[i-1]; /* update e to the new value e - u*u/e */

		if (e <= 1.0f/32768.0f && e >= -1.0f/32768.0f)
			goto zero;

		memcpy(a_temp, a, i*sizeof(float));
		a[i] = 0.0f;
		for (j = 1; j <= i; j++)
			a[j] += k[i-1]*a_temp[i-j];

		m = y[i];
		for (j = 0; j < i; j++)
			m -= x[j]*r[i-j];
		m /= e;

		x[i] = 0.0f;
		for (j = 0; j <= i; j++)
			x[j] += m*a[i-j];
	}

	k[11] = -x[11];

	return;

zero:
	for (i = 0; i < 12; i++)
		x[i] = 0.0f;
	for (i = 0; i < 12; i++)
		k[i] = 0.0f;
}

static void rc_to_lpc(float *x, const float *k)
{
	float a[13]; /* the forward vector */
	unsigned i, j;
	a[0] = 1;

	for (i = 1; i < 13; i++) {
		float a_temp[12];
		memcpy(a_temp, a, i*sizeof(float));
		a[i] = 0.0f;
		for (j = 1; j <= i; j++)
			a[j] += k[i-1]*a_temp[i-j];
	}

	for (i = 1; i < 13; i++)
		x[i-1] = -a[i];
}

static void find_rc(float *rc, const float *samples)
{
	float r[13];
	float lpc[12];
	find_autocorrelations(r, samples);
	levinson_durbin_symmetric(lpc, rc, r, r+1);
}

static void find_excitation(float *excitation, const float *source,
	int length, const float *lpc)
{
	int i, j;

	for (i = 0; i < length; i++) {
		float prediction = 0.0f;
		for (j = 0; j < 12; j++)
			prediction += lpc[j]*source[i-1-j];
		excitation[i] = source[i] - prediction;
	}
}

static void find_pitch(int *pitch_lag, float *pitch_gain,
	const float *excitation)
{
	int max_corr_offset = 108;
	float max_corr_value = 0.0f;
	float history_energy;
	float gain;
	int i, j;

	/* Find the optimal pitch lag. */
	for (i = 108; i < 324; i++) {
		float corr = 0.0f;
		for (j = 0; j < 108; j++)
			corr += excitation[j]*excitation[j-i];
		if (corr > max_corr_value) {
			max_corr_offset = i;
			max_corr_value = corr;
		}
	}

	/* Find the optimal pitch gain. */
	history_energy = 0.0f;
	for (i = 0; i < 108; i++) {
		float value = excitation[i-max_corr_offset];
		history_energy += value*value;
	}

	if (history_energy >= 1/32768.0f) {
		gain = max_corr_value / history_energy;
		gain = CLAMP(gain, 0.0f, 1.0f);

		*pitch_lag = max_corr_offset;
		*pitch_gain = gain;
	} else {
		*pitch_lag = 108;
		*pitch_gain = 0.0f;
	}
}

static void interpolate(float *x, int a, int z)
{
	int i;

	if (z) {
		for (i = !a; i < 108; i+=2)
			x[i] = 0.0f;
	} else {
		for (i = !a; i < 108; i+=2)
			x[i]
				= (x[i-1]+x[i+1]) * .5973859429f
				- (x[i-3]+x[i+3]) * .1145915613f
				+ (x[i-5]+x[i+5]) * .0180326793f;
	}
}

static float interpolation_error(int a, int z, const float *x)
{
	float error = 0.0f;
	int i;

	if (z) {
		for (i = !a; i < 108; i+=2)
			error += x[i]*x[i];
	} else {
		for (i = !a; i < 108; i+=2) {
			float prediction
				= (x[i-1]+x[i+1]) * .5973859429f
				- (x[i-3]+x[i+3]) * .1145915613f
				+ (x[i-5]+x[i+5]) * .0180326793f;
			error += (prediction - x[i])*(prediction - x[i]);
		}
	}

	return error;
}

static void find_a_z_flags(int *a, int *z, const float *innovation)
{
	/* Find the a and z flags such that the least error is introduced
	** in the downsampling step. In case of a tie (e.g. in silence),
	** prefer using the zero flag. Thus, we will test in the order:
	** (a=0,z=1), (a=1,z=1), (a=0,z=0), (a=1,z=1). */
	float error;
	float best_error;
	int best_a = 0, best_z = 1;

	best_error = interpolation_error(0, 1, innovation);

	error = interpolation_error(1, 1, innovation);
	if (error < best_error) {
		best_error = error;
		best_a = 1, best_z = 1;
	}

	error = interpolation_error(0, 0, innovation);
	if (error < best_error) {
		best_error = error;
		best_a = 0, best_z = 0;
	}

	error = interpolation_error(1, 0, innovation);
	if (error < best_error) {
		best_error = error;
		best_a = 1, best_z = 0;
	}

	*a = best_a;
	*z = best_z;
}

struct huffman_code {
	uint16_t bits_value;
	uint16_t bits_count;
};

static const struct huffman_code huffman_models[2][13+1+13] = {
	/* model 0 */
	{
		/* -13 */ {16255, 16},
		/* -12 */ {8063, 15},
		/* -11 */ {3967, 14},
		/* -10 */ {1919, 13},
		/*  -9 */ {895, 12},
		/*  -8 */ {383, 11},
		/*  -7 */ {127, 10},
		/*  -6 */ {63, 8},
		/*  -5 */ {31, 7},
		/*  -4 */ {15, 6},
		/*  -3 */ {7, 5},
		/*  -2 */ {3, 4},
		/*  -1 */ {2, 2},
		/*   0 */ {0, 2},
		/*  +1 */ {1, 2},
		/*  +2 */ {11, 4},
		/*  +3 */ {23, 5},
		/*  +4 */ {47, 6},
		/*  +5 */ {95, 7},
		/*  +6 */ {191, 8},
		/*  +7 */ {639, 10},
		/*  +8 */ {1407, 11},
		/*  +9 */ {2943, 12},
		/* +10 */ {6015, 13},
		/* +11 */ {12159, 14},
		/* +12 */ {24447, 15},
		/* +13 */ {49023, 16}
	},

	/* model 1 */
	{
		/* -13 */ {8127, 15},
		/* -12 */ {4031, 14},
		/* -11 */ {1983, 13},
		/* -10 */ {959, 12},
		/*  -9 */ {447, 11},
		/*  -8 */ {191, 10},
		/*  -7 */ {63, 9},
		/*  -6 */ {31, 7},
		/*  -5 */ {15, 6},
		/*  -4 */ {7, 5},
		/*  -3 */ {3, 4},
		/*  -2 */ {1, 3},
		/*  -1 */ {2, 3},
		/*   0 */ {0, 2},
		/*  +1 */ {6, 3},
		/*  +2 */ {5, 3},
		/*  +3 */ {11, 4},
		/*  +4 */ {23, 5},
		/*  +5 */ {47, 6},
		/*  +6 */ {95, 7},
		/*  +7 */ {319, 9},
		/*  +8 */ {703, 10},
		/*  +9 */ {1471, 11},
		/* +10 */ {3007, 12},
		/* +11 */ {6079, 13},
		/* +12 */ {12223, 14},
		/* +13 */ {24511, 15}
	}
};

static void encode_huffman(struct bit_writer_context *bwc,
	float *innovation_out, int *bits_used_out, float *error_out,
	const float *innovation_in, int halved_innovation,
	int pow, int a, int z)
{
	int interval = halved_innovation ? 2 : 1;
	float inn_gain;
	float total_error = 0.0f;
	int counter;
	int values[108];
	int zero_counts[108];
	int model;
	int bits_start, bits_end;
	int i;

	inn_gain = inn_gains[pow];
	if (!z)
		inn_gain *= 0.5f;

	bits_start = 8*bwc->pos + bwc->written_bits_count;

	if (halved_innovation)
		bwc_write_bits(bwc, pow | (a<<6) | (z<<7), 8);
	else
		bwc_write_bits(bwc, pow, 6);

	for (i = a; i < 108; i += interval) {
		float e;

		values[i] = ROUND(CLAMP(
			innovation_in[i]/inn_gain, -13.0f, 13.0f));

		innovation_out[i] = inn_gain*values[i];

		e = innovation_out[i] - innovation_in[i];
		total_error += e*e;
	}

	*error_out = total_error;

	/* Find the zero runs at each position i (how many zeros
	** in a row there are at position i).
	** When interval=2 and a=1, start the search from i=105 instead
	** of 107 in order to duplicate the off-by-one mistake in the
	** decoder. (Thus, we will subtract a instead of adding.)
	** For details, see: http://wiki.niotso.org/UTK */
	counter = 0;
	for (i = 108 - interval - a; i >= 0; i -= interval) {
		if (values[i] == 0)
			counter++;
		else
			counter = 0;
		zero_counts[i] = counter;
	}

	i = a;
	model = 0;
	while (i < 108) {
		if (zero_counts[i] >= 7) {
			int length = MIN(zero_counts[i], 70);

			if (model == 0)
				bwc_write_bits(bwc, 255 | ((length-7)<<8), 14);
			else
				bwc_write_bits(bwc, 127 | ((length-7)<<7), 13);

			model = 0;
			i += length * interval;
		} else {
			int value = values[i];

			bwc_write_bits(bwc,
				huffman_models[model][13+value].bits_value,
				huffman_models[model][13+value].bits_count);

			model = (value < -1 || value > 1);
			i += interval;
		}
	}

	bits_end = 8*bwc->pos + bwc->written_bits_count;
	*bits_used_out = bits_end - bits_start;
}

static void encode_triangular(struct bit_writer_context *bwc,
	float *innovation_out, int *bits_used_out, float *error_out,
	const float *innovation_in, int halved_innovation,
	int pow, int a, int z)
{
	int interval = halved_innovation ? 2 : 1;
	float inn_gain;
	float total_error = 0.0f;
	int bits_start, bits_end;
	int i;

	inn_gain = 2.0f*inn_gains[pow];
	if (!z)
		inn_gain *= 0.5f;

	bits_start = 8*bwc->pos + bwc->written_bits_count;

	if (halved_innovation)
		bwc_write_bits(bwc, pow | (a<<6) | (z<<7), 8);
	else
		bwc_write_bits(bwc, pow, 6);

	for (i = a; i < 108; i += interval) {
		float e;
		int value = ROUND(CLAMP(
			innovation_in[i]/inn_gain, -1.0f, 1.0f));

		if (value > 0)
			bwc_write_bits(bwc, 3, 2);
		else if (value < 0)
			bwc_write_bits(bwc, 1, 2);
		else
			bwc_write_bits(bwc, 0, 1);

		innovation_out[i] = inn_gain*value;

		e = innovation_out[i] - innovation_in[i];
		total_error += e*e;
	}

	bits_end = 8*bwc->pos + bwc->written_bits_count;
	*bits_used_out = bits_end - bits_start;

	*error_out = total_error;
}

static void low_pass_innovation(float *x, int a, int z)
{
	/* Apply a weak low-pass filter to the innovation signal suitable for
	** downsampling it by 1/2. Note that, since we are throwing out all
	** x[m] samples where m != a+2*k for integer k, we only have to filter
	** the x[n] samples where n = a+2*k. */
	int i;

	/* filter coeffs: (GNU Octave)
	** n = 10; b = sinc((-n/4):.5:(n/4)).*hamming(n+9)(5:(n+5))' */
	for (i = a; i < 108; i+=2)
		x[i] = (z ? 1.0f : 0.5f)*(x[i]
			+ (x[i-1]+x[i+1]) * 0.6189590521549956f
			+ (x[i-3]+x[i+3]) * -0.1633990749076792f
			+ (x[i-5]+x[i+5]) * 0.05858453198856907f);
}

struct innovation_encoding {
	struct bit_writer_context bwc;
	float innovation[108];
	int bits_used;
	float error;
};

static void encode_innovation(struct bit_writer_context *bwc,
	float *innovation, int halved_innovation, int use_huffman,
	int *bits_used, int target_bit_count)
{
	int a = 0, z = 1;
	struct innovation_encoding encodings[2];
	int m = 0;

	if (halved_innovation) {
		find_a_z_flags(&a, &z, innovation);
		low_pass_innovation(innovation, a, z);
	}

	if (use_huffman) {
		/* Encode using the Huffman model. */
		int interval = halved_innovation ? 2 : 1;
		float max_value = 0.0f;
		int min_pow;
		int best_distance = 0;
		int pow;
		int i;

		/* Find the minimum innovation power such that the innovation
		** signal doesn't clip anywhere in time. (We consider clipping
		** a sample by <=0.5 of a quantization level to be okay since
		** the sample already rounds down [towards zero].) */
		for (i = a; i < 108; i += interval) {
			float value = ABS(innovation[i]);
			if (value > max_value)
				max_value = value;
		}
		for (i = 62; i >= 0; i--) {
			if (inn_gains[i]*(!z ? 0.5f : 1.0f)*13.5f
				< max_value)
				break;
		}
		min_pow = i+1;

		/* Find the innovation gain that results in the closest
		** to the target bitrate without clipping occurring. */
		for (pow = min_pow; pow <= 63; pow++) {
			int distance;

			bwc_init(&encodings[m].bwc, inn_buffers[m]);
			bwc_write_bits(&encodings[m].bwc, bwc->buffer[bwc->pos],
				bwc->written_bits_count);

			encode_huffman(&encodings[m].bwc,
				encodings[m].innovation,
				&encodings[m].bits_used,
				&encodings[m].error,
				innovation, halved_innovation,
				pow, a, z);

			distance = ABS(encodings[m].bits_used
				- target_bit_count);
			if (pow == min_pow || distance < best_distance) {
				best_distance = distance;
				m = !m; /* swap the buffers */
			}
		}
	} else {
		/* Encode using the triangular noise model. */
		float best_error = 0.0f;
		int pow;

		/* Find the innovation gain that results in
		** the highest quality. */
		for (pow = 0; pow <= 63; pow++) {
			bwc_init(&encodings[m].bwc, inn_buffers[m]);
			bwc_write_bits(&encodings[m].bwc, bwc->buffer[bwc->pos],
				bwc->written_bits_count);

			encode_triangular(&encodings[m].bwc,
				encodings[m].innovation,
				&encodings[m].bits_used,
				&encodings[m].error,
				innovation, halved_innovation,
				pow, a, z);

			if (pow == 0 || encodings[m].error < best_error) {
				best_error = encodings[m].error;
				m = !m; /* swap the buffers */
			}
		}
	}

	/* Swap the buffers again to return back to our best encoding. */
	m = !m;

	/* Write this encoding out to the UTK bitstream. */
	memcpy(&bwc->buffer[bwc->pos], encodings[m].bwc.buffer,
		encodings[m].bwc.pos+1);
	bwc->pos += encodings[m].bwc.pos;
	bwc->written_bits_count = encodings[m].bwc.written_bits_count;

	/* Update the innovation signal with the quantized version. */
	memcpy(innovation, encodings[m].innovation, 108*sizeof(float));
	if (halved_innovation)
		interpolate(innovation, a, z);

	*bits_used = encodings[m].bits_used;
}

static int parse_arguments(int argc, char *argv[])
{
	int c;
	int value;
	char *endptr;

	prog_name = (argc >= 1 && argv[0][0] != '\0') ? argv[0] : "utkencode";

	while ((c = getopt_long(argc, argv, short_options,
		long_options, NULL)) != -1) {
		switch (c) {
		case 'b':
			bitrate = (int)strtol(optarg, &endptr, 10);
			if (*endptr != '\0'
				|| bitrate < 1000
				|| bitrate > 1000000) {
				fprintf(stderr, "%s: invalid bitrate -- %s\n",
					prog_name, optarg);
				print_usage_error();
				return -1;
			}
			break;
		case 'f':
			force = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'h':
			print_help();
			return 1;
		case 'V':
			print_version();
			return 1;
		case 'H':
			halved_innovation = 1;
			break;
		case 'F':
			halved_innovation = 0;
			break;
		case 'T':
			huffman_threshold = (int)strtol(optarg, &endptr, 10);
			if (*endptr != '\0'
				|| huffman_threshold < 16
				|| huffman_threshold > 32) {
				fprintf(stderr, "%s: invalid Huffman "
					"threshold -- %s\n", prog_name, optarg);
				print_usage_error();
				return -1;
			}
			break;
		case 'S':
			inngain_sig = (int)strtol(optarg, &endptr, 10);
			if (*endptr != '\0'
				|| inngain_sig < 8
				|| inngain_sig > 128
				|| (inngain_sig & 7) != 0) {
				fprintf(stderr, "%s: invalid innovation gain"
					" significand -- %s\n", prog_name,
					optarg);
				print_usage_error();
				return -1;
			}
			break;
		case 'B':
			if (optarg[0] != '1' || optarg[1] != '.'
				|| (value = read_dec_places(optarg+2, 3)) < 0
				|| value < 40
				|| value > 103) {
				fprintf(stderr, "%s: invalid innovation gain"
					" base -- %s\n", prog_name, optarg);
				print_usage_error();
				return -1;
			}
			inngain_base = 1.0f + (float)value/1000.0f;
			break;
		default:
			print_usage_error();
			return -1;
		}
	}

	if (argc - optind == 0) {
		fprintf(stderr, "%s: missing infile\n", prog_name);
		print_usage_error();
		return -1;
	} else if (argc - optind == 1) {
		fprintf(stderr, "%s: missing outfile\n", prog_name);
		print_usage_error();
		return -1;
	} else if (argc - optind >= 3) {
		fprintf(stderr, "%s: too many arguments passed\n", prog_name);
		print_usage_error();
		return -1;
	}

	infile = argv[optind];
	outfile = argv[optind+1];

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	uint8_t wav_header[44];
	uint8_t utk_header[32];
	unsigned bytes_remaining;
	int sampling_rate;
	struct bit_writer_context bwc;
	int i, j;

	ret = parse_arguments(argc, argv);
	if (ret < 0)
		return EXIT_FAILURE;
	else if (ret > 0)
		return EXIT_SUCCESS;

	if (!strcmp(infile, "-")) {
		infp = stdin;
	} else {
		infp = fopen(infile, "rb");
		if (!infp) {
			fprintf(stderr, "%s: failed to open '%s' for"
				" reading: %s\n", prog_name, infile,
				strerror(errno));
			return EXIT_FAILURE;
		}
	}
	setvbuf(infp, NULL, _IOFBF, BUFSIZ);

	if (!strcmp(outfile, "-")) {
		outfp = stdout;
	} else {
		if (!force && file_exists(outfile)) {
			if (quiet) {
				fprintf(stderr, "%s: failed to open '%s' for"
					" writing: file already exists\n",
					prog_name, outfile);
				return EXIT_FAILURE;
			} else {
				fprintf(stderr, "%s: overwrite '%s'? ",
					prog_name, outfile);
				if (getchar() != 'y')
					return EXIT_FAILURE;
			}
		}

		outfp = fopen(outfile, "wb");
		if (!outfp) {
			fprintf(stderr, "%s: failed to open '%s' for"
				" writing: %s\n", prog_name, outfile,
				strerror(errno));
			return EXIT_FAILURE;
		}
	}
	setvbuf(outfp, NULL, _IOFBF, BUFSIZ);

	if (fread(wav_header, 1, 44, infp) != 44) {
		if (ferror(infp))
			fprintf(stderr, "%s: failed to read '%s': %s\n",
				prog_name, infile, strerror(errno));
		else
			fprintf(stderr, "%s: '%s' is not a valid wav file\n",
				prog_name, infile); /* (reached end of file) */
		return EXIT_FAILURE;
	}

	if (memcmp(wav_header, "RIFF", 4) != 0
		|| memcmp(wav_header+8, "WAVEfmt ", 8) != 0) {
		fprintf(stderr, "%s: '%s' is not a valid wav file\n",
			prog_name, infile);
		return EXIT_FAILURE;
	}

	if (READ16(wav_header+20) != 1 /* wFormatTag */
		|| READ16(wav_header+22) != 1 /* nChannels */
		|| READ16(wav_header+32) != 2 /* nBlockAlign */
		|| READ16(wav_header+34) != 16 /* wBitsPerSample */) {
		fprintf(stderr, "%s: wav file must be 1-channel 16-bit LPCM\n",
			prog_name);
		return EXIT_FAILURE;
	}

	sampling_rate = READ32(wav_header+24); /* nSamplesPerSec */
	if (sampling_rate < 1000 || sampling_rate > 1000000) {
		fprintf(stderr, "%s: unsupported sampling rate %d\n",
			prog_name, sampling_rate);
		return EXIT_FAILURE;
	}

	memcpy(utk_header, "UTM0", 4); /* sID */

	/* Drop the last byte from the wav file if there are an odd
	** number of sample bytes. */
	bytes_remaining = READ32(wav_header+40) & (~1);
	WRITE32(utk_header+4, bytes_remaining); /* dwOutSize */

	WRITE32(utk_header+8, 20); /* dwWfxSize */
	memcpy(utk_header+12, wav_header+20, 16); /* WAVEFORMATEX */
	WRITE32(utk_header+28, 0); /* cbSize */

	write_data(outfp, utk_header, 32);

	bwc_init(&bwc, compressed_buffer);

	bwc_write_bits(&bwc, halved_innovation, 1);
	bwc_write_bits(&bwc, 32 - huffman_threshold, 4);
	bwc_write_bits(&bwc, inngain_sig/8 - 1, 4);
	bwc_write_bits(&bwc, ROUND((inngain_base - 1.04f)*1000.0f), 6);
	bwc_flush(&bwc, outfp);

	for (i = 0; i < 12; i++)
		input_samples[i] = 0.0f;
	for (i = 0; i < 324; i++)
		adaptive_codebook[i] = 0.0f;
	for (i = 0; i < 12; i++)
		prev_rc[i] = 0.0f;
	for (i = 0; i < 5; i++)
		innovation[i] = 0.0f;
	for (i = 5+108; i < 5+108+5; i++)
		innovation[i] = 0.0f;

	inn_gains[0] = inngain_sig;
	for (i = 1; i < 64; i++)
		inn_gains[i] = inn_gains[i-1]*inngain_base;

	while (bytes_remaining != 0) {
		/* Encode the next frame of 432 samples. */
		int bytes_to_read;
		int samples_to_read;
		float rc[12];
		float rc_delta[12];
		int use_huffman = 0;

		bytes_to_read = (int)MIN(bytes_remaining, 432*2);
		samples_to_read = bytes_to_read >> 1;

		read_data(infp, wav_buffer, bytes_to_read);
		bytes_remaining -= bytes_to_read;

		for (i = 0; i < samples_to_read; i++) {
			int16_t x = READ16(wav_buffer+2*i);
			input_samples[12+i] = (float)x;
		}
		for (i = samples_to_read; i < 432; i++)
			input_samples[12+i] = 0.0f;

		find_rc(rc, input_samples+12);

		/* Quantize the reflection coefficients.
		** In our encoder, we will not make use of utk_rc_table[0]. */
		for (i = 0; i < 4; i++) {
			int idx = 1+quantize(rc[i], utk_rc_table+1, 63);
			bwc_write_bits(&bwc, idx, 6);
			rc[i] = utk_rc_table[idx];
			if (i == 0 && idx < huffman_threshold)
				use_huffman = 1;
		}
		for (i = 4; i < 12; i++) {
			int idx = quantize(rc[i], utk_rc_table+16, 32);
			bwc_write_bits(&bwc, idx, 5);
			rc[i] = utk_rc_table[16+idx];
		}

		for (i = 0; i < 12; i++)
			rc_delta[i] = (rc[i] - prev_rc[i])/4.0f;

		memcpy(rc, prev_rc, 12*sizeof(float));

		for (i = 0; i < 4; i++) {
			/* Linearly interpolate the reflection coefficients over
			** the four subframes and find the excitation signal. */
			float lpc[12];

			for (j = 0; j < 12; j++)
				rc[j] += rc_delta[j];

			rc_to_lpc(lpc, rc);

			find_excitation(adaptive_codebook+324+12*i,
				input_samples+12+12*i,
				i < 3 ? 12 : 396, lpc);
		}

		memcpy(input_samples, &input_samples[432], 12*sizeof(float));
		memcpy(prev_rc, rc, 12*sizeof(float));

		for (i = 0; i < 4; i++) {
			/* Encode the i'th subframe. */
			float *excitation = adaptive_codebook+324+108*i;
			int pitch_lag;
			float pitch_gain;
			int idx;
			int bits_used;

			find_pitch(&pitch_lag, &pitch_gain, excitation);

			bwc_write_bits(&bwc, pitch_lag - 108, 8);

			idx = ROUND(pitch_gain*15.0f);
			bwc_write_bits(&bwc, idx, 4);
			pitch_gain = (float)idx/15.0f;

			for (j = 0; j < 108; j++)
				innovation[5+j] = excitation[j]
					- pitch_gain*excitation[j-pitch_lag];

			encode_innovation(&bwc, &innovation[5],
				halved_innovation, use_huffman, &bits_used,
				ROUND(bitrate * 432 / sampling_rate / 4) - 18);

			/* Update the adaptive codebook using the quantized
			** innovation signal. */
			for (j = 0; j < 108; j++)
				excitation[j] = innovation[5+j]
					+ pitch_gain*excitation[j-pitch_lag];
		}

		/* Copy the last 3 subframes to the beginning of the
		** adaptive codebook. */
		memcpy(adaptive_codebook, &adaptive_codebook[432],
			324*sizeof(float));

		bwc_flush(&bwc, outfp);
	}

	bwc_pad(&bwc);
	bwc_flush(&bwc, outfp);

	flush_data(outfp);

	fclose(outfp);
	fclose(infp);

	return EXIT_SUCCESS;
}