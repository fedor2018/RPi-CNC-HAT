/*    Copyright (C) 2014 GP Orcullo
 *
 *    Portions of this code is based on stepgen.c by John Kasunich
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *    V0.1 PJS 05NOV2014 updates for PICnc 5axis boards
 *    V0.2 PJS 05NOV2014 converted the output names and added the remaining outputs 
 */

#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "picnc-5a.h"

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#define MODNAME "picnc-5a"
#define PREFIX "picnc-5a"

MODULE_AUTHOR("GP Orcullo modified by P. Shabino");
MODULE_DESCRIPTION("Driver for PICnc 5axis boards");
MODULE_LICENSE("GPL v2");

static int stepwidth = 1;
RTAPI_MP_INT(stepwidth, "Step width in 1/BASEFREQ");

static long pwmfreq = 35000;
RTAPI_MP_LONG(pwmfreq, "PWM frequency in Hz");

typedef struct{
	hal_float_t *position_cmd[NUMAXES],
	            *velocity_cmd[NUMAXES],
	            *position_fb[NUMAXES];
	hal_bit_t   // outputs
				*out1,		//enable
				*out2,		//beeper 
				*out3,  	//Laser fire
				*out4,  	//PWM Laser power
				*out5,  	//PWM Lights
				*out6,  	//PWM Air valve
				*out7,  	//PS on
				*out8,  	
				*out9,  	
			#if NUMAXES < 5 /* CONFIGURE_B_AXIS */
				*out10,  	//Bdir
				*out11,  	//Bstep
			#endif /* CONFIGURE_B_AXIS */
			#if NUMAXES < 4 /* CONFIGURE_A_AXIS */
				*out12,  	//Adir
				*out13,  	//Astep
			#endif /* CONFIGURE_A_AXIS */
			#if NUMAXES < 3 /* CONFIGURE_Z_AXIS */
				*out14,  	//Zdir
				*out15,  	//Zstep
			#endif /* CONFIGURE_Z_AXIS */
			#if NUMAXES < 2 /* CONFIGURE_Y_AXIS */
				*out16,  	//Ydir
				*out17,  	//Ystep
			#endif /* CONFIGURE_Y_AXIS */
			#if NUMAXES < 1 /* CONFIGURE_X_AXIS */
				*out18,  	//Xdir
				*out19,  	//Xstep
			#endif /* CONFIGURE_X_AXIS */
				// inputs
	            *in1,		//stop
	            *in2,		//EPO
	            *in3,		//I2C fault
	            *in4,		//door
	            *in5,		//laser off
	            *in6,		//A block
	            *in7,	
	            *in8,		//B home
	            *in9,		//A home
	            *in10,		//Z home
	            *in11,		//Y home
	            *in12,		//X home
	            *ready,
				*spi_fault;
	hal_float_t scale[NUMAXES],
	            maxaccel[NUMAXES];
	hal_u32_t   *test;
} data_t;

static data_t *data;

static int comp_id;
static const char *modname = MODNAME;
static const char *prefix = PREFIX;

volatile unsigned *gpio, *spi;

volatile int32_t txBuf[BUFSIZE], rxBuf[BUFSIZE];
static u32 pwm_period = 0;

static double dt = 0,				/* update_freq period in seconds */
	recip_dt = 0,				/* reciprocal of period, avoids divides */
	scale_inv[NUMAXES] = { 1.0 },		/* inverse of scale */
	old_vel[NUMAXES] = { 0 },
	old_pos[NUMAXES] = { 0 },
	old_scale[NUMAXES] = { 0 },
	max_vel;
static long old_dtns = 0;			/* update_freq funct period in nsec */
static s32 accum_diff = 0,
	old_count[NUMAXES] = { 0 };
static s64 accum[NUMAXES] = { 0 };		/* 64 bit DDS accumulator */

static void read_spi(void *arg, long period);
static void write_spi(void *arg, long period);
static void update(void *arg, long period);
static void update_outputs(data_t *dat);
static void update_inputs(data_t *dat);
static void read_buf();
static void write_buf();
static int map_gpio();
static void setup_gpio();
static void restore_gpio();

