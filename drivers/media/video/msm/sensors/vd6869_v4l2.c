
#include "msm_sensor.h"
#include "../yushanII/ilp0100_ST_definitions.h"
#include "../yushanII/ilp0100_ST_api.h"
#include "yushanII.h"

#define SENSOR_NAME "vd6869"
#define PLATFORM_DRIVER_NAME "msm_camera_vd6869"
#define vd6869_obj vd6869_##obj

#define VD6869_REG_READ_MODE 0x0101
#define VD6869_READ_NORMAL_MODE 0x0000	/* without mirror/flip */
#define VD6869_READ_MIRROR 0x0001			/* with mirror */
#define VD6869_READ_FLIP 0x0002			/* with flip */
#define VD6869_READ_MIRROR_FLIP 0x0003	/* with mirror/flip */

#define REG_DIGITAL_GAIN_GREEN_R 0x020E
#define REG_DIGITAL_GAIN_RED 0x0210
#define REG_DIGITAL_GAIN_BLUE 0x0212
#define REG_DIGITAL_GAIN_GREEN_B 0x0214

/*cut 0.9*/
#define STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_GREEN_R 0x32F3
#define STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_RED 0x32F5
#define STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_BLUE 0x32F7
#define STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_GREEN_B 0x32F9

#define STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_GREEN_R 0x32E8
#define STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_RED 0x32EA
#define STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_BLUE 0x32EC
#define STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_GREEN_B 0x32EE


/*cut 1.0*/
#define STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_GREEN_R 0x32F3
#define STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_RED 0x32F5
#define STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_BLUE 0x32F7
#define STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_GREEN_B 0x32F9

#define STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_GREEN_R 0x32E8
#define STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_RED 0x32EA
#define STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_BLUE 0x32EC
#define STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_GREEN_B 0x32EE

#define OTP_SIZE 16
#define OTP_FUSE_SIZE 12
#define OTP_WAIT_TIMEOUT 200

DEFINE_MUTEX(vd6869_mut);
DEFINE_MUTEX(vd6869_sensor_init_mut);//CC120826,

struct vd6869_hdr_exp_info_t {
	uint16_t long_coarse_int_time_addr_h;
	uint16_t long_coarse_int_time_addr_l;
	uint16_t short_coarse_int_time_addr_h;
	uint16_t short_coarse_int_time_addr_l;
	uint16_t global_gain_addr;
	uint16_t vert_offset;
	uint32_t sensor_max_linecount; /* HTC ben 20120229 */
};

static struct msm_camera_i2c_reg_conf otp_settings_cut09[] = {
       {0x44c0, 0x01},//Set nvm0_pdn and nvm1_pdn to high
       {0x4500, 0x01},
       {0x44ec, 0x01},// Data write timing settings for memory locatin #0-#127
       {0x44ed, 0x80},
       {0x44f0, 0x04},
       {0x44f1, 0xb0},
       {0x452c, 0x01},
       {0x452d, 0x80},
       {0x4530, 0x04},
       {0x4531, 0xb0},
	{0x3305, 0x00},//read 1k for all OTP
       {0x3303, 0x01},//specify the number of 32bits data involved in operation.
	{0x3304, 0x00},
       {0x3301, 0x02},
};

static struct msm_camera_i2c_reg_conf otp_settings_cut10[] = {
       {0x44c0, 0x01},//Set nvm0_pdn and nvm1_pdn to high
       {0x4500, 0x01},
       {0x44e4, 0x00},//ECC disable:0x1 enable:0x0
       {0x4524, 0x00},//ECC disable:0x1 enable:0x0
       {0x4584, 0x01},//enable OTP power
       {0x44ec, 0x01},// Data write timing settings for memory locatin #0-#127
       {0x44ed, 0x80},
       {0x44f0, 0x04},
       {0x44f1, 0xb0},
       {0x452c, 0x01},
       {0x452d, 0x80},
       {0x4530, 0x04},
       {0x4531, 0xb0},
	{0x3305, 0x00},//read 1k for all OTP
       {0x3303, 0x01},//specify the number of 32bits data involved in operation.
	{0x3304, 0x00},
       {0x3301, 0x02},
};

static struct msm_camera_i2c_reg_conf otp_settings_cut10_NO_ECC[] = {
       {0x44c0, 0x01},//Set nvm0_pdn and nvm1_pdn to high
       {0x4500, 0x01},
       {0x44e4, 0x01},//ECC disable:0x1 enable:0x0
       {0x4524, 0x01},//ECC disable:0x1 enable:0x0
       {0x4584, 0x01},//enable OTP power
       {0x44ec, 0x01},// Data write timing settings for memory locatin #0-#127
       {0x44ed, 0x80},
       {0x44f0, 0x04},
       {0x44f1, 0xb0},
       {0x452c, 0x01},
       {0x452d, 0x80},
       {0x4530, 0x04},
       {0x4531, 0xb0},
	{0x3305, 0x00},//read 1k for all OTP
       {0x3303, 0x01},//specify the number of 32bits data involved in operation.
	{0x3304, 0x00},
       {0x3301, 0x02},
};


static struct msm_sensor_ctrl_t vd6869_s_ctrl;

static struct msm_camera_i2c_reg_conf vd6869_start_settings[] = {
	{0x0100, 0x03},
};

static struct msm_camera_i2c_reg_conf vd6869_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf vd6869_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf vd6869_groupoff_settings[] = {
	{0x104, 0x00},
};


/*cut 0.9 setting*/
static struct msm_camera_i2c_reg_conf vd6869_prev_settings[] = {

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */
/*----------------------------------------------- */
/* Output image size - 1344 x 760 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x05}, /* x_output_size = 1344 */
	{0x34d, 0x40},
	{0x34e, 0x02}, /* y_output_size = 760 */
	{0x34f, 0xf8},
	{0x382, 0x00}, /* x_odd_inc = 3 */
	{0x383, 0x03},
	{0x386, 0x00}, /* y_odd_inc = 3 */
	{0x387, 0x03},


/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x02}, /* frame_length_lines = 640 = (1280/2) lines */
	{0x341, 0x80},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */


/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */
};

static struct msm_camera_i2c_reg_conf vd6869_video_settings[] = {
/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */


/*----------------------------------------------- */
/* Output image size - 2688 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x0a}, /* x_output_size = 2688 */
	{0x34d, 0x80},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},




/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},


	{0x340, 0x05}, /* frame_length_lines = 1280 = (2560/2) lines */
	{0x341, 0x00},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},


/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */


/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */


/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */

};

static struct msm_camera_i2c_reg_conf vd6869_zoe_settings[] = {
/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */


/*----------------------------------------------- */
/* Output image size - 2688 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x0a}, /* x_output_size = 2688 */
	{0x34d, 0x80},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},




/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},


	{0x340, 0x03}, /* frame_length_lines = 800 = (1600/2) lines */
	{0x341, 0x20},
	{0x342, 0x17}, /* line_length_pck = 6000 pcks */
	{0x343, 0x70},


/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */


/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */


/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */

};

static struct msm_camera_i2c_reg_conf vd6869_fast_video_settings[] = {

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */

/*----------------------------------------------- */
/*----------------------------------------------- */
/* Output image size - 1344 x 760 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x05}, /* x_output_size = 1344 */
	{0x34d, 0x40},
	{0x34e, 0x02}, /* y_output_size =  760 */
	{0x34f, 0xf8},
	{0x382, 0x00}, /* x_odd_inc = 3 */
	{0x383, 0x03},
	{0x386, 0x00}, /* y_odd_inc = 3 */
	{0x387, 0x03},


/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x01}, /* frame_length_lines = 400 = (800/2) lines */
	{0x341, 0x90},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */

/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */


};

static struct msm_camera_i2c_reg_conf vd6869_hdr_settings[] = {
/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */

/*----------------------------------------------- */
/* Output image size - 1952 x 1088 */
/*----------------------------------------------- */
	{0x344, 0x01}, /* x_start_addr = 368 */
	{0x345, 0x70},
	{0x346, 0x00}, /* y_start_addr = 216 */
	{0x347, 0xd8},
	{0x348, 0x09}, /* x_addr_end = 2319 */
	{0x349, 0x0f},
	{0x34a, 0x05}, /* y_addr_end = 1303 */
	{0x34b, 0x17},
	{0x34c, 0x07}, /* x_output_size = 1952 */
	{0x34d, 0xa0},
	{0x34e, 0x04}, /* y_output_size = 1088 */
	{0x34f, 0x40},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},


/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x05}, /* frame_length_lines = 1310 lines */
	{0x341, 0x1e},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0xc9}, /* vtiming long2short offset = 201 (To set only in HDR Staggered) */

/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for HDR staggered mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (HDR staggered mode) */
/*----------------------------------------------- */
/* Expo 0 staggered = Long frame */
	{0x32e5, 0x01}, /* coarse_integration_time = 200 */
	{0x32e6, 0x90},
	{0x32e7, 0x80}, /* gain 2.0 == 8 << 4 == 128 */
	{0x32f3, 0x01}, /* digital_gain_greenR	= 1.0x (fixed point 4.8) */
	{0x32f4, 0x00},
	{0x32f5, 0x01}, /* digital_gain_red = 1.0x (fixed point 4.8) */
	{0x32f6, 0x00},
	{0x32f7, 0x01}, /* digital_gain_blue	= 1.0x (fixed point 4.8) */
	{0x32f8, 0x00},
	{0x32f9, 0x01}, /* digital_gain_greenB	= 1.0x (fixed point 4.8) */
	{0x32fa, 0x00},


/* Expo 1 staggered = Short Frame */
	{0x32f0, 0x00}, /* coarse_integration_time = 128 */
	{0x32f1, 0xc8},
	{0x32e8, 0x01}, /* digital_gain_greenR	= 1.0x (fixed point 4.8) */
	{0x32e9, 0x00},
	{0x32ea, 0x01}, /* digital_gain_red = 1.0x (fixed point 4.8) */
	{0x32eb, 0x00},
	{0x32ec, 0x01}, /* digital_gain_blue	= 1.0x (fixed point 4.8) */
	{0x32ed, 0x00},
	{0x32ee, 0x01}, /* digital_gain_greenB	= 1.0x (fixed point 4.8) */
	{0x32ef, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */
};

static struct msm_camera_i2c_reg_conf vd6869_4_3_settings[] = {
/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */

/*----------------------------------------------- */
/* Video timing - 576Mbps */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x03}, /* frame_length_lines = 795 = (1590/2) lines */
	{0x341, 0x1b},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* Output image size - 2048 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x01}, /* x_start_addr = 320 */
	{0x345, 0x40},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x09}, /* x_addr_end = 2367 */
	{0x349, 0x3f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x08}, /* x_output_size = 2048 */
	{0x34d, 0x00},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x01}, /* coarse_integration_time = 256 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},
};

static struct msm_camera_i2c_reg_conf vd6869_16_9_settings_non_hdr[] = {

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */
/*----------------------------------------------- */
/* Output image size - 2688 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x0a}, /* x_output_size = 2688 */
	{0x34d, 0x80},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},


/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x03}, /* frame_length_lines = 795 = (1590/2) lines */
	{0x341, 0x1b},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */


/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */
};

static struct msm_camera_i2c_reg_conf vd6869_start_core_1_settings[] = {
	{0x4440, 0x7F},
};

static struct msm_camera_i2c_reg_conf vd6869_start_core_2_settings[] = {
	{0x4100, 0x01},
};

static struct msm_camera_i2c_reg_conf vd6869_stop_core_1_settings[] = {
	{0x4440, 0x80},
};

static struct msm_camera_i2c_reg_conf vd6869_stop_core_2_settings[] = {
	{0x4100, 0x03},
};

static struct msm_camera_i2c_reg_conf vd6869_recommend_settings_clearbit[] = {
	{0x3337, 0x10}, /* bit 4 */
};

static struct msm_camera_i2c_reg_conf vd6869_recommend_settings[] = {


/*----------------------------------------------- */
/* Patch v1.28 */
/*----------------------------------------------- */
	{0x4440, 0x80},
	{0x4100, 0x03},
	{0x6000, 0x90},
	{0x6001, 0x33},
	{0x6002, 0x39},
	{0x6003, 0xe0},
	{0x6004, 0x90},
	{0x6005, 0x54},
	{0x6006, 0x5d},
	{0x6007, 0xf0},
	{0x6008, 0x02},
	{0x6009, 0x38},
	{0x600a, 0x52},
	{0x600b, 0xc3},
	{0x600c, 0x13},
	{0x600d, 0xc3},
	{0x600e, 0x13},
	{0x600f, 0x90},
	{0x6010, 0x54},
	{0x6011, 0xa8},
	{0x6012, 0x02},
	{0x6013, 0x20},
	{0x6014, 0xfa},
	{0x6015, 0xe0},
	{0x6016, 0xc3},
	{0x6017, 0x13},
	{0x6018, 0xc3},
	{0x6019, 0x13},
	{0x601a, 0x02},
	{0x601b, 0x21},
	{0x601c, 0x88},
	{0x601d, 0xc3},
	{0x601e, 0x13},
	{0x601f, 0xc3},
	{0x6020, 0x13},
	{0x6021, 0x90},
	{0x6022, 0x54},
	{0x6023, 0xa8},
	{0x6024, 0x02},
	{0x6025, 0x22},
	{0x6026, 0x3a},
	{0x6027, 0x90},
	{0x6028, 0x54},
	{0x6029, 0x00},
	{0x602a, 0xe0},
	{0x602b, 0xb4},
	{0x602c, 0x00},
	{0x602d, 0x0c},
	{0x602e, 0x90},
	{0x602f, 0x31},
	{0x6030, 0x47},
	{0x6031, 0xe0},
	{0x6032, 0xc3},
	{0x6033, 0x13},
	{0x6034, 0xff},
	{0x6035, 0xa3},
	{0x6036, 0xe0},
	{0x6037, 0x13},
	{0x6038, 0x80},
	{0x6039, 0x07},
	{0x603a, 0x90},
	{0x603b, 0x31},
	{0x603c, 0x47},
	{0x603d, 0xe0},
	{0x603e, 0xff},
	{0x603f, 0xa3},
	{0x6040, 0xe0},
	{0x6041, 0x02},
	{0x6042, 0x27},
	{0x6043, 0xa5},
	{0x6044, 0x02},
	{0x6045, 0x2e},
	{0x6046, 0x1d},
	{0x6047, 0x90},
	{0x6048, 0x55},
	{0x6049, 0xbc},
	{0x604a, 0xe0},
	{0x604b, 0xb4},
	{0x604c, 0x01},
	{0x604d, 0x17},
	{0x604e, 0x74},
	{0x604f, 0x3b},
	{0x6050, 0x29},
	{0x6051, 0xf8},
	{0x6052, 0xe6},
	{0x6053, 0x11},
	{0x6054, 0xc9},
	{0x6055, 0x70},
	{0x6056, 0x02},
	{0x6057, 0x05},
	{0x6058, 0x0b},
	{0x6059, 0x14},
	{0x605a, 0x78},
	{0x605b, 0x02},
	{0x605c, 0xc3},
	{0x605d, 0x33},
	{0x605e, 0xce},
	{0x605f, 0x33},
	{0x6060, 0xce},
	{0x6061, 0xd8},
	{0x6062, 0xf9},
	{0x6063, 0x80},
	{0x6064, 0x5a},
	{0x6065, 0xe9},
	{0x6066, 0x64},
	{0x6067, 0x02},
	{0x6068, 0x70},
	{0x6069, 0x40},
	{0x606a, 0x78},
	{0x606b, 0x3e},
	{0x606c, 0xe6},
	{0x606d, 0x25},
	{0x606e, 0xe0},
	{0x606f, 0xfe},
	{0x6070, 0x18},
	{0x6071, 0xe6},
	{0x6072, 0x33},
	{0x6073, 0x90},
	{0x6074, 0x33},
	{0x6075, 0x34},
	{0x6076, 0xf0},
	{0x6077, 0xa3},
	{0x6078, 0xce},
	{0x6079, 0xf0},
	{0x607a, 0x90},
	{0x607b, 0x33},
	{0x607c, 0x34},
	{0x607d, 0xe0},
	{0x607e, 0xa3},
	{0x607f, 0x11},
	{0x6080, 0xc9},
	{0x6081, 0x70},
	{0x6082, 0x02},
	{0x6083, 0x05},
	{0x6084, 0x0b},
	{0x6085, 0x14},
	{0x6086, 0x78},
	{0x6087, 0x02},
	{0x6088, 0xc3},
	{0x6089, 0x33},
	{0x608a, 0xce},
	{0x608b, 0x33},
	{0x608c, 0xce},
	{0x608d, 0xd8},
	{0x608e, 0xf9},
	{0x608f, 0x11},
	{0x6090, 0xd1},
	{0x6091, 0x90},
	{0x6092, 0x33},
	{0x6093, 0x34},
	{0x6094, 0xa3},
	{0x6095, 0x11},
	{0x6096, 0xc8},
	{0x6097, 0x70},
	{0x6098, 0x02},
	{0x6099, 0x05},
	{0x609a, 0x0b},
	{0x609b, 0x14},
	{0x609c, 0x78},
	{0x609d, 0x02},
	{0x609e, 0xc3},
	{0x609f, 0x33},
	{0x60a0, 0xce},
	{0x60a1, 0x33},
	{0x60a2, 0xce},
	{0x60a3, 0xd8},
	{0x60a4, 0xf9},
	{0x60a5, 0x11},
	{0x60a6, 0xd1},
	{0x60a7, 0x09},
	{0x60a8, 0x80},
	{0x60a9, 0x17},
	{0x60aa, 0x74},
	{0x60ab, 0x3b},
	{0x60ac, 0x29},
	{0x60ad, 0xf8},
	{0x60ae, 0xe6},
	{0x60af, 0x11},
	{0x60b0, 0xc9},
	{0x60b1, 0x70},
	{0x60b2, 0x02},
	{0x60b3, 0x05},
	{0x60b4, 0x0b},
	{0x60b5, 0x14},
	{0x60b6, 0x78},
	{0x60b7, 0x02},
	{0x60b8, 0xc3},
	{0x60b9, 0x33},
	{0x60ba, 0xce},
	{0x60bb, 0x33},
	{0x60bc, 0xce},
	{0x60bd, 0xd8},
	{0x60be, 0xf9},
	{0x60bf, 0x11},
	{0x60c0, 0xd1},
	{0x60c1, 0x02},
	{0x60c2, 0x09},
	{0x60c3, 0x94},
	{0x60c4, 0x00},
	{0x60c5, 0x00},
	{0x60c6, 0x00},
	{0x60c7, 0x00},
	{0x60c8, 0xe0},
	{0x60c9, 0xfb},
	{0x60ca, 0x05},
	{0x60cb, 0x0c},
	{0x60cc, 0xe5},
	{0x60cd, 0x0c},
	{0x60ce, 0xae},
	{0x60cf, 0x0b},
	{0x60d0, 0x22},
	{0x60d1, 0x12},
	{0x60d2, 0x0a},
	{0x60d3, 0x83},
	{0x60d4, 0x22},
	{0x60d5, 0x3e},
	{0x60d6, 0xf5},
	{0x60d7, 0x83},
	{0x60d8, 0xa3},
	{0x60d9, 0xa3},
	{0x60da, 0xe4},
	{0x60db, 0xf0},
	{0x60dc, 0x22},
	{0x60fa, 0x90},
	{0x60fb, 0x31},
	{0x60fc, 0x51},
	{0x60fd, 0xe0},
	{0x60fe, 0x44},
	{0x60ff, 0x50},
	{0x6100, 0xfe},
	{0x6101, 0xa3},
	{0x6102, 0xe0},
	{0x6103, 0xff},
	{0x6104, 0x02},
	{0x6105, 0x2e},
	{0x6106, 0x47},
	{0x6107, 0x00},
	{0x6108, 0x00},
	{0x6109, 0x00},
	{0x610a, 0x00},
	{0x610b, 0x00},
	{0x610c, 0x90},
	{0x610d, 0x39},
	{0x610e, 0x0b},
	{0x610f, 0xe0},
	{0x6110, 0xb4},
	{0x6111, 0x01},
	{0x6112, 0x04},
	{0x6113, 0x74},
	{0x6114, 0x05},
	{0x6115, 0x80},
	{0x6116, 0x02},
	{0x6117, 0x74},
	{0x6118, 0x06},
	{0x6119, 0x90},
	{0x611a, 0x55},
	{0x611b, 0x0c},
	{0x611c, 0xf0},
	{0x611d, 0x90},
	{0x611e, 0x55},
	{0x611f, 0x04},
	{0x6120, 0xf0},
	{0x6121, 0x90},
	{0x6122, 0x48},
	{0x6123, 0xa4},
	{0x6124, 0x74},
	{0x6125, 0x16},
	{0x6126, 0xf0},
	{0x6127, 0x90},
	{0x6128, 0x33},
	{0x6129, 0x37},
	{0x612a, 0x02},
	{0x612b, 0x38},
	{0x612c, 0x55},
	{0x6137, 0x90},
	{0x6138, 0x30},
	{0x6139, 0x46},
	{0x613a, 0xe0},
	{0x613b, 0x44},
	{0x613c, 0x50},
	{0x613d, 0xfe},
	{0x613e, 0xa3},
	{0x613f, 0xe0},
	{0x6140, 0xff},
	{0x6141, 0x02},
	{0x6142, 0x2e},
	{0x6143, 0x55},
	{0x6144, 0x90},
	{0x6145, 0x39},
	{0x6146, 0x0a},
	{0x6147, 0xe0},
	{0x6148, 0xff},
	{0x6149, 0x90},
	{0x614a, 0x38},
	{0x614b, 0xbb},
	{0x614c, 0xe0},
	{0x614d, 0x54},
	{0x614e, 0x0f},
	{0x614f, 0xb5},
	{0x6150, 0x07},
	{0x6151, 0x08},
	{0x6152, 0x74},
	{0x6153, 0x01},
	{0x6154, 0x90},
	{0x6155, 0x32},
	{0x6156, 0xfb},
	{0x6157, 0xf0},
	{0x6158, 0x80},
	{0x6159, 0x06},
	{0x615a, 0x24},
	{0x615b, 0x01},
	{0x615c, 0x90},
	{0x615d, 0x32},
	{0x615e, 0xfb},
	{0x615f, 0xf0},
	{0x6160, 0x90},
	{0x6161, 0x54},
	{0x6162, 0x70},
	{0x6163, 0x02},
	{0x6164, 0x1d},
	{0x6165, 0x87},
	{0x6166, 0x90},
	{0x6167, 0x32},
	{0x6168, 0xfb},
	{0x6169, 0x74},
	{0x616a, 0x01},
	{0x616b, 0xf0},
	{0x616c, 0x90},
	{0x616d, 0x38},
	{0x616e, 0xbb},
	{0x616f, 0x02},
	{0x6170, 0x0c},
	{0x6171, 0x93},
	{0x6172, 0x02},
	{0x6173, 0x36},
	{0x6174, 0x20},
	{0x6175, 0x02},
	{0x6176, 0x22},
	{0x6177, 0x28},
	{0x618a, 0x74},
	{0x618b, 0x19},
	{0x618c, 0x02},
	{0x618d, 0x0a},
	{0x618e, 0x09},
	{0x6190, 0x90},
	{0x6191, 0x45},
	{0x6192, 0x90},
	{0x6193, 0x74},
	{0x6194, 0x93},
	{0x6195, 0xf0},
	{0x6196, 0xa3},
	{0x6197, 0xa3},
	{0x6198, 0xa3},
	{0x6199, 0xa3},
	{0x619a, 0xf0},
	{0x619b, 0xa3},
	{0x619c, 0xa3},
	{0x619d, 0xa3},
	{0x619e, 0xa3},
	{0x619f, 0xf0},
	{0x61a0, 0xa3},
	{0x61a1, 0xa3},
	{0x61a2, 0xa3},
	{0x61a3, 0xa3},
	{0x61a4, 0xf0},
	{0x61a5, 0x02},
	{0x61a6, 0x10},
	{0x61a7, 0xd5},
	{0x61a8, 0x02},
	{0x61a9, 0x0c},
	{0x61aa, 0xc0},
	{0x61ac, 0x74},
	{0x61ad, 0x52},
	{0x61ae, 0x90},
	{0x61af, 0x48},
	{0x61b0, 0xf1},
	{0x61b1, 0xf0},
	{0x61b2, 0x74},
	{0x61b3, 0x6b},
	{0x61b4, 0x90},
	{0x61b5, 0x48},
	{0x61b6, 0xfd},
	{0x61b7, 0xf0},
	{0x61b8, 0x74},
	{0x61b9, 0x12},
	{0x61ba, 0x90},
	{0x61bb, 0x49},
	{0x61bc, 0x09},
	{0x61bd, 0xf0},
	{0x61be, 0x74},
	{0x61bf, 0x2b},
	{0x61c0, 0x90},
	{0x61c1, 0x49},
	{0x61c2, 0x15},
	{0x61c3, 0xf0},
	{0x61c4, 0x90},
	{0x61c5, 0x49},
	{0x61c6, 0xa5},
	{0x61c7, 0xf0},
	{0x61c8, 0x02},
	{0x61c9, 0x1a},
	{0x61ca, 0x6e},
	{0x61e9, 0x90},
	{0x61ea, 0x54},
	{0x61eb, 0x0c},
	{0x61ec, 0xe5},
	{0x61ed, 0x0c},
	{0x61ee, 0xf0},
	{0x61ef, 0x90},
	{0x61f0, 0x55},
	{0x61f1, 0x14},
	{0x61f2, 0xe5},
	{0x61f3, 0x0b},
	{0x61f4, 0xf0},
	{0x61f5, 0x90},
	{0x61f6, 0x55},
	{0x61f7, 0x10},
	{0x61f8, 0xe4},
	{0x61f9, 0xf0},
	{0x61fa, 0xa3},
	{0x61fb, 0x74},
	{0x61fc, 0x44},
	{0x61fd, 0xf0},
	{0x61fe, 0xf5},
	{0x61ff, 0x0a},
	{0x6200, 0x02},
	{0x6201, 0x37},
	{0x6202, 0xfd},
	{0x6219, 0x90},
	{0x621a, 0x01},
	{0x621b, 0x01},
	{0x621c, 0xe0},
	{0x621d, 0xfe},
	{0x621e, 0x54},
	{0x621f, 0x01},
	{0x6220, 0x00},
	{0x6221, 0x00},
	{0x6222, 0xb4},
	{0x6223, 0x00},
	{0x6224, 0x03},
	{0x6225, 0x02},
	{0x6226, 0x34},
	{0x6227, 0xa4},
	{0x6228, 0x02},
	{0x6229, 0x34},
	{0x622a, 0xab},
	{0x622b, 0x02},
	{0x622c, 0x0d},
	{0x622d, 0x51},
	{0x625c, 0xe5},
	{0x625d, 0x09},
	{0x625e, 0x70},
	{0x625f, 0x3f},
	{0x6260, 0x90},
	{0x6261, 0x39},
	{0x6262, 0x0b},
	{0x6263, 0xe0},
	{0x6264, 0xb4},
	{0x6265, 0x03},
	{0x6266, 0x06},
	{0x6267, 0x90},
	{0x6268, 0x55},
	{0x6269, 0x98},
	{0x626a, 0x02},
	{0x626b, 0x32},
	{0x626c, 0xbf},
	{0x626d, 0x90},
	{0x626e, 0x55},
	{0x626f, 0xa0},
	{0x6270, 0x12},
	{0x6271, 0x0f},
	{0x6272, 0x89},
	{0x6273, 0x12},
	{0x6274, 0x0f},
	{0x6275, 0x82},
	{0x6276, 0xee},
	{0x6277, 0xf0},
	{0x6278, 0xa3},
	{0x6279, 0xef},
	{0x627a, 0xf0},
	{0x627b, 0x90},
	{0x627c, 0x55},
	{0x627d, 0xa4},
	{0x627e, 0x12},
	{0x627f, 0x0f},
	{0x6280, 0x89},
	{0x6281, 0x8c},
	{0x6282, 0x83},
	{0x6283, 0x24},
	{0x6284, 0x04},
	{0x6285, 0xf5},
	{0x6286, 0x82},
	{0x6287, 0xe4},
	{0x6288, 0x3c},
	{0x6289, 0x12},
	{0x628a, 0x32},
	{0x628b, 0x17},
	{0x628c, 0x90},
	{0x628d, 0x55},
	{0x628e, 0x98},
	{0x628f, 0x12},
	{0x6290, 0x0f},
	{0x6291, 0x89},
	{0x6292, 0x12},
	{0x6293, 0x34},
	{0x6294, 0xe9},
	{0x6295, 0x3c},
	{0x6296, 0x12},
	{0x6297, 0x32},
	{0x6298, 0x17},
	{0x6299, 0x90},
	{0x629a, 0x55},
	{0x629b, 0x9c},
	{0x629c, 0x02},
	{0x629d, 0x32},
	{0x629e, 0xe8},
	{0x629f, 0x02},
	{0x62a0, 0x32},
	{0x62a1, 0xea},
	{0x62a2, 0x75},
	{0x62a3, 0x09},
	{0x62a4, 0x08},
	{0x62a5, 0x75},
	{0x62a6, 0x0a},
	{0x62a7, 0x44},
	{0x62a8, 0x02},
	{0x62a9, 0x38},
	{0x62aa, 0x41},
	{0x62b3, 0x90},
	{0x62b4, 0x54},
	{0x62b5, 0x0c},
	{0x62b6, 0x74},
	{0x62b7, 0x0e},
	{0x62b8, 0xf0},
	{0x62b9, 0x75},
	{0x62ba, 0x0c},
	{0x62bb, 0x0e},
	{0x62bc, 0x02},
	{0x62bd, 0x38},
	{0x62be, 0x70},
	{0x62c0, 0xe5},
	{0x62c1, 0x09},
	{0x62c2, 0x70},
	{0x62c3, 0x48},
	{0x62c4, 0x90},
	{0x62c5, 0x01},
	{0x62c6, 0x01},
	{0x62c7, 0xe0},
	{0x62c8, 0x54},
	{0x62c9, 0x02},
	{0x62ca, 0x03},
	{0x62cb, 0x70},
	{0x62cc, 0x07},
	{0x62cd, 0x90},
	{0x62ce, 0x39},
	{0x62cf, 0x0b},
	{0x62d0, 0xe0},
	{0x62d1, 0xb4},
	{0x62d2, 0x03},
	{0x62d3, 0x06},
	{0x62d4, 0x90},
	{0x62d5, 0x55},
	{0x62d6, 0x98},
	{0x62d7, 0x02},
	{0x62d8, 0x32},
	{0x62d9, 0xbf},
	{0x62da, 0x90},
	{0x62db, 0x55},
	{0x62dc, 0xa0},
	{0x62dd, 0x12},
	{0x62de, 0x0f},
	{0x62df, 0x89},
	{0x62e0, 0x12},
	{0x62e1, 0x0f},
	{0x62e2, 0x82},
	{0x62e3, 0xee},
	{0x62e4, 0xf0},
	{0x62e5, 0xa3},
	{0x62e6, 0xef},
	{0x62e7, 0xf0},
	{0x62e8, 0x90},
	{0x62e9, 0x55},
	{0x62ea, 0xa4},
	{0x62eb, 0x12},
	{0x62ec, 0x0f},
	{0x62ed, 0x89},
	{0x62ee, 0x8c},
	{0x62ef, 0x83},
	{0x62f0, 0x24},
	{0x62f1, 0x04},
	{0x62f2, 0xf5},
	{0x62f3, 0x82},
	{0x62f4, 0xe4},
	{0x62f5, 0x3c},
	{0x62f6, 0x12},
	{0x62f7, 0x32},
	{0x62f8, 0x17},
	{0x62f9, 0x90},
	{0x62fa, 0x55},
	{0x62fb, 0x98},
	{0x62fc, 0x12},
	{0x62fd, 0x0f},
	{0x62fe, 0x89},
	{0x62ff, 0x12},
	{0x6300, 0x34},
	{0x6301, 0xe9},
	{0x6302, 0x3c},
	{0x6303, 0x12},
	{0x6304, 0x32},
	{0x6305, 0x17},
	{0x6306, 0x90},
	{0x6307, 0x55},
	{0x6308, 0x9c},
	{0x6309, 0x02},
	{0x630a, 0x32},
	{0x630b, 0xe8},
	{0x630c, 0x00},
	{0x630d, 0x00},
	{0x630e, 0x00},
	{0x630f, 0x90},
	{0x6310, 0x01},
	{0x6311, 0x01},
	{0x6312, 0xe0},
	{0x6313, 0x54},
	{0x6314, 0x02},
	{0x6315, 0x03},
	{0x6316, 0x60},
	{0x6317, 0x07},
	{0x6318, 0x90},
	{0x6319, 0x39},
	{0x631a, 0x0b},
	{0x631b, 0xe0},
	{0x631c, 0xb4},
	{0x631d, 0x03},
	{0x631e, 0x06},
	{0x631f, 0x90},
	{0x6320, 0x55},
	{0x6321, 0xa8},
	{0x6322, 0x02},
	{0x6323, 0x32},
	{0x6324, 0xed},
	{0x6325, 0x90},
	{0x6326, 0x55},
	{0x6327, 0xb0},
	{0x6328, 0x12},
	{0x6329, 0x0f},
	{0x632a, 0x89},
	{0x632b, 0x12},
	{0x632c, 0x0f},
	{0x632d, 0x82},
	{0x632e, 0xee},
	{0x632f, 0xf0},
	{0x6330, 0xa3},
	{0x6331, 0xef},
	{0x6332, 0xf0},
	{0x6333, 0x90},
	{0x6334, 0x55},
	{0x6335, 0xb4},
	{0x6336, 0x12},
	{0x6337, 0x0f},
	{0x6338, 0x89},
	{0x6339, 0x8c},
	{0x633a, 0x83},
	{0x633b, 0x24},
	{0x633c, 0x04},
	{0x633d, 0xf5},
	{0x633e, 0x82},
	{0x633f, 0xe4},
	{0x6340, 0x3c},
	{0x6341, 0x12},
	{0x6342, 0x32},
	{0x6343, 0x17},
	{0x6344, 0x90},
	{0x6345, 0x55},
	{0x6346, 0xa8},
	{0x6347, 0x12},
	{0x6348, 0x0f},
	{0x6349, 0x89},
	{0x634a, 0x12},
	{0x634b, 0x34},
	{0x634c, 0xe9},
	{0x634d, 0x3c},
	{0x634e, 0x12},
	{0x634f, 0x32},
	{0x6350, 0x17},
	{0x6351, 0x90},
	{0x6352, 0x55},
	{0x6353, 0xac},
	{0x6354, 0x02},
	{0x6355, 0x33},
	{0x6356, 0x16},
	{0x6357, 0x02},
	{0x6358, 0x20},
	{0x6359, 0x8a},
	{0x635a, 0x90},
	{0x635b, 0x45},
	{0x635c, 0x90},
	{0x635d, 0xe0},
	{0x635e, 0x54},
	{0x635f, 0xcd},
	{0x6360, 0xf0},
	{0x6361, 0x90},
	{0x6362, 0x45},
	{0x6363, 0x94},
	{0x6364, 0xe0},
	{0x6365, 0x54},
	{0x6366, 0xcd},
	{0x6367, 0xf0},
	{0x6368, 0x90},
	{0x6369, 0x45},
	{0x636a, 0x98},
	{0x636b, 0xe0},
	{0x636c, 0x54},
	{0x636d, 0xcd},
	{0x636e, 0xf0},
	{0x636f, 0x90},
	{0x6370, 0x45},
	{0x6371, 0x9c},
	{0x6372, 0xe0},
	{0x6373, 0x54},
	{0x6374, 0xcd},
	{0x6375, 0xf0},
	{0x6376, 0x02},
	{0x6377, 0x11},
	{0x6378, 0xfa},
	{0x4104, 0xd4},
	{0x4105, 0x0a},
	{0x4108, 0x32},
	{0x410a, 0x00},
	{0x410c, 0x03},
	{0x4110, 0xdf},
	{0x4111, 0x0a},
	{0x4114, 0x8d},
	{0x4115, 0x00},
	{0x4118, 0x03},
	{0x411c, 0x45},
	{0x411d, 0x38},
	{0x4120, 0x00},
	{0x4121, 0x00},
	{0x4124, 0x02},
	{0x4128, 0xf7},
	{0x4129, 0x20},
	{0x412c, 0x0b},
	{0x412d, 0x00},
	{0x4130, 0x02},
	{0x4134, 0x87},
	{0x4135, 0x21},
	{0x4138, 0x15},
	{0x4139, 0x00},
	{0x413c, 0x02},
	{0x4140, 0x37},
	{0x4141, 0x22},
	{0x4144, 0x1d},
	{0x4145, 0x00},
	{0x4148, 0x02},
	{0x414c, 0x9e},
	{0x414d, 0x27},
	{0x4150, 0x27},
	{0x4151, 0x00},
	{0x4154, 0x02},
	{0x4158, 0x3b},
	{0x4159, 0x38},
	{0x415c, 0xa2},
	{0x415d, 0x02},
	{0x4160, 0x02},
	{0x4164, 0x78},
	{0x4165, 0x09},
	{0x4168, 0x47},
	{0x4169, 0x00},
	{0x416c, 0x02},
	{0x4170, 0xb8},
	{0x4171, 0x32},
	{0x4174, 0xc0},
	{0x4175, 0x02},
	{0x4178, 0x02},
	{0x417c, 0x40},
	{0x417d, 0x2e},
	{0x4180, 0xfa},
	{0x4181, 0x00},
	{0x4184, 0x02},
	{0x4188, 0xea},
	{0x4189, 0x32},
	{0x418c, 0x0f},
	{0x418d, 0x03},
	{0x4190, 0x02},
	{0x4194, 0xf6},
	{0x4195, 0x11},
	{0x4198, 0x5a},
	{0x4199, 0x03},
	{0x419c, 0x02},
	{0x41a0, 0x4f},
	{0x41a1, 0x2e},
	{0x41a4, 0x37},
	{0x41a5, 0x01},
	{0x41a8, 0x02},
	{0x41ac, 0x64},
	{0x41ad, 0x21},
	{0x41b0, 0x25},
	{0x41b1, 0x00},
	{0x41b4, 0x03},
	{0x41b8, 0x84},
	{0x41b9, 0x1d},
	{0x41bc, 0x44},
	{0x41bd, 0x01},
	{0x41c0, 0x02},
	{0x41c4, 0x90},
	{0x41c5, 0x0c},
	{0x41c8, 0x66},
	{0x41c9, 0x01},
	{0x41cc, 0x02},
	{0x41d0, 0x1a},
	{0x41d1, 0x36},
	{0x41d4, 0x72},
	{0x41d5, 0x01},
	{0x41d8, 0x02},
	{0x41dc, 0x22},
	{0x41dd, 0x22},
	{0x41e0, 0x75},
	{0x41e1, 0x01},
	{0x41e4, 0x02},
	{0x41e8, 0x7f},
	{0x41e9, 0x20},
	{0x41ec, 0x57},
	{0x41ed, 0x03},
	{0x41f0, 0x02},
	{0x41f4, 0xef},
	{0x41f5, 0x0a},
	{0x41f8, 0x5b},
	{0x41f9, 0x00},
	{0x41fc, 0x03},
	{0x4200, 0xfb},
	{0x4201, 0x0a},
	{0x4204, 0x31},
	{0x4205, 0x00},
	{0x4208, 0x03},
	{0x420c, 0x04},
	{0x420d, 0x0a},
	{0x4210, 0x8a},
	{0x4211, 0x01},
	{0x4214, 0x02},
	{0x4218, 0xfa},
	{0x4219, 0x37},
	{0x421c, 0xe9},
	{0x421d, 0x01},
	{0x4220, 0x02},
	{0x4224, 0xa1},
	{0x4225, 0x34},
	{0x4228, 0x19},
	{0x4229, 0x02},
	{0x422c, 0x02},
	{0x4230, 0x6d},
	{0x4231, 0x38},
	{0x4234, 0xb6},
	{0x4235, 0x02},
	{0x4238, 0x02},
	{0x423c, 0x46},
	{0x423d, 0x23},
	{0x4240, 0x29},
	{0x4241, 0x00},
	{0x4244, 0x03},
	{0x4248, 0xaa},
	{0x4249, 0x14},
	{0x424c, 0x85},
	{0x424d, 0x00},
	{0x4250, 0x03},
	{0x4254, 0x61},
	{0x4255, 0x1a},
	{0x4258, 0xac},
	{0x4259, 0x01},
	{0x425c, 0x02},
	{0x4260, 0xc5},
	{0x4261, 0x10},
	{0x4264, 0x90},
	{0x4265, 0x01},
	{0x4268, 0x02},
	{0x426c, 0x35},
	{0x426d, 0x0d},
	{0x4270, 0x2b},
	{0x4271, 0x02},
	{0x4274, 0x02},
	{0x4278, 0x52},
	{0x4279, 0x38},
	{0x427c, 0x0c},
	{0x427d, 0x01},
	{0x4280, 0x02},
	{0x4100, 0x01},
	{0x4440, 0x7f},

};

