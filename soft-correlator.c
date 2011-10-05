/*
 * Copyright (C) 2011 Jamey Sharp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <fftw3.h>
#include <stdio.h>

#define TRACE 0

struct signal_strength {
	double snr;
	double doppler;
	double phase;
};

/* G2 shift register delay for each PRN is specified in IS-GPS-200,
 * table 3-I. It's stored time-reversed here: the spec is for the number
 * of chips to delay G2's output before using it, but we want to know
 * how many chips into the future to look. */
static const struct SVDATA {
    unsigned char PRN;
    unsigned char Navstar;
    unsigned short advance;
} SV[] = {
    {  1,  63, 1023 -   5, },
    {  2,  56, 1023 -   6, },
    {  3,  37, 1023 -   7, },
    {  4,  35, 1023 -   8, },
    {  5,  64, 1023 -  17, },
    {  6,  36, 1023 -  18, },
    {  7,  62, 1023 - 139, },
    {  8,  44, 1023 - 140, },
    {  9,  33, 1023 - 141, },
    { 10,  38, 1023 - 251, },
    { 11,  46, 1023 - 252, },
    { 12,  59, 1023 - 254, },
    { 13,  43, 1023 - 255, },
    { 14,  49, 1023 - 256, },
    { 15,  60, 1023 - 257, },
    { 16,  51, 1023 - 258, },
    { 17,  57, 1023 - 469, },
    { 18,  50, 1023 - 470, },
    { 19,  54, 1023 - 471, },
    { 20,  47, 1023 - 472, },
    { 21,  52, 1023 - 473, },
    { 22,  53, 1023 - 474, },
    { 23,  55, 1023 - 509, },
    { 24,  23, 1023 - 512, },
    { 25,  24, 1023 - 513, },
    { 26,  26, 1023 - 514, },
    { 27,  27, 1023 - 515, },
    { 28,  48, 1023 - 516, },
    { 29,  61, 1023 - 859, },
    { 30,  39, 1023 - 860, },
    { 31,  58, 1023 - 861, },
    { 32,  22, 1023 - 862, },
};

/* These are precomputed outputs of the G1 and G2 shift registers as
 * specified by IS-GPS-200. */