int is_rpi(void){
	FILE *fp;
	char buf[1024];
	size_t fsize;

	fp = fopen("/proc/cpuinfo", "r");
	fsize = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);
	
	if (fsize == 0 || fsize == sizeof(buf))
		return 0;

	/* NUL terminate the buffer */
	buf[fsize] = '\0';

	/* check if this is running on a broadcom chip */
	if (NULL == strstr(buf, "BCM2708"))
		return 0;
	else
		return -1;
}


int rtapi_app_main(void){
	char name[HAL_NAME_LEN + 1];
	int n, retval;

	/* make sure we are running on an RPi */
	if (!is_rpi()) {
		rtapi_print_msg(RTAPI_MSG_ERR, 
			"%s: ERROR: This driver is for the Raspberry Pi platform only\n",
		        modname);
		return -1;
	}
	
	/* initialise driver */
	comp_id = hal_init(modname);
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n",
		        modname);
		return -1;
	}

	/* allocate shared memory */
	data = hal_malloc(sizeof(data_t));
	if (data == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n",
		        modname);
		hal_exit(comp_id);
		return -1;
	}

	/* configure board */
	retval = map_gpio();
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: cannot map GPIO memory\n", modname);
		return retval;
	}

	setup_gpio();
	
	pwm_period = (SYS_FREQ/pwmfreq) - 1;	/* PeripheralClock/pwmfreq - 1 */

	txBuf[0] = 0x4746433E;		/* this is config data (>CFG) */
	txBuf[1] = stepwidth;
	txBuf[2] = pwm_period;
	write_buf();			/* send config data */

	max_vel = BASEFREQ/(4.0 * stepwidth);	/* calculate velocity limit */

	/* export pins and parameters */
	for (n=0; n<NUMAXES; n++){
		retval = hal_pin_float_newf(HAL_IN, &(data->position_cmd[n]),
		        comp_id, "%s.axis.%01d.position-cmd", prefix, n);
		if (retval < 0) goto error;
		*(data->position_cmd[n]) = 0.0;

		retval = hal_pin_float_newf(HAL_IN, &(data->velocity_cmd[n]),
		        comp_id, "%s.axis.%01d.velocity-cmd", prefix, n);
		if (retval < 0) goto error;
		*(data->position_cmd[n]) = 0.0;

		retval = hal_pin_float_newf(HAL_OUT, &(data->position_fb[n]),
		        comp_id, "%s.axis.%01d.position-fb", prefix, n);
		if (retval < 0) goto error;
		*(data->position_fb[n]) = 0.0;

		retval = hal_param_float_newf(HAL_RW, &(data->scale[n]),
		        comp_id, "%s.axis.%01d.scale", prefix, n);
		if (retval < 0) goto error;
		data->scale[n] = 1.0;

		retval = hal_param_float_newf(HAL_RW, &(data->maxaccel[n]),
		        comp_id, "%s.axis.%01d.maxaccel", prefix, n);
		if (retval < 0) goto error;
		data->maxaccel[n] = 1.0;
	}

	// inputs
	retval = hal_pin_bit_newf(HAL_OUT, &(data->in1), comp_id, "%s.in.1", prefix);
	if (retval < 0) goto error;
	*(data->in1) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in2), comp_id, "%s.in.2", prefix);
	if (retval < 0) goto error;
	*(data->in2) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in3), comp_id, "%s.in.3", prefix);
	if (retval < 0) goto error;
	*(data->in3) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in4), comp_id, "%s.in.4", prefix);
	if (retval < 0) goto error;
	*(data->in4) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in5), comp_id, "%s.in.5", prefix);
	if (retval < 0) goto error;
	*(data->in5) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in6), comp_id, "%s.in.6", prefix);
	if (retval < 0) goto error;
	*(data->in6) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in7), comp_id, "%s.in.7", prefix);
	if (retval < 0) goto error;
	*(data->in7) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in8), comp_id, "%s.in.8", prefix);
	if (retval < 0) goto error;
	*(data->in8) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in9), comp_id, "%s.in.9", prefix);
	if (retval < 0) goto error;
	*(data->in9) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in10), comp_id, "%s.in.10", prefix);
	if (retval < 0) goto error;
	*(data->in10) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in11), comp_id, "%s.in.11", prefix);
	if (retval < 0) goto error;
	*(data->in11) = 0;

	retval = hal_pin_bit_newf(HAL_OUT, &(data->in12), comp_id, "%s.in.12", prefix);
	if (retval < 0) goto error;
	*(data->in12) = 0;

	// outputs
	retval = hal_pin_bit_newf(HAL_IN, &(data->out1), comp_id, "%s.out.1", prefix);
	if (retval < 0) goto error;
	*(data->out1) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out2), comp_id, "%s.out.2", prefix);
	if (retval < 0) goto error;
	*(data->out2) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out3), comp_id, "%s.out.3", prefix);
	if (retval < 0) goto error;
	*(data->out3) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out4), comp_id, "%s.out.4", prefix);
	if (retval < 0) goto error;
	*(data->out4) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out5), comp_id, "%s.out.5", prefix);
	if (retval < 0) goto error;
	*(data->out5) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out6), comp_id, "%s.out.6", prefix);
	if (retval < 0) goto error;
	*(data->out6) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out7), comp_id, "%s.out.7", prefix);
	if (retval < 0) goto error;
	*(data->out7) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out8), comp_id, "%s.out.8", prefix);
	if (retval < 0) goto error;
	*(data->out8) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out9), comp_id, "%s.out.9", prefix);
	if (retval < 0) goto error;
	*(data->out9) = 0;