static struct msm_camera_i2c_reg_conf vd6869_recommend_2_settings[] = {
	{0x3bfe, 0x01},
	{0x3bff, 0x1c},
};

static struct msm_camera_i2c_reg_conf vd6869_recommend_3_settings[] = {
	/*cut 09E*/
	{0x5800, 0xC0}, /* vtminor_0 */
	{0x5804, 0x42}, /* vtminor_1 */
	{0x5808, 0x40}, /* vtminor_2 */
	{0x580c, 0x00}, /* vtminor_3 */
	{0x5810, 0x04}, /* vtminor_4 */
	{0x5814, 0x70}, /* vtminor_5 */
	{0x5818, 0x07}, /* vtminor_6 */
	{0x581c, 0x80}, /* vtminor_7 */
	{0x5820, 0x0C}, /* vtminor_8 */

	{0x5872, 0x1f}, /* streaming option */
	{0x5896, 0xa4}, /* streaming option */
	{0x5800, 0x00}, /* streaming option */

/* Darkcal management */
	{0x4a20, 0x0f}, /* bMode = auto with offset 64 */
	{0x4a22, 0x00}, /* */
	{0x4a23, 0x40},
	{0x4a24, 0x00}, /*  */
	{0x4a25, 0x40},
	{0x4a26, 0x00}, /*  */
	{0x4a27, 0x40},
	{0x4a28, 0x00}, /*  */
	{0x4a29, 0x40},
	{0x33a4, 0x07}, /*  */
	{0x33a5, 0xC0},
	{0x33a6, 0x00}, /*  */
	{0x33a7, 0x40},

	{0x4d20, 0x0f}, /* bMode = auto with offset 64 */
	{0x4d22, 0x00}, /* */
	{0x4d23, 0x40},
	{0x4d24, 0x00}, /*  */
	{0x4d25, 0x40},
	{0x4d26, 0x00}, /*  */
	{0x4d27, 0x40},
	{0x4d28, 0x00}, /*  */
	{0x4d29, 0x40},
	{0x3399, 0x07}, /*  */
	{0x339a, 0xC0},
	{0x339b, 0x00}, /* */
	{0x339c, 0x40},

/*----------------------------------------------- */
/* Pre-streaming internal configuration */
/*----------------------------------------------- */
	{0x33be, 0x0b}, /* Output formatting - enable ULPS */
	{0x114, 0x03}, /* csi_lane_mode = quad lane */
	{0x121, 0x18}, /* extclk_frequency_mhz = 24.0Mhz */
	{0x122, 0x00},

/*----------------------------------------------- */
/* Video timing - PLL clock = 576MHz */
/*----------------------------------------------- */
	{0x304, 0x00}, /* pre_pll_clk_div = 2 */
	{0x305, 0x02},
	{0x306, 0x00}, /* pll_multiplier = 48 */
	{0x307, 0x30},


/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x101, 0x03}, /* image_orientation = XY flip */

/*----------------------------------------------- */
/* HTC module CSI lane mapping */
/*----------------------------------------------- */
/* Silicon Lane1(0) -> Module Lane3(2) (0x489c[1:0]=b10) */
/* Silicon Lane2(1) -> Module Lane1(0) (0x489c[3:2]=b00) */
/* Silicon Lane3(2) -> Module Lane2(1) (0x489c[5:4]=b01) */
/* Silicon Lane4(3) -> Module Lane4(3) (0x489c[7:6]=b11) */
/*----------------------------------------------- */
	{0x489c, 0xd2},
};