static const struct {
	unsigned char G1 : 1;
	unsigned char G2 : 1;
} ca_code_states[1023] = {
	{1,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,1},
	{0,0}, {0,0}, {0,1}, {1,0}, {1,1}, {1,1}, {0,0}, {0,1}, {0,0}, {1,0},
	{0,1}, {0,0}, {1,1}, {1,0}, {1,1}, {0,1}, {1,1}, {1,1}, {0,0}, {0,1},
	{1,0}, {0,1}, {1,0}, {0,0}, {1,0}, {1,0}, {1,0}, {0,1}, {1,1}, {1,1},
	{1,1}, {1,1}, {0,0}, {1,1}, {0,0}, {1,1}, {0,0}, {0,1}, {0,0}, {1,1},
	{1,1}, {1,0}, {1,1}, {0,0}, {1,0}, {0,0}, {0,0}, {1,0}, {0,1}, {1,0},
	{0,1}, {1,0}, {0,0}, {0,1}, {0,1}, {0,1}, {0,0}, {1,1}, {0,1}, {1,1},
	{1,0}, {1,0}, {1,1}, {1,0}, {1,0}, {1,0}, {1,0}, {0,1}, {1,1}, {0,0},
	{1,1}, {0,0}, {1,0}, {0,0}, {1,1}, {0,1}, {1,0}, {1,0}, {1,1}, {1,1},
	{0,1}, {1,1}, {0,1}, {0,0}, {0,1}, {0,1}, {1,1}, {1,1}, {1,0}, {0,1},
	{1,1}, {0,0}, {0,1}, {1,0}, {0,0}, {0,1}, {0,1}, {1,1}, {1,1}, {0,0},
	{0,0}, {1,1}, {0,0}, {1,1}, {1,0}, {0,0}, {1,0}, {0,1}, {1,1}, {1,1},
	{0,1}, {0,0}, {1,1}, {1,1}, {1,0}, {1,0}, {0,0}, {1,1}, {0,0}, {1,0},
	{1,1}, {0,0}, {0,1}, {0,1}, {1,1}, {1,1}, {0,0}, {0,1}, {1,1}, {1,1},
	{1,1}, {1,1}, {1,1}, {1,0}, {0,1}, {0,0}, {1,0}, {0,1}, {1,0}, {0,0},
	{1,1}, {0,0}, {1,1}, {0,1}, {0,0}, {1,1}, {1,1}, {0,0}, {0,0}, {1,1},
	{1,0}, {0,0}, {0,0}, {1,0}, {0,0}, {1,1}, {0,1}, {0,0}, {1,1}, {1,0},
	{1,1}, {1,0}, {1,0}, {0,0}, {1,1}, {0,0}, {0,0}, {1,0}, {1,0}, {1,1},
	{0,0}, {0,1}, {0,0}, {0,0}, {1,0}, {0,1}, {0,0}, {0,1}, {1,0}, {1,1},
	{0,0}, {1,1}, {1,1}, {0,1}, {0,1}, {1,1}, {0,1}, {0,1}, {0,0}, {1,1},
	{0,0}, {1,1}, {0,1}, {0,1}, {1,1}, {1,0}, {0,0}, {1,0}, {1,0}, {1,1},
	{1,1}, {0,0}, {1,1}, {1,1}, {1,0}, {0,1}, {1,0}, {0,0}, {1,0}, {0,1},
	{1,0}, {1,0}, {1,1}, {0,0}, {0,0}, {1,1}, {1,0}, {0,0}, {0,1}, {1,1},
	{1,0}, {1,0}, {0,0}, {1,1}, {1,0}, {1,1}, {0,0}, {1,1}, {1,1}, {1,1},
	{0,0}, {0,0}, {1,0}, {1,1}, {1,0}, {0,0}, {1,1}, {0,1}, {1,1}, {0,0},
	{0,0}, {1,0}, {1,0}, {1,0}, {0,0}, {1,0}, {0,1}, {0,0}, {0,0}, {0,1},
	{0,0}, {1,1}, {1,0}, {1,1}, {1,0}, {0,1}, {1,0}, {1,1}, {0,0}, {1,0},
	{1,0}, {1,1}, {0,1}, {0,0}, {0,1}, {0,1}, {1,0}, {1,0}, {0,0}, {0,1},
	{0,1}, {1,0}, {0,0}, {0,1}, {1,0}, {0,1}, {1,0}, {0,0}, {0,1}, {1,1},
	{0,0}, {1,0}, {1,0}, {0,0}, {0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,0},
	{0,1}, {0,1}, {1,0}, {0,0}, {0,1}, {0,1}, {1,0}, {0,0}, {1,1}, {1,0},
	{0,0}, {1,1}, {0,1}, {0,1}, {1,1}, {0,1}, {1,1}, {1,0}, {1,0}, {0,1},
	{1,1}, {0,1}, {0,0}, {1,1}, {1,0}, {0,0}, {0,1}, {0,0}, {1,1}, {0,1},
	{1,1}, {1,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,1}, {0,0}, {1,0}, {0,1},
	{1,1}, {0,1}, {0,1}, {1,0}, {0,1}, {0,1}, {1,1}, {0,0}, {1,1}, {1,0},
	{1,1}, {1,0}, {1,0}, {0,1}, {1,0}, {1,1}, {1,0}, {1,0}, {0,1}, {0,0},
	{0,0}, {1,1}, {1,1}, {0,1}, {0,0}, {0,1}, {1,0}, {1,1}, {0,1}, {1,1},
	{1,0}, {1,0}, {0,1}, {1,1}, {1,1}, {0,1}, {0,0}, {0,1}, {0,0}, {1,1},
	{1,1}, {1,0}, {1,1}, {0,1}, {0,1}, {1,1}, {0,0}, {0,0}, {1,0}, {1,1},
	{1,1}, {0,0}, {0,1}, {1,0}, {0,1}, {1,1}, {1,0}, {0,1}, {0,0}, {0,1},
	{1,0}, {0,1}, {0,1}, {0,0}, {0,0}, {1,0}, {1,1}, {0,1}, {1,1}, {1,0},
	{1,0}, {1,1}, {1,0}, {1,0}, {1,1}, {0,0}, {0,0}, {1,0}, {1,1}, {1,1},
	{0,1}, {0,1}, {0,1}, {1,1}, {1,0}, {0,1}, {1,1}, {0,0}, {1,0}, {0,1},
	{0,1}, {1,1}, {0,1}, {1,0}, {0,0}, {0,0}, {0,0}, {0,0}, {1,1}, {0,1},
	{0,0}, {0,0}, {0,0}, {1,0}, {0,1}, {0,1}, {1,0}, {0,0}, {1,1}, {1,1},
	{0,0}, {1,1}, {1,0}, {1,1}, {1,0}, {1,1}, {0,0}, {1,0}, {0,1}, {1,1},
	{1,0}, {1,1}, {0,0}, {0,1}, {0,1}, {1,1}, {0,1}, {1,1}, {1,0}, {1,1},
	{0,1}, {0,0}, {1,1}, {0,1}, {0,0}, {0,0}, {0,0}, {1,0}, {1,1}, {1,1},
	{1,1}, {1,0}, {0,0}, {1,0}, {1,1}, {0,1}, {1,1}, {0,0}, {1,1}, {0,1},
	{1,1}, {0,1}, {0,0}, {0,0}, {1,1}, {0,1}, {1,0}, {1,1}, {1,0}, {1,0},
	{0,0}, {1,0}, {1,1}, {0,1}, {0,1}, {1,0}, {1,1}, {1,0}, {0,0}, {0,0},
	{1,0}, {1,0}, {1,0}, {1,0}, {1,0}, {0,1}, {0,1}, {0,1}, {0,0}, {0,0},
	{1,1}, {1,1}, {1,0}, {0,0}, {0,1}, {1,1}, {0,0}, {0,0}, {1,0}, {0,0},
	{1,1}, {0,0}, {1,0}, {1,1}, {0,0}, {0,0}, {1,0}, {0,0}, {1,1}, {1,0},
	{1,0}, {1,0}, {0,1}, {0,1}, {1,0}, {0,0}, {1,0}, {1,1}, {1,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {1,0}, {0,1}, {1,1}, {0,0}, {1,0}, {1,1},
	{0,0}, {1,0}, {1,0}, {0,1}, {0,0}, {1,0}, {1,0}, {0,1}, {0,1}, {0,1},
	{0,0}, {1,1}, {1,0}, {0,1}, {1,0}, {0,1}, {1,1}, {1,1}, {0,0}, {1,1},
	{1,0}, {1,0}, {0,1}, {1,1}, {0,1}, {0,0}, {0,0}, {1,1}, {0,0}, {1,1},
	{0,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,0}, {1,0}, {0,0}, {1,1}, {0,0},
	{0,1}, {0,0}, {1,0}, {1,1}, {1,0}, {0,1}, {0,1}, {1,0}, {1,0}, {0,1},
	{1,1}, {1,1}, {1,0}, {0,0}, {0,1}, {1,1}, {0,1}, {1,0}, {0,0}, {0,0},
	{0,1}, {1,0}, {1,1}, {0,1}, {1,0}, {0,0}, {0,1}, {0,0}, {0,1}, {0,1},
	{0,1}, {1,0}, {1,1}, {0,0}, {0,1}, {1,1}, {0,0}, {0,0}, {1,1}, {0,0},
	{0,0}, {0,1}, {1,0}, {0,1}, {0,0}, {0,0}, {0,0}, {0,0}, {1,1}, {0,0},
	{0,0}, {1,1}, {1,1}, {0,0}, {1,1}, {1,1}, {0,0}, {1,0}, {0,1}, {0,1},
	{1,0}, {1,1}, {1,1}, {1,1}, {0,1}, {0,0}, {1,1}, {1,0}, {0,0}, {1,0},
	{0,1}, {1,1}, {0,1}, {1,0}, {1,0}, {0,0}, {0,0}, {0,1}, {0,0}, {1,1},
	{0,0}, {1,1}, {1,0}, {1,0}, {0,1}, {1,0}, {1,0}, {0,0}, {1,1}, {0,0},
	{0,1}, {0,0}, {1,0}, {1,0}, {0,0}, {0,0}, {0,0}, {0,1}, {1,0}, {0,0},
	{0,0}, {1,0}, {1,0}, {1,1}, {1,0}, {1,0}, {1,0}, {1,1}, {0,0}, {1,0},
	{1,1}, {1,1}, {0,0}, {0,0}, {0,1}, {1,1}, {1,1}, {1,0}, {1,1}, {0,1},
	{0,0}, {0,0}, {0,1}, {0,0}, {0,1}, {1,0}, {1,1}, {1,1}, {0,0}, {1,1},
	{1,1}, {0,0}, {1,1}, {1,1}, {0,1}, {0,1}, {0,1}, {1,0}, {0,1}, {1,0},
	{0,0}, {0,0}, {0,0}, {1,1}, {0,0}, {0,1}, {1,1}, {1,0}, {0,1}, {0,0},
	{1,1}, {0,1}, {0,1}, {0,0}, {0,1}, {0,1}, {1,0}, {1,0}, {0,0}, {1,0},
	{0,0}, {0,1}, {1,0}, {0,1}, {0,1}, {1,0}, {1,0}, {1,0}, {1,0}, {0,0},
	{1,0}, {1,0}, {1,0}, {1,0}, {1,1}, {0,0}, {0,1}, {0,1}, {1,1}, {0,0},
	{1,1}, {0,1}, {1,1}, {0,0}, {1,1}, {1,1}, {0,1}, {1,1}, {0,1}, {0,0},
	{0,0}, {0,0}, {1,1}, {0,1}, {1,1}, {0,1}, {0,1}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,1}, {1,0}, {0,0}, {1,0}, {1,0}, {0,1}, {1,1}, {1,1}, {0,1},
	{1,1}, {1,1}, {1,1}, {1,1}, {0,0}, {0,1}, {1,1}, {1,1}, {1,0}, {1,0},
	{0,0}, {0,0}, {0,1}, {1,1}, {0,1}, {0,1}, {0,0}, {1,1}, {1,0}, {1,0},
	{1,1}, {1,1}, {1,0}, {0,0}, {1,1}, {1,0}, {0,1}, {0,1}, {0,0}, {1,0},
	{1,0}, {1,1}, {0,0}, {1,1}, {0,1}, {1,1}, {1,0}, {0,0}, {1,1}, {0,0},
	{1,1}, {0,0}, {0,1}, {0,0}, {0,0}, {1,0}, {1,0}, {0,1}, {0,1}, {1,0},
	{1,0}, {0,0}, {1,1}, {1,1}, {0,0}, {0,1}, {0,1}, {0,1}, {0,0}, {1,1},
	{1,0}, {0,0}, {0,0}, {0,1}, {0,0}, {0,1}, {0,1}, {0,1}, {0,1}, {1,0},
	{1,0}, {0,1}, {1,0}, {1,0}, {0,0}, {1,1}, {1,1}, {0,0}, {1,1}, {0,0},
	{1,0}, {1,1}, {1,1}, {0,0}, {1,1}, {0,1}, {1,1}, {1,0}, {1,0}, {1,0},
	{0,1}, {0,1}, {0,0}, {0,0}, {1,0}, {0,0}, {1,0}, {0,1}, {1,1}, {0,1},
	{0,0}, {1,1}, {0,1}, {0,0}, {0,1}, {0,1}, {1,1}, {0,0}, {1,1}, {1,1},
	{0,0}, {0,1}, {1,0}, {0,1}, {0,1}, {1,0}, {1,0}, {0,0}, {0,0}, {0,1},
	{0,0}, {0,1}, {1,1}, {0,1}, {0,1}, {0,1}, {1,0}, {0,0}, {0,1}, {1,1},
	{0,0}, {0,0}, {0,0}, {0,1}, {0,1}, {0,1}, {1,1}, {0,0}, {0,0}, {0,1},
	{0,1}, {0,1}, {0,1}, {0,1}, {0,1}, {0,1}, {1,0}, {0,0}, {0,0}, {1,0},
	{0,0}, {0,0}, {1,1}, {0,1}, {0,1}, {1,1}, {1,0}, {0,0}, {1,0}, {0,1},
	{0,0}, {1,0}, {1,0}, {0,1}, {1,0}, {0,1}, {1,1}, {1,0}, {1,1}, {1,1},
	{1,1}, {0,0}, {0,0}, {1,1}, {1,1}, {0,0}, {0,1}, {0,1}, {1,0}, {1,1},
	{1,1}, {1,0}, {1,1}, {0,0}, {0,1}, {1,0}, {0,0}, {0,1}, {0,1}, {1,1},
	{1,1}, {1,1}, {0,0}, {1,0}, {1,1}, {1,0}, {1,0}, {1,1}, {1,1}, {0,0},
	{0,1}, {0,0}, {0,0}, {1,1}, {1,0}, {1,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,1}, {0,1}, {0,0},
};