#if NUMAXES < 5 /* CONFIGURE_B_AXIS */
	retval = hal_pin_bit_newf(HAL_IN, &(data->out10), comp_id, "%s.out.10", prefix);
	if (retval < 0) goto error;
	*(data->out10) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out11), comp_id, "%s.out.11", prefix);
	if (retval < 0) goto error;
	*(data->out11) = 0;
#endif /* CONFIGURE_B_AXIS */
#if NUMAXES < 4 /* CONFIGURE_A_AXIS */
	retval = hal_pin_bit_newf(HAL_IN, &(data->out12), comp_id, "%s.out.12", prefix);
	if (retval < 0) goto error;
	*(data->out12) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out13), comp_id, "%s.out.13", prefix);
	if (retval < 0) goto error;
	*(data->out13) = 0;
#endif /* CONFIGURE_A_AXIS */
#if NUMAXES < 3 /* CONFIGURE_Z_AXIS */
	retval = hal_pin_bit_newf(HAL_IN, &(data->out14), comp_id, "%s.out.14", prefix);
	if (retval < 0) goto error;
	*(data->out14) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out15), comp_id, "%s.out.15", prefix);
	if (retval < 0) goto error;
	*(data->out15) = 0;
#endif /* CONFIGURE_Z_AXIS */
#if NUMAXES < 2 /* CONFIGURE_Y_AXIS */
	retval = hal_pin_bit_newf(HAL_IN, &(data->out16), comp_id, "%s.out.16", prefix);
	if (retval < 0) goto error;
	*(data->out16) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out17), comp_id, "%s.out.17", prefix);
	if (retval < 0) goto error;
	*(data->out17) = 0;
#endif /* CONFIGURE_Y_AXIS */
#if NUMAXES < 1 /* CONFIGURE_X_AXIS */
	retval = hal_pin_bit_newf(HAL_IN, &(data->out18), comp_id, "%s.out.18", prefix);
	if (retval < 0) goto error;
	*(data->out18) = 0;

	retval = hal_pin_bit_newf(HAL_IN, &(data->out19), comp_id, "%s.out.19", prefix);
	if (retval < 0) goto error;
	*(data->out19) = 0;
#endif /* CONFIGURE_X_AXIS */

	// virtual pins for control/status
	retval = hal_pin_bit_newf(HAL_OUT, &(data->ready), comp_id, "%s.ready", prefix);
	if (retval < 0) goto error;
	*(data->ready) = 0;

	retval = hal_pin_bit_newf(HAL_IO, &(data->spi_fault), comp_id, "%s.spi_fault", prefix);
	if (retval < 0) goto error;
	*(data->spi_fault) = 0;

	retval = hal_pin_u32_newf(HAL_IN, &(data->test), comp_id, "%s.test", prefix);
	if (retval < 0) goto error;
	*(data->test) = 0;