static struct msm_camera_i2c_conf_array vd6869_init_conf[] = {
	{&vd6869_stop_core_1_settings[0],
	ARRAY_SIZE(vd6869_stop_core_1_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_stop_core_2_settings[0],
	ARRAY_SIZE(vd6869_stop_core_2_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_recommend_settings[0],
	ARRAY_SIZE(vd6869_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_start_core_2_settings[0],
	ARRAY_SIZE(vd6869_start_core_2_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_start_core_1_settings[0],
	ARRAY_SIZE(vd6869_start_core_1_settings), 50, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_recommend_settings_clearbit[0],
	ARRAY_SIZE(vd6869_recommend_settings_clearbit), 0, MSM_CAMERA_I2C_UNSET_BYTE_MASK},
	{&vd6869_recommend_2_settings[0],
	ARRAY_SIZE(vd6869_recommend_2_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_recommend_3_settings[0],
	ARRAY_SIZE(vd6869_recommend_3_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array vd6869_confs[] = {
	{&vd6869_16_9_settings_non_hdr[0],
	ARRAY_SIZE(vd6869_16_9_settings_non_hdr), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_prev_settings[0],
	ARRAY_SIZE(vd6869_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_video_settings[0],
	ARRAY_SIZE(vd6869_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_fast_video_settings[0],
	ARRAY_SIZE(vd6869_fast_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_hdr_settings[0],
	ARRAY_SIZE(vd6869_hdr_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_4_3_settings[0],
	ARRAY_SIZE(vd6869_4_3_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_fast_video_settings[0],
	ARRAY_SIZE(vd6869_fast_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_16_9_settings_non_hdr[0],
	ARRAY_SIZE(vd6869_16_9_settings_non_hdr), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_zoe_settings[0],
	ARRAY_SIZE(vd6869_zoe_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t vd6869_dimensions[] = {
	{/*non HDR 16:9*/
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x31b, /* 795 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
 		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*Q size*/
		.x_output = 0x540, /* 1344 */
		.y_output = 0x2f8, /* 760 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x280, /* 640 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 2,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*video size*/
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x500, /* 1280 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*fast video size*/
		.x_output = 0x540, /* 1344 */
		.y_output = 0x2f8, /* 760 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x190, /* 400 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 2,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*HDR 16:9*/
		.x_output = 0x7a0, /* 1952 */
		.y_output = 0x440, /* 1088 */
		.line_length_pclk = 0x12c0, /* 4800 */
		.frame_length_lines = 0x51e, /* 1310 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 1,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*4:3*/
		.x_output = 0x800, /* 2048 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x31b, /* 795 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*fast video 5:3*/ /* no use (copy from fast video)*/
		.x_output = 0x540, /* 1344 */
		.y_output = 0x2f8, /* 760 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x190, /* 400 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 2,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*5:3*/ /* no use (copy from non hdr 16:9)*/
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x31b, /* 795 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*zoe size*/
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0x1770, /* 6000 */ //0xBB8, /* 3000 */
		.frame_length_lines = 0x320, /* 800 */ //0x500, /* 1280 */
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
};


/*cut 1.0 setting*/
static struct msm_camera_i2c_reg_conf vd6869_prev_settings_cut10[] = {

//-----------------------------------------------
// Analogue settings for Bin2x2 - 1p0FW7p2Wk0113
//-----------------------------------------------
	{0x5800, 0x00},	// vtminor_0
	{0x5840, 0x00},	// SPEC_ANALOG_0
	{0x5844, 0x00},	// SPEC_ANALOG_1
	{0x5860, 0x15},	// SPEC_ANALOG_5
	{0x58a0, 0x3c},	// SPEC_ANALOG_15


/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x01}, /* full res = 0, binning = 1 */

/*----------------------------------------------- */
/* Output image size - 1344 x 760 */
/*----------------------------------------------- */
       {0x344, 0x00}, /* x_start_addr = 0 */
       {0x345, 0x00},
       {0x346, 0x00}, /* y_start_addr = 0 */
       {0x347, 0x00},
       {0x348, 0x0a}, /* x_addr_end = 2687 */
       {0x349, 0x7f},
       {0x34a, 0x05}, /* y_addr_end = 1519 */
       {0x34b, 0xef},
       {0x34c, 0x05}, /* x_output_size = 1344 */
       {0x34d, 0x40},
       {0x34e, 0x02}, /* y_output_size = 760 */
       {0x34f, 0xf8},
       {0x382, 0x00}, /* x_odd_inc = 1 */
       {0x383, 0x01},
       {0x386, 0x00}, /* y_odd_inc = 1 */
       {0x387, 0x01},


//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x30},     // pll_multiplier = 48

/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 10 */
	{0x301, 0x0a},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 2 */
	{0x30b, 0x02},

       {0x340, 0x03}, /* frame_length_lines = 816 lines */
       {0x341, 0x30},
       {0x342, 0x09}, /* line_length_pck = 2400 pcks */
       {0x343, 0x60},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
	{0x333A, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */


/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */
};

static struct msm_camera_i2c_reg_conf vd6869_video_settings_cut10[] = {

//-----------------------------------------------
// Analogue settings - 1p0FW7p2Wk0113
//-----------------------------------------------
/*aligment with sharp setting*/
	{0x5800, 0xe0},	// vtminor_0
	{0x5840, 0xc0},	// SPEC_ANALOG_0
	{0x5844, 0x01},	// SPEC_ANALOG_1
	{0x5860, 0x14},	// SPEC_ANALOG_5
	{0x58a0, 0x00},	// SPEC_ANALOG_15

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */


/*----------------------------------------------- */
/* Output image size - 2688 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x0a}, /* x_output_size = 2688 */
	{0x34d, 0x80},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},



//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x30},     // pll_multiplier = 48


/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},


	{0x340, 0x0A}, /* frame_length_lines = 2560 lines */
	{0x341, 0x00},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},


/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
	{0x333A, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */

/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */


/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */

};

static struct msm_camera_i2c_reg_conf vd6869_zoe_settings_cut10[] = {
//-----------------------------------------------
// Analogue settings - 1p0FW7p2Wk0113
//-----------------------------------------------
/*aligment with sharp setting*/
	{0x5800, 0xe0},	// vtminor_0
	{0x5840, 0xc0},	// SPEC_ANALOG_0
	{0x5844, 0x01},	// SPEC_ANALOG_1
	{0x5860, 0x14},	// SPEC_ANALOG_5
	{0x58a0, 0x00},	// SPEC_ANALOG_15

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */


/*----------------------------------------------- */
/* Output image size - 2688 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x0a}, /* x_output_size = 2688 */
	{0x34d, 0x80},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},



//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x30},     // pll_multiplier = 48

/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},


	{0x340, 0x06}, /* frame_length_lines = 1600 lines */
	{0x341, 0x40},
	{0x342, 0x17}, /* line_length_pck = 6000 pcks */
	{0x343, 0x70},


/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
	{0x333A, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */

/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */


/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},


/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */

};

static struct msm_camera_i2c_reg_conf vd6869_fast_video_settings_cut10[] = {

//-----------------------------------------------
// Analogue settings for Bin2x2 - 1p0FW7p2Wk0113
//-----------------------------------------------
	{0x5800, 0x00},	// vtminor_0
	{0x5840, 0x00},	// SPEC_ANALOG_0
	{0x5844, 0x00},	// SPEC_ANALOG_1
	{0x5860, 0x15},	// SPEC_ANALOG_5
	{0x58a0, 0x3c},	// SPEC_ANALOG_15


/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x01}, /* full res = 0, binning = 1 */

/*----------------------------------------------- */
/* Output image size - 1344 x 760 */
/*----------------------------------------------- */
       {0x344, 0x00}, /* x_start_addr = 0 */
       {0x345, 0x00},
       {0x346, 0x00}, /* y_start_addr = 0 */
       {0x347, 0x00},
       {0x348, 0x0a}, /* x_addr_end = 2687 */
       {0x349, 0x7f},
       {0x34a, 0x05}, /* y_addr_end = 1519 */
       {0x34b, 0xef},
       {0x34c, 0x05}, /* x_output_size = 1344 */
       {0x34d, 0x40},
       {0x34e, 0x02}, /* y_output_size = 760 */
       {0x34f, 0xf8},
       {0x382, 0x00}, /* x_odd_inc = 1 */
       {0x383, 0x01},
       {0x386, 0x00}, /* y_odd_inc = 1 */
       {0x387, 0x01},

//-----------------------------------------------
// Video timing - PLL clock = 960MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x50},     // pll_multiplier = 80

/*----------------------------------------------- */
/* Video timing 498MHz*/
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 10 */
	{0x301, 0x0a},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 2 */
	{0x30b, 0x02},

       {0x340, 0x03}, /* frame_length_lines = 816 lines */
       {0x341, 0x30},
       {0x342, 0x07}, /* line_length_pck = 2000 pcks */
       {0x343, 0xD0},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
	{0x333A, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */


};

static struct msm_camera_i2c_reg_conf vd6869_hdr_settings_cut10[] = {
//-----------------------------------------------
// Analogue settings - 1p0FW7p2Wk0113
//-----------------------------------------------
/*aligment with sharp setting*/
	{0x5800, 0xe0},	// vtminor_0
	{0x5840, 0xc0},	// SPEC_ANALOG_0
	{0x5844, 0x01},	// SPEC_ANALOG_1
	{0x5860, 0x14},	// SPEC_ANALOG_5
	{0x58a0, 0x00},	// SPEC_ANALOG_15

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */

/*----------------------------------------------- */
/* Output image size - 1952 x 1088 */
/*----------------------------------------------- */
	{0x344, 0x01}, /* x_start_addr = 368 */
	{0x345, 0x70},
	{0x346, 0x00}, /* y_start_addr = 216 */
	{0x347, 0xd8},
	{0x348, 0x09}, /* x_addr_end = 2319 */
	{0x349, 0x0f},
	{0x34a, 0x05}, /* y_addr_end = 1303 */
	{0x34b, 0x17},
	{0x34c, 0x07}, /* x_output_size = 1952 */
	{0x34d, 0xa0},
	{0x34e, 0x04}, /* y_output_size = 1088 */
	{0x34f, 0x40},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},


//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x30},     // pll_multiplier = 48

/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x05}, /* frame_length_lines = 1310 lines */
	{0x341, 0x36},
	{0x342, 0x17}, /* line_length_pck = 6000 pcks */
	{0x343, 0x70},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 201 (To set only in HDR Staggered) */
	{0x333a, 0xc9}, /* vtiming long2short offset = 201 (To set only in HDR Staggered) */

/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for HDR staggered mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (HDR staggered mode) */
/*----------------------------------------------- */
/* Expo 0 staggered = Long frame */
	{0x32e5, 0x01}, /* coarse_integration_time = 400 */
	{0x32e6, 0x90},
	{0x32e7, 0x80}, /* gain 2.0 == 8 << 4 == 128 */
	{0x32e8, 0x01}, /* digital_gain_greenR	= 1.0x (fixed point 4.8) */
	{0x32e9, 0x00},
	{0x32ea, 0x01}, /* digital_gain_red = 1.0x (fixed point 4.8) */
	{0x32eb, 0x00},
	{0x32ec, 0x01}, /* digital_gain_blue	= 1.0x (fixed point 4.8) */
	{0x32ed, 0x00},
	{0x32ee, 0x01}, /* digital_gain_greenB	= 1.0x (fixed point 4.8) */
	{0x32ef, 0x00},

/* Expo 1 staggered = Short Frame */
	{0x32f0, 0x00}, /* coarse_integration_time = 180 */
	{0x32f1, 0xb4},
	{0x32f3, 0x01}, /* digital_gain_greenR	= 1.0x (fixed point 4.8) */
	{0x32f4, 0x00},
	{0x32f5, 0x01}, /* digital_gain_red = 1.0x (fixed point 4.8) */
	{0x32f6, 0x00},
	{0x32f7, 0x01}, /* digital_gain_blue	= 1.0x (fixed point 4.8) */
	{0x32f8, 0x00},
	{0x32f9, 0x01}, /* digital_gain_greenB	= 1.0x (fixed point 4.8) */
	{0x32fa, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */
};

static struct msm_camera_i2c_reg_conf vd6869_4_3_settings_cut10[] = {

//-----------------------------------------------
// Analogue settings - 1p0FW7p2Wk0113
//-----------------------------------------------
/*aligment with sharp setting*/
	{0x5800, 0xe0},	// vtminor_0
	{0x5840, 0xc0},	// SPEC_ANALOG_0
	{0x5844, 0x01},	// SPEC_ANALOG_1
	{0x5860, 0x14},	// SPEC_ANALOG_5
	{0x58a0, 0x00},	// SPEC_ANALOG_15

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */

//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x30},     // pll_multiplier = 48

/*----------------------------------------------- */
/* Video timing - 576Mbps */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x06}, /* frame_length_lines = 1590 lines */
	{0x341, 0x36},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* Output image size - 2048 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x01}, /* x_start_addr = 320 */
	{0x345, 0x40},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x09}, /* x_addr_end = 2367 */
	{0x349, 0x3f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x08}, /* x_output_size = 2048 */
	{0x34d, 0x00},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
	{0x333A, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x01}, /* coarse_integration_time = 256 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},
};

static struct msm_camera_i2c_reg_conf vd6869_16_9_settings_non_hdr_cut10[] = {

//-----------------------------------------------
// Analogue settings - 1p0FW7p2Wk0113
//-----------------------------------------------
/*aligment with sharp setting*/
	{0x5800, 0xe0},	// vtminor_0
	{0x5840, 0xc0},	// SPEC_ANALOG_0
	{0x5844, 0x01},	// SPEC_ANALOG_1
	{0x5860, 0x14},	// SPEC_ANALOG_5
	{0x58a0, 0x00},	// SPEC_ANALOG_15

/*----------------------------------------------- */
/* Sensor configuration */
/*----------------------------------------------- */
	{0x900, 0x00}, /* full res = 0, binning = 1 */
/*----------------------------------------------- */
/* Output image size - 2688 x 1520 */
/*----------------------------------------------- */
	{0x344, 0x00}, /* x_start_addr = 0 */
	{0x345, 0x00},
	{0x346, 0x00}, /* y_start_addr = 0 */
	{0x347, 0x00},
	{0x348, 0x0a}, /* x_addr_end = 2687 */
	{0x349, 0x7f},
	{0x34a, 0x05}, /* y_addr_end = 1519 */
	{0x34b, 0xef},
	{0x34c, 0x0a}, /* x_output_size = 2688 */
	{0x34d, 0x80},
	{0x34e, 0x05}, /* y_output_size = 1520 */
	{0x34f, 0xf0},
	{0x382, 0x00}, /* x_odd_inc = 1 */
	{0x383, 0x01},
	{0x386, 0x00}, /* y_odd_inc = 1 */
	{0x387, 0x01},

//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
	{0x0304, 0x00},
	{0x0305, 0x02},     // pre_pll_clk_div = 2
	{0x0306, 0x00},
	{0x0307, 0x30},     // pll_multiplier = 48


/*----------------------------------------------- */
/* Video timing */
/*----------------------------------------------- */
	{0x300, 0x00}, /* vt_pix_clk_div = 5 */
	{0x301, 0x05},
	{0x302, 0x00}, /* vt_sys_clk_div = 1 */
	{0x303, 0x01},
	{0x30a, 0x00}, /* op_sys_clk_div = 1 */
	{0x30b, 0x01},

	{0x340, 0x06}, /* frame_length_lines = 795 x2  lines */
	{0x341, 0x36},
	{0x342, 0x0b}, /* line_length_pck = 3000 pcks */
	{0x343, 0xb8},

/*----------------------------------------------- */
/* NonHDR / HDR settings */
/*----------------------------------------------- */
	{0x3339, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */
	{0x333A, 0x00}, /* vtiming long2short offset = 0 (To set only in HDR Staggered) */

/*///////////////////////////////////////////////////////// */
/* - Exposure and gains for Non-HDR mode */
/* */
/* - The register Grouped_Parameter_Hold 0x0104 must be */
/* used in conjuction with exposure & gain settings for  */
/* precise timings control in Sequential & Staggered modes */
/* */
/* - Following should be re-written with WriteAutoIncrement */
/* */
/*///////////////////////////////////////////////////////// */

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Hold */
/*----------------------------------------------- */
	{0x104, 0x01}, /* Smia_Setup__Grouped_Parameter = Hold */

/*----------------------------------------------- */
/* Exposure and gains (normal mode) */
/*----------------------------------------------- */
	{0x202, 0x02}, /* coarse_integration_time = 512 */
	{0x203, 0x00},
	{0x204, 0x00}, /* gain 2.0 == 8 << 4 == 128 */
	{0x205, 0x80},
	{0x20e, 0x01}, /* digital_gain_greenR	= 1.0x	(fixed point 4.8) */
	{0x20f, 0x00},
	{0x210, 0x01}, /* digital_gain_red		= 1.0x	(fixed point 4.8) */
	{0x211, 0x00},
	{0x212, 0x01}, /* digital_gain_blue	= 1.0x	(fixed point 4.8) */
	{0x213, 0x00},
	{0x214, 0x01}, /* digital_gain_greenB	= 1.0x	(fixed point 4.8) */
	{0x215, 0x00},

/*----------------------------------------------- */
/* Smia_Setup__Grouped_Parameter = Released */
/*----------------------------------------------- */
	{0x104, 0x00}, /* Smia_Setup__Grouped_Parameter = Releas */
};


static struct msm_camera_i2c_reg_conf vd6869_recommand_cut10[]={
{0x4584, 0x01},
{0x4440, 0x42},
{0x4103, 0x03},
{0x6003, 0xF0},
{0x6004, 0xA3},
{0x6005, 0xA3},
{0x6006, 0xF0},
{0x6007, 0x02},
{0x6008, 0x50},
{0x6009, 0x26},
{0x600A, 0x90},
{0x600B, 0x31},
{0x600C, 0x86},
{0x600D, 0xE0},
{0x600E, 0x78},
{0x600F, 0x61},
{0x6010, 0xF6},
{0x6011, 0x90},
{0x6012, 0x31},
{0x6013, 0x88},
{0x6014, 0xE0},
{0x6015, 0x08},
{0x6016, 0xF6},
{0x6017, 0x90},
{0x6018, 0x31},
{0x6019, 0x8A},
{0x601A, 0xE0},
{0x601B, 0x08},
{0x601C, 0xF6},
{0x601D, 0x90},
{0x601E, 0x31},
{0x601F, 0x8C},
{0x6020, 0xE0},
{0x6021, 0x08},
{0x6022, 0xF6},
{0x6023, 0x02},
{0x6024, 0x0E},
{0x6025, 0xC3},
{0x6026, 0x02},
{0x6027, 0x1A},
{0x6028, 0x4D},
{0x6029, 0xE0},
{0x602A, 0x90},
{0x602B, 0x38},
{0x602C, 0xBB},
{0x602D, 0xE0},
{0x602E, 0xB4},
{0x602F, 0x01},
{0x6030, 0x6D},
{0x6031, 0x90},
{0x6032, 0x55},
{0x6033, 0x6C},
{0x6034, 0xE0},
{0x6035, 0x54},
{0x6036, 0x02},
{0x6037, 0x90},
{0x6038, 0x31},
{0x6039, 0x4C},
{0x603A, 0xB4},
{0x603B, 0x02},
{0x603C, 0x19},
{0x603D, 0xE0},
{0x603E, 0x90},
{0x603F, 0x4D},
{0x6040, 0xB9},
{0x6041, 0xF0},
{0x6042, 0xC3},
{0x6043, 0x04},
{0x6044, 0x90},
{0x6045, 0x4A},
{0x6046, 0xB9},
{0x6047, 0xF0},
{0x6048, 0x50},
{0x6049, 0x09},
{0x604A, 0x90},
{0x604B, 0x31},
{0x604C, 0x4B},
{0x604D, 0xE0},
{0x604E, 0x04},
{0x604F, 0x90},
{0x6050, 0x4A},
{0x6051, 0xB8},
{0x6052, 0xF0},
{0x6053, 0x02},
{0x6054, 0x3B},
{0x6055, 0x52},
{0x6056, 0xE0},
{0x6057, 0x90},
{0x6058, 0x4A},
{0x6059, 0xB9},
{0x605A, 0xF0},
{0x605B, 0xC3},
{0x605C, 0x04},
{0x605D, 0x90},
{0x605E, 0x4D},
{0x605F, 0xB9},
{0x6060, 0xF0},
{0x6061, 0x50},
{0x6062, 0x09},
{0x6063, 0x90},
{0x6064, 0x31},
{0x6065, 0x4B},
{0x6066, 0xE0},
{0x6067, 0x04},
{0x6068, 0x90},
{0x6069, 0x4D},
{0x606A, 0xB8},
{0x606B, 0xF0},
{0x606C, 0x02},
{0x606D, 0x3B},
{0x606E, 0x52},
{0x606F, 0x90},
{0x6070, 0x31},
{0x6071, 0xC5},
{0x6072, 0xE0},
{0x6073, 0x90},
{0x6074, 0x32},
{0x6075, 0xA8},
{0x6076, 0x60},
{0x6077, 0x04},
{0x6078, 0xF0},
{0x6079, 0x02},
{0x607A, 0x45},
{0x607B, 0xFB},
{0x607C, 0xF0},
{0x607D, 0x02},
{0x607E, 0x46},
{0x607F, 0x17},
{0x6080, 0x90},
{0x6081, 0x31},
{0x6082, 0x4A},
{0x6083, 0xE0},
{0x6084, 0x90},
{0x6085, 0x4D},
{0x6086, 0xB5},
{0x6087, 0xF0},
{0x6088, 0x90},
{0x6089, 0x4A},
{0x608A, 0xB5},
{0x608B, 0xF0},
{0x608C, 0x90},
{0x608D, 0x31},
{0x608E, 0x49},
{0x608F, 0xE0},
{0x6090, 0x90},
{0x6091, 0x4D},
{0x6092, 0xB4},
{0x6093, 0xF0},
{0x6094, 0x90},
{0x6095, 0x4A},
{0x6096, 0xB4},
{0x6097, 0xF0},
{0x6098, 0x12},
{0x6099, 0x3C},
{0x609A, 0xB1},
{0x609B, 0x02},
{0x609C, 0x4A},
{0x609D, 0xBF},
{0x609E, 0x90},
{0x609F, 0x31},
{0x60A0, 0x4C},
{0x60A1, 0xE0},
{0x60A2, 0x90},
{0x60A3, 0x4D},
{0x60A4, 0xB9},
{0x60A5, 0xF0},
{0x60A6, 0x90},
{0x60A7, 0x4A},
{0x60A8, 0xB9},
{0x60A9, 0xF0},
{0x60AA, 0x90},
{0x60AB, 0x31},
{0x60AC, 0x4B},
{0x60AD, 0xE0},
{0x60AE, 0x90},
{0x60AF, 0x4D},
{0x60B0, 0xB8},
{0x60B1, 0xF0},
{0x60B2, 0x90},
{0x60B3, 0x4A},
{0x60B4, 0xB8},
{0x60B5, 0xF0},
{0x60B6, 0x02},
{0x60B7, 0x3B},
{0x60B8, 0x52},
{0x60D8, 0xFA},
{0x60D9, 0xE6},
{0x60DA, 0xFB},
{0x60DB, 0x08},
{0x60DC, 0x08},
{0x60DD, 0xE6},
{0x60DE, 0xF9},
{0x60DF, 0x25},
{0x60E0, 0xF0},
{0x60E1, 0xF6},
{0x60E2, 0x18},
{0x60E3, 0xE6},
{0x60E4, 0xCA},
{0x60E5, 0x3A},
{0x60E6, 0xF6},
{0x60E7, 0x22},
{0x60EA, 0x90},
{0x60EB, 0x55},
{0x60EC, 0x00},
{0x60ED, 0x74},
{0x60EE, 0x01},
{0x60EF, 0xF0},
{0x60F0, 0x90},
{0x60F1, 0x55},
{0x60F2, 0x08},
{0x60F3, 0xF0},
{0x60F4, 0x90},
{0x60F5, 0x54},
{0x60F6, 0x00},
{0x60F7, 0xE0},
{0x60F8, 0x90},
{0x60F9, 0x55},
{0x60FA, 0x04},
{0x60FB, 0xB4},
{0x60FC, 0x01},
{0x60FD, 0x04},
{0x60FE, 0x74},
{0x60FF, 0x06},
{0x6100, 0x80},
{0x6101, 0x02},
{0x6102, 0x74},
{0x6103, 0x08},
{0x6104, 0xF0},
{0x6105, 0x90},
{0x6106, 0x55},
{0x6107, 0x0C},
{0x6108, 0xF0},
{0x6109, 0x02},
{0x610A, 0x27},
{0x610B, 0x34},
{0x6110, 0x90},
{0x6111, 0x54},
{0x6112, 0x0C},
{0x6113, 0xE0},
{0x6114, 0x44},
{0x6115, 0x04},
{0x6116, 0x54},
{0x6117, 0xBF},
{0x6118, 0xF0},
{0x6119, 0x90},
{0x611A, 0x33},
{0x611B, 0x37},
{0x611C, 0x02},
{0x611D, 0x27},
{0x611E, 0x90},
{0x6120, 0xAB},
{0x6121, 0x0F},
{0x6122, 0xAA},
{0x6123, 0x0E},
{0x6124, 0x85},
{0x6125, 0x0D},
{0x6126, 0x17},
{0x6127, 0x85},
{0x6128, 0x0C},
{0x6129, 0x16},
{0x612A, 0x02},
{0x612B, 0x3D},
{0x612C, 0xEB},
{0x612D, 0xA3},
{0x612E, 0xA3},
{0x612F, 0xF0},
{0x6130, 0x90},
{0x6131, 0x4A},
{0x6132, 0xC8},
{0x6133, 0x02},
{0x6134, 0x50},
{0x6135, 0x03},
{0x6136, 0xF0},
{0x6137, 0x90},
{0x6138, 0x31},
{0x6139, 0x53},
{0x613A, 0xE0},
{0x613B, 0x90},
{0x613C, 0x4D},
{0x613D, 0xC4},
{0x613E, 0xF0},
{0x613F, 0x90},
{0x6140, 0x4A},
{0x6141, 0xC4},
{0x6142, 0xF0},
{0x6143, 0x90},
{0x6144, 0x31},
{0x6145, 0x54},
{0x6146, 0xE0},
{0x6147, 0x90},
{0x6148, 0x4D},
{0x6149, 0xC5},
{0x614A, 0xF0},
{0x614B, 0x90},
{0x614C, 0x4A},
{0x614D, 0xC5},
{0x614E, 0xF0},
{0x614F, 0x02},
{0x6150, 0x3B},
{0x6151, 0x52},
{0x6152, 0x90},
{0x6153, 0x4D},
{0x6154, 0xC8},
{0x6155, 0x74},
{0x6156, 0x02},
{0x6157, 0xF0},
{0x6158, 0x80},
{0x6159, 0xD3},
{0x615A, 0x85},
{0x615B, 0x0A},
{0x615C, 0x82},
{0x615D, 0x85},
{0x615E, 0x09},
{0x615F, 0x83},
{0x6160, 0xE0},
{0x6161, 0x12},
{0x6162, 0x13},
{0x6163, 0x99},
{0x6164, 0xE4},
{0x6165, 0xFF},
{0x6166, 0x04},
{0x6167, 0x80},
{0x6168, 0x1B},
{0x6169, 0xEF},
{0x616A, 0xC3},
{0x616B, 0x94},
{0x616C, 0x11},
{0x616D, 0x50},
{0x616E, 0x1C},
{0x616F, 0x85},
{0x6170, 0x0A},
{0x6171, 0x82},
{0x6172, 0x85},
{0x6173, 0x09},
{0x6174, 0x83},
{0x6175, 0xE0},
{0x6176, 0x12},
{0x6177, 0x13},
{0x6178, 0x99},
{0x6179, 0x0F},
{0x617A, 0x05},
{0x617B, 0x0A},
{0x617C, 0xE5},
{0x617D, 0x0A},
{0x617E, 0x70},
{0x617F, 0x02},
{0x6180, 0x05},
{0x6181, 0x09},
{0x6182, 0x74},
{0x6183, 0x01},
{0x6184, 0x29},
{0x6185, 0xF9},
{0x6186, 0xE4},
{0x6187, 0x3A},
{0x6188, 0xFA},
{0x6189, 0x80},
{0x618A, 0xDE},
{0x618B, 0x02},
{0x618C, 0x3D},
{0x618D, 0x4C},
{0x618E, 0xE0},
{0x618F, 0x7A},
{0x6190, 0x4A},
{0x6191, 0x90},
{0x6192, 0x38},
{0x6193, 0xBB},
{0x6194, 0xE0},
{0x6195, 0x90},
{0x6196, 0x31},
{0x6197, 0x54},
{0x6198, 0xC3},
{0x6199, 0xB4},
{0x619A, 0x01},
{0x619B, 0x05},
{0x619C, 0xE0},
{0x619D, 0x94},
{0x619E, 0x01},
{0x619F, 0x80},
{0x61A0, 0x01},
{0x61A1, 0xE0},
{0x61A2, 0x90},
{0x61A3, 0x4D},
{0x61A4, 0xC5},
{0x61A5, 0xF0},
{0x61A6, 0x8A},
{0x61A7, 0x83},
{0x61A8, 0xF0},
{0x61A9, 0x90},
{0x61AA, 0x31},
{0x61AB, 0x53},
{0x61AC, 0xE0},
{0x61AD, 0x50},
{0x61AE, 0x02},
{0x61AF, 0x94},
{0x61B0, 0x01},
{0x61B1, 0x90},
{0x61B2, 0x4D},
{0x61B3, 0xC4},
{0x61B4, 0xF0},
{0x61B5, 0x8A},
{0x61B6, 0x83},
{0x61B7, 0xF0},
{0x61B8, 0x02},
{0x61B9, 0x4A},
{0x61BA, 0xC3},
{0x61CA, 0x90},
{0x61CB, 0x54},
{0x61CC, 0x00},
{0x61CD, 0xE0},
{0x61CE, 0xB4},
{0x61CF, 0x01},
{0x61D0, 0x23},
{0x61D1, 0x30},
{0x61D2, 0x01},
{0x61D3, 0x05},
{0x61D4, 0xE4},
{0x61D5, 0xF5},
{0x61D6, 0x0B},
{0x61D7, 0x80},
{0x61D8, 0x03},
{0x61D9, 0x75},
{0x61DA, 0x0B},
{0x61DB, 0x10},
{0x61DC, 0x90},
{0x61DD, 0x55},
{0x61DE, 0x04},
{0x61DF, 0x74},
{0x61E0, 0x05},
{0x61E1, 0xF0},
{0x61E2, 0x90},
{0x61E3, 0x55},
{0x61E4, 0x0C},
{0x61E5, 0xF0},
{0x61E6, 0x75},
{0x61E7, 0x09},
{0x61E8, 0x01},
{0x61E9, 0x75},
{0x61EA, 0x0A},
{0x61EB, 0xA0},
{0x61EC, 0x90},
{0x61ED, 0x54},
{0x61EE, 0x2C},
{0x61EF, 0x74},
{0x61F0, 0x02},
{0x61F1, 0xF0},
{0x61F2, 0x80},
{0x61F3, 0x21},
{0x61F4, 0x30},
{0x61F5, 0x01},
{0x61F6, 0x05},
{0x61F7, 0xE4},
{0x61F8, 0xF5},
{0x61F9, 0x0B},
{0x61FA, 0x80},
{0x61FB, 0x03},
{0x61FC, 0x75},
{0x61FD, 0x0B},
{0x61FE, 0x10},
{0x61FF, 0x90},
{0x6200, 0x55},
{0x6201, 0x04},
{0x6202, 0x74},
{0x6203, 0x06},
{0x6204, 0xF0},
{0x6205, 0x90},
{0x6206, 0x55},
{0x6207, 0x0C},
{0x6208, 0xF0},
{0x6209, 0x75},
{0x620A, 0x09},
{0x620B, 0x00},
{0x620C, 0x75},
{0x620D, 0x0A},
{0x620E, 0xE4},
{0x620F, 0x90},
{0x6210, 0x54},
{0x6211, 0x2C},
{0x6212, 0x74},
{0x6213, 0x03},
{0x6214, 0xF0},
{0x6215, 0x02},
{0x6216, 0x26},
{0x6217, 0xCE},
{0x621A, 0x85},
{0x621B, 0x2E},
{0x621C, 0x82},
{0x621D, 0x85},
{0x621E, 0x2D},
{0x621F, 0x83},
{0x6220, 0xE0},
{0x6221, 0xFD},
{0x6222, 0x54},
{0x6223, 0x0F},
{0x6224, 0x75},
{0x6225, 0x0D},
{0x6226, 0x00},
{0x6227, 0xF5},
{0x6228, 0x0C},
{0x6229, 0xED},
{0x622A, 0x54},
{0x622B, 0x30},
{0x622C, 0xC4},
{0x622D, 0x54},
{0x622E, 0x0F},
{0x622F, 0xF5},
{0x6230, 0x08},
{0x6231, 0xE0},
{0x6232, 0x54},
{0x6233, 0x40},
{0x6234, 0xC4},
{0x6235, 0x13},
{0x6236, 0x13},
{0x6237, 0x54},
{0x6238, 0x03},
{0x6239, 0xF5},
{0x623A, 0x09},
{0x623B, 0x05},
{0x623C, 0x2E},
{0x623D, 0xE5},
{0x623E, 0x2E},
{0x623F, 0x70},
{0x6240, 0x02},
{0x6241, 0x05},
{0x6242, 0x2D},
{0x6243, 0xF5},
{0x6244, 0x82},
{0x6245, 0x85},
{0x6246, 0x2D},
{0x6247, 0x83},
{0x6248, 0xE0},
{0x6249, 0x25},
{0x624A, 0x0D},
{0x624B, 0xF5},
{0x624C, 0x0D},
{0x624D, 0xE4},
{0x624E, 0x35},
{0x624F, 0x0C},
{0x6250, 0xF5},
{0x6251, 0x0C},
{0x6252, 0xE5},
{0x6253, 0x2E},
{0x6254, 0x24},
{0x6255, 0x01},
{0x6256, 0xFF},
{0x6257, 0xE4},
{0x6258, 0x35},
{0x6259, 0x2D},
{0x625A, 0xFA},
{0x625B, 0xA9},
{0x625C, 0x07},
{0x625D, 0x7B},
{0x625E, 0x01},
{0x625F, 0x8B},
{0x6260, 0x2F},
{0x6261, 0xF5},
{0x6262, 0x30},
{0x6263, 0x89},
{0x6264, 0x31},
{0x6265, 0x12},
{0x6266, 0x13},
{0x6267, 0x80},
{0x6268, 0x75},
{0x6269, 0xF0},
{0x626A, 0x08},
{0x626B, 0xA4},
{0x626C, 0xF5},
{0x626D, 0x0F},
{0x626E, 0x85},
{0x626F, 0xF0},
{0x6270, 0x0E},
{0x6271, 0x74},
{0x6272, 0x01},
{0x6273, 0x25},
{0x6274, 0x31},
{0x6275, 0xF5},
{0x6276, 0x31},
{0x6277, 0xE4},
{0x6278, 0x35},
{0x6279, 0x30},
{0x627A, 0xF5},
{0x627B, 0x30},
{0x627C, 0xAB},
{0x627D, 0x2F},
{0x627E, 0xFA},
{0x627F, 0xA9},
{0x6280, 0x31},
{0x6281, 0x12},
{0x6282, 0x13},
{0x6283, 0x80},
{0x6284, 0x54},
{0x6285, 0xE0},
{0x6286, 0xC4},
{0x6287, 0x13},
{0x6288, 0x54},
{0x6289, 0x07},
{0x628A, 0x25},
{0x628B, 0x0F},
{0x628C, 0xF5},
{0x628D, 0x0F},
{0x628E, 0xE4},
{0x628F, 0x35},
{0x6290, 0x0E},
{0x6291, 0xF5},
{0x6292, 0x0E},
{0x6293, 0xF5},
{0x6294, 0x16},
{0x6295, 0x12},
{0x6296, 0x3D},
{0x6297, 0xE4},
{0x6298, 0x85},
{0x6299, 0x11},
{0x629A, 0x82},
{0x629B, 0x85},
{0x629C, 0x10},
{0x629D, 0x83},
{0x629E, 0x12},
{0x629F, 0x14},
{0x62A0, 0xA6},
{0x62A1, 0x85},
{0x62A2, 0x11},
{0x62A3, 0x82},
{0x62A4, 0x85},
{0x62A5, 0x10},
{0x62A6, 0x83},
{0x62A7, 0x12},
{0x62A8, 0x14},
{0x62A9, 0x6E},
{0x62AA, 0xE4},
{0x62AB, 0xFB},
{0x62AC, 0xFA},
{0x62AD, 0xF9},
{0x62AE, 0xF8},
{0x62AF, 0xC3},
{0x62B0, 0x12},
{0x62B1, 0x14},
{0x62B2, 0x1D},
{0x62B3, 0x50},
{0x62B4, 0x02},
{0x62B5, 0x05},
{0x62B6, 0x0B},
{0x62B7, 0x74},
{0x62B8, 0x04},
{0x62B9, 0x25},
{0x62BA, 0x11},
{0x62BB, 0xF5},
{0x62BC, 0x11},
{0x62BD, 0xE4},
{0x62BE, 0x35},
{0x62BF, 0x10},
{0x62C0, 0xF5},
{0x62C1, 0x10},
{0x62C2, 0xAB},
{0x62C3, 0x2F},
{0x62C4, 0xAA},
{0x62C5, 0x30},
{0x62C6, 0xA9},
{0x62C7, 0x31},
{0x62C8, 0x12},
{0x62C9, 0x13},
{0x62CA, 0x80},
{0x62CB, 0xFF},
{0x62CC, 0x54},
{0x62CD, 0x04},
{0x62CE, 0xFE},
{0x62CF, 0x13},
{0x62D0, 0x13},
{0x62D1, 0x54},
{0x62D2, 0x3F},
{0x62D3, 0xF5},
{0x62D4, 0x09},
{0x62D5, 0xEF},
{0x62D6, 0x54},
{0x62D7, 0x03},
{0x62D8, 0xF5},
{0x62D9, 0x08},
{0x62DA, 0xE9},
{0x62DB, 0x24},
{0x62DC, 0x01},
{0x62DD, 0xF9},
{0x62DE, 0xE4},
{0x62DF, 0x3A},
{0x62E0, 0xAF},
{0x62E1, 0x01},
{0x62E2, 0xF5},
{0x62E3, 0x2D},
{0x62E4, 0x8F},
{0x62E5, 0x2E},
{0x62E6, 0x8F},
{0x62E7, 0x82},
{0x62E8, 0xF5},
{0x62E9, 0x83},
{0x62EA, 0xE0},
{0x62EB, 0x75},
{0x62EC, 0xF0},
{0x62ED, 0x10},
{0x62EE, 0xA4},
{0x62EF, 0xF5},
{0x62F0, 0x0D},
{0x62F1, 0x85},
{0x62F2, 0xF0},
{0x62F3, 0x0C},
{0x62F4, 0x05},
{0x62F5, 0x2E},
{0x62F6, 0xE5},
{0x62F7, 0x2E},
{0x62F8, 0x70},
{0x62F9, 0x02},
{0x62FA, 0x05},
{0x62FB, 0x2D},
{0x62FC, 0xF5},
{0x62FD, 0x82},
{0x62FE, 0x85},
{0x62FF, 0x2D},
{0x6300, 0x83},
{0x6301, 0xE0},
{0x6302, 0x54},
{0x6303, 0xF0},
{0x6304, 0xC4},
{0x6305, 0x54},
{0x6306, 0x0F},
{0x6307, 0x25},
{0x6308, 0x0D},
{0x6309, 0xF5},
{0x630A, 0x0D},
{0x630B, 0xE4},
{0x630C, 0x35},
{0x630D, 0x0C},
{0x630E, 0xF5},
{0x630F, 0x0C},
{0x6310, 0x75},
{0x6311, 0x2F},
{0x6312, 0x01},
{0x6313, 0x85},
{0x6314, 0x2D},
{0x6315, 0x30},
{0x6316, 0x85},
{0x6317, 0x2E},
{0x6318, 0x31},
{0x6319, 0xAB},
{0x631A, 0x2F},
{0x631B, 0xAA},
{0x631C, 0x30},
{0x631D, 0xA9},
{0x631E, 0x31},
{0x631F, 0x12},
{0x6320, 0x13},
{0x6321, 0x80},
{0x6322, 0x54},
{0x6323, 0x0F},
{0x6324, 0x75},
{0x6325, 0xF0},
{0x6326, 0x80},
{0x6327, 0xA4},
{0x6328, 0xF5},
{0x6329, 0x0F},
{0x632A, 0x85},
{0x632B, 0xF0},
{0x632C, 0x0E},
{0x632D, 0x74},
{0x632E, 0x01},
{0x632F, 0x25},
{0x6330, 0x31},
{0x6331, 0xF5},
{0x6332, 0x31},
{0x6333, 0xE4},
{0x6334, 0x35},
{0x6335, 0x30},
{0x6336, 0xF5},
{0x6337, 0x30},
{0x6338, 0xFA},
{0x6339, 0xA9},
{0x633A, 0x31},
{0x633B, 0x12},
{0x633C, 0x13},
{0x633D, 0x80},
{0x633E, 0xC3},
{0x633F, 0x13},
{0x6340, 0x25},
{0x6341, 0x0F},
{0x6342, 0xF5},
{0x6343, 0x0F},
{0x6344, 0xE4},
{0x6345, 0x35},
{0x6346, 0x0E},
{0x6347, 0xF5},
{0x6348, 0x0E},
{0x6349, 0xE9},
{0x634A, 0x24},
{0x634B, 0x01},
{0x634C, 0xF9},
{0x634D, 0xE4},
{0x634E, 0x3A},
{0x634F, 0xAF},
{0x6350, 0x01},
{0x6351, 0xF5},
{0x6352, 0x2D},
{0x6353, 0x8F},
{0x6354, 0x2E},
{0x6355, 0x02},
{0x6356, 0x28},
{0x6357, 0xED},
{0x635A, 0x02},
{0x635B, 0x57},
{0x635C, 0xAA},
{0x635D, 0xE0},
{0x635E, 0xFF},
{0x635F, 0x90},
{0x6360, 0x55},
{0x6361, 0x94},
{0x6362, 0xE0},
{0x6363, 0xB5},
{0x6364, 0x07},
{0x6365, 0x14},
{0x6366, 0x90},
{0x6367, 0x54},
{0x6368, 0x4C},
{0x6369, 0xE0},
{0x636A, 0xFE},
{0x636B, 0xA3},
{0x636C, 0xE0},
{0x636D, 0xFF},
{0x636E, 0x90},
{0x636F, 0x55},
{0x6370, 0x58},
{0x6371, 0xE0},
{0x6372, 0x6E},
{0x6373, 0x70},
{0x6374, 0x03},
{0x6375, 0xA3},
{0x6376, 0xE0},
{0x6377, 0x6F},
{0x6378, 0x60},
{0x6379, 0x08},
{0x637A, 0x90},
{0x637B, 0x33},
{0x637C, 0x16},
{0x637D, 0x74},
{0x637E, 0x01},
{0x637F, 0xF0},
{0x6380, 0x80},
{0x6381, 0x20},
{0x6382, 0x90},
{0x6383, 0x33},
{0x6384, 0x16},
{0x6385, 0xE0},
{0x6386, 0xB4},
{0x6387, 0x01},
{0x6388, 0x0F},
{0x6389, 0xE4},
{0x638A, 0x90},
{0x638B, 0x33},
{0x638C, 0x40},
{0x638D, 0xF0},
{0x638E, 0x90},
{0x638F, 0x33},
{0x6390, 0x4C},
{0x6391, 0xF0},
{0x6392, 0x90},
{0x6393, 0x33},
{0x6394, 0x16},
{0x6395, 0xF0},
{0x6396, 0x80},
{0x6397, 0x0A},
{0x6398, 0x90},
{0x6399, 0x33},
{0x639A, 0x40},
{0x639B, 0x74},
{0x639C, 0x01},
{0x639D, 0xF0},
{0x639E, 0x90},
{0x639F, 0x33},
{0x63A0, 0x4C},
{0x63A1, 0xF0},
{0x63A2, 0x12},
{0x63A3, 0x29},
{0x63A4, 0x2B},
{0x63A5, 0x02},
{0x63A6, 0x23},
{0x63A7, 0xC8},
{0x63AA, 0x90},
{0x63AB, 0x54},
{0x63AC, 0x50},
{0x63AD, 0xE0},
{0x63AE, 0xFF},
{0x63AF, 0xA3},
{0x63B0, 0xE0},
{0x63B1, 0x90},
{0x63B2, 0x54},
{0x63B3, 0x54},
{0x63B4, 0xCF},
{0x63B5, 0xF0},
{0x63B6, 0xA3},
{0x63B7, 0xEF},
{0x63B8, 0xF0},
{0x63B9, 0x90},
{0x63BA, 0x33},
{0x63BB, 0x39},
{0x63BC, 0x02},
{0x63BD, 0x05},
{0x63BE, 0xF6},
{0x63C0, 0x02},
{0x63C1, 0x13},
{0x63C2, 0x09},
{0x63C3, 0x02},
{0x63C4, 0x3B},
{0x63C5, 0x39},
{0x63CA, 0xE5},
{0x63CB, 0x09},
{0x63CC, 0x75},
{0x63CD, 0xF0},
{0x63CE, 0x0C},
{0x63CF, 0xA4},
{0x63D0, 0x24},
{0x63D1, 0x41},
{0x63D2, 0xF5},
{0x63D3, 0x82},
{0x63D4, 0xE4},
{0x63D5, 0x34},
{0x63D6, 0x33},
{0x63D7, 0xAF},
{0x63D8, 0x82},
{0x63D9, 0x90},
{0x63DA, 0x39},
{0x63DB, 0x3E},
{0x63DC, 0x12},
{0x63DD, 0x56},
{0x63DE, 0x7E},
{0x63DF, 0xE0},
{0x63E0, 0xFE},
{0x63E1, 0xA3},
{0x63E2, 0xE0},
{0x63E3, 0x24},
{0x63E4, 0x02},
{0x63E5, 0xFF},
{0x63E6, 0xE4},
{0x63E7, 0x3E},
{0x63E8, 0x90},
{0x63E9, 0x39},
{0x63EA, 0x44},
{0x63EB, 0x12},
{0x63EC, 0x56},
{0x63ED, 0x7E},
{0x63EE, 0xE0},
{0x63EF, 0xFE},
{0x63F0, 0xA3},
{0x63F1, 0xE0},
{0x63F2, 0x24},
{0x63F3, 0x0A},
{0x63F4, 0xFF},
{0x63F5, 0xE4},
{0x63F6, 0x3E},
{0x63F7, 0x90},
{0x63F8, 0x39},
{0x63F9, 0x4C},
{0x63FA, 0xF0},
{0x63FB, 0xA3},
{0x63FC, 0xEF},
{0x63FD, 0xF0},
{0x63FE, 0xE5},
{0x63FF, 0x09},
{0x6400, 0x75},
{0x6401, 0xF0},
{0x6402, 0x08},
{0x6403, 0xA4},
{0x6404, 0x24},
{0x6405, 0x57},
{0x6406, 0xF5},
{0x6407, 0x82},
{0x6408, 0xE4},
{0x6409, 0x34},
{0x640A, 0x33},
{0x640B, 0xAF},
{0x640C, 0x82},
{0x640D, 0x90},
{0x640E, 0x39},
{0x640F, 0x54},
{0x6410, 0xF0},
{0x6411, 0xA3},
{0x6412, 0xEF},
{0x6413, 0xF0},
{0x6414, 0xE5},
{0x6415, 0x09},
{0x6416, 0x90},
{0x6417, 0x39},
{0x6418, 0x36},
{0x6419, 0x70},
{0x641A, 0x09},
{0x641B, 0x74},
{0x641C, 0x39},
{0x641D, 0xF0},
{0x641E, 0xA3},
{0x641F, 0x74},
{0x6420, 0x26},
{0x6421, 0xF0},
{0x6422, 0x80},
{0x6423, 0x07},
{0x6424, 0x74},
{0x6425, 0x39},
{0x6426, 0xF0},
{0x6427, 0xA3},
{0x6428, 0x74},
{0x6429, 0x2E},
{0x642A, 0xF0},
{0x642B, 0xE4},
{0x642C, 0xF5},
{0x642D, 0x08},
{0x642E, 0xE5},
{0x642F, 0x08},
{0x6430, 0xC3},
{0x6431, 0x94},
{0x6432, 0x04},
{0x6433, 0x40},
{0x6434, 0x03},
{0x6435, 0x02},
{0x6436, 0x55},
{0x6437, 0x5C},
{0x6438, 0x90},
{0x6439, 0x39},
{0x643A, 0x36},
{0x643B, 0xE0},
{0x643C, 0xFE},
{0x643D, 0xA3},
{0x643E, 0xE0},
{0x643F, 0xFF},
{0x6440, 0xF5},
{0x6441, 0x82},
{0x6442, 0x8E},
{0x6443, 0x83},
{0x6444, 0xE0},
{0x6445, 0xFC},
{0x6446, 0xA3},
{0x6447, 0xE0},
{0x6448, 0x4C},
{0x6449, 0x60},
{0x644A, 0x12},
{0x644B, 0xE5},
{0x644C, 0x09},
{0x644D, 0x75},
{0x644E, 0xF0},
{0x644F, 0x0C},
{0x6450, 0xA4},
{0x6451, 0x24},
{0x6452, 0x40},
{0x6453, 0xF5},
{0x6454, 0x82},
{0x6455, 0xE4},
{0x6456, 0x34},
{0x6457, 0x33},
{0x6458, 0xF5},
{0x6459, 0x83},
{0x645A, 0xE0},
{0x645B, 0x70},
{0x645C, 0x15},
{0x645D, 0x90},
{0x645E, 0x39},
{0x645F, 0x4C},
{0x6460, 0x12},
{0x6461, 0x56},
{0x6462, 0xB9},
{0x6463, 0xE0},
{0x6464, 0xFD},
{0x6465, 0xA3},
{0x6466, 0xE0},
{0x6467, 0x8F},
{0x6468, 0x82},
{0x6469, 0x8E},
{0x646A, 0x83},
{0x646B, 0xCD},
{0x646C, 0xF0},
{0x646D, 0xA3},
{0x646E, 0xED},
{0x646F, 0xF0},
{0x6470, 0x80},
{0x6471, 0x4C},
{0x6472, 0x12},
{0x6473, 0x56},
{0x6474, 0x92},
{0x6475, 0x12},
{0x6476, 0x4C},
{0x6477, 0xE7},
{0x6478, 0x90},
{0x6479, 0x39},
{0x647A, 0x5E},
{0x647B, 0xEE},
{0x647C, 0xF0},
{0x647D, 0xA3},
{0x647E, 0xEF},
{0x647F, 0xF0},
{0x6480, 0x90},
{0x6481, 0x39},
{0x6482, 0x4C},
{0x6483, 0xE0},
{0x6484, 0xFE},
{0x6485, 0xA3},
{0x6486, 0xE0},
{0x6487, 0xF5},
{0x6488, 0x82},
{0x6489, 0x8E},
{0x648A, 0x83},
{0x648B, 0xE0},
{0x648C, 0xFE},
{0x648D, 0xA3},
{0x648E, 0xE0},
{0x648F, 0xFF},
{0x6490, 0x12},
{0x6491, 0x4C},
{0x6492, 0xE7},
{0x6493, 0x90},
{0x6494, 0x39},
{0x6495, 0x5E},
{0x6496, 0xE0},
{0x6497, 0xFC},
{0x6498, 0xA3},
{0x6499, 0xE0},
{0x649A, 0xFD},
{0x649B, 0x7B},
{0x649C, 0x01},
{0x649D, 0x12},
{0x649E, 0x4C},
{0x649F, 0x8F},
{0x64A0, 0x7D},
{0x64A1, 0x03},
{0x64A2, 0x7F},
{0x64A3, 0x00},
{0x64A4, 0x7E},
{0x64A5, 0x44},
{0x64A6, 0x12},
{0x64A7, 0x4C},
{0x64A8, 0xB5},
{0x64A9, 0x90},
{0x64AA, 0x39},
{0x64AB, 0x5E},
{0x64AC, 0xE0},
{0x64AD, 0xFE},
{0x64AE, 0xA3},
{0x64AF, 0xE0},
{0x64B0, 0xFF},
{0x64B1, 0xE4},
{0x64B2, 0xFD},
{0x64B3, 0x12},
{0x64B4, 0x4C},
{0x64B5, 0xB5},
{0x64B6, 0x12},
{0x64B7, 0x56},
{0x64B8, 0x6F},
{0x64B9, 0xEE},
{0x64BA, 0xF0},
{0x64BB, 0xA3},
{0x64BC, 0xEF},
{0x64BD, 0xF0},
{0x64BE, 0x12},
{0x64BF, 0x56},
{0x64C0, 0x92},
{0x64C1, 0x12},
{0x64C2, 0x4C},
{0x64C3, 0xE7},
{0x64C4, 0x8E},
{0x64C5, 0x13},
{0x64C6, 0x8F},
{0x64C7, 0x14},
{0x64C8, 0x90},
{0x64C9, 0x39},
{0x64CA, 0x67},
{0x64CB, 0xE0},
{0x64CC, 0xFE},
{0x64CD, 0xA3},
{0x64CE, 0xE0},
{0x64CF, 0xFF},
{0x64D0, 0x90},
{0x64D1, 0x39},
{0x64D2, 0x6B},
{0x64D3, 0xE0},
{0x64D4, 0xFC},
{0x64D5, 0xA3},
{0x64D6, 0xE0},
{0x64D7, 0xFD},
{0x64D8, 0x90},
{0x64D9, 0x39},
{0x64DA, 0x69},
{0x64DB, 0xE0},
{0x64DC, 0xFA},
{0x64DD, 0xA3},
{0x64DE, 0xE0},
{0x64DF, 0xFB},
{0x64E0, 0x90},
{0x64E1, 0x39},
{0x64E2, 0x6D},
{0x64E3, 0xE0},
{0x64E4, 0xF5},
{0x64E5, 0x10},
{0x64E6, 0xA3},
{0x64E7, 0xE0},
{0x64E8, 0xF5},
{0x64E9, 0x11},
{0x64EA, 0x90},
{0x64EB, 0x39},
{0x64EC, 0x66},
{0x64ED, 0xE0},
{0x64EE, 0xF5},
{0x64EF, 0x12},
{0x64F0, 0x90},
{0x64F1, 0x39},
{0x64F2, 0x6F},
{0x64F3, 0xE0},
{0x64F4, 0xF5},
{0x64F5, 0x15},
{0x64F6, 0xA3},
{0x64F7, 0xE0},
{0x64F8, 0xF5},
{0x64F9, 0x16},
{0x64FA, 0x12},
{0x64FB, 0x55},
{0x64FC, 0x5F},
{0x64FD, 0x12},
{0x64FE, 0x56},
{0x64FF, 0x6F},
{0x6500, 0xE0},
{0x6501, 0xFC},
{0x6502, 0xA3},
{0x6503, 0xE0},
{0x6504, 0xFD},
{0x6505, 0x90},
{0x6506, 0x39},
{0x6507, 0x3E},
{0x6508, 0xE0},
{0x6509, 0xFA},
{0x650A, 0xA3},
{0x650B, 0xE0},
{0x650C, 0xF5},
{0x650D, 0x82},
{0x650E, 0x8A},
{0x650F, 0x83},
{0x6510, 0xE0},
{0x6511, 0xFA},
{0x6512, 0xA3},
{0x6513, 0xE0},
{0x6514, 0xC3},
{0x6515, 0x9D},
{0x6516, 0xFD},
{0x6517, 0xEA},
{0x6518, 0x9C},
{0x6519, 0xCD},
{0x651A, 0xC3},
{0x651B, 0x9F},
{0x651C, 0xFF},
{0x651D, 0xED},
{0x651E, 0x9E},
{0x651F, 0xFE},
{0x6520, 0x90},
{0x6521, 0x39},
{0x6522, 0x44},
{0x6523, 0x12},
{0x6524, 0x56},
{0x6525, 0xB9},
{0x6526, 0xEE},
{0x6527, 0xF0},
{0x6528, 0xA3},
{0x6529, 0xEF},
{0x652A, 0xF0},
{0x652B, 0x12},
{0x652C, 0x56},
{0x652D, 0x92},
{0x652E, 0x90},
{0x652F, 0x39},
{0x6530, 0x54},
{0x6531, 0x12},
{0x6532, 0x56},
{0x6533, 0xB9},
{0x6534, 0xEE},
{0x6535, 0xF0},
{0x6536, 0xA3},
{0x6537, 0xEF},
{0x6538, 0xF0},
{0x6539, 0x05},
{0x653A, 0x08},
{0x653B, 0x90},
{0x653C, 0x39},
{0x653D, 0x4C},
{0x653E, 0x12},
{0x653F, 0x56},
{0x6540, 0xA3},
{0x6541, 0x90},
{0x6542, 0x39},
{0x6543, 0x3E},
{0x6544, 0x12},
{0x6545, 0x56},
{0x6546, 0xA3},
{0x6547, 0x90},
{0x6548, 0x39},
{0x6549, 0x44},
{0x654A, 0x12},
{0x654B, 0x56},
{0x654C, 0xA3},
{0x654D, 0x90},
{0x654E, 0x39},
{0x654F, 0x36},
{0x6550, 0x12},
{0x6551, 0x56},
{0x6552, 0xA3},
{0x6553, 0x90},
{0x6554, 0x39},
{0x6555, 0x54},
{0x6556, 0x12},
{0x6557, 0x56},
{0x6558, 0xA3},
{0x6559, 0x02},
{0x655A, 0x54},
{0x655B, 0x2E},
{0x655C, 0x02},
{0x655D, 0x26},
{0x655E, 0x04},
{0x655F, 0x8E},
{0x6560, 0x0A},
{0x6561, 0x8F},
{0x6562, 0x0B},
{0x6563, 0x8A},
{0x6564, 0x0E},
{0x6565, 0x8B},
{0x6566, 0x0F},
{0x6567, 0x8D},
{0x6568, 0x82},
{0x6569, 0x8C},
{0x656A, 0x83},
{0x656B, 0xE5},
{0x656C, 0x12},
{0x656D, 0x70},
{0x656E, 0x05},
{0x656F, 0x02},
{0x6570, 0x56},
{0x6571, 0xE0},
{0x6572, 0x00},
{0x6573, 0x00},
{0x6574, 0xAD},
{0x6575, 0x82},
{0x6576, 0xAC},
{0x6577, 0x83},
{0x6578, 0xE4},
{0x6579, 0xFF},
{0x657A, 0xFE},
{0x657B, 0x12},
{0x657C, 0x56},
{0x657D, 0x4C},
{0x657E, 0x40},
{0x657F, 0x06},
{0x6580, 0xF5},
{0x6581, 0x83},
{0x6582, 0xF5},
{0x6583, 0x82},
{0x6584, 0x80},
{0x6585, 0x10},
{0x6586, 0xAF},
{0x6587, 0x82},
{0x6588, 0xAE},
{0x6589, 0x83},
{0x658A, 0x7D},
{0x658B, 0x00},
{0x658C, 0x7C},
{0x658D, 0x3E},
{0x658E, 0x12},
{0x658F, 0x56},
{0x6590, 0x5E},
{0x6591, 0x50},
{0x6592, 0x03},
{0x6593, 0x90},
{0x6594, 0x3E},
{0x6595, 0x00},
{0x6596, 0xAD},
{0x6597, 0x11},
{0x6598, 0xAC},
{0x6599, 0x10},
{0x659A, 0xE4},
{0x659B, 0xFF},
{0x659C, 0xFE},
{0x659D, 0x12},
{0x659E, 0x56},
{0x659F, 0x4C},
{0x65A0, 0x40},
{0x65A1, 0x06},
{0x65A2, 0xF5},
{0x65A3, 0x10},
{0x65A4, 0xF5},
{0x65A5, 0x11},
{0x65A6, 0x80},
{0x65A7, 0x13},
{0x65A8, 0x7D},
{0x65A9, 0x00},
{0x65AA, 0x7C},
{0x65AB, 0x3E},
{0x65AC, 0xAF},
{0x65AD, 0x11},
{0x65AE, 0xAE},
{0x65AF, 0x10},
{0x65B0, 0x12},
{0x65B1, 0x56},
{0x65B2, 0x5E},
{0x65B3, 0x50},
{0x65B4, 0x06},
{0x65B5, 0x75},
{0x65B6, 0x10},
{0x65B7, 0x3E},
{0x65B8, 0x75},
{0x65B9, 0x11},
{0x65BA, 0x00},
{0x65BB, 0xE5},
{0x65BC, 0x12},
{0x65BD, 0x14},
{0x65BE, 0x70},
{0x65BF, 0x06},
{0x65C0, 0x85},
{0x65C1, 0x13},
{0x65C2, 0x1B},
{0x65C3, 0x85},
{0x65C4, 0x14},
{0x65C5, 0x1C},
{0x65C6, 0xAD},
{0x65C7, 0x0B},
{0x65C8, 0xAC},
{0x65C9, 0x0A},
{0x65CA, 0xAF},
{0x65CB, 0x1C},
{0x65CC, 0xAE},
{0x65CD, 0x1B},
{0x65CE, 0x12},
{0x65CF, 0x56},
{0x65D0, 0x4C},
{0x65D1, 0x50},
{0x65D2, 0x08},
{0x65D3, 0x85},
{0x65D4, 0x83},
{0x65D5, 0x1D},
{0x65D6, 0x85},
{0x65D7, 0x82},
{0x65D8, 0x1E},
{0x65D9, 0x80},
{0x65DA, 0x63},
{0x65DB, 0xAD},
{0x65DC, 0x0F},
{0x65DD, 0xAC},
{0x65DE, 0x0E},
{0x65DF, 0xAF},
{0x65E0, 0x1C},
{0x65E1, 0xAE},
{0x65E2, 0x1B},
{0x65E3, 0x12},
{0x65E4, 0x56},
{0x65E5, 0x5E},
{0x65E6, 0x50},
{0x65E7, 0x08},
{0x65E8, 0x85},
{0x65E9, 0x10},
{0x65EA, 0x1D},
{0x65EB, 0x85},
{0x65EC, 0x11},
{0x65ED, 0x1E},
{0x65EE, 0x80},
{0x65EF, 0x4E},
{0x65F0, 0xAD},
{0x65F1, 0x82},
{0x65F2, 0xAC},
{0x65F3, 0x83},
{0x65F4, 0x7B},
{0x65F5, 0x01},
{0x65F6, 0xAF},
{0x65F7, 0x11},
{0x65F8, 0xAE},
{0x65F9, 0x10},
{0x65FA, 0x12},
{0x65FB, 0x4C},
{0x65FC, 0x8F},
{0x65FD, 0x8E},
{0x65FE, 0x17},
{0x65FF, 0x8F},
{0x6600, 0x18},
{0x6601, 0xAD},
{0x6602, 0x0B},
{0x6603, 0xAC},
{0x6604, 0x0A},
{0x6605, 0xAF},
{0x6606, 0x0F},
{0x6607, 0xAE},
{0x6608, 0x0E},
{0x6609, 0x12},
{0x660A, 0x4C},
{0x660B, 0x8F},
{0x660C, 0x7D},
{0x660D, 0x03},
{0x660E, 0xAF},
{0x660F, 0x18},
{0x6610, 0xAE},
{0x6611, 0x17},
{0x6612, 0x12},
{0x6613, 0x4C},
{0x6614, 0xC6},
{0x6615, 0x8E},
{0x6616, 0x19},
{0x6617, 0x8F},
{0x6618, 0x1A},
{0x6619, 0xAD},
{0x661A, 0x0B},
{0x661B, 0xAC},
{0x661C, 0x0A},
{0x661D, 0xAF},
{0x661E, 0x1C},
{0x661F, 0xAE},
{0x6620, 0x1B},
{0x6621, 0x12},
{0x6622, 0x4C},
{0x6623, 0x8F},
{0x6624, 0x7D},
{0x6625, 0x02},
{0x6626, 0xAF},
{0x6627, 0x1A},
{0x6628, 0xAE},
{0x6629, 0x19},
{0x662A, 0x12},
{0x662B, 0x4C},
{0x662C, 0xC6},
{0x662D, 0x8E},
{0x662E, 0x17},
{0x662F, 0x8F},
{0x6630, 0x18},
{0x6631, 0xAF},
{0x6632, 0x82},
{0x6633, 0xAE},
{0x6634, 0x83},
{0x6635, 0xE4},
{0x6636, 0xFD},
{0x6637, 0x12},
{0x6638, 0x4C},
{0x6639, 0xC6},
{0x663A, 0x8E},
{0x663B, 0x1D},
{0x663C, 0x8F},
{0x663D, 0x1E},
{0x663E, 0x02},
{0x663F, 0x56},
{0x6640, 0xCA},
{0x6641, 0x00},
{0x6642, 0x00},
{0x6643, 0x00},
{0x6644, 0x00},
{0x6645, 0x00},
{0x6646, 0x00},
{0x6647, 0x00},
{0x6648, 0x00},
{0x6649, 0x00},
{0x664A, 0x00},
{0x664B, 0x22},
{0x664C, 0x8E},
{0x664D, 0xC3},
{0x664E, 0x8F},
{0x664F, 0xC2},
{0x6650, 0x8C},
{0x6651, 0xC5},
{0x6652, 0x8D},
{0x6653, 0xC4},
{0x6654, 0x75},
{0x6655, 0xC0},
{0x6656, 0x84},
{0x6657, 0x30},
{0x6658, 0xCF},
{0x6659, 0xFD},
{0x665A, 0xA2},
{0x665B, 0xCE},
{0x665C, 0xB3},
{0x665D, 0x22},
{0x665E, 0x8E},
{0x665F, 0xC3},
{0x6660, 0x8F},
{0x6661, 0xC2},
{0x6662, 0x8C},
{0x6663, 0xC5},
{0x6664, 0x8D},
{0x6665, 0xC4},
{0x6666, 0x75},
{0x6667, 0xC0},
{0x6668, 0x84},
{0x6669, 0x30},
{0x666A, 0xCF},
{0x666B, 0xFD},
{0x666C, 0xA2},
{0x666D, 0xCE},
{0x666E, 0x22},
{0x666F, 0x12},
{0x6670, 0x4B},
{0x6671, 0x94},
{0x6672, 0x90},
{0x6673, 0x39},
{0x6674, 0x36},
{0x6675, 0xE0},
{0x6676, 0xFC},
{0x6677, 0xA3},
{0x6678, 0xE0},
{0x6679, 0xF5},
{0x667A, 0x82},
{0x667B, 0x8C},
{0x667C, 0x83},
{0x667D, 0x22},
{0x667E, 0xF0},
{0x667F, 0xA3},
{0x6680, 0xEF},
{0x6681, 0xF0},
{0x6682, 0xE5},
{0x6683, 0x09},
{0x6684, 0x75},
{0x6685, 0xF0},
{0x6686, 0x0C},
{0x6687, 0xA4},
{0x6688, 0x24},
{0x6689, 0x49},
{0x668A, 0xF5},
{0x668B, 0x82},
{0x668C, 0xE4},
{0x668D, 0x34},
{0x668E, 0x33},
{0x668F, 0xF5},
{0x6690, 0x83},
{0x6691, 0x22},
{0x6692, 0x90},
{0x6693, 0x39},
{0x6694, 0x36},
{0x6695, 0xE0},
{0x6696, 0xFE},
{0x6697, 0xA3},
{0x6698, 0xE0},
{0x6699, 0xF5},
{0x669A, 0x82},
{0x669B, 0x8E},
{0x669C, 0x83},
{0x669D, 0xE0},
{0x669E, 0xFE},
{0x669F, 0xA3},
{0x66A0, 0xE0},
{0x66A1, 0xFF},
{0x66A2, 0x22},
{0x66A3, 0xE4},
{0x66A4, 0x75},
{0x66A5, 0xF0},
{0x66A6, 0x02},
{0x66A7, 0x02},
{0x66A8, 0x13},
{0x66A9, 0xBD},
{0x66AA, 0x75},
{0x66AB, 0xF0},
{0x66AC, 0x0C},
{0x66AD, 0xA4},
{0x66AE, 0x24},
{0x66AF, 0x3F},
{0x66B0, 0xF5},
{0x66B1, 0x82},
{0x66B2, 0xE4},
{0x66B3, 0x34},
{0x66B4, 0x33},
{0x66B5, 0xF5},
{0x66B6, 0x83},
{0x66B7, 0xE0},
{0x66B8, 0x22},
{0x66B9, 0xE0},
{0x66BA, 0xFC},
{0x66BB, 0xA3},
{0x66BC, 0xE0},
{0x66BD, 0xF5},
{0x66BE, 0x82},
{0x66BF, 0x8C},
{0x66C0, 0x83},
{0x66C1, 0x22},
{0x66CA, 0x7B},
{0x66CB, 0x02},
{0x66CC, 0xAD},
{0x66CD, 0x16},
{0x66CE, 0xAC},
{0x66CF, 0x15},
{0x66D0, 0xAF},
{0x66D1, 0x1E},
{0x66D2, 0xAE},
{0x66D3, 0x1D},
{0x66D4, 0x12},
{0x66D5, 0x4C},
{0x66D6, 0x8F},
{0x66D7, 0x8E},
{0x66D8, 0x1D},
{0x66D9, 0x8F},
{0x66DA, 0x1E},
{0x66DB, 0x02},
{0x66DC, 0x56},
{0x66DD, 0x4B},
{0x66E0, 0xE4},
{0x66E1, 0x7B},
{0x66E2, 0x02},
{0x66E3, 0xFD},
{0x66E4, 0xFC},
{0x66E5, 0xAF},
{0x66E6, 0x16},
{0x66E7, 0xAE},
{0x66E8, 0x15},
{0x66E9, 0x02},
{0x66EA, 0x56},
{0x66EB, 0xD4},
{0x66F0, 0x90},
{0x66F1, 0x40},
{0x66F2, 0x00},
{0x66F3, 0xE0},
{0x66F4, 0x44},
{0x66F5, 0x04},
{0x66F6, 0xF0},
{0x66F7, 0xE0},
{0x66F8, 0x44},
{0x66F9, 0x02},
{0x66FA, 0xF0},
{0x66FB, 0x90},
{0x66FC, 0x40},
{0x66FD, 0x03},
{0x66FE, 0xE0},
{0x66FF, 0x44},
{0x6700, 0x02},
{0x6701, 0xF0},
{0x6702, 0x90},
{0x6703, 0x4C},
{0x6704, 0xA4},
{0x6705, 0xE4},
{0x6706, 0xF0},
{0x6707, 0xA3},
{0x6708, 0x74},
{0x6709, 0x07},
{0x670A, 0xF0},
{0x670B, 0x90},
{0x670C, 0x4F},
{0x670D, 0xA4},
{0x670E, 0xE4},
{0x670F, 0xF0},
{0x6710, 0xA3},
{0x6711, 0x74},
{0x6712, 0x07},
{0x6713, 0xF0},
{0x6714, 0x02},
{0x6715, 0x36},
{0x6716, 0xEB},
{0x671A, 0x90},
{0x671B, 0x40},
{0x671C, 0x00},
{0x671D, 0xE0},
{0x671E, 0x54},
{0x671F, 0xFD},
{0x6720, 0xF0},
{0x6721, 0x90},
{0x6722, 0x40},
{0x6723, 0x03},
{0x6724, 0xE0},
{0x6725, 0x54},
{0x6726, 0xFD},
{0x6727, 0xF0},
{0x6728, 0x02},
{0x6729, 0x48},
{0x672A, 0x4B},
{0x6730, 0x90},
{0x6731, 0x40},
{0x6732, 0x00},
{0x6733, 0xE0},
{0x6734, 0x44},
{0x6735, 0x04},
{0x6736, 0xF0},
{0x6737, 0x90},
{0x6738, 0x33},
{0x6739, 0xF4},
{0x673A, 0xE0},
{0x673B, 0x04},
{0x673C, 0xF0},
{0x673D, 0x90},
{0x673E, 0x32},
{0x673F, 0xA7},
{0x6740, 0xE0},
{0x6741, 0x44},
{0x6742, 0x02},
{0x6743, 0xF0},
{0x6744, 0x02},
{0x6745, 0x49},
{0x6746, 0x0E},
{0x674A, 0x90},
{0x674B, 0x33},
{0x674C, 0xF6},
{0x674D, 0xE0},
{0x674E, 0x04},
{0x674F, 0xF0},
{0x6750, 0x90},
{0x6751, 0x32},
{0x6752, 0xA7},
{0x6753, 0xE0},
{0x6754, 0x44},
{0x6755, 0x08},
{0x6756, 0xF0},
{0x6757, 0x02},
{0x6758, 0x44},
{0x6759, 0x30},
{0x675A, 0x90},
{0x675B, 0x32},
{0x675C, 0xA6},
{0x675D, 0x02},
{0x675E, 0x23},
{0x675F, 0x80},
{0x6762, 0x90},
{0x6763, 0x32},
{0x6764, 0xA6},
{0x6765, 0xE0},
{0x6766, 0xB4},
{0x6767, 0x08},
{0x6768, 0x03},
{0x6769, 0x02},
{0x676A, 0x24},
{0x676B, 0x09},
{0x676C, 0xB4},
{0x676D, 0x09},
{0x676E, 0x13},
{0x676F, 0x7F},
{0x6770, 0x01},
{0x6771, 0x12},
{0x6772, 0x24},
{0x6773, 0x66},
{0x6774, 0x90},
{0x6775, 0x32},
{0x6776, 0xA6},
{0x6777, 0x74},
{0x6778, 0x03},
{0x6779, 0xF0},
{0x677A, 0xA3},
{0x677B, 0xE0},
{0x677C, 0x54},
{0x677D, 0xF7},
{0x677E, 0xF0},
{0x677F, 0x02},
{0x6780, 0x24},
{0x6781, 0x63},
{0x6782, 0xB4},
{0x6783, 0x07},
{0x6784, 0x13},
{0x6785, 0x7F},
{0x6786, 0x00},
{0x6787, 0x12},
{0x6788, 0x24},
{0x6789, 0x66},
{0x678A, 0x90},
{0x678B, 0x32},
{0x678C, 0xA6},
{0x678D, 0x74},
{0x678E, 0x03},
{0x678F, 0xF0},
{0x6790, 0xA3},
{0x6791, 0xE0},
{0x6792, 0x54},
{0x6793, 0xFD},
{0x6794, 0xF0},
{0x6795, 0x02},
{0x6796, 0x24},
{0x6797, 0x63},
{0x6798, 0x02},
{0x6799, 0x24},
{0x679A, 0x33},
{0x679B, 0x02},
{0x679C, 0x4B},
{0x679D, 0xD1},
{0x67AA, 0x90},
{0x67AB, 0x54},
{0x67AC, 0xA8},
{0x67AD, 0xE0},
{0x67AE, 0xFF},
{0x67AF, 0x90},
{0x67B0, 0x55},
{0x67B1, 0x94},
{0x67B2, 0xE0},
{0x67B3, 0xB5},
{0x67B4, 0x07},
{0x67B5, 0x28},
{0x67B6, 0x90},
{0x67B7, 0x54},
{0x67B8, 0x4C},
{0x67B9, 0xE0},
{0x67BA, 0xFE},
{0x67BB, 0xA3},
{0x67BC, 0xE0},
{0x67BD, 0xFF},
{0x67BE, 0x90},
{0x67BF, 0x55},
{0x67C0, 0x58},
{0x67C1, 0xE0},
{0x67C2, 0xB5},
{0x67C3, 0x06},
{0x67C4, 0x19},
{0x67C5, 0xA3},
{0x67C6, 0xE0},
{0x67C7, 0xB5},
{0x67C8, 0x07},
{0x67C9, 0x14},
{0x67CA, 0x90},
{0x67CB, 0x54},
{0x67CC, 0x50},
{0x67CD, 0xE0},
{0x67CE, 0xFE},
{0x67CF, 0xA3},
{0x67D0, 0xE0},
{0x67D1, 0xFF},
{0x67D2, 0x90},
{0x67D3, 0x55},
{0x67D4, 0x5C},
{0x67D5, 0xE0},
{0x67D6, 0x6E},
{0x67D7, 0x70},
{0x67D8, 0x03},
{0x67D9, 0xA3},
{0x67DA, 0xE0},
{0x67DB, 0x6F},
{0x67DC, 0x60},
{0x67DD, 0x03},
{0x67DE, 0x02},
{0x67DF, 0x53},
{0x67E0, 0x7A},
{0x67E1, 0x02},
{0x67E2, 0x53},
{0x67E3, 0x82},
{0x67EE, 0x02},
{0x67EF, 0x29},
{0x67F0, 0x7E},
{0x67F1, 0x02},
{0x67F2, 0x29},
{0x67F3, 0xEE},
{0x67F4, 0x02},
{0x67F5, 0x2A},
{0x67F6, 0x44},
{0x67F7, 0xE0},
{0x67F8, 0x44},
{0x67F9, 0x01},
{0x67FA, 0x02},
{0x67FB, 0x18},
{0x67FC, 0x59},
{0x67FD, 0x02},
{0x67FE, 0x24},
{0x67FF, 0xDB},
{0x4106, 0x18},
{0x4107, 0x74},
{0x410A, 0x00},
{0x410B, 0x09},
{0x410F, 0x03},
{0x4112, 0x4A},
{0x4113, 0xBC},
{0x4116, 0x00},
{0x4117, 0x80},
{0x411B, 0x02},
{0x411E, 0x3B},
{0x411F, 0x51},
{0x4122, 0x00},
{0x4123, 0x29},
{0x4127, 0x02},
{0x412A, 0x1D},
{0x412B, 0x77},
{0x412E, 0x00},
{0x412F, 0x70},
{0x4133, 0x03},
{0x4136, 0x30},
{0x4137, 0x4A},
{0x413A, 0x00},
{0x413B, 0x00},
{0x413F, 0x03},
{0x4142, 0x3D},
{0x4143, 0x28},
{0x4146, 0x01},
{0x4147, 0x5A},
{0x414B, 0x02},
{0x414E, 0x45},
{0x414F, 0xEB},
{0x4152, 0x00},
{0x4153, 0x6F},
{0x4157, 0x02},
{0x415A, 0x26},
{0x415B, 0x83},
{0x415E, 0x01},
{0x415F, 0xCA},
{0x4163, 0x02},
{0x4166, 0x27},
{0x4167, 0x1C},
{0x416A, 0x00},
{0x416B, 0xEA},
{0x416F, 0x02},
{0x4172, 0x27},
{0x4173, 0x8D},
{0x4176, 0x01},
{0x4177, 0x10},
{0x417B, 0x02},
{0x417E, 0x27},
{0x417F, 0xB7},
{0x4182, 0x02},
{0x4183, 0x1A},
{0x4187, 0x02},
{0x418A, 0x29},
{0x418B, 0x6A},
{0x418E, 0x07},
{0x418F, 0xEE},
{0x4193, 0x02},
{0x4196, 0x29},
{0x4197, 0xDE},
{0x419A, 0x07},
{0x419B, 0xF1},
{0x419F, 0x02},
{0x41A2, 0x36},
{0x41A3, 0x78},
{0x41A6, 0x06},
{0x41A7, 0xF0},
{0x41AB, 0x02},
{0x41AE, 0x23},
{0x41AF, 0xC5},
{0x41B2, 0x03},
{0x41B3, 0x5A},
{0x41B7, 0x02},
{0x41BA, 0x48},
{0x41BB, 0x44},
{0x41BE, 0x07},
{0x41BF, 0x1A},
{0x41C3, 0x02},
{0x41C6, 0x49},
{0x41C7, 0x07},
{0x41CA, 0x07},
{0x41CB, 0x30},
{0x41CF, 0x02},
{0x41D2, 0x05},
{0x41D3, 0xF3},
{0x41D6, 0x03},
{0x41D7, 0xAA},
{0x41DB, 0x02},
{0x41DE, 0x25},
{0x41DF, 0x48},
{0x41E2, 0x03},
{0x41E3, 0xCA},
{0x41E7, 0x02},
{0x41EA, 0x18},
{0x41EB, 0x57},
{0x41EE, 0x07},
{0x41EF, 0xF7},
{0x41F3, 0x02},
{0x41F6, 0x3B},
{0x41F7, 0x0A},
{0x41FA, 0x03},
{0x41FB, 0xC3},
{0x41FF, 0x02},
{0x4202, 0x12},
{0x4203, 0xFC},
{0x4206, 0x03},
{0x4207, 0xC0},
{0x420B, 0x02},
{0x420E, 0x44},
{0x420F, 0x07},
{0x4212, 0x07},
{0x4213, 0x4A},
{0x4217, 0x02},
{0x421A, 0x23},
{0x421B, 0x6C},
{0x421E, 0x07},
{0x421F, 0x5A},
{0x4223, 0x02},
{0x4226, 0x23},
{0x4227, 0xFE},
{0x422A, 0x07},
{0x422B, 0x62},
{0x422F, 0x02},
{0x4232, 0x4B},
{0x4233, 0xCE},
{0x4236, 0x07},
{0x4237, 0x9B},
{0x423B, 0x02},
{0x423E, 0x2A},
{0x423F, 0x34},
{0x4242, 0x07},
{0x4243, 0xF4},
{0x4247, 0x02},
{0x424A, 0x24},
{0x424B, 0x92},
{0x424E, 0x07},
{0x424F, 0xFD},
{0x4253, 0x02},
{0x4256, 0x27},
{0x4257, 0xA6},
{0x425A, 0x00},
{0x425B, 0xFE},
{0x425F, 0x03},
{0x4262, 0x3D},
{0x4263, 0xE4},
{0x4266, 0x01},
{0x4267, 0x20},
{0x426B, 0x02},
{0x426E, 0x4A},
{0x426F, 0xC2},
{0x4272, 0x01},
{0x4273, 0x8E},
{0x4277, 0x02},
{0x427A, 0x19},
{0x427B, 0xBD},
{0x427E, 0x01},
{0x427F, 0x52},
{0x4283, 0x02},
{0x4103, 0x01},
{0x4440, 0x43},
};


static struct msm_camera_i2c_reg_conf vd6869_recommand_cut10_1[]={

{0x4584, 0x00},
{0x4E01, 0x01},
{0x4B01, 0x01},
{0x4A20, 0x3b},
{0x4D20, 0x3b},
{0x3337, 0x07},
{0x5524, 0x19},
{0x5528, 0x19},
{0x33BE, 0x03},
{0x0105, 0x01},
{0x3BFE, 0x02},
{0x3BFF, 0x29},
{0x5804, 0x42},
{0x5808, 0x40},
{0x580C, 0x00},
{0x5814, 0x70},
{0x581C, 0x80},
{0x5820, 0x0d},
{0x5852, 0x38},
{0x5856, 0xc4},
{0x5864, 0x54},
{0x5868, 0xcf},
{0x5872, 0x1f},
{0x5876, 0x62},
{0x5880, 0x00},
{0x5884, 0x01},
{0x5888, 0x10},
{0x5892, 0x3e},
{0x5896, 0xfd},
{0x58A4, 0xff},
{0x58A8, 0xff},
{0x58B2, 0xff},
{0x58B6, 0x03},
{0x54DC, 0x14},
{0x33A4, 0x07},
{0x33A5, 0xc0},
{0x33A6, 0x00},
{0x33A7, 0x40},
{0x3399, 0x07},
{0x339A, 0xc0},
{0x339B, 0x00},
{0x339C, 0x40},
{0x4DD6, 0x07},
{0x4DD7, 0xc0},
{0x4DDA, 0x07},
{0x4DDB, 0xc0},
{0x4AD6, 0x07},
{0x4AD7, 0xc0},
{0x4ADA, 0x07},
{0x4ADB, 0xc0},
{0x4E66, 0x00},
{0x4E67, 0x00},
{0x4E6A, 0x00},
{0x4E6B, 0x00},
{0x4B66, 0x00},
{0x4B67, 0x00},
{0x4B6A, 0x00},
{0x4B6B, 0x00},
{0x0902, 0x03},
{0x3967, 0x4e},
{0x3968, 0xbc},
{0x396B, 0x00},
{0x396C, 0x00},
{0x3969, 0x51},
{0x396A, 0xAC},
{0x396D, 0x3e},
{0x396E, 0x00},
{0x396F, 0x49},
{0x3970, 0x20},
{0x3966, 0x01},
{0x32FD, 0x02},
{0x32FC, 0x01},
{0x3392, 0x01},
{0x3395, 0x01},


//-----------------------------------------------
// Pre-streaming internal configuration
//-----------------------------------------------
{0x0114, 0x03},                   // csi_lane_mode = quad lane
{0x0121, 0x18},
{0x0122, 0x00},     // extclk_frequency_mhz = 24.0Mhz

//-----------------------------------------------
// Video timing - PLL clock = 576MHz
//-----------------------------------------------
{0x0304, 0x00},
{0x0305, 0x02},     // pre_pll_clk_div = 2
{0x0306, 0x00},
{0x0307, 0x30},     // pll_multiplier = 48


//-----------------------------------------------
// Sensor configuration
//-----------------------------------------------
{0x0101, 0x03},     // image_orientation = XY flip
{0x5848, 0x00},	// SPEC_ANALOG_2
{0x5810, 0x44},	// vtminor_4
{0x5818, 0xc7},	// vtminor_6: if(HDRStag + Binning + Yflip)=0xc6, else=0xc7

};


static struct msm_camera_i2c_reg_conf vd6869_recommand_cut10_2[]={
/*aligment with sharp setting*/
{0x4E01, 0x01},	//tiny flash
{0x4B01, 0x01},	//tiny flash
};


static struct msm_camera_i2c_conf_array vd6869_init_conf_cut10[] = {
	/*cut 1.0*/
	{&vd6869_recommand_cut10[0],
	ARRAY_SIZE(vd6869_recommand_cut10), 50, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_recommand_cut10_1[0],
	ARRAY_SIZE(vd6869_recommand_cut10_1), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_recommand_cut10_2[0],
	ARRAY_SIZE(vd6869_recommand_cut10_2), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array vd6869_confs_cut10[] = {
	{&vd6869_16_9_settings_non_hdr_cut10[0],
	ARRAY_SIZE(vd6869_16_9_settings_non_hdr_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_prev_settings_cut10[0],
	ARRAY_SIZE(vd6869_prev_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_video_settings_cut10[0],
	ARRAY_SIZE(vd6869_video_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_fast_video_settings_cut10[0],
	ARRAY_SIZE(vd6869_fast_video_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_hdr_settings_cut10[0],
	ARRAY_SIZE(vd6869_hdr_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_4_3_settings_cut10[0],
	ARRAY_SIZE(vd6869_4_3_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_fast_video_settings_cut10[0],
	ARRAY_SIZE(vd6869_fast_video_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_16_9_settings_non_hdr_cut10[0],
	ARRAY_SIZE(vd6869_16_9_settings_non_hdr_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&vd6869_zoe_settings_cut10[0],
	ARRAY_SIZE(vd6869_zoe_settings_cut10), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t vd6869_dimensions_cut10[] = {
	{/*non HDR 16:9*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x636, /* 795x2 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*Q size*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0x540, /* 1344 */
		.y_output = 0x2f8, /* 760 */
		.line_length_pclk = 0x960, /* 2400 */
		.frame_length_lines = 0x330, /* 816 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 115200000,
		.op_pixel_clk = 115200000,
		.binning_factor = 2,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*video size*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0xA00, /* 2560 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*fast video size*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0x540, /* 1344 */
		.y_output = 0x2f8, /* 760 */
		.line_length_pclk = 0x7D0, /* 2000 */
		.frame_length_lines = 0x330, /* 816 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 199200000,
		.op_pixel_clk = 199200000,
		.binning_factor = 2,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*HDR 16:9*/
		.x_addr_start = 0x170,/* x_start_addr = 368 */
		.y_addr_start = 0xd8,  /* y_start_addr = 216 */
		.x_output = 0x7a0, /* 1952 */
		.y_output = 0x440, /* 1088 */
		.line_length_pclk = 0x2580, /* 4800*2 */
		.frame_length_lines = 0x536, /* 1334 */
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 1,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*4:3*/
		.x_addr_start = 0x140,/* x_start_addr = 320 */
		.y_addr_start = 0,
		.x_output = 0x800, /* 2048 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x636, /* 1590 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*fast video 5:3*/ /* no use (copy from fast video)*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0x540, /* 1344 */
		.y_output = 0x2f8, /* 760 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x190, /* 400 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*5:3*/ /* no use (copy from non hdr 16:9)*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0xBB8, /* 3000 */
		.frame_length_lines = 0x31b, /* 795 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
	{/*zoe size*/
		.x_addr_start = 0,
		.y_addr_start = 0,
		.x_output = 0xA80, /* 2688 */
		.y_output = 0x5F0, /* 1520 */
		.line_length_pclk = 0x1770, /* 6000 */
		.frame_length_lines = 0x640, /* 1600 */
		/*this is use for calculate framerate in aec, since frame length line is different with cut0.9
		   so times 2 in vt clock for correct fomula in calculate fps*/
		.vt_pixel_clk = 230400000,
		.op_pixel_clk = 230400000,
		.binning_factor = 1,
		.is_hdr = 0,
		.yushan_status_line_enable = 1,
		.yushan_status_line = 2,
		.yushan_sensor_status_line = 2,
	},
};


static struct v4l2_subdev_info vd6869_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_csid_vc_cfg vd6869_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params vd6869_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 4,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = vd6869_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 4,
		.settle_cnt = 20,
	},
};

static struct msm_camera_csi2_params *vd6869_csi_params_array[] = {
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params,
	&vd6869_csi_params
};

static struct msm_sensor_output_reg_addr_t vd6869_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t vd6869_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0365,
};
#define SENSOR_REGISTER_MAX_LINECOUNT 0xffff
#define SENSOR_VERT_OFFSET 25

static struct msm_sensor_exp_gain_info_t vd6869_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x204,
	.vert_offset = SENSOR_VERT_OFFSET, /* vert_offset would be used to limit sensor_max_linecount, need to re-calculate sensor_max_linecount if changed */
	.min_vert = 4, /* min coarse integration time */ /* HTC Angie 20111019 - Fix FPS */
	.sensor_max_linecount = SENSOR_REGISTER_MAX_LINECOUNT-SENSOR_VERT_OFFSET, /* sensor max linecount = max unsigned value of linecount register size - vert_offset */ /* HTC ben 20120229 */
};

#define SENSOR_VERT_OFFSET_HDR 4

static struct vd6869_hdr_exp_info_t vd6869_hdr_gain_info = {
	.long_coarse_int_time_addr_h = 0x32e5,
	.long_coarse_int_time_addr_l = 0x32e6,
	.short_coarse_int_time_addr_h = 0x32f0,
	.short_coarse_int_time_addr_l = 0x32f1,
	.global_gain_addr = 0x32e7,
	.vert_offset = SENSOR_VERT_OFFSET_HDR,
	.sensor_max_linecount = SENSOR_REGISTER_MAX_LINECOUNT-SENSOR_VERT_OFFSET_HDR, /* sensor max linecount = max unsigned value of linecount register size - vert_offset */ /* HTC ben 20120229 */
};


static int vd6869_i2c_read(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	int i = 0,rc = 0;
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++) {
		rc = msm_camera_i2c_read(client, addr,data,data_type);
		if(rc < 0)
			pr_err("%s write otp init table error.... retry", __func__);
		else
			return rc;
		mdelay(1);
	}
	return rc;
}


int32_t vd6869_set_dig_gain(struct msm_sensor_ctrl_t *s_ctrl, uint16_t dig_gain)
{
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		REG_DIGITAL_GAIN_GREEN_R, dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		REG_DIGITAL_GAIN_RED, dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		REG_DIGITAL_GAIN_BLUE, dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		REG_DIGITAL_GAIN_GREEN_B, dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);

	return 0;
}

int32_t vd6869_set_hdr_dig_gain_cut09
	(struct msm_sensor_ctrl_t *s_ctrl, uint16_t long_dig_gain, uint16_t short_dig_gain)
{
	int rc = 0;
	/*HDR long exposure frame*/
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_GREEN_R, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_RED, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_BLUE, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_DIGITAL_GAIN_GREEN_B, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;

	/*HDR short exposure frame*/
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_GREEN_R, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_RED, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_BLUE, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_DIGITAL_GAIN_GREEN_B, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;

	return 0;
}


int32_t vd6869_set_hdr_dig_gain_cut10
	(struct msm_sensor_ctrl_t *s_ctrl, uint16_t long_dig_gain, uint16_t short_dig_gain)
{
	int rc;
	/*HDR long exposure frame*/
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_GREEN_R, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_RED, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_BLUE, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_LONG_REG_CUT10_DIGITAL_GAIN_GREEN_B, long_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;

	/*HDR short exposure frame*/
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_GREEN_R, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_RED, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_BLUE, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		STAGGERED_HDR_SHORT_REG_CUT10_DIGITAL_GAIN_GREEN_B, short_dig_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	if(rc < 0)
		return rc;

	return 0;

}

int32_t vd6869_set_hdr_dig_gain(struct msm_sensor_ctrl_t *s_ctrl, uint16_t long_dig_gain, uint16_t short_dig_gain)
{
	int rc = 0;
	if(s_ctrl->sensordata->sensor_cut == 0)/*cut0.9*/
		rc = vd6869_set_hdr_dig_gain_cut09(s_ctrl, long_dig_gain, short_dig_gain);
	else if(s_ctrl->sensordata->sensor_cut == 1)/*cut1.0*/
		rc = vd6869_set_hdr_dig_gain_cut10(s_ctrl, long_dig_gain, short_dig_gain);
	else{
		pr_err("vd6869 unknow version");
		rc = -1;
	}

	return rc;
}

static int vd6869_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	if (data->sensor_platform_info){
		vd6869_s_ctrl.mirror_flip = data->sensor_platform_info->mirror_flip;

		if(vd6869_s_ctrl.driver_ic == 0x11){/*Closed Loop-LC898212*/
			if(data->sensor_platform_info->sensor_mount_angle == ANGLE_90){
				vd6869_s_ctrl.mirror_flip = CAMERA_SENSOR_NONE;
			}
			else if(data->sensor_platform_info->sensor_mount_angle == ANGLE_270){
				vd6869_s_ctrl.mirror_flip = CAMERA_SENSOR_MIRROR_FLIP;
			}
		}else if(vd6869_s_ctrl.driver_ic == 0x1){//Default is the rumbas_act
			if(data->sensor_platform_info->sensor_mount_angle == ANGLE_270){/*The mount angle is different in dlxp projects*/
				vd6869_s_ctrl.mirror_flip = CAMERA_SENSOR_NONE;
			}
		}else
			pr_info("%s vd6869_s_ctrl->sensordata->sensor_cut=%d", __func__, vd6869_s_ctrl.sensordata->sensor_cut);
		data->sensor_platform_info->mirror_flip = vd6869_s_ctrl.mirror_flip;
		pr_info("vd6869_sensor_open_init,vd6869_s_ctrl.mirror_flip=%d,data->sensor_platform_info->mirror_flip=%d", vd6869_s_ctrl.mirror_flip, data->sensor_platform_info->mirror_flip);
		pr_info("vd6869_sensor_open_init,sensor_mount_angle=%d", data->sensor_platform_info->sensor_mount_angle);
		pr_info("vd6869_sensor_open_init,vd6869_s_ctrl.driver_ic=%d", vd6869_s_ctrl.driver_ic);
		/*Chuck add ews enable flag*/
		vd6869_s_ctrl.ews_enable = data->sensor_platform_info->ews_enable;
		pr_info("vd6869_s_ctrl.ews_enable=%d", vd6869_s_ctrl.ews_enable);
	}
	return rc;
}

#define VD6869_VER_UNKNOWN 0xFF

struct vd6869_ver_map {
	uint8_t val;
	char *str;
};

static struct vd6869_ver_map vd6869_ver_tab[] = {  /* vd6869_ver_map.str max length: 32 - strlen(vd6869NAME) -1 */
	{ 0x09, "(0.9)"},	/* ver 0.9 */
	{ 0x0A, "(0.9e)"},	/* ver 0.9 enhancement */
	{ 0x10, "(1.0)"},	/* ver 1.0 */
	{ VD6869_VER_UNKNOWN, "(unknown)"}  /* VD6869_VER_UNKNOWN item must be the last one of this table */
};

static uint8_t vd6869_ver = VD6869_VER_UNKNOWN;
static uint8_t vd6869_year_mon = 0;
static uint8_t vd6869_date = 0;

static const char *vd6869Vendor = "st";
static const char *vd6869NAME = "vd6869";
static const char *vd6869Size = "cinesensor";

static ssize_t sensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	uint8_t i = 0;
	uint8_t len = ARRAY_SIZE(vd6869_ver_tab);
	char vd6869NAME_ver[32];
	uint16_t year = 0;
	uint8_t month = 0;
	uint8_t date = 0;
	CDBG("%s called\n", __func__);

	memset(vd6869NAME_ver, 0, sizeof(vd6869NAME_ver));
	for (i = 0; i < len; i++) {
		if (vd6869_ver == vd6869_ver_tab[i].val)
			break;
	}
	if (i < len)  /* mapped, show its string */
		snprintf(vd6869NAME_ver, sizeof(vd6869NAME_ver), "%s%s",
			vd6869NAME, vd6869_ver_tab[i].str);
	else  /* unmapped, show unknown and its value */
		snprintf(vd6869NAME_ver, sizeof(vd6869NAME_ver), "%s%s-%02X",
			vd6869NAME, vd6869_ver_tab[len - 1].str, vd6869_ver);
	CDBG("%s: version(%d) : %s\n", __func__, vd6869_ver, vd6869NAME_ver);

	year  = ((vd6869_year_mon & 0xf0)>> 4);
	month = (vd6869_year_mon & 0x0f);
	date  = ((vd6869_date & 0xf8) >> 3);

	if((year == 0)&&(month == 0)&&(date == 0))/* not support OTP date */
		pr_err("%s: Invalid OTP date\n", __func__);
	else
		year += 2000; /* year:13, align format to 2013 */

	snprintf(buf, PAGE_SIZE, "%s %s %s %04d-%02d-%02d \n", vd6869Vendor, vd6869NAME_ver, vd6869Size, year, month, date);
	/*snprintf(buf, PAGE_SIZE, "%s %s %s\n", vd6869Vendor, vd6869NAME, vd6869Size);*/  /* without version */
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);

static struct kobject *android_vd6869;

static int vd6869_sysfs_init(void)
{
	int ret ;
	pr_info("%s: vd6869:kobject creat and add\n", __func__);

	android_vd6869 = kobject_create_and_add("android_camera", NULL);
	if (android_vd6869 == NULL) {
		pr_info("vd6869_sysfs_init: subsystem_register " \
		"failed\n");
		ret = -ENOMEM;
		return ret ;
	}
	pr_info("vd6869:sysfs_create_file\n");
	ret = sysfs_create_file(android_vd6869, &dev_attr_sensor.attr);
	if (ret) {
		pr_info("vd6869_sysfs_init: sysfs_create_file " \
		"failed\n");
		kobject_del(android_vd6869);
	}

	return 0 ;
}

int32_t vd6869_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int	rc = 0;
	pr_info("%s\n", __func__);

	rc = msm_sensor_i2c_probe(client, id);
	if(rc >= 0)
		vd6869_sysfs_init();
	pr_info("%s: rc(%d)\n", __func__, rc);
	return rc;
}

static const struct i2c_device_id vd6869_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&vd6869_s_ctrl},
	{ }
};

static struct i2c_driver vd6869_i2c_driver = {
	.id_table = vd6869_i2c_id,
	.probe  = vd6869_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client vd6869_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int vd6869_shut_down_otp(struct msm_sensor_ctrl_t *s_ctrl,uint16_t addr, uint16_t data){
	int rc=0,i;
	for(i = 0; i < OTP_WAIT_TIMEOUT;i++){
		/*shut down power*/
		rc = msm_camera_i2c_write(
			s_ctrl->sensor_i2c_client,
			addr,data,MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0){
			pr_err("%s shut down OTP error 0x%x:0x%x",__func__,addr,data);
		}else{
			pr_info("%s shut down OTP success 0x%x:0x%x",__func__,addr,data);
			return rc;
		}
		mdelay(1);
	}
	pr_err("%s shut down time out 0x%x",__func__,addr);
	return rc;
}

static int vd6869_cut09_init_otp(struct msm_sensor_ctrl_t *s_ctrl){
	int i,rc = 0;
	uint16_t read_data = 0;

	/*cut 0.9 manual read OTP*/
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++) {
		rc = msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client
			, otp_settings_cut09,
			ARRAY_SIZE(otp_settings_cut09),
			MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0)
			pr_err("%s write otp init table error.... retry", __func__);
		else{
			pr_info("%s OTP table init done",__func__);
			break;
		}
		mdelay(1);
	}

	/*wait OTP ready*/
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++){
		/*0x3302 is read OTP status 0x00 READY 0x01 PROGRAM 0x02 READ*/
		rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			(0x3302),&read_data,
			MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0){
			pr_err("%s read OTP status error",__func__);
		} else if(read_data == 0x00){
			rc = vd6869_shut_down_otp(s_ctrl,0x44c0,0x00);
			if(rc < 0)
				return rc;
			rc = vd6869_shut_down_otp(s_ctrl,0x4500,0x00);
			if(rc < 0)
				return rc;
			break;
		}
		mdelay(1);
	}
	return rc;
}

static int vd6869_cut10_init_otp(struct msm_sensor_ctrl_t *s_ctrl){
	int i,rc = 0;
	uint16_t read_data = 0;

	/*cut 1.0 manual read OTP*/
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++) {
		rc = msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client
			, otp_settings_cut10,
			ARRAY_SIZE(otp_settings_cut10),
			MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0)
			pr_err("%s write otp init table error.... retry", __func__);
		else{
			pr_info("%s OTP table init done",__func__);
			break;
		}
		mdelay(1);
	}

	/*wait OTP ready*/
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++){
		/*0x3302 is read OTP status 0x00 READY 0x01 PROGRAM 0x02 READ*/
		rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			(0x3302),&read_data,
			MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0){
			pr_err("%s read OTP status error",__func__);
		} else if(read_data == 0x00){
			rc = vd6869_shut_down_otp(s_ctrl,0x4584,0x00);
			if(rc < 0)
				return rc;
			rc = vd6869_shut_down_otp(s_ctrl,0x44c0,0x00);
			if(rc < 0)
				return rc;
			rc = vd6869_shut_down_otp(s_ctrl,0x4500,0x00);
			if(rc < 0)
				return rc;
			break;
		}
		mdelay(1);
	}
	return rc;
}


static int vd6869_cut10_init_otp_NO_ECC(struct msm_sensor_ctrl_t *s_ctrl){
	int i,rc = 0;
	uint16_t read_data = 0;

	/*cut 1.0 manual read OTP*/
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++) {
		rc = msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client
			, otp_settings_cut10_NO_ECC,
			ARRAY_SIZE(otp_settings_cut10_NO_ECC),
			MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0)
			pr_err("%s write otp init table error.... retry", __func__);
		else{
			pr_info("%s OTP table init done",__func__);
			break;
		}
		mdelay(1);
	}

	/*wait OTP ready*/
	for(i = 0 ;i < OTP_WAIT_TIMEOUT; i++){
		/*0x3302 is read OTP status 0x00 READY 0x01 PROGRAM 0x02 READ*/
		rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			(0x3302),&read_data,
			MSM_CAMERA_I2C_BYTE_DATA);

		if(rc < 0){
			pr_err("%s read OTP status error",__func__);
		} else if(read_data == 0x00){
			rc = vd6869_shut_down_otp(s_ctrl,0x4584,0x00);
			if(rc < 0)
				return rc;
			rc = vd6869_shut_down_otp(s_ctrl,0x44c0,0x00);
			if(rc < 0)
				return rc;
			rc = vd6869_shut_down_otp(s_ctrl,0x4500,0x00);
			if(rc < 0)
				return rc;
			break;
		}
		mdelay(1);
	}
	return rc;
}



// HTC_START pg 20130220 lc898212 act enable

/*
OTP Location

Burn in times                       1st(Layer 0)    2nd(Layer 1)    3rd(Layer 2)

0 Module vendor                     0x3C8           0x3D8           0x3B8
1 LENS                              0x3C9           0x3D9           0x3B9
2 Sensor Version                    0x3CA           0x3DA           0x3BA
3 Driver IC Vendor & Version        0x3CB           0x3DB           0x3BB
4 Actuator vender ID & Version      0x3CC           0x3DC           0x3BC

5 Module ID                         0x3A0           0x380           0x388
6 Module ID                         0x3A1           0x381           0x389
7 Module ID                         0x3A2           0x382           0x38A

8 BAIS Calibration data             0x3CD           0x3DD           0x3BD
9 OFFSET Calibration data           0x3CE           0x3DE           0x3BE
a VCM bottom mech. Limit (MSByte)   0x3C0           0x3D0           0x3B0
b VCM bottom mech. Limit (LSByte)   0x3C1           0x3D1           0x3B1
c Infinity position code (MSByte)   0x3C2           0x3D2           0x3B2
d Infinity position code (LSByte)   0x3C3           0x3D3           0x3B3
e Macro position code (MSByte)      0x3C4           0x3D4           0x3B4
f Macro position code (LSByte)      0x3C5           0x3D5           0x3B5
10 VCM top mech. Limit (MSByte      0x3C6           0x3D6           0x3B6
11 VCM top mech. Limit (LSByte)     0x3C7           0x3D7           0x3B7
*/

#define VD6869_LITEON_OTP_SIZE 0x12
/*M7 projects*/
const static short new_otp_addr[3][VD6869_LITEON_OTP_SIZE] = {
    //0,    1,    2,    3,    4,    5,    6,    7,    8,    9,    a,    b,    c,    d,    e,    f,   10,   11
    {0x3C8,0x3C9,0x3CA,0x3CB,0x3CC,0x3A0,0x3A1,0x3A2,0x3CD,0x3CE,0x3C0,0x3C1,0x3C2,0x3C3,0x3C4,0x3C5,0x3C6,0x3C7}, // layer 1
    {0x3D8,0x3D9,0x3DA,0x3DB,0x3DC,0x380,0x381,0x382,0x3DD,0x3DE,0x3D0,0x3D1,0x3D2,0x3D3,0x3D4,0x3D5,0x3D6,0x3D7}, // layer 2
    {0x3B8,0x3B9,0x3BA,0x3BB,0x3BC,0x388,0x389,0x38A,0x3BD,0x3BE,0x3B0,0x3B1,0x3B2,0x3B3,0x3B4,0x3B5,0x3B6,0x3B7}, // layer 3
};
/*dlxp_ul, dlpchina, m4_ul for the issue component*/
const static short old_otp_addr[3][VD6869_LITEON_OTP_SIZE] = {
    //0,    1,    2,    3,    4,    5,    6,    7,    8,    9,    a,    b,    c,    d,    e,    f,   10,   11
    {0x3C8,0x3C9,0x3CA,0x3CB,0x3CC,0x3A0,0x3A1,0x3A2,0x3CD,0x3CE,0x3C0,0x3C1,0x3C2,0x3C3,0x3C4,0x3C5,0x3C6,0x3C7}, // layer 1
    {0x3D8,0x3D9,0x3DA,0x3DB,0x3DC,0x380,0x381,0x382,0x3DD,0x3DE,0x3D0,0x3D1,0x3D2,0x3D3,0x3D4,0x3D5,0x3D6,0x3D7}, // layer 2
    {0x3B8,0x3B9,0x3BA,0x3BB,0x3BC,0x388,0x389,0x3AA,0x3BD,0x3BE,0x3B0,0x3B1,0x3B2,0x3B3,0x3B4,0x3B5,0x3B6,0x3B7}, // layer 3
};

#if defined(CONFIG_ACT_OIS_BINDER)
extern void HtcActOisBinder_set_OIS_OTP(uint8_t *otp_data, uint8_t otp_size);

#define VD6869_LITEON_OIS_OTP_SIZE 34
const static short ois_addr[3][VD6869_LITEON_OIS_OTP_SIZE] = {
    //0,    1,    2,    3,    4,    5,    6,    7,    8,    9,    a,    b,    c,    d,    e,    f,   10,   11
    {0x090,0x091,0x092,0x093,0x094,0x095,0x096,0x097,0x098,0x099,0x09A,0x09B,0x09C,0x09D,0x09E,0x09F,0x0A0,0x0A1,0x0A2,0x0A3,0x0A4,0x0A5,0x0A6,0x0A7,0x0A8,0x0A9,0x0AA,0x0AB,0x0AC,0x0AD,0x0AE,0x0AF,0x0F0,0x0F4}, // layer 1
    {0x0B0,0x0B1,0x0B2,0x0B3,0x0B4,0x0B5,0x0B6,0x0B7,0x0B8,0x0B9,0x0BA,0x0BB,0x0BC,0x0BD,0x0BE,0x0BF,0x0C0,0x0C1,0x0C2,0x0C3,0x0C4,0x0C5,0x0C6,0x0C7,0x0C8,0x0C9,0x0CA,0x0CB,0x0CC,0x0CD,0x0CE,0x0CF,0x0F8,0x0F9}, // layer 2
    {0x0D0,0x0D1,0x0D2,0x0D3,0x0D4,0x0D5,0x0D6,0x0D7,0x0D8,0x0D9,0x0DA,0x0DB,0x0DC,0x0DD,0x0DE,0x0DF,0x0E0,0x0E1,0x0E2,0x0E3,0x0E4,0x0E5,0x0E6,0x0E7,0x0E8,0x0E9,0x0EA,0x0EB,0x0EC,0x0ED,0x0EE,0x0EF,0x0FC,0x0FD}, // layer 3
};
#endif

int vd6869_read_fuseid_liteon(struct sensor_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl, bool first)
{
	int32_t i,j;
	int32_t rc = 0;
	const int32_t offset = 0x33fa;
	static int32_t valid_layer=-1;
	uint16_t ews_data[4] = {0};

    static uint8_t otp[VD6869_LITEON_OTP_SIZE];
#if defined(CONFIG_ACT_OIS_BINDER)
	int32_t ois_valid_layer=-1;
	static uint8_t ois_otp[VD6869_LITEON_OIS_OTP_SIZE];
#endif
	uint16_t read_data = 0;

	if (first) {
		if (s_ctrl->sensordata->sensor_cut == 1) {
			//check for LiteOn module only
			for (i=0; i<4; i++) {
				rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, (offset + 0x3F4 + i), &ews_data[i], MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
				if (rc < 0){
					pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, (offset + 0x3F4 + i));
					return rc;
				}
				pr_info("%s: read OTP 0x%x = 0x%x\n", __func__, (0x3F4 + i), ews_data[i]);
			}
			if(vd6869_s_ctrl.ews_enable){/*sys_rev < PVT*/
				if (ews_data[0]==0 && ews_data[1]==0 && ews_data[2]==0 && ews_data[3]==1) {
					pr_info("%s: Apply OTP ECC enable", __func__);
					vd6869_cut10_init_otp(s_ctrl);
				} else {
					//disable sensor OTP ECC for all LiteOn module(None ECC)
					vd6869_cut10_init_otp_NO_ECC(s_ctrl);
					pr_info("%s: Apply OTP ECC disable", __func__);
				}
			}else {/*sys_rev > PVT*/
				pr_info("%s: Apply OTP ECC enable, vd6869_s_ctrl.ews_enable=%d", __func__, vd6869_s_ctrl.ews_enable);
				vd6869_cut10_init_otp(s_ctrl);
			}
		}

        // start from layer 2
        for (j=2; j>=0; --j) {
            for (i=0; i<VD6869_LITEON_OTP_SIZE; ++i) {
                if(vd6869_s_ctrl.ews_enable){
                    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, old_otp_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
	                if (rc < 0){
	                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, old_otp_addr[j][i]);
	                    return rc;
	                }
	                pr_info("%s: OTP addr 0x%x = 0x%x\n", __func__,  old_otp_addr[j][i], read_data);
                }else{
	                rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, new_otp_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
	                if (rc < 0){
	                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, new_otp_addr[j][i]);
	                    return rc;
	                }
	                pr_info("%s: OTP addr 0x%x = 0x%x\n", __func__,  new_otp_addr[j][i], read_data);
                }
                otp[i]= read_data;

                if (read_data)
                    valid_layer = j;
            }
            if (valid_layer!=-1)
                break;
        }
        pr_info("%s: OTP valid layer = %d\n", __func__,  valid_layer);

#if defined(CONFIG_ACT_OIS_BINDER)
        // start from layer 2
        for (j=2; j>=0; --j) {
            for (i=0; i<VD6869_LITEON_OIS_OTP_SIZE; ++i) {
                rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, ois_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
                if (rc < 0){
                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, ois_addr[j][i]);
                    return rc;
                }
                pr_info("%s: OTP ois_addr 0x%x = 0x%x\n", __func__,  ois_addr[j][i], read_data);

                ois_otp[i]= read_data;

                if (read_data)
                    ois_valid_layer = j;
            }
            if (ois_valid_layer!=-1)
                break;
        }
        pr_info("%s: OTP OIS valid layer = %d\n", __func__,  ois_valid_layer);


	if (ois_valid_layer!=-1) {
		for(i=0; i<VD6869_LITEON_OIS_OTP_SIZE;i ++)
			pr_info("read out OTP OIS data = 0x%x\n", ois_otp[i]);

		HtcActOisBinder_set_OIS_OTP(ois_otp, VD6869_LITEON_OIS_OTP_SIZE);
	}