static int cacode(int chip, int sv)
{
	int g2chip = chip + SV[sv].advance;
	if(g2chip >= 1023)
		g2chip -= 1023;
	return ca_code_states[chip].G1 ^ ca_code_states[g2chip].G2;
}

static void read_samples(fftw_complex *data, unsigned int data_len)
{
	unsigned int i;
	float buf[2];
	for(i = 0; i < data_len; ++i)
	{
		fread(buf, sizeof(float), 2, stdin);
		data[i][0] = buf[0];
		data[i][1] = buf[1];
	}

	if(TRACE)
	{
		printf("# input data\n");
		for(i = 0; i < data_len; ++i)
			printf("%f\t%f\n", data[i][0], data[i][1]);
		printf("\n");
	}

	fftw_plan p = fftw_plan_dft_1d(data_len, data, data, FFTW_FORWARD, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);
	fftw_execute(p);
	fftw_destroy_plan(p);

	if(TRACE)
		printf("# input data FFT\n");
	for(i = 0; i < data_len; ++i)
	{
		if(TRACE)
			printf("%f\t%f\n", data[i][0], data[i][1]);
		/* precompute the complex conjugate of the data FFT */
		data[i][1] = -data[i][1];
	}
	if(TRACE)
		printf("\n");
}

static void complex_mul(fftw_complex to, fftw_complex a, fftw_complex b)
{
	to[0] = a[0] * b[0] - a[1] * b[1];
	to[1] = a[1] * b[0] + a[0] * b[1];
}