error:
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: pin export failed with err=%i\n",
		        modname, retval);
		hal_exit(comp_id);
		return -1;
	}

	/* export functions */
	rtapi_snprintf(name, sizeof(name), "%s.read", prefix);
	retval = hal_export_funct(name, read_spi, data, 1, 0, comp_id);
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: read function export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}
	rtapi_snprintf(name, sizeof(name), "%s.write", prefix);
	/* no FP operations */
	retval = hal_export_funct(name, write_spi, data, 0, 0, comp_id);
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: write function export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}
	rtapi_snprintf(name, sizeof(name), "%s.update", prefix);
	retval = hal_export_funct(name, update, data, 1, 0, comp_id);
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: update function export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}

	rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);
	hal_ready(comp_id);
	return 0;
}

void rtapi_app_exit(void){
	restore_gpio();
	munmap((void *)gpio,BLOCK_SIZE);
	munmap((void *)spi,BLOCK_SIZE);
	hal_exit(comp_id);
}

void read_spi(void *arg, long period){
	int i;
	static int startup = 0;
	data_t *dat = (data_t *)arg;

	read_buf();
	/* update input status */
	update_inputs(dat);
	*(dat->test) = rxBuf[2];

	/* command >CM2 */
	txBuf[0] = 0x324D433E;
	update_outputs(dat);

	write_buf();

	/* check for change in period */
	if (period != old_dtns) {
		old_dtns = period;
		dt = period * 0.000000001;
		recip_dt = 1.0 / dt;
	}

	/* check for scale change */
	for (i = 0; i < NUMAXES; i++) {
		if (dat->scale[i] != old_scale[i]) {
			old_scale[i] = dat->scale[i];
			/* scale must not be 0 */
			if ((dat->scale[i] < 1e-20) && (dat->scale[i] > -1e-20))
				dat->scale[i] = 1.0;
			scale_inv[i] = (1.0 / STEP_MASK) / dat->scale[i];
		}
	}

	read_buf();

	/* sanity check */
	if (rxBuf[0] == (0x314D433E ^ ~0)) {
		*(dat->ready) = 1;
	} else {
		*(dat->ready) = 0;
		if (!startup)
			startup = 1;
		else
			*(dat->spi_fault) = 1;
	}

	/* update outputs */
	for (i = 0; i < NUMAXES; i++) {
		/* the DDS uses 32 bit counter, this code converts
		   that counter into 64 bits */
		accum_diff = get_position(i) - old_count[i];
		old_count[i] = get_position(i);
		accum[i] += accum_diff;

		*(dat->position_fb[i]) = (float)(accum[i]) * scale_inv[i];
	}
}

void write_spi(void *arg, long period){
	write_buf();
}