#endif


    }
    // vendor
    vd6869_ver = otp[2]; // HTC pg 20130329 add sensor cut info
    cdata->sensor_ver = otp[2];

    if (board_mfg_mode()) {
        if(vd6869_s_ctrl.ews_enable) /*sys_rev < PVT*/
            msm_dump_otp_to_file (PLATFORM_DRIVER_NAME, old_otp_addr[valid_layer], otp, VD6869_LITEON_OTP_SIZE);
        else
            msm_dump_otp_to_file (PLATFORM_DRIVER_NAME, new_otp_addr[valid_layer], otp, VD6869_LITEON_OTP_SIZE);
    }
    // fuseid
    cdata->cfg.fuse.fuse_id_word1 = 0;
    cdata->cfg.fuse.fuse_id_word2 = otp[5];
    cdata->cfg.fuse.fuse_id_word3 = otp[6];
    cdata->cfg.fuse.fuse_id_word4 = otp[7];

    // vcm
    cdata->af_value.VCM_BIAS = otp[8];
    cdata->af_value.VCM_OFFSET = otp[9];
    cdata->af_value.VCM_BOTTOM_MECH_MSB = otp[0xa];
    cdata->af_value.VCM_BOTTOM_MECH_LSB = otp[0xb];
    cdata->af_value.AF_INF_MSB = otp[0xc];
    cdata->af_value.AF_INF_LSB = otp[0xd];
    cdata->af_value.AF_MACRO_MSB = otp[0xe];
    cdata->af_value.AF_MACRO_LSB = otp[0xf];
    cdata->af_value.VCM_TOP_MECH_MSB = otp[0x10];
    cdata->af_value.VCM_TOP_MECH_LSB = otp[0x11];
    cdata->af_value.VCM_VENDOR_ID_VERSION = otp[4];
    pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
    pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
    pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
    pr_info("%s: OTP Driver IC Vendor & Version = 0x%x\n",  __func__,  otp[3]);
    pr_info("%s: OTP Actuator vender ID & Version = 0x%x\n",__func__,  otp[4]);

    pr_info("%s: OTP fuse 0 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word1);
    pr_info("%s: OTP fuse 1 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word2);
    pr_info("%s: OTP fuse 2 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word3);
    pr_info("%s: OTP fuse 3 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word4);

    pr_info("%s: OTP BAIS Calibration data = 0x%x\n",           __func__,  cdata->af_value.VCM_BIAS);
    pr_info("%s: OTP OFFSET Calibration data = 0x%x\n",         __func__,  cdata->af_value.VCM_OFFSET);
    pr_info("%s: OTP VCM bottom mech. Limit (MSByte) = 0x%x\n", __func__,  cdata->af_value.VCM_BOTTOM_MECH_MSB);
    pr_info("%s: OTP VCM bottom mech. Limit (LSByte) = 0x%x\n", __func__,  cdata->af_value.VCM_BOTTOM_MECH_LSB);
    pr_info("%s: OTP Infinity position code (MSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_MSB);
    pr_info("%s: OTP Infinity position code (LSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_LSB);
    pr_info("%s: OTP Macro position code (MSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_MSB);
    pr_info("%s: OTP Macro position code (LSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_LSB);
    pr_info("%s: OTP VCM top mech. Limit (MSByte) = 0x%x\n",    __func__,  cdata->af_value.VCM_TOP_MECH_MSB);
    pr_info("%s: OTP VCM top mech. Limit (LSByte) = 0x%x\n",    __func__,  cdata->af_value.VCM_TOP_MECH_LSB);

    return 0;
}
// HTC_END pg 20130220 lc898212 act enable
int vd6869_read_fuseid_sharp(struct sensor_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl,bool first)
{
/*
                                    1st Layer	2nd Layer	3rd Layer

0   Year[7:4], Month[3:0]	        0x3A0	    0x380	    0x388
1   Date[7:3],000	                0x3A1	    0x381	    0x389
2   Start VCM code (MSByte)	        0x3C0	    0x3D0	    0x3B0
3   Start VCM code (LSByte)	        0x3C1	    0x3D1	    0x3B1
4   Infinity position code (MSByte)	0x3C2	    0x3D2	    0x3B2
5   Infinity position code (LSByte)	0x3C3	    0x3D3	    0x3B3
6   Macro position code (MSByte)	0x3C4	    0x3D4	    0x3B4
7   Macro position code (LSByte)	0x3C5	    0x3D5	    0x3B5
8   0x00	                        0x3C6	    0x3D6	    0x3B6
9   0x00	                        0x3C7	    0x3D7	    0x3B7
a   Module Vendor ID	            0x3C8	    0x3D8	    0x3B8
b   Lens ID	                        0x3C9	    0x3D9	    0x3B9
c   Sensor Version	                0x3CA	    0x3DA	    0x3BA
d   Driver ID	                    0x3CB	    0x3DB	    0x3BB
e   Actuator ID	                    0x3CC	    0x3DC	    0x3BC
f   0x00	                        0x3CD	    0x3DD	    0x3BD
10  0x00	                        0x3CE	    0x3DE	    0x3BE
11  CheckSum	                    0x3CF	    0x3DF	    0x3BF

12  fuse id 0                       0x3f4       0x3f4       0x3f4
13  fuse id 1                       0x3f5       0x3f5       0x3f5
14  fuse id 2                       0x3f6       0x3f6       0x3f6
15  fuse id 3                       0x3f7       0x3f7       0x3f7
16  fuse id 4                       0x3f8       0x3f8       0x3f8
17  fuse id 5                       0x3f9       0x3f9       0x3f9
18  fuse id 6                       0x3fa       0x3fa       0x3fa
19  fuse id 7                       0x3fb       0x3fb       0x3fb
1a  fuse id 8                       0x3fc       0x3fc       0x3fc
1b  fuse id 9                       0x3fd       0x3fd       0x3fd
1c  fuse id 10                      0x3fe       0x3fe       0x3fe
1d  fuse id 11                      0x3ff       0x3ff       0x3ff

*/
    #define SHARP_OTP_SIZE 0x12
    const static short sharp_otp_addr[3][SHARP_OTP_SIZE] = {
        //0,   1,    2,    3,    4,    5,    6,    7,    8,    9,    a,    b,    c,    d,    e,    f,   10,    11,
        {0x3A0,0x3A1,0x3C0,0x3C1,0x3C2,0x3C3,0x3C4,0x3C5,0x3C6,0x3C7,0x3C8,0x3C9,0x3CA,0x3CB,0x3CC,0x3CD,0x3CE,0x3CF},
        {0x380,0x381,0x3D0,0x3D1,0x3D2,0x3D3,0x3D4,0x3D5,0x3D6,0x3D7,0x3D8,0x3D9,0x3DA,0x3DB,0x3DC,0x3DD,0x3DE,0x3DF},
        {0x388,0x389,0x3B0,0x3B1,0x3B2,0x3B3,0x3B4,0x3B5,0x3B6,0x3B7,0x3B8,0x3B9,0x3BA,0x3BB,0x3BC,0x3BD,0x3BE,0x3BF}
    };
    #define SHARP_FUSEID_SIZE 0xc
    const static short sharp_fuseid_addr[SHARP_FUSEID_SIZE]={0x3f4,0x3f5,0x3f6,0x3f7,0x3f8,0x3f9,0x3fa,0x3fb,0x3fc,0x3fd,0x3fe,0x3ff};

    int32_t rc = 0;
    int i,j;
    uint16_t read_data = 0;
    static uint8_t otp[SHARP_OTP_SIZE+SHARP_FUSEID_SIZE];
    const int32_t offset = 0x33fa;
    static int32_t valid_layer=-1;
#if defined(CONFIG_ACT_OIS_BINDER)
    int32_t ois_valid_layer=-1;
    static uint8_t ois_otp[VD6869_LITEON_OIS_OTP_SIZE];
#endif

    if (first) {
        // otp data
        for (j=2; j>=0; --j) {
           for (i=0; i<SHARP_OTP_SIZE; ++i) {
               rc = vd6869_i2c_read(s_ctrl->sensor_i2c_client, sharp_otp_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
               if (rc < 0){
                   pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, sharp_otp_addr[j][i]);
                   return rc;
               }
               pr_info("%s: OTP addr 0x%x = 0x%x\n", __func__,  sharp_otp_addr[j][i], read_data);

               otp[i]= read_data;

               if (read_data)
                   valid_layer = j;
           }
           if (valid_layer!=-1)
               break;
        }
        pr_info("%s: OTP valid layer = %d\n", __func__,  valid_layer);
        // fuse id
        for (i=0;i<SHARP_FUSEID_SIZE;++i) {
            rc = vd6869_i2c_read(s_ctrl->sensor_i2c_client, sharp_fuseid_addr[i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0){
               pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, sharp_fuseid_addr[i]);
               return rc;
            }
            pr_info("%s: OTP fuseid 0x%x = 0x%x\n", __func__,  sharp_fuseid_addr[i], read_data);

            otp[i+SHARP_OTP_SIZE]= read_data;
        }

    #if defined(CONFIG_ACT_OIS_BINDER)
        // start from layer 2
        for (j=2; j>=0; --j) {
            for (i=0; i<VD6869_LITEON_OIS_OTP_SIZE; ++i) {
                rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, ois_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
                if (rc < 0){
                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, ois_addr[j][i]);
                    return rc;
                }
                pr_info("%s: OTP ois_addr 0x%x = 0x%x\n", __func__,  ois_addr[j][i], read_data);

                ois_otp[i]= read_data;

                if (read_data)
                    ois_valid_layer = j;
            }
            if (ois_valid_layer!=-1)
                break;
        }
        pr_info("%s: OTP OIS valid layer = %d\n", __func__,  ois_valid_layer);

    	if (ois_valid_layer!=-1) {
    		for(i=0; i<VD6869_LITEON_OIS_OTP_SIZE;i ++)
    			pr_info("read out OTP OIS data = 0x%x\n", ois_otp[i]);

    		HtcActOisBinder_set_OIS_OTP(ois_otp, VD6869_LITEON_OIS_OTP_SIZE);
    	}
    #endif
    }
    if (board_mfg_mode())
        msm_dump_otp_to_file (PLATFORM_DRIVER_NAME, sharp_otp_addr[valid_layer], otp, sizeof (otp));

    vd6869_year_mon = otp[0];
    vd6869_date = otp[1];

    cdata->sensor_ver = vd6869_ver = otp[0xc];
    cdata->cfg.fuse.fuse_id_word1 = 0;
    cdata->cfg.fuse.fuse_id_word2 =
        (otp[0x12]<<24) |
        (otp[0x13]<<16) |
        (otp[0x14]<<8) |
        (otp[0x15]);
    cdata->cfg.fuse.fuse_id_word3 =
        (otp[0x16]<<24) |
        (otp[0x17]<<16) |
        (otp[0x18]<<8) |
        (otp[0x19]);
    cdata->cfg.fuse.fuse_id_word4 =
        (otp[0x1a]<<24) |
        (otp[0x1b]<<16) |
        (otp[0x1c]<<8) |
        (otp[0x1d]);
    /*PASS DATA to RUMBAS_ACT*/
    cdata->af_value.VCM_START_MSB = otp[2];
    cdata->af_value.VCM_START_LSB = otp[3];
    cdata->af_value.AF_INF_MSB = otp[4];
    cdata->af_value.AF_INF_LSB = otp[5];
    cdata->af_value.AF_MACRO_MSB = otp[6];
    cdata->af_value.AF_MACRO_LSB = otp[7];
    cdata->af_value.ACT_ID = otp[0xe];

    pr_info("vd6869_year_mon=0x%x\n", vd6869_year_mon);
    pr_info("vd6869_date=0x%x\n", vd6869_date);
    pr_info("%s: VenderID=%x,LensID=%x,SensorID=%02x, DriverId=%02x\n", __func__,
        otp[0xa], otp[0xb], otp[0xc], otp[0xd]);
    pr_info("%s: ModuleFuseID= %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", __func__,
        otp[0x12], otp[0x13], otp[0x14], otp[0x15], otp[0x16], otp[0x17], otp[0x18], otp[0x19], otp[0x1a], otp[0x1b], otp[0x1c], otp[0x1d]);

    pr_info("vd6869: fuse->fuse_id_word1:%x\n",cdata->cfg.fuse.fuse_id_word1);
    pr_info("vd6869: fuse->fuse_id_word2:0x%08x\n",cdata->cfg.fuse.fuse_id_word2);
    pr_info("vd6869: fuse->fuse_id_word3:0x%08x\n",cdata->cfg.fuse.fuse_id_word3);
    pr_info("vd6869: fuse->fuse_id_word4:0x%08x\n",cdata->cfg.fuse.fuse_id_word4);

    pr_info("vd6869: VCM START:0x%02x\n", cdata->af_value.VCM_START_MSB << 8 |cdata->af_value.VCM_START_LSB);
    pr_info("vd6869: Infinity position:0x%02x\n", cdata->af_value.AF_INF_MSB << 8 | cdata->af_value.AF_INF_LSB);
    pr_info("vd6869: Macro position:0x%02x\n", cdata->af_value.AF_MACRO_MSB << 8 | cdata->af_value.AF_MACRO_LSB);

    pr_info("VCM_START_MSB =0x%x\n", cdata->af_value.VCM_START_MSB);
    pr_info("VCM_START_LSB =0x%x\n", cdata->af_value.VCM_START_LSB);
    pr_info("AF_INF_MSB =0x%x\n", cdata->af_value.AF_INF_MSB);
    pr_info("AF_INF_LSB =0x%x\n", cdata->af_value.AF_INF_LSB);
    pr_info("AF_MACRO_MSB =0x%x\n", cdata->af_value.AF_MACRO_MSB);
    pr_info("AF_MACRO_LSB =0x%x\n", cdata->af_value.AF_MACRO_LSB);
    pr_info("ACT_ID =0x%x\n", cdata->af_value.ACT_ID);

    return 0;
}
// HTC_START pg 20130220 lc898212 act enable

int vd6869_read_fuseid_liteon_mfg_core(struct sensor_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl, bool first,uint8_t* otp )
{
    int32_t i,j;
    int32_t rc = 0;
    const int32_t offset = 0x33fa;
    int32_t valid_layer=-1;


#if defined(CONFIG_ACT_OIS_BINDER)
    int32_t ois_valid_layer=-1;
    static uint8_t ois_otp[VD6869_LITEON_OIS_OTP_SIZE];
#endif
    uint16_t read_data = 0;

    if (first) {
        // start from layer 2
        for (j=2; j>=0; --j) {
            for (i=0; i<VD6869_LITEON_OTP_SIZE; ++i) {
                if(vd6869_s_ctrl.ews_enable){/*sys_rev < PVT*/
	                rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, old_otp_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
	                if (rc < 0){
	                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__,old_otp_addr[j][i]);
	                    return rc;
	                }
	                pr_info("%s: OTP addr 0x%x = 0x%x\n", __func__,  old_otp_addr[j][i], read_data);
                }else{/*sys_rev > PVT*/
	                rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, new_otp_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
	                if (rc < 0){
	                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__,new_otp_addr[j][i]);
	                    return rc;
	                }
	                pr_info("%s: OTP addr 0x%x = 0x%x\n", __func__,  new_otp_addr[j][i], read_data);
                }
                otp[i]= read_data;

                if (read_data)
                    valid_layer = j;
            }
            if (valid_layer!=-1)
                break;
        }
        pr_info("%s: OTP valid layer = %d\n", __func__,  valid_layer);

#if defined(CONFIG_ACT_OIS_BINDER)
        // start from layer 2
        for (j=2; j>=0; --j) {
            for (i=0; i<VD6869_LITEON_OIS_OTP_SIZE; ++i) {
                rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, ois_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
                if (rc < 0){
                    pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, ois_addr[j][i]);
                    return rc;
                }
                pr_info("%s: OTP ois_addr 0x%x = 0x%x\n", __func__,  ois_addr[j][i], read_data);

                ois_otp[i]= read_data;

                if (read_data)
                    ois_valid_layer = j;
            }
            if (ois_valid_layer!=-1)
                break;
        }
        pr_info("%s: OTP OIS valid layer = %d\n", __func__,  ois_valid_layer);


    if (ois_valid_layer!=-1) {
        for(i=0; i<VD6869_LITEON_OIS_OTP_SIZE;i ++)
            pr_info("read out OTP OIS data = 0x%x\n", ois_otp[i]);

        HtcActOisBinder_set_OIS_OTP(ois_otp, VD6869_LITEON_OIS_OTP_SIZE);
    }
#endif


    }
    // vendor
    vd6869_ver = otp[2]; // HTC pg 20130329 add sensor cut info
    cdata->sensor_ver = otp[2];

    // fuseid
    cdata->cfg.fuse.fuse_id_word1 = 0;
    cdata->cfg.fuse.fuse_id_word2 = otp[5];
    cdata->cfg.fuse.fuse_id_word3 = otp[6];
    cdata->cfg.fuse.fuse_id_word4 = otp[7];

    // vcm
    cdata->af_value.VCM_BIAS = otp[8];
    cdata->af_value.VCM_OFFSET = otp[9];
    cdata->af_value.VCM_BOTTOM_MECH_MSB = otp[0xa];
    cdata->af_value.VCM_BOTTOM_MECH_LSB = otp[0xb];
    cdata->af_value.AF_INF_MSB = otp[0xc];
    cdata->af_value.AF_INF_LSB = otp[0xd];
    cdata->af_value.AF_MACRO_MSB = otp[0xe];
    cdata->af_value.AF_MACRO_LSB = otp[0xf];
    cdata->af_value.VCM_TOP_MECH_MSB = otp[0x10];
    cdata->af_value.VCM_TOP_MECH_LSB = otp[0x11];
    cdata->af_value.VCM_VENDOR_ID_VERSION = otp[4];
    pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
    pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
    pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
    pr_info("%s: OTP Driver IC Vendor & Version = 0x%x\n",  __func__,  otp[3]);
    pr_info("%s: OTP Actuator vender ID & Version = 0x%x\n",__func__,  otp[4]);

    pr_info("%s: OTP fuse 0 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word1);
    pr_info("%s: OTP fuse 1 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word2);
    pr_info("%s: OTP fuse 2 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word3);
    pr_info("%s: OTP fuse 3 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word4);

    pr_info("%s: OTP BAIS Calibration data = 0x%x\n",           __func__,  cdata->af_value.VCM_BIAS);
    pr_info("%s: OTP OFFSET Calibration data = 0x%x\n",         __func__,  cdata->af_value.VCM_OFFSET);
    pr_info("%s: OTP VCM bottom mech. Limit (MSByte) = 0x%x\n", __func__,  cdata->af_value.VCM_BOTTOM_MECH_MSB);
    pr_info("%s: OTP VCM bottom mech. Limit (LSByte) = 0x%x\n", __func__,  cdata->af_value.VCM_BOTTOM_MECH_LSB);
    pr_info("%s: OTP Infinity position code (MSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_MSB);
    pr_info("%s: OTP Infinity position code (LSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_LSB);
    pr_info("%s: OTP Macro position code (MSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_MSB);
    pr_info("%s: OTP Macro position code (LSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_LSB);
    pr_info("%s: OTP VCM top mech. Limit (MSByte) = 0x%x\n",    __func__,  cdata->af_value.VCM_TOP_MECH_MSB);
    pr_info("%s: OTP VCM top mech. Limit (LSByte) = 0x%x\n",    __func__,  cdata->af_value.VCM_TOP_MECH_LSB);

    return 0;

}
int vd6869_read_fuseid_liteon_mfg(struct sensor_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl, bool first)
{
#define VD6869_LITEON_OTP_SIZE 0x12

    uint8_t otp_ecc[VD6869_LITEON_OTP_SIZE];
    uint8_t otp_no_ecc[VD6869_LITEON_OTP_SIZE];
    int i=0;
    int match=1;

    vd6869_read_fuseid_liteon_mfg_core (cdata, s_ctrl, first, otp_ecc);
    vd6869_cut10_init_otp_NO_ECC(s_ctrl);
    vd6869_read_fuseid_liteon_mfg_core (cdata, s_ctrl, first, otp_no_ecc);
    vd6869_cut10_init_otp (s_ctrl);
    for (i=0;i<VD6869_LITEON_OTP_SIZE;++i) {

        if (otp_ecc[i] != otp_no_ecc[i]) {
            pr_info ("%s: cmp_otp otp not match at 0x%x ecc/no_ecc=0x%x/0x%x",__func__,i,otp_ecc[i],otp_no_ecc[i]);
            match=0;
        }
    }

    if (match)
    {
        pr_info ("%s: cmp_otp otp match",__func__);
    }

    return 0;
}

int32_t vd6869_read_otp_valid_layer(struct msm_sensor_ctrl_t *s_ctrl, int8_t *valid_layer, bool first)
{
	int32_t rc = 0;
	const int32_t offset = 0x33fa;
	uint16_t read_data = 0;
	int j=2,i=0;
	static int8_t layer=-1;
	if (first) {
		for (j=2; j>=0; --j) {
			for (i=0; i<VD6869_LITEON_OTP_SIZE; ++i) {
				rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, new_otp_addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
				if (rc < 0){
					pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, new_otp_addr[j][i]);
					return rc;
				}
				pr_info("%s: OTP addr 0x%x = 0x%x\n", __func__,  new_otp_addr[j][i], read_data);

				if (read_data)
					layer = j;
			}
			if (layer!=-1)
				break;
		}
	}

	*valid_layer = layer;
	pr_info("%s: OTP valid layer = %d\n", __func__,  *valid_layer);
	return rc;
}

int vd6869_read_module_vendor(struct msm_sensor_ctrl_t *s_ctrl, uint8_t valid_layer, uint8_t* module_vendor, uint8_t* driver_ic, bool first)
{
	int32_t rc = 0;
	uint16_t read_data = 0;
	static uint8_t moduler=0,driver=0;
	const int32_t offset = 0x33fa;
	int16_t retry_count = 0;
	int16_t retry_max = 10;

	if (first) {

		do {

			if (s_ctrl->sensordata->sensor_cut == 1 && retry_count > 0) {
				rc = vd6869_cut10_init_otp(s_ctrl);
				if (rc<0)
					return rc;
			}else if(s_ctrl->sensordata->sensor_cut == 0){/* HTC_start chuck add cut0.9 OTP init seq*/
				rc = vd6869_cut09_init_otp(s_ctrl);
				if (rc<0)
					return rc;
			}

			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, new_otp_addr[valid_layer][0]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2, the first element is the same , use new_otp_addr to be the dafault
			if (rc < 0) {
				pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, new_otp_addr[valid_layer][0]+offset);
				return rc;
			}
			moduler = (uint8_t)(read_data&0x00FF);

			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, new_otp_addr[valid_layer][3]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c5
			if (rc < 0) {
				pr_err("%s: msm_camera_i2c_read 0x%x failed\n", __func__, new_otp_addr[valid_layer][3]+offset);
				return rc;
			}
			driver = (uint8_t)(read_data&0x00FF);

			if (driver == 0 || moduler == 0) {
				pr_err("OTP read error : driver=0x%x, moduler=0x%x  Apply retry mechanism  retry_count=%d\n", driver, moduler, retry_count);
				retry_count++;
				msleep(10);
			}

		} while ((driver == 0 || moduler == 0) && (retry_count <= retry_max));

	}

	*module_vendor = moduler;
	*driver_ic = driver;
	vd6869_s_ctrl.driver_ic = driver;
	pr_info("module_vendor = 0x%x\n", *module_vendor);
	pr_info("driver_ic = 0x%x\n", *driver_ic);
	return rc;
}


static int vd6869_lookup_actuator(struct msm_sensor_ctrl_t *s_ctrl, char *actuator_name)
{
	int i;
	struct msm_camera_sensor_info *sdata;
	struct msm_actuator_info *actuator_info;
	int actuator_index = -1;

	if ((s_ctrl == NULL) || (s_ctrl->sensordata == NULL))
		return -EFAULT;
	if (actuator_name == NULL)
		return -EFAULT;

	sdata = (struct msm_camera_sensor_info *) s_ctrl->sensordata;

	for(i=0; i<sdata->num_actuator_info_table; i++) {
		actuator_info = &sdata->actuator_info_table[i][0];
		pr_info("index=%d   actuator_info->board_info->type=%s\n", i, actuator_info->board_info->type);
		if(!strncmp(actuator_info->board_info->type, actuator_name, strlen(actuator_name))) {
			actuator_index = i;
			break;
		}
	}

	return actuator_index;
}

int vd6869_read_fuseid(struct sensor_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;
	uint8_t module_vendor;
	uint8_t driver_ic;
	static bool first=true;
	struct msm_camera_sensor_info *sdata = (struct msm_camera_sensor_info *) s_ctrl->sensordata;
	int actuator_index = -1;
	int8_t valid_layer = -1;

	if (first) {
		if (s_ctrl->sensordata->sensor_cut == 1) {
				rc = vd6869_cut10_init_otp(s_ctrl);
			if (rc<0)
				return rc;
		}

		if(vd6869_s_ctrl.ews_enable)
			pr_info("%s, vd6869_s_ctrl.ews_enable=%d", __func__, vd6869_s_ctrl.ews_enable);/*Dlxp_ul,Dlp_china, M4*/
		else{
			rc = vd6869_read_otp_valid_layer (s_ctrl,&valid_layer,first);/*M7 projects*/
			if (rc<0) {
				pr_err("%s: failed %d\n", __func__, rc);
				first = false;
				return rc;
			}
		}
	}

	if (valid_layer < 0)
		valid_layer = 0;

	rc = vd6869_read_module_vendor (s_ctrl, valid_layer, &module_vendor, &driver_ic, first);
	if (rc<0) {
		pr_err("%s: failed %d\n", __func__, rc);
        first = false;
		return rc;
	}
	cdata->af_value.VCM_VENDOR = module_vendor;

/* HTC_START 20130329 */
	if (sdata->num_actuator_info_table > 1)
	{
		pr_info("%s: driver_ic=%d\n", __func__, driver_ic);

		if (driver_ic == 0x21) { //ONSEMI(TI201)
			actuator_index = vd6869_lookup_actuator(s_ctrl, "ti201_act");
		} else if (driver_ic == 0x11) { //Closed-Loop VCM (LC898212)
			actuator_index = vd6869_lookup_actuator(s_ctrl, "lc898212_act");
		} else if (driver_ic == 0x1)  { //RUMBA_S
			actuator_index = vd6869_lookup_actuator(s_ctrl, "rumbas_act");
		}

		if (actuator_index >= 0 && actuator_index < sdata->num_actuator_info_table)
			sdata->actuator_info = &sdata->actuator_info_table[actuator_index][0];
		else {
			pr_info("%s: Actuator lookup fail, use the default actuator in board file\n", __func__);
		}

		pr_info("%s: sdata->actuator_info->board_info->type=%s", __func__, sdata->actuator_info->board_info->type);
		pr_info("%s: sdata->actuator_info->board_info->addr=0x%x", __func__, sdata->actuator_info->board_info->addr);
	}
/* HTC_END */

	switch (module_vendor) {
		case 0x1:
			rc = vd6869_read_fuseid_sharp (cdata,s_ctrl,first);
			break;
		case 0x2:
		#if defined(CONFIG_MACH_M4_UL) || defined(CONFIG_MACH_M4_U) || defined(CONFIG_MACH_ZIP_CL)
		    if (board_mfg_mode())
		        rc = vd6869_read_fuseid_liteon_mfg (cdata,s_ctrl,first);
		#endif
		    rc = vd6869_read_fuseid_liteon (cdata,s_ctrl,first);
			break;
		default:
			pr_err("%s unknown module vendor = 0x%x\n",__func__, module_vendor);
			break;
	}

	first = false;
	return rc;
//#ifdef CONFIG_LC898212_ACT
//    return vd6869_read_fuseid_liteon (cdata,s_ctrl);
//#else
//    return vd6869_read_fuseid_sharp (cdata,s_ctrl);
//#endif
}
// HTC_END pg 20130220 lc898212 act enable

int32_t vd6869_power_up(struct msm_sensor_ctrl_t *s_ctrl)//(const struct msm_camera_sensor_info *sdata)
{
	int rc;
	struct msm_camera_sensor_info *sdata = NULL;
	uint16_t sensor_version = 0x01;
	struct sensor_cfg_data cdata;

	pr_info("%s called\n", __func__);

	if (s_ctrl && s_ctrl->sensordata)
		sdata = s_ctrl->sensordata;
	else {
		pr_info("%s: failed to s_ctrl sensordata NULL\n", __func__);
		return (-1);
	}

	if (sdata->camera_power_on == NULL) {
		pr_info("%s: failed to sensor platform_data didnt register\n", __func__);
		return -EIO;
	}

	pr_info("vd6869_power_down,sdata->htc_image=%d",sdata->htc_image);
	if (!sdata->use_rawchip && (sdata->htc_image != HTC_CAMERA_IMAGE_YUSHANII_BOARD)) {
		rc = msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
		if (rc < 0) {
			pr_info("%s: msm_camio_sensor_clk_on failed:%d\n",
			 __func__, rc);
			goto enable_mclk_failed;
		}
	}
	rc = sdata->camera_power_on();
	if (rc < 0) {
		pr_info("%s failed to enable power\n", __func__);
		goto enable_power_on_failed;
	}

	rc = msm_sensor_set_power_up(s_ctrl);//(sdata);
	if (rc < 0) {
		pr_info("%s msm_sensor_power_up failed\n", __func__);
		goto enable_sensor_power_up_failed;
	}
	/*check sensor version*/
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x45B0, &sensor_version, MSM_CAMERA_I2C_BYTE_DATA);

	if(sensor_version == 0x00){/*cut 0.9*/
		s_ctrl->sensordata->sensor_cut = 0;
	} else {/*cut 1.0*/
		s_ctrl->sensordata->sensor_cut = 1;
	}
	vd6869_sensor_open_init(sdata);

	if(sensor_version == 0x00){/*cut 0.9*/
		pr_info("vd6869 apply cut 09 setting");
		s_ctrl->sensordata->sensor_cut = 0;
		vd6869_read_fuseid(&cdata,s_ctrl);/*HTC_Chuck*/
	} else {/*cut 1.0*/
		if(vd6869_s_ctrl.msm_sensor_reg != NULL){
			pr_info("vd6869 apply cut 10 setting");
			vd6869_s_ctrl.msm_sensor_reg->init_settings = &vd6869_init_conf_cut10[0];
			vd6869_s_ctrl.msm_sensor_reg->init_size = ARRAY_SIZE(vd6869_init_conf_cut10);
			vd6869_s_ctrl.msm_sensor_reg->mode_settings = &vd6869_confs_cut10[0];
			vd6869_s_ctrl.msm_sensor_reg->num_conf = ARRAY_SIZE(vd6869_confs_cut10);
			vd6869_s_ctrl.msm_sensor_reg->output_settings = &vd6869_dimensions_cut10[0];
		}else
			pr_err("vd6869 msm_sensor_reg is NULL");
		s_ctrl->sensordata->sensor_cut = 1;
		vd6869_read_fuseid(&cdata,s_ctrl);
	}

	return rc;

enable_sensor_power_up_failed:
	if (sdata->camera_power_off == NULL)
		pr_info("%s: failed to sensor platform_data didnt register\n", __func__);
	else
		sdata->camera_power_off();
enable_power_on_failed:
	msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
enable_mclk_failed:
	return rc;
}

int32_t vd6869_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct msm_camera_sensor_info *sdata = NULL;
	pr_info("%s called\n", __func__);

	if (s_ctrl && s_ctrl->sensordata)
		sdata = s_ctrl->sensordata;
	else {
		pr_info("%s: failed to s_ctrl sensordata NULL\n", __func__);
		return (-1);
	}

	if (sdata->camera_power_off == NULL) {
		pr_err("%s: failed to sensor platform_data didnt register\n", __func__);
		return -EIO;
	}

	rc = msm_sensor_set_power_down(s_ctrl);
	if (rc < 0)
		pr_info("%s: msm_sensor_power_down failed\n", __func__);

	if (!sdata->use_rawchip && (sdata->htc_image != HTC_CAMERA_IMAGE_YUSHANII_BOARD)) {
		msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
		if (rc < 0)
			pr_info("%s: msm_camio_sensor_clk_off failed:%d\n",
				 __func__, rc);
	}
	rc = sdata->camera_power_off();//HTC_CAM chuck fine tune power seqence
	if (rc < 0)
		pr_info("%s: failed to disable power\n", __func__);

	return rc;
}

static int __init msm_sensor_init_module(void)
{
	pr_info("vd6869 %s\n", __func__);
	return i2c_add_driver(&vd6869_i2c_driver);
}

static struct v4l2_subdev_core_ops vd6869_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops vd6869_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops vd6869_subdev_ops = {
	.core = &vd6869_subdev_core_ops,
	.video  = &vd6869_subdev_video_ops,
};

static int vd6869_read_VCM_driver_IC_info(	struct msm_sensor_ctrl_t *s_ctrl)
{
#if 0
	int32_t  rc;
	int page = 0;
	unsigned short info_value = 0, info_index = 0;
	unsigned short  OTP = 0;
	struct msm_camera_i2c_client *msm_camera_i2c_client = s_ctrl->sensor_i2c_client;
	struct msm_camera_sensor_info *sdata;

	pr_info("%s: sensor OTP information:\n", __func__);

	s_ctrl = get_sctrl(&s_ctrl->sensor_v4l2_subdev);
	sdata = (struct msm_camera_sensor_info *) s_ctrl->sensordata;

	if ((sdata->actuator_info_table == NULL) || (sdata->num_actuator_info_table <= 1))
	{
		pr_info("%s: failed to actuator_info_table == NULL or num_actuator_info_table <= 1 return 0\n", __func__);
		return 0;
	}
	rc = sdata->camera_power_off();
	if (rc < 0)
		pr_info("%s: failed to disable power\n", __func__);

	//Set Sensor to SW-Standby
	rc = msm_camera_i2c_write_b(msm_camera_i2c_client, 0x0100, 0x00);
	if (rc < 0)
		pr_info("%s: i2c_write_b 0x0100 fail\n", __func__);

	//Set Input clock freq.(24MHz)
	rc = msm_camera_i2c_write_b(msm_camera_i2c_client, 0x3368, 0x18);
	if (rc < 0)
		pr_info("%s: i2c_write_b 0x3368 fail\n", __func__);

	rc = msm_camera_i2c_write_b(msm_camera_i2c_client, 0x3369, 0x00);
	if (rc < 0)
		pr_info("%s: i2c_write_b 0x3369 fail\n", __func__);

	//set read mode
	rc = msm_camera_i2c_write_b(msm_camera_i2c_client, 0x3400, 0x01);
	if (rc < 0)
		pr_info("%s: i2c_write_b 0x3400 fail\n", __func__);

	mdelay(4);

	//select information index, Driver ID at 10th index
	info_index = 10;
	/*Read page 3 to Page 0*/
	for (page = 3; page >= 0; page--) {
		//Select page
		rc = msm_camera_i2c_write_b(msm_camera_i2c_client, 0x3402, page);
		if (rc < 0)
			pr_info("%s: i2c_write_b 0x3402 (select page %d) fail\n", __func__, page);

		//Select information index. Driver ID at 10th index
		//for formal sample
		rc = msm_camera_i2c_read_b(msm_camera_i2c_client, (0x3410 + info_index), &info_value);
		if (rc < 0)
			pr_info("%s: i2c_read_b 0x%x fail\n", __func__, (0x3410 + info_index));

		/* some values of fuseid are maybe zero */
		if (((info_value & 0x0F) != 0) || page < 0)
			break;

		//for first sample
		rc = msm_camera_i2c_read_b(msm_camera_i2c_client, (0x3404 + info_index), &info_value);
		if (rc < 0)
			pr_info("%s: i2c_read_b 0x%x fail\n", __func__, (0x3404 + info_index));

		/* some values of fuseid are maybe zero */
		if (((info_value & 0x0F) != 0) || page < 0)
			break;

	}

	OTP = (short)(info_value&0x0F);
	pr_info("%s: OTP=%d\n", __func__, OTP);

	if (sdata->num_actuator_info_table > 1)
	{
		if (OTP == 1) //AD5816
			sdata->actuator_info = &sdata->actuator_info_table[2][0];
		else if (OTP == 2) //TI201
			sdata->actuator_info = &sdata->actuator_info_table[1][0];

		pr_info("%s: sdata->actuator_info->board_info->type=%s", __func__, sdata->actuator_info->board_info->type);
		pr_info("%s: sdata->actuator_info->board_info->addr=0x%x", __func__, sdata->actuator_info->board_info->addr);
	}

	/* interface disable */
#endif
	return 0;
}

/* HTC_START 20121105 - video hdr */
int vd6869_write_hdr_exp_gain1_ex(struct msm_sensor_ctrl_t *s_ctrl,
		int mode, uint16_t gain, uint16_t long_dig_gain, uint16_t short_dig_gain, uint32_t long_line, uint32_t short_line)
{
	uint32_t fl_lines;
	uint8_t offset;

/* HTC_START Angie 20111019 - Fix FPS */
	uint32_t fps_divider = Q10;

	if (mode == SENSOR_PREVIEW_MODE)
		fps_divider = s_ctrl->fps_divider;

/* HTC_START ben 20120229 */
	if(long_line > s_ctrl->sensor_exp_gain_info->sensor_max_linecount)
		long_line = s_ctrl->sensor_exp_gain_info->sensor_max_linecount;
	if(short_line > s_ctrl->sensor_exp_gain_info->sensor_max_linecount)
		short_line = s_ctrl->sensor_exp_gain_info->sensor_max_linecount;
/* HTC_END */

	fl_lines = s_ctrl->curr_frame_length_lines;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (long_line * Q10 > (fl_lines - offset) * fps_divider)
		fl_lines = long_line + offset;
	else
		fl_lines = (fl_lines * fps_divider) / Q10;
	if (short_line * Q10 > (fl_lines - offset) * fps_divider)
		fl_lines = short_line + offset;
	else
		fl_lines = (fl_lines * fps_divider) / Q10;
/* HTC_END */
	if(short_line > 254)
		short_line = 254;
	if(short_line < 1)
		short_line = 1;

	if(long_line > 1088)
		long_line = 1088;
	if(long_line < 1)
		long_line = 1;

	pr_info("[CAM] gain:%d long_dig_gain:%d short_dig_gain:%d long exposure:0x%x 0x%x(%d)(short exposure:0x%x 0x%x(%d))",
		gain,
		long_dig_gain,
		short_dig_gain,
		(long_line&0xff00),
		(long_line&0x00ff),
		long_line,
		(short_line & 0xff00),
		(short_line & 0x00ff),
		short_line);

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	//mdelay(250);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	/*long exposure*/
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		vd6869_hdr_gain_info.long_coarse_int_time_addr_h, long_line>>8,//,0x06,//(long_line&0xff00)>>8,
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		vd6869_hdr_gain_info.long_coarse_int_time_addr_l, (long_line&0x00ff),//0x38,//(long_line&0x00ff),
		MSM_CAMERA_I2C_BYTE_DATA);

	/*short exposure*/
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		vd6869_hdr_gain_info.short_coarse_int_time_addr_h, (short_line)>>8,//0x00,// (short_line & 0xff00)>>8,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		vd6869_hdr_gain_info.short_coarse_int_time_addr_l, (short_line & 0x00ff),//0xc7,// (short_line & 0x00ff),
		MSM_CAMERA_I2C_BYTE_DATA);
	/*gain*/
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		vd6869_hdr_gain_info.global_gain_addr, gain,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* HTC_START Gary_Yang : Video HDR*/
	if (s_ctrl->func_tbl->sensor_set_hdr_dig_gain)
		s_ctrl->func_tbl->sensor_set_hdr_dig_gain(s_ctrl, long_dig_gain, short_dig_gain);
	/* HTC_END*/

	//YushanII_set_hdr_exp(gain,dig_gain,long_line,short_line);

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;

}
int vd6869_write_hdr_outdoor_flag(struct msm_sensor_ctrl_t *s_ctrl, uint8_t is_outdoor)
{
    int indoor_line_length, outdoor_line_length;
    indoor_line_length = 9600;
    outdoor_line_length = 6000;
    if(s_ctrl->sensordata->sensor_cut == 0){/*sensor cut 0.9*/
	indoor_line_length = indoor_line_length /2;
	outdoor_line_length = outdoor_line_length /2;
    }

    if (is_outdoor)
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
            s_ctrl->sensor_output_reg_addr->line_length_pclk, outdoor_line_length,
            MSM_CAMERA_I2C_WORD_DATA);
    else
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
            s_ctrl->sensor_output_reg_addr->line_length_pclk, indoor_line_length,
            MSM_CAMERA_I2C_WORD_DATA);
    pr_info("[CAM] vd6869_write_hdr_outdoor_flag is outdoor = %d", is_outdoor);
    return 0;
}

/* HTC_END */

int vd6869_write_non_hdr_exp_gain1_ex(struct msm_sensor_ctrl_t *s_ctrl,
		int mode, uint16_t gain, uint16_t dig_gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;

/* HTC_START Angie 20111019 - Fix FPS */
	uint32_t fps_divider = Q10;

/* HTC_START robert 20121126 set lin count for OIS*/
/*TODO: Redesign new path, get line cnt from sensor*/
	ois_line = line;
/* HTC_END */
	if (mode == SENSOR_PREVIEW_MODE)
		fps_divider = s_ctrl->fps_divider;

/* HTC_START ben 20120229 */
	if(line > s_ctrl->sensor_exp_gain_info->sensor_max_linecount)
		line = s_ctrl->sensor_exp_gain_info->sensor_max_linecount;
/* HTC_END */

	fl_lines = s_ctrl->curr_frame_length_lines;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line * Q10 > (fl_lines - offset) * fps_divider)
		fl_lines = line + offset;
	else
		fl_lines = (fl_lines * fps_divider) / Q10;
/* HTC_END */

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	/* HTC_START pg digi gain 20100710 */
	if (s_ctrl->func_tbl->sensor_set_dig_gain)
		s_ctrl->func_tbl->sensor_set_dig_gain(s_ctrl, dig_gain);
	/* HTC_END pg digi gain 20100710 */
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

void vd6869_start_stream_hdr(struct msm_sensor_ctrl_t *s_ctrl){

//	uint16_t read_data;
	pr_info("1031,vd6869_start_stream,HDR");

	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->start_stream_conf,
		s_ctrl->msm_sensor_reg->start_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	/*TODO: after phase in cut 10, this code can be remove*/
	if(vd6869_ver == 0x09){/*cut 09*/
		/*analog setting*/
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x5800, 0x00,
		MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x5896, 0xb4,
		MSM_CAMERA_I2C_BYTE_DATA);
	}else if(vd6869_ver == 0x0A){/*cut 09E*/
		/*analog setting*/
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x5896, 0xbd,
		MSM_CAMERA_I2C_BYTE_DATA);
	}

#if 0
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x30CC, 0x0,
		MSM_CAMERA_I2C_BYTE_DATA);
	mdelay(50);

	/* clear 0x3300 bit 4 */
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
		0x3300, &read_data,
		MSM_CAMERA_I2C_BYTE_DATA);
	read_data &= 0xEF;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x3300, read_data,
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x4424, 0x07,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x30CC, 0x3,
		MSM_CAMERA_I2C_BYTE_DATA);