static void complex_conj_mul(fftw_complex to, fftw_complex a, fftw_complex b)
{
	to[0] = a[0] * b[0] + a[1] * b[1];
	to[1] = a[1] * b[0] - a[0] * b[1];
}

static void update_stats(struct signal_strength *stats, double bin_width, int shift, double phase, double snr_0, double snr_1, double snr_2)
{
	double shift_correction;
	/* ignore this sample if it is not a local peak */
	if(snr_0 > snr_1 || snr_2 > snr_1)
		return;
	/* take only the highest peak */
	if(snr_1 <= stats->snr)
		return;

	/* do a quadratic interpolation of the three points around this peak */
	/* XXX: there's no reason to believe the peaks are parabolic. */
	shift_correction = 0.5 * (snr_0 - snr_2) / (snr_0 - 2 * snr_1 + snr_2);
	stats->snr = snr_1 - 0.25 * (snr_0 - snr_2) * shift_correction;
	stats->doppler = (shift + shift_correction) * bin_width;
	stats->phase = phase;
}

static struct signal_strength check_satellite(unsigned int sample_freq, fftw_complex *data_fft, unsigned int data_fft_len, int sv)
{
	struct signal_strength stats;
	const unsigned int len = sample_freq / 1000;
	const unsigned int fft_len = len / 2 + 1;
	fftw_complex *prod = fftw_malloc(sizeof(fftw_complex) * len);
	void *ca_buf = fftw_malloc(sizeof(fftw_complex) * fft_len);
	double *ca_samples = ca_buf;
	fftw_complex *ca_fft = ca_buf;
	const double samples_per_chip = sample_freq / 1023e3;
	const int max_shift = 5000 * data_fft_len / sample_freq;
	const double bin_width = (double) sample_freq / data_fft_len;
	double snr_1 = 0, snr_2 = 0, best_phase_1 = 0;
	unsigned int i;
	int shift;
	fftw_plan fft = fftw_plan_dft_r2c_1d(len, ca_samples, ca_fft, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);
	fftw_plan ifft = fftw_plan_dft_1d(len, prod, prod, FFTW_BACKWARD, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);