void update(void *arg, long period){
	int i;
	data_t *dat = (data_t *)arg;
	double max_accl, vel_cmd, dv, new_vel,
	       dp, pos_cmd, curr_pos, match_accl, match_time, avg_v,
	       est_out, est_cmd, est_err;

	for (i = 0; i < NUMAXES; i++) {
		/* set internal accel limit to its absolute max, which is
		   zero to full speed in one thread period */
		max_accl = max_vel * recip_dt;

		/* check for user specified accel limit parameter */
		if (dat->maxaccel[i] <= 0.0) {
			/* set to zero if negative */
			dat->maxaccel[i] = 0.0;
		} else {
			/* parameter is non-zero, compare to max_accl */
			if ((dat->maxaccel[i] * fabs(dat->scale[i])) > max_accl) {
				/* parameter is too high, lower it */
				dat->maxaccel[i] = max_accl / fabs(dat->scale[i]);
			} else {
				/* lower limit to match parameter */
				max_accl = dat->maxaccel[i] * fabs(dat->scale[i]);
			}
		}

		/* calculate position command in counts */
		pos_cmd = *(dat->position_cmd[i]) * dat->scale[i];
		/* calculate velocity command in counts/sec */
		vel_cmd = (pos_cmd - old_pos[i]) * recip_dt;
		old_pos[i] = pos_cmd;

		/* apply frequency limit */
		if (vel_cmd > max_vel) {
			vel_cmd = max_vel;
		} else if (vel_cmd < -max_vel) {
			vel_cmd = -max_vel;
		}

		/* determine which way we need to ramp to match velocity */
		if (vel_cmd > old_vel[i])
			match_accl = max_accl;
		else
			match_accl = -max_accl;

		/* determine how long the match would take */
		match_time = (vel_cmd - old_vel[i]) / match_accl;
		/* calc output position at the end of the match */
		avg_v = (vel_cmd + old_vel[i]) * 0.5;
		curr_pos = (double)(accum[i]) * (1.0 / STEP_MASK);
		est_out = curr_pos + avg_v * match_time;
		/* calculate the expected command position at that time */
		est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * dt);
		/* calculate error at that time */
		est_err = est_out - est_cmd;

		if (match_time < dt) {
			/* we can match velocity in one period */
			if (fabs(est_err) < 0.0001) {
				/* after match the position error will be acceptable */
				/* so we just do the velocity match */
				new_vel = vel_cmd;
			} else {
				/* try to correct position error */
				new_vel = vel_cmd - 0.5 * est_err * recip_dt;
				/* apply accel limits */
				if (new_vel > (old_vel[i] + max_accl * dt)) {
					new_vel = old_vel[i] + max_accl * dt;
				} else if (new_vel < (old_vel[i] - max_accl * dt)) {
					new_vel = old_vel[i] - max_accl * dt;
				}
			}
		} else {
			/* calculate change in final position if we ramp in the
			opposite direction for one period */
			dv = -2.0 * match_accl * dt;
			dp = dv * match_time;
			/* decide which way to ramp */
			if (fabs(est_err + dp * 2.0) < fabs(est_err)) {
				match_accl = -match_accl;
			}
			/* and do it */
			new_vel = old_vel[i] + match_accl * dt;
		}

		/* apply frequency limit */
		if (new_vel > max_vel) {
			new_vel = max_vel;
		} else if (new_vel < -max_vel) {
			new_vel = -max_vel;
		}

		old_vel[i] = new_vel;
		/* calculate new velocity cmd */
		update_velocity(i, (new_vel * VELSCALE));
	}

	/* this is a command (>CM1) */
	txBuf[0] = 0x314D433E;
}

void update_outputs(data_t *dat){
	//pjs float duty;
	//pjs int i;

	/* update pic32 output */
	txBuf[1]  = (*(dat->out1) ? 1l : 0) << 0;
	txBuf[1] |= (*(dat->out2) ? 1l : 0) << 1;
	txBuf[1] |= (*(dat->out3) ? 1l : 0) << 2;
	txBuf[1] |= (*(dat->out4) ? 1l : 0) << 3;
	txBuf[1] |= (*(dat->out5) ? 1l : 0) << 4;
	txBuf[1] |= (*(dat->out6) ? 1l : 0) << 5;
	txBuf[1] |= (*(dat->out7) ? 1l : 0) << 6;
	txBuf[1] |= (*(dat->out8) ? 1l : 0) << 7;
	txBuf[1] |= (*(dat->out9) ? 1l : 0) << 8;
#if NUMAXES < 5 /* CONFIGURE_B_AXIS */
	txBuf[1] |= (*(dat->out10) ? 1l : 0) << 9;
	txBuf[1] |= (*(dat->out11) ? 1l : 0) << 10;
#endif /* CONFIGURE_B_AXIS */
#if NUMAXES < 4 /* CONFIGURE_A_AXIS */
	txBuf[1] |= (*(dat->out12) ? 1l : 0) << 11;
	txBuf[1] |= (*(dat->out13) ? 1l : 0) << 12;
#endif /* CONFIGURE_A_AXIS */
#if NUMAXES < 3 /* CONFIGURE_Z_AXIS */
	txBuf[1] |= (*(dat->out14) ? 1l : 0) << 13;
	txBuf[1] |= (*(dat->out15) ? 1l : 0) << 14;
#endif /* CONFIGURE_Z_AXIS */
#if NUMAXES < 2 /* CONFIGURE_Y_AXIS */
	txBuf[1] |= (*(dat->out16) ? 1l : 0) << 15;
	txBuf[1] |= (*(dat->out17) ? 1l : 0) << 16;
#endif /* CONFIGURE_Y_AXIS */
#if NUMAXES < 1 /* CONFIGURE_X_AXIS */
	txBuf[1] |= (*(dat->out18) ? 1l : 0) << 17;
	txBuf[1] |= (*(dat->out19) ? 1l : 0) << 18;
#endif /* CONFIGURE_X_AXIS */
}