#endif

}

void vd6869_start_stream_non_hdr(struct msm_sensor_ctrl_t *s_ctrl){
	//uint16_t read_data;
	pr_info("vd6869_start_stream,non-HDR");

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x100, 0x01,
		MSM_CAMERA_I2C_BYTE_DATA);
	/*TODO: after phase in cut 10, this code can be remove*/
	if(vd6869_ver == 0x09){/*cut 09*/
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x5896, 0xa4,
		MSM_CAMERA_I2C_BYTE_DATA);
	}else if(vd6869_ver == 0x0A){/*cut 09E*/
		/*analog setting*/
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x5896, 0xbd,
		MSM_CAMERA_I2C_BYTE_DATA);
	}
#if 0
	mdelay(50);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x30CC, 0x0,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* clear 0x3300 bit 4 */
	msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
		0x3300, &read_data,
		MSM_CAMERA_I2C_BYTE_DATA);
	read_data &= 0xEF;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x3300, read_data,
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x4424, 0x07,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x30CC, 0x01,
		MSM_CAMERA_I2C_BYTE_DATA);
  #endif

}

void vd6869_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("%s: called\n", __func__);

	if(s_ctrl->sensordata->hdr_mode){
		vd6869_start_stream_hdr(s_ctrl);
	}else{
		vd6869_start_stream_non_hdr(s_ctrl);
	}
}