	for(i = 0; i < len; ++i)
		ca_samples[i] = cacode((int) (i / samples_per_chip), sv) ? 1 : -1;

	fftw_execute(fft);
	fftw_destroy_plan(fft);

	if(TRACE)
	{
		printf("# SV %d C/A code FFT\n", SV[sv].PRN);
		for(i = 0; i < fft_len; ++i)
			printf("%f\t%f\n", ca_fft[i][0], ca_fft[i][1]);
		printf("\n");
	}

	if(TRACE)
		printf("# SV %d correlation\n", SV[sv].PRN);
	stats.snr = 0;
	for(shift = -max_shift; shift <= max_shift; ++shift)
	{
		const double doppler = shift * bin_width;
		double max_pwr = 0, tot_pwr = 0, best_phase = 0, snr;
		for(i = 0; i < len / 2; ++i)
		{
			complex_mul(prod[i], data_fft[(i * (data_fft_len / len) + shift + data_fft_len) % data_fft_len], ca_fft[i]);
			complex_conj_mul(prod[len - 1 - i], data_fft[((len - 1 - i) * (data_fft_len / len) + shift + data_fft_len) % data_fft_len], ca_fft[i + 1]);
		}

		fftw_execute(ifft);

		for(i = 0; i < len; ++i)
		{
			double pwr = prod[i][0] * prod[i][0] + prod[i][1] * prod[i][1];
			double phase = i * (1023.0 / len);
			if(TRACE)
				printf("%f\t%f\t%f\n", doppler, phase, pwr);
			if(pwr > max_pwr)
			{
				max_pwr = pwr;
				best_phase = phase;
			}
			tot_pwr += pwr;
		}

		snr = max_pwr / (tot_pwr / len);
		update_stats(&stats, bin_width, shift - 1, best_phase_1, snr_2, snr_1, snr);
		if(TRACE)
			printf("# best for doppler %f: code phase %f, S/N %f\n", doppler, best_phase, snr);

		snr_2 = snr_1;
		snr_1 = snr;
		best_phase_1 = best_phase;
	}
	update_stats(&stats, bin_width, max_shift, best_phase_1, snr_2, snr_1, 0);
	if(TRACE)
		printf("\n");

	fftw_destroy_plan(ifft);
	fftw_free(ca_buf);
	fftw_free(prod);
	return stats;
}

int main()
{
	const unsigned int sample_freq = 4000000;
	const unsigned int data_len = sample_freq * 20 / 1000;
	fftw_complex *data = fftw_malloc(sizeof(fftw_complex) * data_len);
	struct signal_strength signals[sizeof SV / sizeof *SV];
	int i;

	read_samples(data, data_len);
	for(i = 0; i < (sizeof SV / sizeof *SV); ++i)
		signals[i] = check_satellite(sample_freq, data, data_len, i);

	printf("# SV, S/N ratio, doppler shift (Hz), phase (chips)\n");
	for(i = 0; i < (sizeof SV / sizeof *SV); ++i)
		printf("%d\t%f\t%f\t%f\n", SV[i].PRN, signals[i].snr, signals[i].doppler, signals[i].phase);
	printf("\n");

	fftw_free(data);
	fftw_cleanup();
	return 0;
}