static s32 debounce(s32 A){
	static s32 B = 0;
	static s32 C = 0;
	static s32 Z = 0;

	Z = (Z & (A | B | C)) | (A & B & C);
	C = B;
	B = A;

	return Z;
}

void update_inputs(data_t *dat){
	//pjs int i;
	s32 x;

	x = debounce(rxBuf[1]);

	*(dat->in1)  = (x & 0b000000000001) ? 1 : 0;
	*(dat->in2)  = (x & 0b000000000010) ? 1 : 0;
	*(dat->in3)  = (x & 0b000000000100) ? 1 : 0;
	*(dat->in4)  = (x & 0b000000001000) ? 1 : 0;
	*(dat->in5)  = (x & 0b000000010000) ? 1 : 0;
	*(dat->in6)  = (x & 0b000000100000) ? 1 : 0;
	*(dat->in7)  = (x & 0b000001000000) ? 1 : 0;
	*(dat->in8)  = (x & 0b000010000000) ? 1 : 0;
	*(dat->in9)  = (x & 0b000100000000) ? 1 : 0;
	*(dat->in10) = (x & 0b001000000000) ? 1 : 0;
	*(dat->in11) = (x & 0b010000000000) ? 1 : 0;
	*(dat->in12) = (x & 0b100000000000) ? 1 : 0;
}

void read_buf(){
	char *buf;
	int i;

	/* read buffer */
	buf = (char *)rxBuf;
	for (i=0; i<SPIBUFSIZE; i++) {
		*buf++ = BCM2835_SPIFIFO;
	}

}

void write_buf(){
	char *buf;
	int i;

	/* activate transfer */
	BCM2835_SPICS = SPI_CS_TA;

	/* send txBuf */
	buf = (char *)txBuf;
	for (i=0; i<SPIBUFSIZE; i++) {
		BCM2835_SPIFIFO = *buf++;
	}

	/* wait until transfer is finished */
	while (!(BCM2835_SPICS & SPI_CS_DONE));

	/* deactivate transfer */
	BCM2835_SPICS = 0;

}

int map_gpio(){
	int fd;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't open /dev/mem \n",modname);
		return -1;
	}

	/* mmap GPIO */
	gpio = mmap(
	        NULL,
	        BLOCK_SIZE,
	        PROT_READ|PROT_WRITE,
	        MAP_SHARED,
	        fd,
	        BCM2835_GPIO_BASE);

	if (gpio == MAP_FAILED) {
		rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't map gpio\n",modname);
		close(fd);
		return -1;
	}

	/* mmap SPI */
	spi = mmap(
	        NULL,
	        BLOCK_SIZE,
	        PROT_READ|PROT_WRITE,
	        MAP_SHARED,
	        fd,
	        BCM2835_SPI_BASE);

	close(fd);

	if (spi == MAP_FAILED) {
		rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't map spi\n",modname);
		return -1;
	}

	return 0;
}

void setup_gpio(){
	u32 x;

	/*    GPIO USAGE
	 *
	 *	GPIO	Dir	Signal
	 *
	 *	8	OUT	CE0
	 *	9	IN	MISO
	 *	10	OUT	MOSI
	 *	11	OUT	SCLK
	 *
	 */
	 
	/* change SPI pins */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (8*3) | 0b111 << (9*3));
	x |= (0b100 << (8*3) | 0b100 << (9*3));
	BCM2835_GPFSEL0 = x;

	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (0*3) | 0b111 << (1*3));
	x |= (0b100 << (0*3) | 0b100 << (1*3));
	BCM2835_GPFSEL1 = x;

	/* set up SPI */
	BCM2835_SPICLK = SPICLKDIV;

	BCM2835_SPICS = 0;

	/* clear FIFOs */
	BCM2835_SPICS |= SPI_CS_CLEAR_RX | SPI_CS_CLEAR_TX;
}

void restore_gpio(){
	u32 x;

	/* change all used pins back to inputs */

	/* change SPI pins to inputs*/
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (8*3) | 0b111 << (9*3));
	BCM2835_GPFSEL0 = x;

	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (0*3) | 0b111 << (1*3));
	BCM2835_GPFSEL1 = x;
}