/* HTC_START 20121221 - VD6869 need wait maximum of one frame duration to prevent start stream command issue sensor during this phase*/
void vd6869_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t read_data = 0;
	uint16_t read_count = 0;

	CDBG("%s: called\n", __func__);

	if ((s_ctrl->sensordata->htc_image == HTC_CAMERA_IMAGE_YUSHANII_BOARD) && (s_ctrl->msm_sensor_reg->stop_stream_conf_yushanii))
	{
		msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->stop_stream_conf_yushanii,
		s_ctrl->msm_sensor_reg->stop_stream_conf_size_yushanii,
		s_ctrl->msm_sensor_reg->default_data_type);
	} else {
		msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->stop_stream_conf,
		s_ctrl->msm_sensor_reg->stop_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
	}

	do {
		msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			0x32a6, &read_data,
			MSM_CAMERA_I2C_BYTE_DATA);

		msleep(5);
		read_count++;
	}	while((read_data != 0x1) && (read_count < 400));	//note: 0x1 means steam idle, we had to wait sensor enter idle mode then start issue i2c command to sensor, in order to prevent worst case, the timeout will be 2sec.

	pr_info("[CAM]%s,read_data=%d,try_count=%d", __func__, read_data,read_count);

}
/* HTC_END 20121221*/

int vd6869_write_output_settings_specific(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int rc = 0;
	uint16_t value = 0;

	/* Apply sensor mirror/flip */
	if (vd6869_s_ctrl.mirror_flip == CAMERA_SENSOR_MIRROR_FLIP)
		value = VD6869_READ_MIRROR_FLIP;
	else if (vd6869_s_ctrl.mirror_flip == CAMERA_SENSOR_MIRROR)
		value = VD6869_READ_MIRROR;
	else if (vd6869_s_ctrl.mirror_flip == CAMERA_SENSOR_FLIP)
		value = VD6869_READ_FLIP;
	else
		value = VD6869_READ_NORMAL_MODE;

	rc = msm_camera_i2c_write(vd6869_s_ctrl.sensor_i2c_client,
		VD6869_REG_READ_MODE, value, MSM_CAMERA_I2C_BYTE_DATA);
	pr_info("value=%d\n", value);
	if (rc < 0) {
		pr_err("%s set mirror_flip failed\n", __func__);
		return rc;
	}

	return rc;
}
void vd6869_yushanII_set_output_format(struct msm_sensor_ctrl_t *sensor,int res, Ilp0100_structFrameFormat *output_format)
{
	pr_info("[CAM]%s\n",__func__);
	output_format->ActiveLineLengthPixels =
		sensor->msm_sensor_reg->output_settings[res].x_output;
	output_format->ActiveFrameLengthLines =
		sensor->msm_sensor_reg->output_settings[res].y_output;
	output_format->LineLengthPixels =
		sensor->msm_sensor_reg->output_settings[res].line_length_pclk;
	/*cut0.9 non hdr needs to times 2 in frame length line*/
	if(sensor->sensordata->sensor_cut == 0 &&
		sensor->sensordata->hdr_mode == 0)
		output_format->FrameLengthLines = 
			sensor->msm_sensor_reg->output_settings[res].frame_length_lines*2;
	else/*cut 1.0 or cut 0.9 with non HDR*/
		output_format->FrameLengthLines =
		sensor->msm_sensor_reg->output_settings[res].frame_length_lines;

	/* HTC_START steven bring sensor parms 20121119 */
	/*status line configuration*/
	output_format->StatusLinesOutputted = sensor->msm_sensor_reg->output_settings[res].yushan_status_line_enable;
	output_format->StatusNbLines = sensor->msm_sensor_reg->output_settings[res].yushan_status_line;
	output_format->ImageOrientation = sensor->sensordata->sensor_platform_info->mirror_flip;
	output_format->StatusLineLengthPixels =
		sensor->msm_sensor_reg->output_settings[res].x_output;
	/* HTC_END steven bring sensor parms 20121119 */

	output_format->MinInterframe =
		(output_format->FrameLengthLines -
		output_format->ActiveFrameLengthLines -
		output_format->StatusNbLines);//206;//HDR mode
	output_format->AutomaticFrameParamsUpdate = TRUE;

	if(sensor->sensordata->hdr_mode)
		output_format->HDRMode = STAGGERED;
	else
		output_format->HDRMode = HDR_NONE;

	output_format->Hoffset =
			sensor->msm_sensor_reg->output_settings[res].x_addr_start;
	output_format->Voffset =
			sensor->msm_sensor_reg->output_settings[res].y_addr_start;

	/*TOOD:vd6869 should provide this value*/
	output_format->HScaling = 1;
	output_format->VScaling = 1;

	if(sensor->msm_sensor_reg->output_settings[res].binning_factor == 2){/*binning mode*/
		output_format->Binning = 0x22;
	}else{/*non binning*/
		output_format->Binning = 0x11;
	}
}

void vd6869_yushanII_set_parm(struct msm_sensor_ctrl_t *sensor, int res,Ilp0100_structSensorParams *YushanII_sensor)
{
	/* HTC_END steven bring sensor parms 20121119 */
	YushanII_sensor->StatusNbLines = sensor->msm_sensor_reg->output_settings[res].yushan_sensor_status_line;
    /*this should be sensor maximum y size, not sensor resolution*/
	YushanII_sensor->FullActiveLines =
		sensor->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].y_output;
	/*this should be sensor maximum x size, not sensor resolution*/
	YushanII_sensor->FullActivePixels =
		sensor->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].x_output;
	/*this should be minimum LineLength of the sensor*/
	YushanII_sensor->MinLineLength =
		sensor->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].line_length_pclk;
}

static struct msm_sensor_fn_t vd6869_func_tbl = {
	.sensor_start_stream = vd6869_start_stream,
	.sensor_stop_stream = vd6869_stop_stream,	/* HTC_START - 20121221 VD6869 need wait maximum of one frame duration to prevent start stream command issue sensor during this phase*/
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
/* HTC_START 20121105 - video hdr*/
	.sensor_write_exp_gain_ex = vd6869_write_non_hdr_exp_gain1_ex,//vd6869_write_non_hdr_exp_gain1_ex,
	.sensor_write_hdr_exp_gain_ex = vd6869_write_hdr_exp_gain1_ex,//vd6869_write_hdr_exp_gain1_ex,
	.sensor_write_hdr_outdoor_flag = vd6869_write_hdr_outdoor_flag,//vd6869_write_hdr_exp_gain1_ex,
/* HTC_END*/
	.sensor_write_snapshot_exp_gain_ex = msm_sensor_write_exp_gain1_ex,
	.sensor_set_dig_gain = vd6869_set_dig_gain,
	.sensor_set_hdr_dig_gain = vd6869_set_hdr_dig_gain,/* HTC_START Gary_Yang : Video HDR*/
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = msm_sensor_setting_parallel,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = vd6869_power_up,
	.sensor_power_down = vd6869_power_down,
	.sensor_i2c_read_fuseid = vd6869_read_fuseid,
	.sensor_i2c_read_vcm_driver_ic = vd6869_read_VCM_driver_IC_info,
	.sensor_write_output_settings_specific = vd6869_write_output_settings_specific,
	.sensor_yushanII_set_output_format = vd6869_yushanII_set_output_format,
    .sensor_yushanII_set_parm = vd6869_yushanII_set_parm,
};

static struct msm_sensor_reg_t vd6869_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = vd6869_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(vd6869_start_settings),
	.stop_stream_conf = vd6869_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(vd6869_stop_settings),
	.group_hold_on_conf = vd6869_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(vd6869_groupon_settings),
	.group_hold_off_conf = vd6869_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(vd6869_groupoff_settings),
	.init_settings = &vd6869_init_conf[0],
	.init_size = ARRAY_SIZE(vd6869_init_conf),
	.mode_settings = &vd6869_confs[0],
	.output_settings = &vd6869_dimensions[0],
	.num_conf = ARRAY_SIZE(vd6869_confs),
};

static struct msm_sensor_ctrl_t vd6869_s_ctrl = {
	.msm_sensor_reg = &vd6869_regs,
	.sensor_i2c_client = &vd6869_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &vd6869_reg_addr,
	.sensor_id_info = &vd6869_id_info,
	.sensor_exp_gain_info = &vd6869_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &vd6869_csi_params_array[0],
	.msm_sensor_mutex = &vd6869_mut,
	.sensor_i2c_driver = &vd6869_i2c_driver,
	.sensor_v4l2_subdev_info = vd6869_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(vd6869_subdev_info),
	.sensor_v4l2_subdev_ops = &vd6869_subdev_ops,
	.func_tbl = &vd6869_func_tbl,
	.sensor_first_mutex = &vd6869_sensor_init_mut, //CC120826,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("ST 4MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
