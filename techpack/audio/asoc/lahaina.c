// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/soc/qcom/fsa4480-i2c.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <soc/snd_event.h>
#include <dsp/audio_notifier.h>
#include <soc/swr-common.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6core.h>
#include <soc/soundwire.h>
#include "device_event.h"
#include "msm-pcm-routing-v2.h"
#include "asoc/msm-cdc-pinctrl.h"
#include "asoc/wcd-mbhc-v2.h"
#include "codecs/wcd938x/wcd938x-mbhc.h"
#include "codecs/wcd937x/wcd937x-mbhc.h"
#include "codecs/wsa883x/wsa883x.h"
#include "codecs/wcd938x/wcd938x.h"
#include "codecs/wcd937x/wcd937x.h"
#include "codecs/bolero/bolero-cdc.h"
#include <dt-bindings/sound/audio-codec-port-types.h>
#include "codecs/bolero/wsa-macro.h"
#include "lahaina-port-config.h"
#include "msm_dailink.h"
#include "msm-common.h"

#define DRV_NAME "lahaina-asoc-snd"
#define __CHIPSET__ "LAHAINA "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_11P025KHZ 11025
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_22P05KHZ  22050
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_88P2KHZ   88200
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_176P4KHZ  176400
#define SAMPLING_RATE_192KHZ    192000
#define SAMPLING_RATE_352P8KHZ  352800
#define SAMPLING_RATE_384KHZ    384000

#define IS_FRACTIONAL(x) \
((x == SAMPLING_RATE_11P025KHZ) || (x == SAMPLING_RATE_22P05KHZ) || \
(x == SAMPLING_RATE_44P1KHZ) || (x == SAMPLING_RATE_88P2KHZ) || \
(x == SAMPLING_RATE_176P4KHZ) || (x == SAMPLING_RATE_352P8KHZ))

#define IS_MSM_INTERFACE_MI2S(x) \
((x == PRIM_MI2S) || (x == SEC_MI2S) || (x == TERT_MI2S))

#define WCD9XXX_MBHC_DEF_RLOADS     5
#define WCD9XXX_MBHC_DEF_BUTTONS    8
#define CODEC_EXT_CLK_RATE          9600000
#define ADSP_STATE_READY_TIMEOUT_MS 3000
#define DEV_NAME_STR_LEN            32
#define WCD_MBHC_HS_V_MAX           1600

#define TDM_CHANNEL_MAX		8
#define TDM_SLOT_OFFSET_MAX 	32

#define MI2S_NUM_CHANNELS	2

#define MSM_LL_QOS_VALUE	300 /* time in us to ensure LPM doesn't go in C3/C4 */

#define ADSP_STATE_READY_TIMEOUT_MS 3000

#define WCN_CDC_SLIM_RX_CH_MAX 2
#define WCN_CDC_SLIM_TX_CH_MAX 2
#define WCN_CDC_SLIM_TX_CH_MAX_LITO 3

#define SWR_MAX_SLAVE_DEVICES 6

enum {
	RX_PATH = 0,
	TX_PATH,
	MAX_PATH,
};

enum {
	TDM_0 = 0,
	TDM_1,
	TDM_2,
	TDM_3,
	TDM_4,
	TDM_5,
	TDM_6,
	TDM_7,
	TDM_PORT_MAX,
};

#define TDM_MAX_SLOTS 16
#define TDM_SLOT_WIDTH_BITS 32
#define TDM_SLOT_WIDTH_BYTES TDM_SLOT_WIDTH_BITS/8

enum {
	TDM_PRI = 0,
	TDM_SEC,
	TDM_TERT,
	TDM_QUAT,
	TDM_QUIN,
	TDM_SEN,
	TDM_INTERFACE_MAX,
};

enum {
	PRIM_AUX_PCM = 0,
	SEC_AUX_PCM,
	TERT_AUX_PCM,
	QUAT_AUX_PCM,
	QUIN_AUX_PCM,
	SEN_AUX_PCM,
	AUX_PCM_MAX,
};

enum {
	WSA_CDC_DMA_RX_0 = 0,
	WSA_CDC_DMA_RX_1,
	RX_CDC_DMA_RX_0,
	RX_CDC_DMA_RX_1,
	RX_CDC_DMA_RX_2,
	RX_CDC_DMA_RX_3,
	RX_CDC_DMA_RX_5,
	RX_CDC_DMA_RX_6,
	CDC_DMA_RX_MAX,
};

enum {
	WSA_CDC_DMA_TX_0 = 0,
	WSA_CDC_DMA_TX_1,
	WSA_CDC_DMA_TX_2,
	TX_CDC_DMA_TX_0,
	TX_CDC_DMA_TX_3,
	TX_CDC_DMA_TX_4,
	VA_CDC_DMA_TX_0,
	VA_CDC_DMA_TX_1,
	VA_CDC_DMA_TX_2,
	CDC_DMA_TX_MAX,
};

enum {
	SLIM_RX_7 = 0,
	SLIM_RX_MAX,
};
enum {
	SLIM_TX_7 = 0,
	SLIM_TX_8,
	SLIM_TX_MAX,
};

enum {
	AFE_LOOPBACK_TX_IDX = 0,
	AFE_LOOPBACK_TX_IDX_MAX,
};

#define MCLK_CFG_CELLS 5
struct ext_mclk_freq_cfg {
	u32 clk_freq;
	u32 div2x;
	u32 m;
	u32 n;
	u32 d;
};

/* Coupled with "qcom,ext-mclk-srcs" DTSI property */
#define MCLK_SRCS_CELLS 3
struct ext_mclk_src_cfg {
	u32 clk_id;
	u32 clk_root;
	u32 gpio_idx;
};

struct ext_mclk_cfg_info {
	u32 mclk_freq;
	const char *prop;
	struct ext_mclk_freq_cfg *mclk_cfg;
	u32 num_mclk_cfg;
};

struct ext_mclk_gpio_info {
	struct device_node *gpio_p; /* used by pinctrl API */
	const char *ext_mclk_muxsel_str;
	uint32_t ext_mclk_muxsel_val;
	uint32_t ref_cnt;
	struct mutex lock;
};

struct ext_mclk_src_info {
	u32 clk_id;
	u32 clk_root;
	struct ext_mclk_gpio_info *gpio_info;
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

struct tdm_dev_config {
	unsigned int tdm_slot_offset[TDM_MAX_SLOTS];
};

enum {
	EXT_DISP_RX_IDX_DP = 0,
	EXT_DISP_RX_IDX_DP1,
	EXT_DISP_RX_IDX_MAX,
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

static bool ext_mclk_enable;
static uint32_t num_ext_mclk_gpios;
static struct ext_mclk_gpio_info *ext_mclk_gpio_info;
static struct ext_mclk_src_cfg *ext_mclk_src_info;
static struct ext_mclk_cfg_info ext_mclk_freq_info[MCLK_FREQ_MAX] = {
	[MCLK_FREQ_11P2896_MHZ] = {11289600, "ext-mclk-cfg-11p2896", NULL, 0},
	[MCLK_FREQ_12P288_MHZ]  = {12288000, "ext-mclk-cfg-12p288",  NULL, 0},
	[MCLK_FREQ_16P384_MHZ]  = {16384000, "ext-mclk-cfg-16p384",  NULL, 0},
	[MCLK_FREQ_19P200_MHZ]  = {19200000, "ext-mclk-cfg-19p200",  NULL, 0},
	[MCLK_FREQ_22P5792_MHZ] = {22579200, "ext-mclk-cfg-22p5792", NULL, 0},
	[MCLK_FREQ_24P576_MHZ]  = {24576000, "ext-mclk-cfg-24p576",  NULL, 0},
};

/* Default configuration of slimbus channels */
static struct dev_config slim_rx_cfg[] = {
	[SLIM_RX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config slim_tx_cfg[] = {
	[SLIM_TX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_8] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

/* Default configuration of external display BE */
static struct dev_config ext_disp_rx_cfg[] = {
	[EXT_DISP_RX_IDX_DP] =   {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[EXT_DISP_RX_IDX_DP1] =   {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config usb_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

static struct dev_config usb_tx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 1,
};

static struct dev_config proxy_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

static struct afe_clk_set mi2s_clk[MI2S_MAX] = {
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_SEN_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
};

struct mi2s_conf {
	struct mutex lock;
	u32 ref_cnt;
	u32 msm_is_mi2s_master;
	bool audio_core_vote;
};

static u32 mi2s_ebit_clk[MI2S_MAX] = {
	Q6AFE_LPASS_CLK_ID_PRI_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_SEC_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_EBIT,
};

static struct mi2s_conf mi2s_intf_conf[MI2S_MAX];

/* Default configuration of TDM channels */
static struct dev_config tdm_rx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* QUIN TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* SEN TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
};

static struct dev_config tdm_tx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* QUIN TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* SEN TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
};

/* Default configuration of AUX PCM channels */
static struct dev_config aux_pcm_rx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUIN_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEN_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_tx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUIN_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEN_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

/* Default configuration of MI2S channels */
static struct dev_config mi2s_rx_cfg[] = {
	[PRIM_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[QUIN_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[SEN_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config mi2s_tx_cfg[] = {
	[PRIM_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUIN_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEN_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct tdm_dev_config pri_tdm_dev_config[MAX_PATH][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{ {0,   4, 0xFFFF} }, /* RX_0 */
		{ {8,  12, 0xFFFF} }, /* RX_1 */
		{ {16, 20, 0xFFFF} }, /* RX_2 */
		{ {24, 28, 0xFFFF} }, /* RX_3 */
		{ {0xFFFF} }, /* RX_4 */
		{ {0xFFFF} }, /* RX_5 */
		{ {0xFFFF} }, /* RX_6 */
		{ {0xFFFF} }, /* RX_7 */
	},
	{
		{ {0,   4,      8, 12, 0xFFFF} }, /* TX_0 */
		{ {8,  12, 0xFFFF} }, /* TX_1 */
		{ {16, 20, 0xFFFF} }, /* TX_2 */
		{ {24, 28, 0xFFFF} }, /* TX_3 */
		{ {0xFFFF} }, /* TX_4 */
		{ {0xFFFF} }, /* TX_5 */
		{ {0xFFFF} }, /* TX_6 */
		{ {0xFFFF} }, /* TX_7 */
	},
};

static struct tdm_dev_config sec_tdm_dev_config[MAX_PATH][TDM_PORT_MAX] = {
	{ /* SEC TDM */
		{ {0,   4, 0xFFFF} }, /* RX_0 */
		{ {8,  12, 0xFFFF} }, /* RX_1 */
		{ {16, 20, 0xFFFF} }, /* RX_2 */
		{ {24, 28, 0xFFFF} }, /* RX_3 */
		{ {0xFFFF} }, /* RX_4 */
		{ {0xFFFF} }, /* RX_5 */
		{ {0xFFFF} }, /* RX_6 */
		{ {0xFFFF} }, /* RX_7 */
	},
	{
		{ {0,   4, 0xFFFF} }, /* TX_0 */
		{ {8,  12, 0xFFFF} }, /* TX_1 */
		{ {16, 20, 0xFFFF} }, /* TX_2 */
		{ {24, 28, 0xFFFF} }, /* TX_3 */
		{ {0xFFFF} }, /* TX_4 */
		{ {0xFFFF} }, /* TX_5 */
		{ {0xFFFF} }, /* TX_6 */
		{ {0xFFFF} }, /* TX_7 */
	},
};

static struct tdm_dev_config tert_tdm_dev_config[MAX_PATH][TDM_PORT_MAX] = {
	{ /* TERT TDM */
		{ {0,   4, 0xFFFF} }, /* RX_0 */
		{ {8,  12, 0xFFFF} }, /* RX_1 */
		{ {16, 20, 0xFFFF} }, /* RX_2 */
		{ {24, 28, 0xFFFF} }, /* RX_3 */
		{ {0xFFFF} }, /* RX_4 */
		{ {0xFFFF} }, /* RX_5 */
		{ {0xFFFF} }, /* RX_6 */
		{ {0xFFFF} }, /* RX_7 */
	},
	{
		{ {0,   4, 0xFFFF} }, /* TX_0 */
		{ {8,  12, 0xFFFF} }, /* TX_1 */
		{ {16, 20, 0xFFFF} }, /* TX_2 */
		{ {24, 28, 0xFFFF} }, /* TX_3 */
		{ {0xFFFF} }, /* TX_4 */
		{ {0xFFFF} }, /* TX_5 */
		{ {0xFFFF} }, /* TX_6 */
		{ {0xFFFF} }, /* TX_7 */
	},
};

static struct tdm_dev_config quat_tdm_dev_config[MAX_PATH][TDM_PORT_MAX] = {
	{ /* QUAT TDM */
		{ {0,   4, 0xFFFF} }, /* RX_0 */
		{ {8,  12, 0xFFFF} }, /* RX_1 */
		{ {16, 20, 0xFFFF} }, /* RX_2 */
		{ {24, 28, 0xFFFF} }, /* RX_3 */
		{ {0xFFFF} }, /* RX_4 */
		{ {0xFFFF} }, /* RX_5 */
		{ {0xFFFF} }, /* RX_6 */
		{ {0xFFFF} }, /* RX_7 */
	},
	{
		{ {0,   4, 0xFFFF} }, /* TX_0 */
		{ {8,  12, 0xFFFF} }, /* TX_1 */
		{ {16, 20, 0xFFFF} }, /* TX_2 */
		{ {24, 28, 0xFFFF} }, /* TX_3 */
		{ {0xFFFF} }, /* TX_4 */
		{ {0xFFFF} }, /* TX_5 */
		{ {0xFFFF} }, /* TX_6 */
		{ {0xFFFF} }, /* TX_7 */
	},
};

static struct tdm_dev_config quin_tdm_dev_config[MAX_PATH][TDM_PORT_MAX] = {
	{ /* QUIN TDM */
		{ {0,   4, 0xFFFF} }, /* RX_0 */
		{ {8,  12, 0xFFFF} }, /* RX_1 */
		{ {16, 20, 0xFFFF} }, /* RX_2 */
		{ {24, 28, 0xFFFF} }, /* RX_3 */
		{ {0xFFFF} }, /* RX_4 */
		{ {0xFFFF} }, /* RX_5 */
		{ {0xFFFF} }, /* RX_6 */
		{ {0xFFFF} }, /* RX_7 */
	},
	{
		{ {0,   4, 0xFFFF} }, /* TX_0 */
		{ {8,  12, 0xFFFF} }, /* TX_1 */
		{ {16, 20, 0xFFFF} }, /* TX_2 */
		{ {24, 28, 0xFFFF} }, /* TX_3 */
		{ {0xFFFF} }, /* TX_4 */
		{ {0xFFFF} }, /* TX_5 */
		{ {0xFFFF} }, /* TX_6 */
		{ {0xFFFF} }, /* TX_7 */
	},
};

static struct tdm_dev_config sen_tdm_dev_config[MAX_PATH][TDM_PORT_MAX] = {
	{ /* SEN TDM */
		{ {0,   4, 0xFFFF} }, /* RX_0 */
		{ {8,  12, 0xFFFF} }, /* RX_1 */
		{ {16, 20, 0xFFFF} }, /* RX_2 */
		{ {24, 28, 0xFFFF} }, /* RX_3 */
		{ {0xFFFF} }, /* RX_4 */
		{ {0xFFFF} }, /* RX_5 */
		{ {0xFFFF} }, /* RX_6 */
		{ {0xFFFF} }, /* RX_7 */
	},
	{
		{ {0,   4, 0xFFFF} }, /* TX_0 */
		{ {8,  12, 0xFFFF} }, /* TX_1 */
		{ {16, 20, 0xFFFF} }, /* TX_2 */
		{ {24, 28, 0xFFFF} }, /* TX_3 */
		{ {0xFFFF} }, /* TX_4 */
		{ {0xFFFF} }, /* TX_5 */
		{ {0xFFFF} }, /* TX_6 */
		{ {0xFFFF} }, /* TX_7 */
	},
};

static void *tdm_cfg[TDM_INTERFACE_MAX] = {
	pri_tdm_dev_config,
	sec_tdm_dev_config,
	tert_tdm_dev_config,
	quat_tdm_dev_config,
	quin_tdm_dev_config,
	sen_tdm_dev_config,
};

/* Default configuration of Codec DMA Interface RX */
static struct dev_config cdc_dma_rx_cfg[] = {
	[WSA_CDC_DMA_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[WSA_CDC_DMA_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

/* Default configuration of Codec DMA Interface TX */
static struct dev_config cdc_dma_tx_cfg[] = {
	[WSA_CDC_DMA_TX_0] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[WSA_CDC_DMA_TX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[WSA_CDC_DMA_TX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TX_CDC_DMA_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TX_CDC_DMA_TX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TX_CDC_DMA_TX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[VA_CDC_DMA_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 8},
	[VA_CDC_DMA_TX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 8},
	[VA_CDC_DMA_TX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 8},
};

static struct dev_config afe_loopback_tx_cfg[] = {
	[AFE_LOOPBACK_TX_IDX] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

/* TDM default slot config */
struct tdm_slot_cfg {
	u32 width;
	u32 num;
};

static struct tdm_slot_cfg tdm_slot[TDM_INTERFACE_MAX] = {
	/* PRI TDM */
	{32, 8},
	/* SEC TDM */
	{32, 8},
	/* TERT TDM */
	{32, 8},
	/* QUAT TDM */
	{32, 8},
	/* QUIN TDM */
	{32, 8},
	/* SEN TDM*/
	{32, 8}
};

static unsigned int tdm_rx_slot_offset
	[TDM_INTERFACE_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* PRI TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* SEC TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* TERT TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* QUAT TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* QUIN TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* SEN TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF}
};

static unsigned int tdm_tx_slot_offset
	[TDM_INTERFACE_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* PRI TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* SEC TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* TERT TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* QUAT TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* QUIN TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF},
	/* SEN TDM */
	{0, 4, 8, 12, 16, 20, 24, 0xFFFF}
};

static int msm_vi_feed_tx_ch = 2;
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					  "S32_LE"};
static char const *cdc80_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static char const *ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *usb_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
					"KHZ_16", "KHZ_22P05",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static const char *const usb_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static char const *tdm_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
					"KHZ_16", "KHZ_22P05",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static const char *const tdm_slot_num_text[] = {"One", "Two", "Four",
					"Eight", "Sixteen", "ThirtyTwo"};
static const char *const tdm_slot_width_text[] = {"16", "24", "32"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S32_LE"};
static char const *tdm_ch_text[] = {
		"One", "Two", "Three", "Four", "Five", "Six", "Seven",
		"Eight", "Nine", "Ten", "Eleven", "Twelve", "Thirteen",
		"Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen",
		"Nineteen", "Twenty", "TwentyOne", "TwentyTwo", "TwentyThree",
		"TwentyFour", "TwentyFive", "TwentySix", "TwentySeven",
		"TwentyEight", "TwentyNine", "Thirty", "ThirtyOne", "ThirtyTwo"
};
static const char *const auxpcm_rate_text[] = {"KHZ_8", "KHZ_16"};
static char const *mi2s_rate_text[] = {"KHZ_8", "KHZ_11P025", "KHZ_16",
				      "KHZ_22P05", "KHZ_32", "KHZ_44P1",
				      "KHZ_48", "KHZ_88P2", "KHZ_96",
				      "KHZ_176P4", "KHZ_192","KHZ_352P8",
				      "KHZ_384"};
static const char *const mi2s_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};

static const char *const cdc_dma_rx_ch_text[] = {"One", "Two"};
static const char *const cdc_dma_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static char const *cdc_dma_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
						 "KHZ_16", "KHZ_22P05",
						 "KHZ_32", "KHZ_44P1", "KHZ_48",
						 "KHZ_88P2", "KHZ_96",
						 "KHZ_176P4", "KHZ_192",
						 "KHZ_352P8", "KHZ_384"};
static char const *cdc80_dma_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
						 "KHZ_16", "KHZ_22P05",
						 "KHZ_32", "KHZ_44P1", "KHZ_48",
						 "KHZ_88P2", "KHZ_96",
						 "KHZ_176P4", "KHZ_192"};
static char const *ext_disp_bit_format_text[] = {"S16_LE", "S24_LE",
						 "S24_3LE"};
static char const *ext_disp_sample_rate_text[] = {"KHZ_48", "KHZ_96",
						"KHZ_192", "KHZ_32", "KHZ_44P1",
						"KHZ_88P2", "KHZ_176P4"};
static char const *bt_sample_rate_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96"};
static char const *bt_sample_rate_rx_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96"};
static char const *bt_sample_rate_tx_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96"};
static const char *const afe_loopback_tx_ch_text[] = {"One", "Two"};

static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(vi_feed_tx_chs, vi_feed_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(proxy_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_sample_rate, tdm_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_sample_rate, tdm_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_slot_num, tdm_slot_num_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_slot_width, tdm_slot_width_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_format, tdm_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_format, tdm_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_chs, tdm_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_chs, tdm_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sen_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sen_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(aux_pcm_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(aux_pcm_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sen_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sen_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sen_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sen_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_0_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_1_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_0_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_1_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_2_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_3_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_5_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_6_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_0_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_1_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_2_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_0_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_3_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_4_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_0_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_1_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_2_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_3_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_4_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_2_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_3_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_4_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(va_cdc_dma_tx_2_sample_rate,
				cdc_dma_sample_rate_text);

/* WCD9370 */
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_3_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_5_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_0_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_1_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_2_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_3_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc70_dma_rx_5_sample_rate,
				cdc80_dma_sample_rate_text);

/* WCD9375 */
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_3_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_5_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_2_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_3_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc75_dma_rx_5_sample_rate,
				cdc_dma_sample_rate_text);

/* WCD9380 */
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_0_format, cdc80_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_1_format, cdc80_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_2_format, cdc80_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_3_format, cdc80_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_5_format, cdc80_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_6_format, cdc80_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_0_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_1_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_2_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_3_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_5_sample_rate,
				cdc80_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc80_dma_rx_6_sample_rate,
				cdc80_dma_sample_rate_text);
/* WCD9385 */
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_3_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_5_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_6_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_2_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_3_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_5_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc85_dma_rx_6_sample_rate,
				cdc_dma_sample_rate_text);

static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_format, ext_disp_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_sample_rate,
				ext_disp_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate, bt_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate_rx, bt_sample_rate_rx_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate_tx, bt_sample_rate_tx_text);
static SOC_ENUM_SINGLE_EXT_DECL(afe_loopback_tx_chs, afe_loopback_tx_ch_text);

static bool is_initial_boot;
static bool codec_reg_done;
static struct snd_soc_card snd_soc_card_lahaina_msm;
static int dmic_0_1_gpio_cnt;
static int dmic_2_3_gpio_cnt;
static int dmic_4_5_gpio_cnt;

static void *def_wcd_mbhc_cal(void);

static int msm_rx_tx_codec_init(struct snd_soc_pcm_runtime*);
static int msm_int_wsa_init(struct snd_soc_pcm_runtime*);

/*
 * Need to report LINEIN
 * if R/L channel impedance is larger than 5K ohm
 */
static struct wcd_mbhc_config wcd_mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.detect_extn_cable = true,
	.mono_stero_detection = false,
	.swap_gnd_mic = NULL,
	.hs_ext_micbias = true,
	.key_code[0] = KEY_MEDIA,
	.key_code[1] = KEY_VOICECOMMAND,
	.key_code[2] = KEY_VOLUMEUP,
	.key_code[3] = KEY_VOLUMEDOWN,
	.key_code[4] = 0,
	.key_code[5] = 0,
	.key_code[6] = 0,
	.key_code[7] = 0,
	.linein_th = 5000,
	.moisture_en = false,
	.mbhc_micbias = MIC_BIAS_2,
	.anc_micbias = MIC_BIAS_2,
	.enable_anc_mic_detect = false,
	.moisture_duty_cycle_en = true,
};

/* set audio task affinity to core 1 & 2 */
static const unsigned int audio_core_list[] = {1, 2};
static cpumask_t audio_cpu_map = CPU_MASK_NONE;
static struct dev_pm_qos_request *msm_audio_req;
static unsigned int qos_client_active_cnt;

static int lahaina_audio_vote(struct snd_soc_card *card, bool enable)
{
	struct msm_asoc_mach_data *pdata = NULL;
	int ret = 0;

	if (!card) {
		pr_err("%s: sound card is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = snd_soc_card_get_drvdata(card);
	if (!pdata || !pdata->lpass_audio_hw_vote) {
		pr_err("%s: lpass audio hw voting not supported\n", __func__);
		return -EINVAL;
	}

	/* Locking and reference counting is handled by the underlying clock
	 * framework.
	 */
	if (enable) {
		ret = clk_prepare_enable(pdata->lpass_audio_hw_vote);
		if (ret < 0) {
			dev_err(card->dev, "%s: audio vote error: %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(pdata->lpass_audio_hw_vote);
	}

	return ret;
}

static int lahaina_populate_ext_mclk_cfg(struct ext_mclk_cfg_info *freq_cfg,
				uint32_t mclk_freq,
				struct afe_param_id_clock_set_v2_t *dyn_mclk_cfg)
{
	struct ext_mclk_freq_cfg *mclk_cfg = NULL;
	uint32_t mclk_cfg_entries = 0;
	enum afe_mclk_freq freq = MCLK_FREQ_MIN;
	int i = 0;

	if (!freq_cfg || !dyn_mclk_cfg)
		return -EINVAL;

	for (freq = MCLK_FREQ_MIN; freq < MCLK_FREQ_MAX; freq++) {
		if (freq_cfg[freq].mclk_freq == mclk_freq)
			break;
	}

	if (freq == MCLK_FREQ_MAX) {
		pr_err("%s: unsupported mclk freq: %u\n", __func__, mclk_freq);
		return -EINVAL;
	}

	if (!freq_cfg[freq].mclk_cfg ||
	    !freq_cfg[freq].num_mclk_cfg) {
		pr_err("%s: cfg table unavailable for mclk freq: %u\n",
			   __func__, mclk_freq);
		return -EINVAL;
	}

	mclk_cfg = freq_cfg[freq].mclk_cfg;
	mclk_cfg_entries = freq_cfg[freq].num_mclk_cfg;

	for (i = 0; i < mclk_cfg_entries; i++) {
		if (mclk_cfg[i].clk_freq == dyn_mclk_cfg->clk_freq_in_hz) {
			dyn_mclk_cfg->divider_2x = mclk_cfg[i].div2x;
			dyn_mclk_cfg->m = mclk_cfg[i].m;
			dyn_mclk_cfg->n = mclk_cfg[i].n;
			dyn_mclk_cfg->d = mclk_cfg[i].d;
			break;
		}
	}

	if (i == mclk_cfg_entries) {
		pr_err("%s: requested output mclk freq %u is not supported\n",
		       __func__, dyn_mclk_cfg->clk_freq_in_hz);
		return -EINVAL;
	}

	return 0;
}

static int lahaina_handle_ext_mclk_gpio(struct snd_soc_card *card,
					struct ext_mclk_gpio_info *gpio_info,
					uint32_t enable)
{
	int ret = 0;

	if (!gpio_info)
		return -EINVAL;

	mutex_lock(&gpio_info->lock);
	if (enable) {
		if (++gpio_info->ref_cnt == 1) {
			ret = msm_cdc_pinctrl_select_active_state(gpio_info->gpio_p);
			if (ret) {
				dev_err(card->dev, "%s: couldn't activate mclk pinctrl\n",
				       __func__);
				goto unlock;
			}

			if (gpio_info->ext_mclk_muxsel_str != NULL) {
				ret = lahaina_audio_vote(card, true);
				if (ret) {
					dev_err(card->dev, "%s: HW voting failed, ret: %d\n",
					       __func__, ret);
					goto unlock;
				}

				ret = afe_set_lpass_ext_mclk_mux_cfg(gpio_info->ext_mclk_muxsel_str,
					gpio_info->ext_mclk_muxsel_val);
				lahaina_audio_vote(card, false);
			}
		}
	} else {
		if (--gpio_info->ref_cnt == 0) {
			ret = msm_cdc_pinctrl_select_sleep_state(gpio_info->gpio_p);
			if (ret) {
				dev_err(card->dev, "%s: couldn't deactivate mclk pinctrl\n",
				       __func__);
			}
		}
	}

unlock:
	mutex_unlock(&gpio_info->lock);
	if (ret)
		enable ? --gpio_info->ref_cnt : ++gpio_info->ref_cnt;

	return ret;
}

static int lahaina_enable_and_get_mclk_cfg(void *private_data, uint32_t enable,
				uint32_t mclk_freq,
				struct afe_param_id_clock_set_v2_t *dyn_mclk_cfg)
{
	struct snd_soc_card *card = (struct snd_soc_card *)private_data;
	struct msm_asoc_mach_data *pdata = NULL;
	struct ext_mclk_src_info *ext_mclk_src = NULL;
	int i = 0;
	int ret = 0;

	if (!card || !dyn_mclk_cfg)
		return -EINVAL;

	if (!ext_mclk_enable) {
		dev_err(card->dev, "%s: ext mclk support not enabled on the platform\n",
			__func__);
		return -EOPNOTSUPP;
	}

	pdata = snd_soc_card_get_drvdata(card);
	if (!pdata || !pdata->ext_mclk_srcs || !pdata->num_ext_mclk_srcs)
		return -EINVAL;

	for (i = 0; i < pdata->num_ext_mclk_srcs; i++) {
		if (pdata->ext_mclk_srcs[i].clk_id == dyn_mclk_cfg->clk_id)
			break;
	}

	if (i == pdata->num_ext_mclk_srcs) {
		dev_err(card->dev, "%s: unsupported clk id for ext mclk support: %u\n",
		       __func__, dyn_mclk_cfg->clk_id);
		return -EINVAL;
	}

	ext_mclk_src = &pdata->ext_mclk_srcs[i];
	if (!ext_mclk_src) {
		dev_err(card->dev, "%s: ext mclk src/clk cfg unavailable for mclk: %u\n",
		       __func__, pdata->ext_mclk_srcs[i].clk_id);
		return -EINVAL;
	}

	/* Populate clk root */
	dyn_mclk_cfg->clk_root = (uint16_t) ext_mclk_src->clk_root;

	/* Populate div2x, M, N, D values */
	ret = lahaina_populate_ext_mclk_cfg(ext_mclk_freq_info, mclk_freq,
					    dyn_mclk_cfg);
	if (ret) {
		dev_err(card->dev, "%s: unable to populate ext mclk cfg, ret: %d\n", __func__, ret);
		goto reset;
	}

	/* Enable/disable ext mclk GPIO */
	ret = lahaina_handle_ext_mclk_gpio(card, ext_mclk_src->gpio_info, enable);
	if (ret) {
		dev_err(card->dev, "%s: unable to enable/disable ext mclk gpio, ret: %d\n",
		       __func__, ret);
		goto reset;
	}

	return 0;

reset:
	/* Reset clk cfg */
	dyn_mclk_cfg->divider_2x = 0;
	dyn_mclk_cfg->m = 0;
	dyn_mclk_cfg->n = 0;
	dyn_mclk_cfg->d = 0;
	dyn_mclk_cfg->clk_root = 0;

	return ret;
}

static void msm_audio_add_qos_request(void)
{
	int i;
	int cpu = 0;

	msm_audio_req = kcalloc(num_possible_cpus(),
		sizeof(struct dev_pm_qos_request), GFP_KERNEL);
	if (!msm_audio_req)
		return;

	for (i = 0; i < ARRAY_SIZE(audio_core_list); i++) {
		if (audio_core_list[i] >= num_possible_cpus())
			pr_err("%s incorrect cpu id: %d specified.\n",
				__func__, audio_core_list[i]);
		else
			cpumask_set_cpu(audio_core_list[i], &audio_cpu_map);
	}

	for_each_cpu(cpu, &audio_cpu_map) {
		dev_pm_qos_add_request(get_cpu_device(cpu),
			&msm_audio_req[cpu],
			DEV_PM_QOS_RESUME_LATENCY,
			PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE);
		pr_debug("%s set cpu affinity to core %d.\n", __func__, cpu);
	}
}

static void msm_audio_remove_qos_request(void)
{
	int cpu = 0;

	if (msm_audio_req) {
		for_each_cpu(cpu, &audio_cpu_map) {
			dev_pm_qos_remove_request(
				&msm_audio_req[cpu]);
			pr_debug("%s remove cpu affinity of core %d.\n",
				__func__, cpu);
		}
		kfree(msm_audio_req);
	}
}

static void msm_audio_update_qos_request(u32 latency)
{
	int cpu = 0;

	if (msm_audio_req) {
		for_each_cpu(cpu, &audio_cpu_map) {
			dev_pm_qos_update_request(
				&msm_audio_req[cpu], latency);
			pr_debug("%s update latency of core %d to %ul.\n",
				__func__, cpu, latency);
		}
	}
}

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p,
					     int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n,
			   unsigned int bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);

		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static int usb_audio_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (usb_rx_cfg.sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: usb_audio_rx_sample_rate = %d\n", __func__,
		 usb_rx_cfg.sample_rate);
	return 0;
}

static int usb_audio_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 12:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, usb_audio_rx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		usb_rx_cfg.sample_rate);
	return 0;
}

static int usb_audio_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (usb_tx_cfg.sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	default:
		sample_rate_val = 6;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: usb_audio_tx_sample_rate = %d\n", __func__,
		 usb_tx_cfg.sample_rate);
	return 0;
}

static int usb_audio_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 12:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, usb_audio_tx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		usb_tx_cfg.sample_rate);
	return 0;
}
static int afe_loopback_tx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: afe_loopback_tx_ch  = %d\n", __func__,
		afe_loopback_tx_cfg[0].channels);
	ucontrol->value.enumerated.item[0] =
		afe_loopback_tx_cfg[0].channels - 1;

	return 0;
}

static int afe_loopback_tx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	afe_loopback_tx_cfg[0].channels =
			ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: afe_loopback_tx_ch  = %d\n", __func__,
			afe_loopback_tx_cfg[0].channels);

	return 1;
}

static int usb_audio_rx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (usb_rx_cfg.bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: usb_audio_rx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_rx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int usb_audio_rx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: usb_audio_rx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_rx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int usb_audio_tx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (usb_tx_cfg.bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: usb_audio_tx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_tx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int usb_audio_tx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: usb_audio_tx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_tx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int usb_audio_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: usb_audio_rx_ch  = %d\n", __func__,
		 usb_rx_cfg.channels);
	ucontrol->value.integer.value[0] = usb_rx_cfg.channels - 1;
	return 0;
}

static int usb_audio_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_rx_cfg.channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: usb_audio_rx_ch = %d\n", __func__, usb_rx_cfg.channels);
	return 1;
}

static int usb_audio_tx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: usb_audio_tx_ch  = %d\n", __func__,
		 usb_tx_cfg.channels);
	ucontrol->value.integer.value[0] = usb_tx_cfg.channels - 1;
	return 0;
}

static int usb_audio_tx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_tx_cfg.channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: usb_audio_tx_ch = %d\n", __func__, usb_tx_cfg.channels);
	return 1;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_vi_feed_tx_ch - 1;
	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_vi_feed_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_vi_feed_tx_ch = %d\n", __func__, msm_vi_feed_tx_ch);
	return 1;
}

static int ext_disp_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx = 0;

	if (strnstr(kcontrol->id.name, "Display Port RX",
		    sizeof("Display Port RX"))) {
		idx = EXT_DISP_RX_IDX_DP;
	} else if (strnstr(kcontrol->id.name, "Display Port1 RX",
		    sizeof("Display Port1 RX"))) {
		idx = EXT_DISP_RX_IDX_DP1;
	} else {
		pr_err("%s: unsupported BE: %s\n",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int ext_disp_rx_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ext_disp_rx_cfg[idx].bit_format) {
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: ext_disp_rx[%d].format = %d, ucontrol value = %ld\n",
		 __func__, idx, ext_disp_rx_cfg[idx].bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int ext_disp_rx_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		ext_disp_rx_cfg[idx].bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		ext_disp_rx_cfg[idx].bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		ext_disp_rx_cfg[idx].bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: ext_disp_rx[%d].format = %d, ucontrol value = %ld\n",
		 __func__, idx, ext_disp_rx_cfg[idx].bit_format,
		 ucontrol->value.integer.value[0]);

	return 0;
}

static int ext_disp_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.integer.value[0] =
			ext_disp_rx_cfg[idx].channels - 2;

	pr_debug("%s: ext_disp_rx[%d].ch = %d\n", __func__,
		 idx, ext_disp_rx_cfg[idx].channels);

	return 0;
}

static int ext_disp_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ext_disp_rx_cfg[idx].channels =
			ucontrol->value.integer.value[0] + 2;

	pr_debug("%s: ext_disp_rx[%d].ch = %d\n", __func__,
		 idx, ext_disp_rx_cfg[idx].channels);
	return 1;
}

static int ext_disp_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val;
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ext_disp_rx_cfg[idx].sample_rate) {
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 6;
		break;

	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 5;
		break;

	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 4;
		break;

	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: ext_disp_rx[%d].sample_rate = %d\n", __func__,
		 idx, ext_disp_rx_cfg[idx].sample_rate);

	return 0;
}

static int ext_disp_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ucontrol->value.integer.value[0]) {
	case 6:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 5:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 4:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 2:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, ext_disp_rx[%d].sample_rate = %d\n",
		 __func__, ucontrol->value.integer.value[0], idx,
		 ext_disp_rx_cfg[idx].sample_rate);
	return 0;
}

static int proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: proxy_rx channels = %d\n",
		 __func__, proxy_rx_cfg.channels);
	ucontrol->value.integer.value[0] = proxy_rx_cfg.channels - 2;

	return 0;
}

static int proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	proxy_rx_cfg.channels = ucontrol->value.integer.value[0] + 2;
	pr_debug("%s: proxy_rx channels = %d\n",
		 __func__, proxy_rx_cfg.channels);

	return 1;
}

static int tdm_get_mode(struct snd_kcontrol *kcontrol)
{
	int mode;

	if (strnstr(kcontrol->id.name, "PRI",
		sizeof(kcontrol->id.name))) {
		mode = TDM_PRI;
	} else if (strnstr(kcontrol->id.name, "SEC",
		sizeof(kcontrol->id.name))) {
		mode = TDM_SEC;
	} else if (strnstr(kcontrol->id.name, "TERT",
		sizeof(kcontrol->id.name))) {
		mode = TDM_TERT;
	} else if (strnstr(kcontrol->id.name, "QUAT",
		sizeof(kcontrol->id.name))) {
		mode = TDM_QUAT;
	} else if (strnstr(kcontrol->id.name, "QUIN",
		sizeof(kcontrol->id.name))) {
		mode = TDM_QUIN;
	} else if (strnstr(kcontrol->id.name, "SEN",
		sizeof(kcontrol->id.name))) {
		mode = TDM_SEN;
	} else {
		pr_err("%s: unsupported mode in: %s",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}
	return mode;
}

static int tdm_get_port_idx(struct snd_kcontrol *kcontrol,
			    struct tdm_port *port)
{
	if (port) {
                port->mode = tdm_get_mode(kcontrol);
		if (port->mode < 0)
		    return port->mode;
		if (strnstr(kcontrol->id.name, "RX_0",
		    sizeof(kcontrol->id.name)) ||
		    strnstr(kcontrol->id.name, "TX_0",
		    sizeof(kcontrol->id.name))) {
			port->channel = TDM_0;
		} else if (strnstr(kcontrol->id.name, "RX_1",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_1",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_1;
		} else if (strnstr(kcontrol->id.name, "RX_2",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_2",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_2;
		} else if (strnstr(kcontrol->id.name, "RX_3",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_3",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_3;
		} else if (strnstr(kcontrol->id.name, "RX_4",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_4",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_4;
		} else if (strnstr(kcontrol->id.name, "RX_5",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_5",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_5;
		} else if (strnstr(kcontrol->id.name, "RX_6",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_6",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_6;
		} else if (strnstr(kcontrol->id.name, "RX_7",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_7",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_7;
		} else {
			pr_err("%s: unsupported channel in: %s\n",
				__func__, kcontrol->id.name);
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}
	return 0;
}

static int tdm_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 11:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 12:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int tdm_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	default:
		sample_rate_val = 6;
		break;
	}
	return sample_rate_val;
}

static int tdm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_sample_rate_val(
			tdm_rx_cfg[port.mode][port.channel].sample_rate);

		pr_debug("%s: tdm_rx_sample_rate = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].sample_rate =
			tdm_get_sample_rate(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_rx_sample_rate = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_sample_rate_val(
			tdm_tx_cfg[port.mode][port.channel].sample_rate);

		pr_debug("%s: tdm_tx_sample_rate = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].sample_rate =
			tdm_get_sample_rate(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_tx_sample_rate = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_get_format(int value)
{
	int format = 0;

	switch (value) {
	case 0:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return format;
}

static int tdm_get_format_val(int format)
{
	int value = 0;

	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 1;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 2;
		break;
	default:
		value = 0;
		break;
	}
	return value;
}

static int tdm_rx_format_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_format_val(
				tdm_rx_cfg[port.mode][port.channel].bit_format);

		pr_debug("%s: tdm_rx_bit_format = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_format_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].bit_format =
			tdm_get_format(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_rx_bit_format = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_format_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_format_val(
				tdm_tx_cfg[port.mode][port.channel].bit_format);

		pr_debug("%s: tdm_tx_bit_format = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_format_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].bit_format =
			tdm_get_format(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_tx_bit_format = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_ch_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {

		ucontrol->value.enumerated.item[0] =
			tdm_rx_cfg[port.mode][port.channel].channels - 1;

		pr_debug("%s: tdm_rx_ch = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].channels - 1,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_ch_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].channels =
			ucontrol->value.enumerated.item[0] + 1;

		pr_debug("%s: tdm_rx_ch = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].channels,
			 ucontrol->value.enumerated.item[0] + 1);
	}
	return ret;
}

static int tdm_tx_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] =
			tdm_tx_cfg[port.mode][port.channel].channels - 1;

		pr_debug("%s: tdm_tx_ch = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].channels - 1,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].channels =
			ucontrol->value.enumerated.item[0] + 1;

		pr_debug("%s: tdm_tx_ch = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].channels,
			 ucontrol->value.enumerated.item[0] + 1);
	}
	return ret;
}

static int tdm_get_slot_num_val(int slot_num)
{
	int slot_num_val;

	switch (slot_num) {
	case 1:
		slot_num_val = 0;
		break;
	case 2:
		slot_num_val = 1;
		break;
	case 4:
		slot_num_val = 2;
		break;
	case 8:
		slot_num_val = 3;
		break;
	case 16:
		slot_num_val = 4;
		break;
	case 32:
		slot_num_val = 5;
		break;
	default:
		slot_num_val = 5;
		break;
	}
	return slot_num_val;
}

static int tdm_get_slot_num(int value)
{
	int slot_num;

	switch (value) {
	case 0:
		slot_num = 1;
		break;
	case 1:
		slot_num = 2;
		break;
	case 2:
		slot_num = 4;
		break;
	case 3:
		slot_num = 8;
		break;
	case 4:
		slot_num = 16;
		break;
	case 5:
		slot_num = 32;
		break;
	default:
		slot_num = 8;
		break;
	}
	return slot_num;
}

static int tdm_slot_num_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	ucontrol->value.enumerated.item[0] =
		tdm_get_slot_num_val(tdm_slot[mode].num);
	pr_debug("%s: mode = %d, tdm_slot_num = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].num,
		ucontrol->value.enumerated.item[0]);
	return 0;
}

static int tdm_slot_num_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	tdm_slot[mode].num =
		tdm_get_slot_num(ucontrol->value.enumerated.item[0]);
	pr_debug("%s: mode = %d, tdm_slot_num = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].num,
		ucontrol->value.enumerated.item[0]);
	return 0;
}

static int tdm_get_slot_width_val(int slot_width)
{
	int slot_width_val;

	switch (slot_width) {
	case 16:
		slot_width_val = 0;
		break;
	case 24:
		slot_width_val = 1;
		break;
	case 32:
		slot_width_val = 2;
		break;
	default:
		slot_width_val = 2;
		break;
	}
	return slot_width_val;
}

static int tdm_slot_width_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	ucontrol->value.enumerated.item[0] =
		tdm_get_slot_width_val(tdm_slot[mode].width);
	pr_debug("%s: mode = %d, tdm_slot_width = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].width,
		ucontrol->value.enumerated.item[0]);
	return 0;
}

static int tdm_get_slot_width(int value)
{
	int slot_width;

	switch (value) {
	case 0:
		slot_width = 16;
		break;
	case 1:
		slot_width = 24;
		break;
	case 2:
		slot_width = 32;
		break;
	default:
		slot_width = 32;
		break;
	}
	return slot_width;
}

static int tdm_slot_width_put(struct snd_kcontrol *kcontrol,
struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	tdm_slot[mode].width =
		tdm_get_slot_width(ucontrol->value.enumerated.item[0]);
	pr_debug("%s: mode = %d, tdm_slot_width = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].width,
		ucontrol->value.enumerated.item[0]);
	return 0;
}

static int tdm_rx_slot_mapping_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	slot_offset = tdm_rx_slot_offset[mode];
	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		ucontrol->value.integer.value[i] = slot_offset[i];
		pr_debug("%s: offset %d, value %d\n",
			__func__, i, slot_offset[i]);
	}
	return 0;
}

static int tdm_rx_slot_mapping_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	slot_offset = tdm_rx_slot_offset[mode];
	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		slot_offset[i] = ucontrol->value.integer.value[i];
		pr_debug("%s: offset %d, value %d\n",
			__func__, i, slot_offset[i]);
	}
	return 0;
}
static int tdm_tx_slot_mapping_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	slot_offset = tdm_tx_slot_offset[mode];
	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		ucontrol->value.integer.value[i] = slot_offset[i];
		pr_debug("%s: offset %d, value %d\n",
			__func__, i, slot_offset[i]);
	}
	return 0;
}

static int tdm_tx_slot_mapping_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}
	slot_offset = tdm_tx_slot_offset[mode];
	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		slot_offset[i] = ucontrol->value.integer.value[i];
		pr_debug("%s: offset %d, value %d\n",
			__func__, i, slot_offset[i]);
	}
	return 0;
}

static int tdm_slot_map_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int slot_index = 0;
	int interface = ucontrol->value.integer.value[0];
	int channel = ucontrol->value.integer.value[1];
	unsigned int offset_val = 0;
	unsigned int *slot_offset = NULL;
	struct tdm_dev_config *config = NULL;
	unsigned int max_slot_offset = 0;
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_component *component = NULL;

	if (interface < 0  || interface >= (TDM_INTERFACE_MAX * MAX_PATH)) {
		pr_err("%s: incorrect interface = %d\n", __func__, interface);
		return -EINVAL;
	}
	if (channel < 0  || channel >= TDM_PORT_MAX) {
		pr_err("%s: incorrect channel = %d\n", __func__, channel);
		return -EINVAL;
	}

	pr_debug("%s: interface = %d, channel = %d\n", __func__,
		interface, channel);

	component = snd_soc_kcontrol_component(kcontrol);
	pdata = snd_soc_card_get_drvdata(component->card);
	config = ((struct tdm_dev_config *) tdm_cfg[interface / MAX_PATH]) +
			((interface % MAX_PATH) * TDM_PORT_MAX) + channel;
	if (!config) {
		pr_err("%s: tdm config is NULL\n", __func__);
		return -EINVAL;
	}
	slot_offset = config->tdm_slot_offset;
	if (!slot_offset) {
		pr_err("%s: slot offset is NULL\n", __func__);
		return -EINVAL;
	}

	max_slot_offset = TDM_SLOT_WIDTH_BYTES * (pdata->tdm_max_slots - 1);

	for (slot_index = 0; slot_index < pdata->tdm_max_slots; slot_index++) {
		offset_val = ucontrol->value.integer.value[MAX_PATH +
				slot_index];
		/* Offset value can only be 0, 4, 8, .. */
		if (offset_val % 4 == 0 && offset_val <= max_slot_offset)
			slot_offset[slot_index] = offset_val;
		pr_debug("%s: slot offset[%d] = %d\n", __func__,
			slot_index, slot_offset[slot_index]);
	}

	return 0;
}

static int aux_pcm_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx = 0;

	if (strnstr(kcontrol->id.name, "PRIM_AUX_PCM",
		    sizeof("PRIM_AUX_PCM"))) {
		idx = PRIM_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "SEC_AUX_PCM",
			 sizeof("SEC_AUX_PCM"))) {
		idx = SEC_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "TERT_AUX_PCM",
			 sizeof("TERT_AUX_PCM"))) {
		idx = TERT_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "QUAT_AUX_PCM",
			 sizeof("QUAT_AUX_PCM"))) {
		idx = QUAT_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "QUIN_AUX_PCM",
			 sizeof("QUIN_AUX_PCM"))) {
		idx = QUIN_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "SEN_AUX_PCM",
			 sizeof("SEN_AUX_PCM"))) {
		idx = SEN_AUX_PCM;
	} else {
		pr_err("%s: unsupported port: %s\n",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int aux_pcm_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 0:
	default:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return sample_rate;
}

static int aux_pcm_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		sample_rate_val = 0;
		break;
	}
	return sample_rate_val;
}

static int mi2s_auxpcm_get_format(int value)
{
	int format = 0;

	switch (value) {
	case 0:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 3:
		format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return format;
}

static int mi2s_auxpcm_get_format_value(int format)
{
	int value = 0;

	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 1;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		value = 2;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 3;
		break;
	default:
		value = 0;
		break;
	}
	return value;
}

static int aux_pcm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
	     aux_pcm_get_sample_rate_val(aux_pcm_rx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_rx_cfg[idx].sample_rate =
		aux_pcm_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
	     aux_pcm_get_sample_rate_val(aux_pcm_tx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_tx_cfg[idx].sample_rate =
		aux_pcm_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_rx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_auxpcm_get_format_value(aux_pcm_rx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		idx, aux_pcm_rx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_rx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_rx_cfg[idx].bit_format =
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		  idx, aux_pcm_rx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_tx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_auxpcm_get_format_value(aux_pcm_tx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		idx, aux_pcm_tx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_tx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_tx_cfg[idx].bit_format =
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		  idx, aux_pcm_tx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx = 0;

	if (strnstr(kcontrol->id.name, "PRIM_MI2S_RX",
	    sizeof("PRIM_MI2S_RX"))) {
		idx = PRIM_MI2S;
	} else if (strnstr(kcontrol->id.name, "SEC_MI2S_RX",
		 sizeof("SEC_MI2S_RX"))) {
		idx = SEC_MI2S;
	} else if (strnstr(kcontrol->id.name, "TERT_MI2S_RX",
		 sizeof("TERT_MI2S_RX"))) {
		idx = TERT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUAT_MI2S_RX",
		 sizeof("QUAT_MI2S_RX"))) {
		idx = QUAT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUIN_MI2S_RX",
		 sizeof("QUIN_MI2S_RX"))) {
		idx = QUIN_MI2S;
	} else if (strnstr(kcontrol->id.name, "SEN_MI2S_RX",
		 sizeof("SEN_MI2S_RX"))) {
		idx = SEN_MI2S;
	} else if (strnstr(kcontrol->id.name, "PRIM_MI2S_TX",
		 sizeof("PRIM_MI2S_TX"))) {
		idx = PRIM_MI2S;
	} else if (strnstr(kcontrol->id.name, "SEC_MI2S_TX",
		 sizeof("SEC_MI2S_TX"))) {
		idx = SEC_MI2S;
	} else if (strnstr(kcontrol->id.name, "TERT_MI2S_TX",
		 sizeof("TERT_MI2S_TX"))) {
		idx = TERT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUAT_MI2S_TX",
		 sizeof("QUAT_MI2S_TX"))) {
		idx = QUAT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUIN_MI2S_TX",
		 sizeof("QUIN_MI2S_TX"))) {
		idx = QUIN_MI2S;
	} else if (strnstr(kcontrol->id.name, "SEN_MI2S_TX",
		 sizeof("SEN_MI2S_TX"))) {
		idx = SEN_MI2S;
	} else {
		pr_err("%s: unsupported channel: %s\n",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int mi2s_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 11:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 12:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int mi2s_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	default:
		sample_rate_val = 6;
		break;
	}
	return sample_rate_val;
}

static int mi2s_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_sample_rate_val(mi2s_rx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].sample_rate =
		mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_sample_rate_val(mi2s_tx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].sample_rate =
		mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_rx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_auxpcm_get_format_value(mi2s_rx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		idx, mi2s_rx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_rx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].bit_format =
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		  idx, mi2s_rx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_tx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_auxpcm_get_format_value(mi2s_tx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		idx, mi2s_tx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_tx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].bit_format =
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		  idx, mi2s_tx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}
static int msm_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: msm_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = mi2s_rx_cfg[idx].channels - 1;

	return 0;
}

static int msm_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].channels);

	return 1;
}

static int msm_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: msm_mi2s_[%d]_tx_ch  = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = mi2s_tx_cfg[idx].channels - 1;

	return 0;
}

static int msm_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_mi2s_[%d]_tx_ch  = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].channels);

	return 1;
}

static int msm_get_port_id(int be_id)
{
	int afe_port_id = 0;

	switch (be_id) {
	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_PRI_MI2S_TX:
		afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_SECONDARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_TERTIARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_QUINARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_QUINARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_SENARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_SENARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_SENARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_SENARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_0:
		afe_port_id = AFE_PORT_ID_VA_CODEC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_1:
		afe_port_id = AFE_PORT_ID_VA_CODEC_DMA_TX_1;
		break;
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_2:
		afe_port_id = AFE_PORT_ID_VA_CODEC_DMA_TX_2;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
		afe_port_id = AFE_PORT_ID_WSA_CODEC_DMA_RX_0;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		afe_port_id = AFE_PORT_ID_WSA_CODEC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
		afe_port_id = AFE_PORT_ID_WSA_CODEC_DMA_RX_1;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
		afe_port_id = AFE_PORT_ID_WSA_CODEC_DMA_TX_1;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
		afe_port_id = AFE_PORT_ID_WSA_CODEC_DMA_TX_2;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_0;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
		afe_port_id = AFE_PORT_ID_TX_CODEC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_1;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_1:
		afe_port_id = AFE_PORT_ID_TX_CODEC_DMA_TX_1;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_2;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_2:
		afe_port_id = AFE_PORT_ID_TX_CODEC_DMA_TX_2;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_3:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_3;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
		afe_port_id = AFE_PORT_ID_TX_CODEC_DMA_TX_3;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_4:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_4;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
		afe_port_id = AFE_PORT_ID_TX_CODEC_DMA_TX_4;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_5:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_5;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_5:
		afe_port_id = AFE_PORT_ID_TX_CODEC_DMA_TX_5;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_6:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_6;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_7:
		afe_port_id = AFE_PORT_ID_RX_CODEC_DMA_RX_7;
		break;
	default:
		pr_err("%s: Invalid BE id: %d\n", __func__, be_id);
		afe_port_id = -EINVAL;
	}

	return afe_port_id;
}

static u32 get_mi2s_bits_per_sample(u32 bit_format)
{
	u32 bit_per_sample = 0;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		bit_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bit_per_sample = 16;
		break;
	}

	return bit_per_sample;
}

static int msm_mi2s_set_sclk(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int port_id = 0;
	/* Rx and Tx DAIs should use same clk index */
	int index = (cpu_dai->id) / 2;

	port_id = msm_get_port_id(rtd->dai_link->id);
	if (port_id < 0) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto err;
	}

	mi2s_clk[index].enable = enable;
	ret = afe_set_lpass_clock_v2(port_id,
				     &mi2s_clk[index]);
	if (ret < 0) {
		dev_err(rtd->card->dev,
			"%s: afe lpass clock failed for port 0x%x , err:%d\n",
			__func__, port_id, ret);
		goto err;
	}

err:
	return ret;
}

static int lahaina_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	if (cpu_dai->id == AFE_PORT_ID_PRIMARY_TDM_RX) {
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_PRIMARY_TDM_TX) {
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_SECONDARY_TDM_RX) {
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_SECONDARY_TDM_TX) {
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_TERTIARY_TDM_RX) {
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_TERTIARY_TDM_TX) {
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_QUATERNARY_TDM_RX) {
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_QUATERNARY_TDM_TX) {
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_QUINARY_TDM_RX) {
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUIN][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_QUINARY_TDM_TX) {
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUIN][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_SENARY_TDM_RX) {
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_SEN][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEN][TDM_0].sample_rate;
	} else if (cpu_dai->id == AFE_PORT_ID_SENARY_TDM_TX) {
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEN][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEN][TDM_0].sample_rate;
	} else {
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	pr_debug("%s: dai id = 0x%x channels = %d rate = %d format = 0x%x\n",
		__func__, cpu_dai->id, channels->max, rate->max,
		params_format(params));

	return 0;
}

static int cdc_dma_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx = 0;

	if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_RX_0",
		sizeof("WSA_CDC_DMA_RX_0")))
		idx = WSA_CDC_DMA_RX_0;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_RX_1",
		sizeof("WSA_CDC_DMA_RX_0")))
		idx = WSA_CDC_DMA_RX_1;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_0",
		sizeof("RX_CDC_DMA_RX_0")))
		idx = RX_CDC_DMA_RX_0;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_1",
		sizeof("RX_CDC_DMA_RX_1")))
		idx = RX_CDC_DMA_RX_1;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_2",
		sizeof("RX_CDC_DMA_RX_2")))
		idx = RX_CDC_DMA_RX_2;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_3",
		sizeof("RX_CDC_DMA_RX_3")))
		idx = RX_CDC_DMA_RX_3;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_5",
		sizeof("RX_CDC_DMA_RX_5")))
		idx = RX_CDC_DMA_RX_5;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_6",
		sizeof("RX_CDC_DMA_RX_6")))
		idx = RX_CDC_DMA_RX_6;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_TX_0",
		sizeof("WSA_CDC_DMA_TX_0")))
		idx = WSA_CDC_DMA_TX_0;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_TX_1",
		sizeof("WSA_CDC_DMA_TX_1")))
		idx = WSA_CDC_DMA_TX_1;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_TX_2",
		sizeof("WSA_CDC_DMA_TX_2")))
		idx = WSA_CDC_DMA_TX_2;
	else if (strnstr(kcontrol->id.name, "TX_CDC_DMA_TX_0",
		sizeof("TX_CDC_DMA_TX_0")))
		idx = TX_CDC_DMA_TX_0;
	else if (strnstr(kcontrol->id.name, "TX_CDC_DMA_TX_3",
		sizeof("TX_CDC_DMA_TX_3")))
		idx = TX_CDC_DMA_TX_3;
	else if (strnstr(kcontrol->id.name, "TX_CDC_DMA_TX_4",
		sizeof("TX_CDC_DMA_TX_4")))
		idx = TX_CDC_DMA_TX_4;
	else if (strnstr(kcontrol->id.name, "VA_CDC_DMA_TX_0",
		sizeof("VA_CDC_DMA_TX_0")))
		idx = VA_CDC_DMA_TX_0;
	else if (strnstr(kcontrol->id.name, "VA_CDC_DMA_TX_1",
		sizeof("VA_CDC_DMA_TX_1")))
		idx = VA_CDC_DMA_TX_1;
	else if (strnstr(kcontrol->id.name, "VA_CDC_DMA_TX_2",
		sizeof("VA_CDC_DMA_TX_2")))
		idx = VA_CDC_DMA_TX_2;
	else {
		pr_err("%s: unsupported channel: %s\n",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return idx;
}

static int cdc_dma_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0 || ch_num >= CDC_DMA_RX_MAX) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	pr_debug("%s: cdc_dma_rx_ch  = %d\n", __func__,
		 cdc_dma_rx_cfg[ch_num].channels - 1);
	ucontrol->value.integer.value[0] = cdc_dma_rx_cfg[ch_num].channels - 1;
	return 0;
}

static int cdc_dma_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0 || ch_num >= CDC_DMA_RX_MAX) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	cdc_dma_rx_cfg[ch_num].channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: cdc_dma_rx_ch = %d\n", __func__,
		cdc_dma_rx_cfg[ch_num].channels);
	return 1;
}

static int cdc_dma_rx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0 || ch_num >= CDC_DMA_RX_MAX) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (cdc_dma_rx_cfg[ch_num].bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: cdc_dma_rx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_rx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int cdc_dma_rx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0 || ch_num >= CDC_DMA_RX_MAX) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: cdc_dma_rx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_rx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}


static int cdc_dma_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	default:
		sample_rate_val = 6;
		break;
	}
	return sample_rate_val;
}

static int cdc_dma_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 11:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 12:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int cdc_dma_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0 || ch_num >= CDC_DMA_RX_MAX) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	ucontrol->value.enumerated.item[0] =
		cdc_dma_get_sample_rate_val(cdc_dma_rx_cfg[ch_num].sample_rate);

	pr_debug("%s: cdc_dma_rx_sample_rate = %d\n", __func__,
		 cdc_dma_rx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0 || ch_num >= CDC_DMA_RX_MAX) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	cdc_dma_rx_cfg[ch_num].sample_rate =
		cdc_dma_get_sample_rate(ucontrol->value.enumerated.item[0]);


	pr_debug("%s: control value = %d, cdc_dma_rx_sample_rate = %d\n",
		__func__, ucontrol->value.enumerated.item[0],
		cdc_dma_rx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_tx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	pr_debug("%s: cdc_dma_tx_ch  = %d\n", __func__,
		 cdc_dma_tx_cfg[ch_num].channels);
	ucontrol->value.integer.value[0] = cdc_dma_tx_cfg[ch_num].channels - 1;
	return 0;
}

static int cdc_dma_tx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	cdc_dma_tx_cfg[ch_num].channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: cdc_dma_tx_ch = %d\n", __func__,
		cdc_dma_tx_cfg[ch_num].channels);
	return 1;
}

static int cdc_dma_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val;
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (cdc_dma_tx_cfg[ch_num].sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	default:
		sample_rate_val = 6;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: cdc_dma_tx_sample_rate = %d\n", __func__,
		 cdc_dma_tx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (ucontrol->value.integer.value[0]) {
	case 12:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, cdc_dma_tx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		cdc_dma_tx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_tx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (cdc_dma_tx_cfg[ch_num].bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: cdc_dma_tx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_tx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int cdc_dma_tx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: cdc_dma_tx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_tx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int msm_cdc_dma_get_idx_from_beid(int32_t be_id)
{
	int idx = 0;

	switch (be_id) {
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
		idx = WSA_CDC_DMA_RX_0;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		idx = WSA_CDC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
		idx = WSA_CDC_DMA_RX_1;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
		idx = WSA_CDC_DMA_TX_1;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
		idx = WSA_CDC_DMA_TX_2;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
		idx = RX_CDC_DMA_RX_0;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
		idx = RX_CDC_DMA_RX_1;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
		idx = RX_CDC_DMA_RX_2;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_3:
		idx = RX_CDC_DMA_RX_3;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_5:
		idx = RX_CDC_DMA_RX_5;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_6:
		idx = RX_CDC_DMA_RX_6;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
		idx = TX_CDC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
		idx = TX_CDC_DMA_TX_3;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
		idx = TX_CDC_DMA_TX_4;
		break;
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_0:
		idx = VA_CDC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_1:
		idx = VA_CDC_DMA_TX_1;
		break;
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_2:
		idx = VA_CDC_DMA_TX_2;
		break;
	default:
		idx = RX_CDC_DMA_RX_0;
		break;
	}

	return idx;
}

static int msm_bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * Slimbus_7_Rx/Tx sample rate values should always be in sync (same)
	 * when used for BT_SCO use case. Return either Rx or Tx sample rate
	 * value.
	 */
	switch (slim_rx_cfg[SLIM_RX_7].sample_rate) {
	case SAMPLING_RATE_96KHZ:
		ucontrol->value.integer.value[0] = 5;
		break;
	case SAMPLING_RATE_88P2KHZ:
		ucontrol->value.integer.value[0] = 4;
		break;
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SAMPLING_RATE_44P1KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d\n", __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_16KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 5:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_96KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_rx = %d, slim7_tx = %d, value = %d\n",
		 __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate,
		 slim_tx_cfg[SLIM_TX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_bt_sample_rate_rx_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (slim_rx_cfg[SLIM_RX_7].sample_rate) {
	case SAMPLING_RATE_96KHZ:
		ucontrol->value.integer.value[0] = 5;
		break;
	case SAMPLING_RATE_88P2KHZ:
		ucontrol->value.integer.value[0] = 4;
		break;
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SAMPLING_RATE_44P1KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate rx = %d\n", __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_rx_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 5:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rate: slim7_rx = %d, value = %d\n",
		 __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_bt_sample_rate_tx_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (slim_tx_cfg[SLIM_TX_7].sample_rate) {
	case SAMPLING_RATE_96KHZ:
		ucontrol->value.integer.value[0] = 5;
		break;
	case SAMPLING_RATE_88P2KHZ:
		ucontrol->value.integer.value[0] = 4;
		break;
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SAMPLING_RATE_44P1KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate tx = %d\n", __func__,
		 slim_tx_cfg[SLIM_TX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_tx_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 5:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rate: slim7_tx = %d, value = %d\n",
		 __func__,
		 slim_tx_cfg[SLIM_TX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static const struct snd_kcontrol_new msm_int_snd_controls[] = {
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_0 Channels", wsa_cdc_dma_rx_0_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_1 Channels", wsa_cdc_dma_rx_1_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Channels", rx_cdc_dma_rx_0_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Channels", rx_cdc_dma_rx_1_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Channels", rx_cdc_dma_rx_2_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Channels", rx_cdc_dma_rx_3_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Channels", rx_cdc_dma_rx_5_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_6 Channels", rx_cdc_dma_rx_6_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_0 Channels", wsa_cdc_dma_tx_0_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_1 Channels", wsa_cdc_dma_tx_1_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_2 Channels", wsa_cdc_dma_tx_2_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_0 Channels", tx_cdc_dma_tx_0_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_3 Channels", tx_cdc_dma_tx_3_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_4 Channels", tx_cdc_dma_tx_4_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_0 Channels", va_cdc_dma_tx_0_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_1 Channels", va_cdc_dma_tx_1_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_2 Channels", va_cdc_dma_tx_2_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_0 Format", wsa_cdc_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_1 Format", wsa_cdc_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_1 Format", wsa_cdc_dma_tx_1_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_2 Format", wsa_cdc_dma_tx_2_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_0 Format", tx_cdc_dma_tx_0_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_3 Format", tx_cdc_dma_tx_3_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_4 Format", tx_cdc_dma_tx_4_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_0 Format", va_cdc_dma_tx_0_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_1 Format", va_cdc_dma_tx_1_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_2 Format", va_cdc_dma_tx_2_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_0 SampleRate",
			wsa_cdc_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_1 SampleRate",
			wsa_cdc_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_0 SampleRate",
			wsa_cdc_dma_tx_0_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_1 SampleRate",
			wsa_cdc_dma_tx_1_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_2 SampleRate",
			wsa_cdc_dma_tx_2_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_0 SampleRate",
			tx_cdc_dma_tx_0_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_3 SampleRate",
			tx_cdc_dma_tx_3_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_4 SampleRate",
			tx_cdc_dma_tx_4_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_0 SampleRate",
			va_cdc_dma_tx_0_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_1 SampleRate",
			va_cdc_dma_tx_1_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("VA_CDC_DMA_TX_2 SampleRate",
			va_cdc_dma_tx_2_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
};

static const struct snd_kcontrol_new msm_int_wcd9370_snd_controls[] = {
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Format", rx_cdc70_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Format", rx_cdc70_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Format", rx_cdc70_dma_rx_2_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Format", rx_cdc70_dma_rx_3_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Format", rx_cdc70_dma_rx_5_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 SampleRate",
			rx_cdc70_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 SampleRate",
			rx_cdc70_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 SampleRate",
			rx_cdc70_dma_rx_2_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 SampleRate",
			rx_cdc70_dma_rx_3_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 SampleRate",
			rx_cdc70_dma_rx_5_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
};

static const struct snd_kcontrol_new msm_int_wcd9375_snd_controls[] = {
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Format", rx_cdc75_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Format", rx_cdc75_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Format", rx_cdc75_dma_rx_2_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Format", rx_cdc75_dma_rx_3_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Format", rx_cdc75_dma_rx_5_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 SampleRate",
			rx_cdc75_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 SampleRate",
			rx_cdc75_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 SampleRate",
			rx_cdc75_dma_rx_2_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 SampleRate",
			rx_cdc75_dma_rx_3_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 SampleRate",
			rx_cdc75_dma_rx_5_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
};

static const struct snd_kcontrol_new msm_int_wcd9380_snd_controls[] = {
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Format", rx_cdc80_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Format", rx_cdc80_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Format", rx_cdc80_dma_rx_2_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Format", rx_cdc80_dma_rx_3_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Format", rx_cdc80_dma_rx_5_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_6 Format", rx_cdc80_dma_rx_6_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 SampleRate",
			rx_cdc80_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 SampleRate",
			rx_cdc80_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 SampleRate",
			rx_cdc80_dma_rx_2_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 SampleRate",
			rx_cdc80_dma_rx_3_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 SampleRate",
			rx_cdc80_dma_rx_5_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_6 SampleRate",
			rx_cdc80_dma_rx_6_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
};

static const struct snd_kcontrol_new msm_int_wcd9385_snd_controls[] = {
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Format", rx_cdc85_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Format", rx_cdc85_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Format", rx_cdc85_dma_rx_2_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Format", rx_cdc85_dma_rx_3_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Format", rx_cdc85_dma_rx_5_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_6 Format", rx_cdc85_dma_rx_6_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 SampleRate",
			rx_cdc85_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 SampleRate",
			rx_cdc85_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 SampleRate",
			rx_cdc85_dma_rx_2_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 SampleRate",
			rx_cdc85_dma_rx_3_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 SampleRate",
			rx_cdc85_dma_rx_5_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_6 SampleRate",
			rx_cdc85_dma_rx_6_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
};

static const struct snd_kcontrol_new msm_common_snd_controls[] = {
	SOC_ENUM_EXT("USB_AUDIO_RX SampleRate", usb_rx_sample_rate,
			usb_audio_rx_sample_rate_get,
			usb_audio_rx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_TX SampleRate", usb_tx_sample_rate,
			usb_audio_tx_sample_rate_get,
			usb_audio_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEN_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEN_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX SampleRate", prim_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_RX SampleRate", sec_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_RX SampleRate", tert_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_RX SampleRate", quat_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_AUX_PCM_RX SampleRate", quin_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEN_AUX_PCM_RX SampleRate", sen_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX SampleRate", prim_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_TX SampleRate", sec_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_TX SampleRate", tert_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_TX SampleRate", quat_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_AUX_PCM_TX SampleRate", quin_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEN_AUX_PCM_TX SampleRate", sen_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_MI2S_RX SampleRate", prim_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_RX SampleRate", sec_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_MI2S_RX SampleRate", tert_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX SampleRate", quat_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_MI2S_RX SampleRate", quin_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("SEN_MI2S_RX SampleRate", sen_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_MI2S_TX SampleRate", prim_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_TX SampleRate", sec_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_MI2S_TX SampleRate", tert_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX SampleRate", quat_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_MI2S_TX SampleRate", quin_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("SEN_MI2S_TX SampleRate", sen_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Format", usb_rx_format,
			usb_audio_rx_format_get, usb_audio_rx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Format", usb_tx_format,
			usb_audio_tx_format_get, usb_audio_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEN_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEN_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("QUIN_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("SEN_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("QUIN_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("SEN_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("PRIM_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("TERT_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("QUIN_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("SEN_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("PRIM_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("TERT_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("QUIN_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("SEN_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Channels", usb_rx_chs,
			usb_audio_rx_ch_get, usb_audio_rx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Channels", usb_tx_chs,
			usb_audio_tx_ch_get, usb_audio_tx_ch_put),
	SOC_ENUM_EXT("PROXY_RX Channels", proxy_rx_chs,
			proxy_rx_ch_get, proxy_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEN_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEN_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("PRI_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("SEC_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("TERT_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("QUAT_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("QUIN_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("SEN_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("PRI_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("SEC_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("TERT_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("QUAT_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("QUIN_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("SEN_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("PRIM_MI2S_RX Channels", prim_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Channels", sec_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("TERT_MI2S_RX Channels", tert_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Channels", quat_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUIN_MI2S_RX Channels", quin_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("SEN_MI2S_RX Channels", sen_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("PRIM_MI2S_TX Channels", prim_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Channels", sec_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("TERT_MI2S_TX Channels", tert_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Channels", quat_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("QUIN_MI2S_TX Channels", quin_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("SEN_MI2S_TX Channels", sen_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("Display Port RX Channels", ext_disp_rx_chs,
			ext_disp_rx_ch_get, ext_disp_rx_ch_put),
	SOC_ENUM_EXT("Display Port RX Bit Format", ext_disp_rx_format,
			ext_disp_rx_format_get, ext_disp_rx_format_put),
	SOC_ENUM_EXT("Display Port RX SampleRate", ext_disp_rx_sample_rate,
			ext_disp_rx_sample_rate_get,
			ext_disp_rx_sample_rate_put),
	SOC_ENUM_EXT("Display Port1 RX Channels", ext_disp_rx_chs,
			ext_disp_rx_ch_get, ext_disp_rx_ch_put),
	SOC_ENUM_EXT("Display Port1 RX Bit Format", ext_disp_rx_format,
			ext_disp_rx_format_get, ext_disp_rx_format_put),
	SOC_ENUM_EXT("Display Port1 RX SampleRate", ext_disp_rx_sample_rate,
			ext_disp_rx_sample_rate_get,
			ext_disp_rx_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate", bt_sample_rate,
			msm_bt_sample_rate_get,
			msm_bt_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate RX", bt_sample_rate_rx,
			msm_bt_sample_rate_rx_get,
			msm_bt_sample_rate_rx_put),
	SOC_ENUM_EXT("BT SampleRate TX", bt_sample_rate_tx,
			msm_bt_sample_rate_tx_get,
			msm_bt_sample_rate_tx_put),
	SOC_ENUM_EXT("AFE_LOOPBACK_TX Channels", afe_loopback_tx_chs,
			afe_loopback_tx_ch_get, afe_loopback_tx_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", vi_feed_tx_chs,
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_SINGLE_MULTI_EXT("TDM Slot Map", SND_SOC_NOPM, 0, 255, 0,
			TDM_MAX_SLOTS + MAX_PATH, NULL, tdm_slot_map_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_TX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_RX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEN_TDM_TX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEN_TDM_RX SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX SampleRate", prim_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX SampleRate", prim_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
};

static int msm_ext_mclk_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ext_mclk_enable;
	return 0;
}

static int msm_ext_mclk_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ext_mclk_enable = (bool) ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new msm_ext_mclk_controls[] = {
	SOC_SINGLE_BOOL_EXT("EXT MCLK Enable", 0,
			    msm_ext_mclk_get, msm_ext_mclk_put),
};

static int msm_ext_disp_get_idx_from_beid(int32_t be_id)
{
	int idx;

	switch (be_id) {
	case MSM_BACKEND_DAI_DISPLAY_PORT_RX:
		idx = EXT_DISP_RX_IDX_DP;
		break;
	case MSM_BACKEND_DAI_DISPLAY_PORT_RX_1:
		idx = EXT_DISP_RX_IDX_DP1;
		break;
	default:
		pr_err("%s: Incorrect ext_disp BE id %d\n", __func__, be_id);
		idx = -EINVAL;
		break;
	}

	return idx;
}

static int lahaina_send_island_va_config(int32_t be_id)
{
	int rc = 0;
	int port_id = 0xFFFF;

	port_id = msm_get_port_id(be_id);
	if (port_id < 0) {
		pr_err("%s: Invalid island interface, be_id: %d\n",
		       __func__, be_id);
		rc = -EINVAL;
	} else {
		/*
		 * send island mode config
		 * This should be the first configuration
		 */
		rc = afe_send_port_island_mode(port_id);
		if (rc)
			pr_err("%s: afe send island mode failed %d\n",
				__func__, rc);
	}

	return rc;
}

static int lahaina_send_power_mode(int32_t be_id)
{
	int rc = 0;
	int port_id = 0xFFFF;

	port_id = msm_get_port_id(be_id);
	if (port_id < 0) {
		pr_err("%s: Invalid power interface, be_id: %d\n",
			__func__, be_id);
		rc = -EINVAL;
	} else {
		/*
		 * send island mode config
		 * This should be the first configuration
		 *
		 */
		rc = afe_send_port_island_mode(port_id);
		if (rc)
			pr_err("%s: afe send island mode failed %d\n",
				__func__, rc);
		/*
		 * send power mode config
		 * This should be set after island configuration
		 */
		rc = afe_send_port_power_mode(port_id);
		if (rc)
			pr_err("%s: afe send power mode failed %d\n",
				__func__, rc);
	}
	return rc;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	int idx = 0, rc = 0;

	pr_debug("%s: dai_id= %d, format = %d, rate = %d\n",
		  __func__, dai_link->id, params_format(params),
		  params_rate(params));

	switch (dai_link->id) {
	case MSM_BACKEND_DAI_USB_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				usb_rx_cfg.bit_format);
		rate->min = rate->max = usb_rx_cfg.sample_rate;
		channels->min = channels->max = usb_rx_cfg.channels;
		break;

	case MSM_BACKEND_DAI_USB_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				usb_tx_cfg.bit_format);
		rate->min = rate->max = usb_tx_cfg.sample_rate;
		channels->min = channels->max = usb_tx_cfg.channels;
		break;

	case MSM_BACKEND_DAI_DISPLAY_PORT_RX:
	case MSM_BACKEND_DAI_DISPLAY_PORT_RX_1:
		idx = msm_ext_disp_get_idx_from_beid(dai_link->id);
		if (idx < 0) {
			pr_err("%s: Incorrect ext disp idx %d\n",
			       __func__, idx);
			rc = idx;
			goto done;
		}

		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				ext_disp_rx_cfg[idx].bit_format);
		rate->min = rate->max = ext_disp_rx_cfg[idx].sample_rate;
		channels->min = channels->max = ext_disp_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_AFE_PCM_RX:
		channels->min = channels->max = proxy_rx_cfg.channels;
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;

	case MSM_BACKEND_DAI_PRI_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_PRI_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEC_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEC_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_TERT_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_TERT_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUAT_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUAT_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUIN_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_QUIN][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUIN_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_QUIN][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEN_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_SEN][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_SEN][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEN_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_SEN][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_SEN][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[PRIM_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[PRIM_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[SEC_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[SEC_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[TERT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[TERT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[QUAT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[QUAT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUIN_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[QUIN_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[QUIN_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[QUIN_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUIN_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[QUIN_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[QUIN_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[QUIN_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEN_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[SEN_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[SEN_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[SEN_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEN_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[SEN_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[SEN_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[SEN_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[PRIM_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[PRIM_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[PRIM_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_PRI_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[PRIM_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[PRIM_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[PRIM_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[SEC_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[SEC_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[SEC_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SECONDARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[SEC_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[SEC_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[SEC_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_TERTIARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[TERT_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[TERT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[TERT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[TERT_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[TERT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[TERT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[QUAT_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[QUAT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[QUAT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[QUAT_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[QUAT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[QUAT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[QUIN_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[QUIN_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[QUIN_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[QUIN_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[QUIN_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[QUIN_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SENARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[SEN_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[SEN_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[SEN_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SENARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[SEN_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[SEN_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[SEN_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_3:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_5:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_6:
		idx = msm_cdc_dma_get_idx_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				cdc_dma_rx_cfg[idx].bit_format);
		rate->min = rate->max = cdc_dma_rx_cfg[idx].sample_rate;
		channels->min = channels->max = cdc_dma_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_0:
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_1:
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_2:
		idx = msm_cdc_dma_get_idx_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				cdc_dma_tx_cfg[idx].bit_format);
		rate->min = rate->max = cdc_dma_tx_cfg[idx].sample_rate;
		channels->min = channels->max = cdc_dma_tx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		idx = msm_cdc_dma_get_idx_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				SNDRV_PCM_FORMAT_S32_LE);
		rate->min = rate->max = cdc_dma_tx_cfg[idx].sample_rate;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[SLIM_RX_7].bit_format);
		rate->min = rate->max = slim_rx_cfg[SLIM_RX_7].sample_rate;
		channels->min = channels->max =
			slim_rx_cfg[SLIM_RX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[SLIM_TX_7].bit_format);
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_7].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_8_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_8].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_8].channels;
		break;

	case MSM_BACKEND_DAI_AFE_LOOPBACK_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				afe_loopback_tx_cfg[idx].bit_format);
		rate->min = rate->max = afe_loopback_tx_cfg[idx].sample_rate;
		channels->min = channels->max =
				afe_loopback_tx_cfg[idx].channels;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}

done:
	return rc;
}

static bool msm_usbc_swap_gnd_mic(struct snd_soc_component *component, bool active)
{
	struct snd_soc_card *card = component->card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	if (!pdata->fsa_handle)
		return false;

	return fsa4480_switch_event(pdata->fsa_handle, FSA_MIC_GND_SWAP);
}

static bool msm_swap_gnd_mic(struct snd_soc_component *component, bool active)
{
	int value = 0;
	bool ret = false;
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;

	if (!component) {
		pr_err("%s component is NULL\n", __func__);
		return false;
	}
	card = component->card;
	pdata = snd_soc_card_get_drvdata(card);

	if (!pdata)
		return false;

	if (wcd_mbhc_cfg.enable_usbc_analog)
		return msm_usbc_swap_gnd_mic(component, active);

	/* if usbc is not defined, swap using us_euro_gpio_p */
	if (pdata->us_euro_gpio_p) {
		value = msm_cdc_pinctrl_get_state(
				pdata->us_euro_gpio_p);
		if (value)
			msm_cdc_pinctrl_select_sleep_state(
					pdata->us_euro_gpio_p);
		else
			msm_cdc_pinctrl_select_active_state(
					pdata->us_euro_gpio_p);
		dev_dbg(component->dev, "%s: swap select switch %d to %d\n",
			__func__, value, !value);
		ret = true;
	}

	return ret;
}

static int lahaina_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int slot_width = TDM_SLOT_WIDTH_BITS;
	int channels, slots;
	unsigned int slot_mask, rate, clk_freq;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

    /* currently only supporting TDM_RX_0 and TDM_TX_0 */
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		channels = tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		channels = tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		channels = tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		channels = tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT];
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX:
		channels = tdm_rx_cfg[TDM_QUIN][TDM_0].channels;
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUIN];
		break;
	case AFE_PORT_ID_SENARY_TDM_RX:
		channels = tdm_rx_cfg[TDM_SEN][TDM_0].channels;
		slots = tdm_slot[TDM_SEN].num;
		slot_width = tdm_slot[TDM_SEN].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEN];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		channels = tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		channels = tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		channels = tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		channels = tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT];
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX:
		channels = tdm_tx_cfg[TDM_QUIN][TDM_0].channels;
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUIN];
		break;
	case AFE_PORT_ID_SENARY_TDM_TX:
		channels = tdm_tx_cfg[TDM_SEN][TDM_0].channels;
		slots = tdm_slot[TDM_SEN].num;
		slot_width = tdm_slot[TDM_SEN].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEN];
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		if (slot_offset[i] != AFE_SLOT_MAPPING_OFFSET_INVALID)
			offset_channels++;
		else
			break;
	}

	if (offset_channels == 0) {
		pr_err("%s: invalid offset_channels %d\n",
			__func__, offset_channels);
		return -EINVAL;
	}

	if (channels > offset_channels) {
		pr_err("%s: channels %d exceed offset_channels %d\n",
			__func__, channels, offset_channels);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/*2 slot config - bits 0 and 1 set for the first two slots */
		slot_mask = 0xFFFFFFFF >> (32-channels);

		pr_debug("%s: tdm rx slot_width %d slots %d slot_mask %x\n",
			__func__, slot_width, slots, slot_mask);

		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, slot_mask,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm rx slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		pr_debug("%s: tdm rx channels: %d\n", __func__, channels);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			0, NULL, channels, slot_offset);
		if (ret < 0) {
			pr_err("%s: failed to set tdm rx channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/*2 slot config - bits 0 and 1 set for the first two slots */
		slot_mask = 0xFFFFFFFF >> (32-channels);

		pr_debug("%s: tdm tx slot_width %d slots %d slot_mask %x\n",
			__func__, slot_width, slots, slot_mask);

		ret = snd_soc_dai_set_tdm_slot(cpu_dai, slot_mask, 0,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm tx slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		pr_debug("%s: tdm tx channels: %d\n", __func__, channels);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			channels, slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set tdm tx channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		ret = -EINVAL;
		pr_err("%s: invalid use case, err:%d\n",
			__func__, ret);
		goto end;
	}

	rate = params_rate(params);
	clk_freq = rate * slot_width * slots;
	if (clk_freq > 24576000) {
		ret = -EINVAL;
		pr_err("%s: clk frequency > 24.576MHz %d\n",
			__func__, clk_freq);
		goto end;
	}
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, clk_freq, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		pr_err("%s: failed to set tdm clk, err:%d\n",
			__func__, ret);

end:
	return ret;
}

static int msm_get_tdm_mode(u32 port_id)
{
	int tdm_mode;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		tdm_mode = TDM_PRI;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		tdm_mode = TDM_SEC;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		tdm_mode = TDM_TERT;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		tdm_mode = TDM_QUAT;
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX:
	case AFE_PORT_ID_QUINARY_TDM_TX:
		tdm_mode = TDM_QUIN;
		break;
	case AFE_PORT_ID_SENARY_TDM_RX:
	case AFE_PORT_ID_SENARY_TDM_TX:
		tdm_mode = TDM_SEN;
		break;
	default:
		pr_err("%s: Invalid port id: %d\n", __func__, port_id);
		tdm_mode = -EINVAL;
	}
	return tdm_mode;
}

static int lahaina_tdm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int tdm_mode = msm_get_tdm_mode(cpu_dai->id);

	if (tdm_mode >= TDM_INTERFACE_MAX || tdm_mode < 0) {
		ret = -EINVAL;
		pr_err("%s: Invalid TDM interface %d\n",
			__func__, ret);
		return ret;
	}

	if (pdata->mi2s_gpio_p[tdm_mode]) {
		if (atomic_read(&(pdata->mi2s_gpio_ref_count[tdm_mode]))
									== 0) {
			ret = msm_cdc_pinctrl_select_active_state(
				pdata->mi2s_gpio_p[tdm_mode]);
			if (ret) {
				pr_err("%s: TDM GPIO pinctrl set active failed with %d\n",
					__func__, ret);
				goto done;
			}
		}
		atomic_inc(&(pdata->mi2s_gpio_ref_count[tdm_mode]));
	}

done:
	return ret;
}

static void lahaina_tdm_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int tdm_mode = msm_get_tdm_mode(cpu_dai->id);

	if (tdm_mode >= TDM_INTERFACE_MAX || tdm_mode < 0) {
		ret = -EINVAL;
		pr_err("%s: Invalid TDM interface %d\n",
			__func__, ret);
		return;
	}

	if (pdata->mi2s_gpio_p[tdm_mode]) {
		atomic_dec(&(pdata->mi2s_gpio_ref_count[tdm_mode]));
		if (atomic_read(&(pdata->mi2s_gpio_ref_count[tdm_mode]))
									== 0) {
			ret = msm_cdc_pinctrl_select_sleep_state(
				pdata->mi2s_gpio_p[tdm_mode]);
			if (ret)
				pr_err("%s: TDM GPIO pinctrl set sleep failed with %d\n",
					__func__, ret);
		}
	}
}

#ifndef CONFIG_AUXPCM_DISABLE
static int lahaina_aux_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	u32 aux_mode = cpu_dai->id - 1;

	if (aux_mode >= AUX_PCM_MAX) {
		ret = -EINVAL;
		pr_err("%s: Invalid AUX interface %d\n",
			__func__, ret);
		return ret;
	}

	if (pdata->mi2s_gpio_p[aux_mode]) {
		if (atomic_read(&(pdata->mi2s_gpio_ref_count[aux_mode]))
									== 0) {
			ret = msm_cdc_pinctrl_select_active_state(
				pdata->mi2s_gpio_p[aux_mode]);
			if (ret) {
				pr_err("%s: AUX GPIO pinctrl set active failed with %d\n",
					__func__, ret);
				goto done;
			}
		}
		atomic_inc(&(pdata->mi2s_gpio_ref_count[aux_mode]));
	}

done:
	return ret;
}

static void lahaina_aux_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	u32 aux_mode = cpu_dai->id - 1;

	if (aux_mode >= AUX_PCM_MAX) {
		pr_err("%s: Invalid AUX interface %d\n",
			__func__, ret);
		return;
	}

	if (pdata->mi2s_gpio_p[aux_mode]) {
		atomic_dec(&(pdata->mi2s_gpio_ref_count[aux_mode]));
		if (atomic_read(&(pdata->mi2s_gpio_ref_count[aux_mode]))
									== 0) {
			ret = msm_cdc_pinctrl_select_sleep_state(
				pdata->mi2s_gpio_p[aux_mode]);
			if (ret)
				pr_err("%s: AUX GPIO pinctrl set sleep failed with %d\n",
					__func__, ret);
		}
	}
}
#endif

static int msm_snd_cdc_dma_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	switch (dai_link->id) {
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_0:
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_1:
	case MSM_BACKEND_DAI_VA_CDC_DMA_TX_2:
		ret = lahaina_send_island_va_config(dai_link->id);
		if (ret)
			pr_err("%s: send island va cfg failed, err: %d\n",
			       __func__, ret);
		break;
	default:
		ret = lahaina_send_power_mode(dai_link->id);
		if (ret)
			pr_err("%s: send power mode failed, err: %d\n",
				__func__, ret);
		break;
	}

	return ret;
}

static void set_cps_config(struct snd_soc_pcm_runtime *rtd,
				u32 num_ch, u32 ch_mask)
{
	int i = 0, j = 0;
	int val = 0;
	u8 dev_num = 0;
	int ch_configured = 0;
	char wsa_cdc_name[DEV_NAME_STR_LEN];
	struct snd_soc_component *component = NULL;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
        struct msm_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(rtd->card);
	struct lpass_swr_spkr_dep_cfg_t *spkr_dep_cfg_ptr;

	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return;
	}

	if (!num_ch) {
		pr_err("%s: channel count is 0\n", __func__);
		return;
	}

	if (!ch_mask) {
		pr_err("%s: channel mask is 0\n", __func__);
		return;
	}

	if (!pdata->get_wsa_dev_num) {
		pr_err("%s: get_wsa_dev_num is NULL\n", __func__);
		return;
	}

	if (!pdata->cps_config.spkr_dep_cfg) {
		pr_err("%s: spkr_dep_cfg is NULL\n", __func__);
		return;
	}

	if (!pdata->cps_config.hw_reg_cfg.lpass_wr_cmd_reg_phy_addr ||
	   !pdata->cps_config.hw_reg_cfg.lpass_rd_cmd_reg_phy_addr ||
	   !pdata->cps_config.hw_reg_cfg.lpass_rd_fifo_reg_phy_addr) {
		pr_err("%s: cps static configuration is not set\n", __func__);
		return;
	}

	pdata->cps_config.lpass_hw_intf_cfg_mode = 1;

	while (ch_configured < num_ch) {
		if (!(ch_mask & (1 << i))) {
			i++;
			continue;
		}

		snprintf(wsa_cdc_name, sizeof(wsa_cdc_name), "wsa-codec.%d",
			 i+1);

		component = snd_soc_rtdcom_lookup(rtd, wsa_cdc_name);
		if (!component) {
			pr_err("%s: %s component is NULL\n", __func__,
				wsa_cdc_name);
			return;
		}

		dev_num = pdata->get_wsa_dev_num(component);
		if (dev_num < 0 || dev_num > SWR_MAX_SLAVE_DEVICES) {
			pr_err("%s: invalid slave dev num : %d\n", __func__,
				dev_num);
			return;
		}

		spkr_dep_cfg_ptr = &(pdata->cps_config.spkr_dep_cfg[i]);

		/* Clear stale dev num info */
		spkr_dep_cfg_ptr->vbatt_pkd_reg_addr &= 0xFFFF;
		spkr_dep_cfg_ptr->temp_pkd_reg_addr &= 0xFFFF;

		val = 0;

		/* bits 20:23 carry swr device number */
		val |= dev_num << 20;

		/* bits 24:27 carry read length in bytes */
		val |= 1 << 24;

		/* bits 16:19 carry command id */
		val |= (i*2) << 16;

		/* Update dev num in packed reg addr */
		spkr_dep_cfg_ptr->vbatt_pkd_reg_addr |= val;

		val &= 0xFF0FFFF;
		val |= ((i*2)+1) << 16;
		spkr_dep_cfg_ptr->temp_pkd_reg_addr |= val;

		/* Retain dev_num from val */
		val &= 0x00F00000;
		for (j = 0; j < MAX_CPS_LEVELS; j++)
		{
			val &= 0xFFF0FFFF;
			val |= ((i * 3) + j) << 16;
			spkr_dep_cfg_ptr->value_normal_thrsd[j] |= val;
			spkr_dep_cfg_ptr->value_low1_thrsd[j] |= val;
			spkr_dep_cfg_ptr->value_low2_thrsd[j] |= val;
		}
		i++;
		ch_configured++;
	}

	afe_set_cps_config(msm_get_port_id(dai_link->id),
					&pdata->cps_config, ch_mask);
}

static int msm_snd_cdc_dma_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	int ret = 0;
	u32 rx_ch_cdc_dma, tx_ch_cdc_dma;
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	u32 user_set_tx_ch = 0;
	u32 user_set_rx_ch = 0;
	u32 ch_id;

	ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, &tx_ch_cdc_dma, &rx_ch_cnt,
				&rx_ch_cdc_dma);
	if (ret < 0) {
		pr_err("%s: failed to get codec chan map, err:%d\n",
			__func__, ret);
		goto err;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (dai_link->id) {
		case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
		case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_3:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_4:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_5:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_6:
		{
			ch_id = msm_cdc_dma_get_idx_from_beid(dai_link->id);
			pr_debug("%s: id %d rx_ch=%d\n", __func__,
				ch_id, cdc_dma_rx_cfg[ch_id].channels);
			user_set_rx_ch = cdc_dma_rx_cfg[ch_id].channels;
			ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
					  user_set_rx_ch, &rx_ch_cdc_dma);
			if (ret < 0) {
				pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
				goto err;
			}

			if (dai_link->id == MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0 ||
			    dai_link->id == MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1) {
				set_cps_config(rtd, user_set_rx_ch,
						rx_ch_cdc_dma);
			}
		}
		break;
		}
	} else {
		switch (dai_link->id) {
		case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		{
			user_set_tx_ch = msm_vi_feed_tx_ch;
		}
		break;
		case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
		case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
		case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
		case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
		case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
		case MSM_BACKEND_DAI_VA_CDC_DMA_TX_0:
		case MSM_BACKEND_DAI_VA_CDC_DMA_TX_1:
		case MSM_BACKEND_DAI_VA_CDC_DMA_TX_2:
		{
			ch_id = msm_cdc_dma_get_idx_from_beid(dai_link->id);
			pr_debug("%s: id %d tx_ch=%d\n", __func__,
				ch_id, cdc_dma_tx_cfg[ch_id].channels);
			user_set_tx_ch = cdc_dma_tx_cfg[ch_id].channels;
		}
		break;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, user_set_tx_ch,
					&tx_ch_cdc_dma, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
			goto err;
		}
	}

err:
	return ret;
}

static int msm_fe_qos_prepare(struct snd_pcm_substream *substream)
{
	if (pm_qos_request_active(&substream->latency_pm_qos_req))
		pm_qos_remove_request(&substream->latency_pm_qos_req);

	qos_client_active_cnt++;
	if (qos_client_active_cnt == 1)
		msm_audio_update_qos_request(MSM_LL_QOS_VALUE);

	return 0;
}

static void msm_fe_qos_shutdown(struct snd_pcm_substream *substream)
{
	(void)substream;

	if (qos_client_active_cnt > 0)
		qos_client_active_cnt--;
	if (qos_client_active_cnt == 0)
		msm_audio_update_qos_request(PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE);
}

void mi2s_disable_audio_vote(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	/* Rx and Tx DAIs should use same clk index */
	int index = (cpu_dai->id) / 2;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int sample_rate = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sample_rate = mi2s_rx_cfg[index].sample_rate;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		sample_rate = mi2s_tx_cfg[index].sample_rate;
	} else {
		pr_err("%s: invalid stream %d\n", __func__, substream->stream);
		return;
	}

	if ((IS_MSM_INTERFACE_MI2S(index) && IS_FRACTIONAL(sample_rate)) ||
	    mi2s_intf_conf[index].audio_core_vote) {
		if (pdata->lpass_audio_hw_vote != NULL) {
			if (--pdata->core_audio_vote_count == 0) {
				clk_disable_unprepare(
					pdata->lpass_audio_hw_vote);
			} else if (pdata->core_audio_vote_count < 0) {
				pr_err("%s: audio vote mismatch\n", __func__);
				pdata->core_audio_vote_count = 0;
			}
			mi2s_intf_conf[index].audio_core_vote = false;
		} else {
			pr_err("%s: Invalid lpass audio hw node\n", __func__);
		}
	}
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	/* Rx and Tx DAIs should use same clk index */
	int index = (cpu_dai->id) / 2;
	unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
#ifndef ENABLE_WSA
	struct snd_soc_component *component = NULL;
	struct snd_soc_dai **codec_dais = rtd->codec_dais;
	int i;
#endif
	int sample_rate = 0;
	u32 bit_per_sample = 0;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

	if (index < PRIM_MI2S || index >= MI2S_MAX) {
		ret = -EINVAL;
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, cpu_dai->id);
		goto err;
	}
	/*
	 * Mutex protection in case the same MI2S
	 * interface using for both TX and RX so
	 * that the same clock won't be enable twice.
	 */
	mutex_lock(&mi2s_intf_conf[index].lock);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sample_rate = mi2s_rx_cfg[index].sample_rate;
		bit_per_sample =
		    get_mi2s_bits_per_sample(mi2s_rx_cfg[index].bit_format);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		sample_rate = mi2s_tx_cfg[index].sample_rate;
		bit_per_sample =
		    get_mi2s_bits_per_sample(mi2s_tx_cfg[index].bit_format);
	} else {
		pr_err("%s: invalid stream %d\n", __func__, substream->stream);
		ret = -EINVAL;
		goto vote_err;
	}

	if (++mi2s_intf_conf[index].ref_cnt == 1) {
		if (IS_MSM_INTERFACE_MI2S(index) && IS_FRACTIONAL(sample_rate)) {
			if (pdata->lpass_audio_hw_vote == NULL) {
				dev_err(rtd->card->dev, "%s: Invalid lpass audio hw node\n",
					__func__);
				ret = -EINVAL;
				goto clean_up;
			}
			if (pdata->core_audio_vote_count == 0) {
				ret = clk_prepare_enable(pdata->lpass_audio_hw_vote);
				if (ret < 0) {
					dev_err(rtd->card->dev, "%s: audio vote error\n",
						__func__);
					goto clean_up;
				}
			}
			pdata->core_audio_vote_count++;
			mi2s_intf_conf[index].audio_core_vote = true;
		}

		mi2s_clk[index].clk_freq_in_hz = (sample_rate *
					MI2S_NUM_CHANNELS * bit_per_sample);
		dev_dbg(rtd->card->dev, "%s: clock rate %ul\n", __func__,
			mi2s_clk[index].clk_freq_in_hz);

		/* Check if msm needs to provide the clock to the interface */
		if (!mi2s_intf_conf[index].msm_is_mi2s_master)
			mi2s_clk[index].clk_id = mi2s_ebit_clk[index];
		ret = msm_mi2s_set_sclk(substream, true);
		if (ret < 0) {
			dev_err(rtd->card->dev,
				"%s: afe lpass clock failed to enable MI2S clock, err:%d\n",
				__func__, ret);
			goto clean_up;
		}

		if (pdata->mi2s_gpio_p[index]) {
			if (atomic_read(&(pdata->mi2s_gpio_ref_count[index]))
									== 0) {
				ret = msm_cdc_pinctrl_select_active_state(
						pdata->mi2s_gpio_p[index]);
				if (ret) {
					pr_err("%s: MI2S GPIO pinctrl set active failed with %d\n",
						__func__, ret);
					goto clk_off;
				}
			}
			atomic_inc(&(pdata->mi2s_gpio_ref_count[index]));
		}
#ifndef ENABLE_WSA
		for (i = 0; i < rtd->num_codecs; i++) {
			component = codec_dais[i]->component;
			snd_soc_dai_set_fmt(codec_dais[i],
					SND_SOC_DAIFMT_CBS_CFS |
					SND_SOC_DAIFMT_I2S);
			snd_soc_component_set_sysclk(component, 0, 0,
					mi2s_clk[index].clk_freq_in_hz,
					SND_SOC_CLOCK_IN);
		}
#endif
	}
	if (!mi2s_intf_conf[index].msm_is_mi2s_master)
		fmt = SND_SOC_DAIFMT_CBM_CFM;
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret < 0) {
		pr_err("%s: set fmt cpu dai failed for MI2S (%d), err:%d\n",
			__func__, index, ret);
		goto clk_off;
	}

clk_off:
	if (ret < 0)
		msm_mi2s_set_sclk(substream, false);
clean_up:
	if (ret < 0) {
		mi2s_intf_conf[index].ref_cnt--;
		mi2s_disable_audio_vote(substream);
	}
vote_err:
	mutex_unlock(&mi2s_intf_conf[index].lock);
err:
	return ret;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	/* Rx and Tx DAIs should use same clk index */
	int index = (rtd->cpu_dai->id) / 2;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	if (index < PRIM_MI2S || index >= MI2S_MAX) {
		pr_err("%s:invalid MI2S DAI(%d)\n", __func__, index);
		return;
	}

	mutex_lock(&mi2s_intf_conf[index].lock);
	if (--mi2s_intf_conf[index].ref_cnt == 0) {
		if (pdata->mi2s_gpio_p[index]) {
			atomic_dec(&(pdata->mi2s_gpio_ref_count[index]));
			if (atomic_read(&(pdata->mi2s_gpio_ref_count[index]))
									== 0) {
				ret = msm_cdc_pinctrl_select_sleep_state(
						pdata->mi2s_gpio_p[index]);
				if (ret)
					pr_err("%s: MI2S GPIO pinctrl set sleep failed with %d\n",
						__func__, ret);
			}
		}

		ret = msm_mi2s_set_sclk(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed for MI2S (%d); ret=%d\n",
				__func__, index, ret);
		mi2s_disable_audio_vote(substream);
	}
	mutex_unlock(&mi2s_intf_conf[index].lock);
}

static int msm_wcn_hw_params_lito(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	u32 rx_ch[WCN_CDC_SLIM_RX_CH_MAX], tx_ch[WCN_CDC_SLIM_TX_CH_MAX_LITO];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	int ret = 0;

	dev_dbg(rtd->dev, "%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
	if (ret) {
		dev_err(rtd->dev,
			"%s: failed to get BTFM codec chan map\n, err:%d\n",
			__func__, ret);
		goto err;
	}

	dev_dbg(rtd->dev, "%s: tx_ch_cnt(%d) BE id %d\n",
		__func__, tx_ch_cnt, dai_link->id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, rx_ch_cnt, rx_ch);
	if (ret)
		dev_err(rtd->dev, "%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);

err:
	return ret;
}

static int msm_wcn_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	u32 rx_ch[WCN_CDC_SLIM_RX_CH_MAX], tx_ch[WCN_CDC_SLIM_TX_CH_MAX];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	int ret = 0;

	dev_dbg(rtd->dev, "%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
	if (ret) {
		dev_err(rtd->dev,
			"%s: failed to get BTFM codec chan map\n, err:%d\n",
			__func__, ret);
		goto err;
	}

	dev_dbg(rtd->dev, "%s: tx_ch_cnt(%d) BE id %d\n",
		__func__, tx_ch_cnt, dai_link->id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, rx_ch_cnt, rx_ch);
	if (ret)
		dev_err(rtd->dev, "%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);

err:
	return ret;
}

#ifndef CONFIG_AUXPCM_DISABLE
static struct snd_soc_ops lahaina_aux_be_ops = {
	.startup = lahaina_aux_snd_startup,
	.shutdown = lahaina_aux_snd_shutdown
};
#endif

static struct snd_soc_ops lahaina_tdm_be_ops = {
	.hw_params = lahaina_tdm_snd_hw_params,
	.startup = lahaina_tdm_snd_startup,
	.shutdown = lahaina_tdm_snd_shutdown
};

static struct snd_soc_ops msm_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_fe_qos_ops = {
	.prepare = msm_fe_qos_prepare,
	.shutdown = msm_fe_qos_shutdown,
};

static struct snd_soc_ops msm_cdc_dma_be_ops = {
	.startup = msm_snd_cdc_dma_startup,
	.hw_params = msm_snd_cdc_dma_hw_params,
};

static struct snd_soc_ops msm_wcn_ops = {
	.hw_params = msm_wcn_hw_params,
};

static struct snd_soc_ops msm_wcn_ops_lito = {
	.hw_params = msm_wcn_hw_params_lito,
};

static int msm_dmic_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	u32 dmic_idx;
	int *dmic_gpio_cnt;
	struct device_node *dmic_gpio;
	char  *wname;

	wname = strpbrk(w->name, "012345");
	if (!wname) {
		dev_err(component->dev, "%s: widget not found\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(wname, 10, &dmic_idx);
	if (ret < 0) {
		dev_err(component->dev, "%s: Invalid DMIC line on the codec\n",
			__func__);
		return -EINVAL;
	}

	pdata = snd_soc_card_get_drvdata(component->card);

	switch (dmic_idx) {
	case 0:
	case 1:
		dmic_gpio_cnt = &dmic_0_1_gpio_cnt;
		dmic_gpio = pdata->dmic01_gpio_p;
		break;
	case 2:
	case 3:
		dmic_gpio_cnt = &dmic_2_3_gpio_cnt;
		dmic_gpio = pdata->dmic23_gpio_p;
		break;
	case 4:
	case 5:
		dmic_gpio_cnt = &dmic_4_5_gpio_cnt;
		dmic_gpio = pdata->dmic45_gpio_p;
		break;
	default:
		dev_err(component->dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: event %d DMIC%d dmic_gpio_cnt %d\n",
			__func__, event, dmic_idx, *dmic_gpio_cnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		(*dmic_gpio_cnt)++;
		if (*dmic_gpio_cnt == 1) {
			ret = msm_cdc_pinctrl_select_active_state(
						dmic_gpio);
			if (ret < 0) {
				pr_err("%s: gpio set cannot be activated %sd",
					__func__, "dmic_gpio");
				return ret;
			}
		}

		break;
	case SND_SOC_DAPM_POST_PMD:
		(*dmic_gpio_cnt)--;
		if (*dmic_gpio_cnt == 0) {
			ret = msm_cdc_pinctrl_select_sleep_state(
					dmic_gpio);
			if (ret < 0) {
				pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "dmic_gpio");
				return ret;
			}
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm_int_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Analog Mic1", NULL),
	SND_SOC_DAPM_MIC("Analog Mic2", NULL),
	SND_SOC_DAPM_MIC("Analog Mic3", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic0", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic1", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic2", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic3", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic4", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic5", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
	SND_SOC_DAPM_MIC("Digital Mic7", NULL),
};

static int msm_wcn_init(struct snd_soc_pcm_runtime *rtd)
{
	unsigned int rx_ch[WCN_CDC_SLIM_RX_CH_MAX] = {157, 158};
	unsigned int tx_ch[WCN_CDC_SLIM_TX_CH_MAX]  = {159, 160};
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	return snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					   tx_ch, ARRAY_SIZE(rx_ch), rx_ch);
}

static int msm_wcn_init_lito(struct snd_soc_pcm_runtime *rtd)
{
	unsigned int rx_ch[WCN_CDC_SLIM_RX_CH_MAX] = {157, 158};
	unsigned int tx_ch[WCN_CDC_SLIM_TX_CH_MAX_LITO]  = {159, 160, 161};
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	return snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					   tx_ch, ARRAY_SIZE(rx_ch), rx_ch);
}

static struct snd_info_entry *msm_snd_info_create_subdir(struct module *mod,
				const char *name,
				struct snd_info_entry *parent)
{
	struct snd_info_entry *entry;

	entry = snd_info_create_module_entry(mod, name, parent);
	if (!entry)
		return NULL;
	entry->mode = S_IFDIR | 0555;
	if (snd_info_register(entry) < 0) {
		snd_info_free_entry(entry);
		return NULL;
	}
	return entry;
}

static void *def_wcd_mbhc_cal(void)
{
	void *wcd_mbhc_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_high;

	wcd_mbhc_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD9XXX_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!wcd_mbhc_cal)
		return NULL;

	WCD_MBHC_CAL_PLUG_TYPE_PTR(wcd_mbhc_cal)->v_hs_max = WCD_MBHC_HS_V_MAX;
	WCD_MBHC_CAL_BTN_DET_PTR(wcd_mbhc_cal)->num_btn = WCD_MBHC_DEF_BUTTONS;
	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(wcd_mbhc_cal);
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	btn_high[0] = 75;
	btn_high[1] = 137;
	btn_high[2] = 237;
	btn_high[3] = 500;
	btn_high[4] = 500;
	btn_high[5] = 500;
	btn_high[6] = 500;
	btn_high[7] = 500;

	return wcd_mbhc_cal;
}

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = MSM_DAILINK_NAME(Media1),
		.stream_name = "MultiMedia1",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA1,
		SND_SOC_DAILINK_REG(multimedia1),
	},
	{/* hw:x,1 */
		.name = MSM_DAILINK_NAME(Media2),
		.stream_name = "MultiMedia2",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA2,
		SND_SOC_DAILINK_REG(multimedia2),
	},
	{/* hw:x,2 */
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_VOICEMMODE1,
		SND_SOC_DAILINK_REG(voicemmode1),
	},
	{/* hw:x,3 */
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_VOIP,
		SND_SOC_DAILINK_REG(msmvoip),
	},
	{/* hw:x,4 */
		.name = MSM_DAILINK_NAME(ULL),
		.stream_name = "MultiMedia3",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA3,
		SND_SOC_DAILINK_REG(multimedia3),
	},
	{/* hw:x,5 */
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(afepcm_rx),
	},
	{/* hw:x,6 */
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(afepcm_tx),
	},
	{/* hw:x,7 */
		.name = MSM_DAILINK_NAME(Compress1),
		.stream_name = "Compress1",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA4,
		SND_SOC_DAILINK_REG(multimedia4),
	},
	/* Hostless PCM purpose */
	{/* hw:x,8 */
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(auxpcm_hostless),
	},
	{/* hw:x,9 */
		.name = MSM_DAILINK_NAME(LowLatency),
		.stream_name = "MultiMedia5",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA5,
		.ops = &msm_fe_qos_ops,
		SND_SOC_DAILINK_REG(multimedia5),
	},
	{/* hw:x,10 */
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM1,
		SND_SOC_DAILINK_REG(listen1),
	},
	/* Multiple Tunnel instances */
	{/* hw:x,11 */
		.name = MSM_DAILINK_NAME(Compress2),
		.stream_name = "Compress2",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA7,
		SND_SOC_DAILINK_REG(multimedia7),
	},
	{/* hw:x,12 */
		.name = MSM_DAILINK_NAME(MultiMedia10),
		.stream_name = "MultiMedia10",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA10,
		SND_SOC_DAILINK_REG(multimedia10),
	},
	{/* hw:x,13 */
		.name = MSM_DAILINK_NAME(ULL_NOIRQ),
		.stream_name = "MM_NOIRQ",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA8,
		.ops = &msm_fe_qos_ops,
		SND_SOC_DAILINK_REG(multimedia8),
	},
	/* HDMI Hostless */
	{/* hw:x,14 */
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(hdmi_rx_hostless),
	},
	{/* hw:x,15 */
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_VOICEMMODE2,
		SND_SOC_DAILINK_REG(voicemmode2),
	},
	/* LSM FE */
	{/* hw:x,16 */
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM2,
		SND_SOC_DAILINK_REG(listen2),
	},
	{/* hw:x,17 */
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM3,
		SND_SOC_DAILINK_REG(listen3),
	},
	{/* hw:x,18 */
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM4,
		SND_SOC_DAILINK_REG(listen4),
	},
	{/* hw:x,19 */
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM5,
		SND_SOC_DAILINK_REG(listen5),
	},
	{/* hw:x,20 */
		.name = "Listen 6 Audio Service",
		.stream_name = "Listen 6 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM6,
		SND_SOC_DAILINK_REG(listen6),
	},
	{/* hw:x,21 */
		.name = "Listen 7 Audio Service",
		.stream_name = "Listen 7 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM7,
		SND_SOC_DAILINK_REG(listen7),
	},
	{/* hw:x,22 */
		.name = "Listen 8 Audio Service",
		.stream_name = "Listen 8 Audio Service",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_LSM8,
		SND_SOC_DAILINK_REG(listen8),
	},
	{/* hw:x,23 */
		.name = MSM_DAILINK_NAME(Media9),
		.stream_name = "MultiMedia9",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA9,
		SND_SOC_DAILINK_REG(multimedia9),
	},
	{/* hw:x,24 */
		.name = MSM_DAILINK_NAME(Compress4),
		.stream_name = "Compress4",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA11,
		SND_SOC_DAILINK_REG(multimedia11),
	},
	{/* hw:x,25 */
		.name = MSM_DAILINK_NAME(Compress5),
		.stream_name = "Compress5",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA12,
		SND_SOC_DAILINK_REG(multimedia12),
	},
	{/* hw:x,26 */
		.name = MSM_DAILINK_NAME(Compress6),
		.stream_name = "Compress6",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA13,
		SND_SOC_DAILINK_REG(multimedia13),
	},
	{/* hw:x,27 */
		.name = MSM_DAILINK_NAME(Compress7),
		.stream_name = "Compress7",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA14,
		SND_SOC_DAILINK_REG(multimedia14),
	},
	{/* hw:x,28 */
		.name = MSM_DAILINK_NAME(Compress8),
		.stream_name = "Compress8",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA15,
		SND_SOC_DAILINK_REG(multimedia15),
	},
	{/* hw:x,29 */
		.name = MSM_DAILINK_NAME(ULL_NOIRQ_2),
		.stream_name = "MM_NOIRQ_2",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA16,
		.ops = &msm_fe_qos_ops,
		SND_SOC_DAILINK_REG(multimedia16),
	},
	{/* hw:x,30 */
		.name = "CDC_DMA Hostless",
		.stream_name = "CDC_DMA Hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(cdcdma_hostless),
	},
	{/* hw:x,31 */
		.name = "TX3_CDC_DMA Hostless",
		.stream_name = "TX3_CDC_DMA Hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(tx3_cdcdma_hostless),
	},
	{/* hw:x,32 */
		.name = "Tertiary MI2S TX_Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless Capture",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(tert_mi2s_tx_hostless),
	},
};

static struct snd_soc_dai_link msm_bolero_fe_dai_links[] = {
	{/* hw:x,33 */
		.name = LPASS_BE_WSA_CDC_DMA_TX_0,
		.stream_name = "WSA CDC DMA0 Capture",
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(wsa_cdcdma0_capture),
	},
};

static struct snd_soc_dai_link msm_bolero_fe_stub_dai_links[] = {
	{/* hw:x,33 */
		.name = LPASS_BE_WSA_CDC_DMA_TX_0,
		.stream_name = "WSA CDC DMA0 Capture",
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(wsa_cdcdma0_capture_stub),
	},
};

static struct snd_soc_dai_link msm_common_misc_fe_dai_links[] = {
	{/* hw:x,34 */
		.name = MSM_DAILINK_NAME(ASM Loopback),
		.stream_name = "MultiMedia6",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA6,
		SND_SOC_DAILINK_REG(multimedia6),
	},
	{/* hw:x,35 */
		.name = "USB Audio Hostless",
		.stream_name = "USB Audio Hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(usbaudio_hostless),
	},
	{/* hw:x,36 */
		.name = "SLIMBUS_7 Hostless",
		.stream_name = "SLIMBUS_7 Hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(slimbus7_hostless),
	},
	{/* hw:x,37 */
		.name = "Compress Capture",
		.stream_name = "Compress9",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA17,
		SND_SOC_DAILINK_REG(multimedia17),
	},
	{/* hw:x,38 */
		.name = "SLIMBUS_8 Hostless",
		.stream_name = "SLIMBUS_8 Hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(slimbus8_hostless),
	},
	{/* hw:x,39 */
		.name = LPASS_BE_TX_CDC_DMA_TX_5,
		.stream_name = "TX CDC DMA5 Capture",
		.id = MSM_BACKEND_DAI_TX_CDC_DMA_TX_5,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(tx_cdcdma5_tx),
	},
	{/* hw:x,40 */
		.name = MSM_DAILINK_NAME(Media31),
		.stream_name = "MultiMedia31",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA31,
		SND_SOC_DAILINK_REG(multimedia31),
	},
	{/* hw:x,41 */
		.name = MSM_DAILINK_NAME(Media32),
		.stream_name = "MultiMedia32",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA32,
		SND_SOC_DAILINK_REG(multimedia32),
	},
	{/* hw:x,42 */
		.name = "MSM AFE-PCM TX1",
		.stream_name = "AFE-PROXY TX1",
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(afepcm_tx1),
	},
        {/* hw:x,43 */
		.name = MSM_DAILINK_NAME(Compress3),
		.stream_name = "Compress3",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.id = MSM_FRONTEND_DAI_MULTIMEDIA10,
		SND_SOC_DAILINK_REG(multimedia10),
	},
	{/* hw:x,44 */
		.name = "Secondary MI2S_TX Hostless",
		.stream_name = "Secondary MI2S_TX Hostless Capture",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(sec_mi2s_tx_hostless),
	},
	/* DISP PORT Hostless */
	{/* hw:x,45 */
		.name = "DISPLAY_PORT_RX_HOSTLESS",
		.stream_name = "DISPLAY_PORT_RX_HOSTLESS",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(display_port_hostless),
	},
};

static struct snd_soc_dai_link msm_common_be_dai_links[] = {
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(afe_pcm_rx),
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(afe_pcm_tx),
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(incall_record_tx),
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(incall_record_rx),
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(voice_playback_tx),
	},
	/* Incall Music 2 BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE2_PLAYBACK_TX,
		.stream_name = "Voice2 Farend Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(voice2_playback_tx),
	},
	/* Proxy Tx BACK END DAI Link */
	{
		.name = LPASS_BE_PROXY_TX,
		.stream_name = "Proxy Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_PROXY_TX,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(proxy_tx),
	},
	/* Proxy Rx BACK END DAI Link */
	{
		.name = LPASS_BE_PROXY_RX,
		.stream_name = "Proxy Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_PROXY_RX,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(proxy_rx),
	},
	{
		.name = LPASS_BE_USB_AUDIO_RX,
		.stream_name = "USB Audio Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_USB_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(usb_audio_rx),
	},
	{
		.name = LPASS_BE_USB_AUDIO_TX,
		.stream_name = "USB Audio Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_USB_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(usb_audio_tx),
	},
};


static struct snd_soc_dai_link msm_tdm_be_dai_links[] = {
	{
		.name = LPASS_BE_PRI_TDM_RX_0,
		.stream_name = "Primary TDM0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_PRI_TDM_RX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(pri_tdm_rx_0),
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_0,
		.stream_name = "Primary TDM0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pri_tdm_tx_0),
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_0,
		.stream_name = "Secondary TDM0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SEC_TDM_RX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(sec_tdm_rx_0),
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_0,
		.stream_name = "Secondary TDM0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SEC_TDM_TX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sec_tdm_tx_0),
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_0,
		.stream_name = "Tertiary TDM0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_TERT_TDM_RX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(tert_tdm_rx_0),
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_0,
		.stream_name = "Tertiary TDM0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TERT_TDM_TX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(tert_tdm_tx_0),
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_0,
		.stream_name = "Quaternary TDM0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(quat_tdm_rx_0),
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_0,
		.stream_name = "Quaternary TDM0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quat_tdm_tx_0),
	},
	{
		.name = LPASS_BE_QUIN_TDM_RX_0,
		.stream_name = "Quinary TDM0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUIN_TDM_RX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(quin_tdm_rx_0),
	},
	{
		.name = LPASS_BE_QUIN_TDM_TX_0,
		.stream_name = "Quinary TDM0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUIN_TDM_TX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quin_tdm_tx_0),
	},
	{
		.name = LPASS_BE_SEN_TDM_RX_0,
		.stream_name = "Senary TDM0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SEN_TDM_RX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(sen_tdm_rx_0),
	},
	{
		.name = LPASS_BE_SEN_TDM_TX_0,
		.stream_name = "Senary TDM0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SEN_TDM_TX_0,
		.be_hw_params_fixup = lahaina_tdm_be_hw_params_fixup,
		.ops = &lahaina_tdm_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sen_tdm_tx_0),
	},
};

static struct snd_soc_dai_link msm_wcn_be_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_7_RX,
		.stream_name = "Slimbus7 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SLIMBUS_7_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.init = &msm_wcn_init,
		.ops = &msm_wcn_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(slimbus_7_rx),
	},
	{
		.name = LPASS_BE_SLIMBUS_7_TX,
		.stream_name = "Slimbus7 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SLIMBUS_7_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(slimbus_7_tx),
	},
};

static struct snd_soc_dai_link msm_wcn_btfm_be_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_7_RX,
		.stream_name = "Slimbus7 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SLIMBUS_7_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.init = &msm_wcn_init_lito,
		.ops = &msm_wcn_ops_lito,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(slimbus_7_rx),
	},
	{
		.name = LPASS_BE_SLIMBUS_7_TX,
		.stream_name = "Slimbus7 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SLIMBUS_7_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops_lito,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(slimbus_7_tx),
	},
	{
		.name = LPASS_BE_SLIMBUS_8_TX,
		.stream_name = "Slimbus8 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SLIMBUS_8_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops_lito,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(slimbus_8_tx),
	},
};

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static struct snd_soc_dai_link ext_disp_be_dai_link[] = {
	/* DISP PORT BACK END DAI Link */
	{
		.name = LPASS_BE_DISPLAY_PORT,
		.stream_name = "Display Port Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_DISPLAY_PORT_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(display_port),
	},
	/* DISP PORT 1 BACK END DAI Link */
	{
		.name = LPASS_BE_DISPLAY_PORT1,
		.stream_name = "Display Port1 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_DISPLAY_PORT_RX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(display_port1),
	},
};
#endif

#ifndef ENABLE_WSA
static int cs35l41_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai **codec_dais = rtd->codec_dais;
	struct snd_soc_dapm_context *dapm;
	int i;

	for (i = 0; i < rtd->num_codecs; i++) {
		dapm = snd_soc_component_get_dapm(codec_dais[i]->component);
		if (dapm->component->name_prefix == NULL) {
			pr_debug("%s: name_prefix=NULL\n", __func__);
			snd_soc_dapm_ignore_suspend(dapm, "AMP Playback");
			snd_soc_dapm_ignore_suspend(dapm, "AMP Capture");
			snd_soc_dapm_ignore_suspend(dapm, "SPK");
		} else if (!strcmp(dapm->component->name_prefix, "L")) {
			pr_debug("%s: name_prefix=L\n", __func__);
			snd_soc_dapm_ignore_suspend(dapm, "L AMP Playback");
			snd_soc_dapm_ignore_suspend(dapm, "L AMP Capture");
			snd_soc_dapm_ignore_suspend(dapm, "L SPK");
		} else if (!strcmp(dapm->component->name_prefix, "R")) {
			pr_debug("%s: name_prefix=R\n", __func__);
			snd_soc_dapm_ignore_suspend(dapm, "R AMP Playback");
			snd_soc_dapm_ignore_suspend(dapm, "R AMP Capture");
			snd_soc_dapm_ignore_suspend(dapm, "R SPK");
		}
	}
	snd_soc_dapm_sync(dapm);

	return 0;
}
#endif

static struct snd_soc_dai_link msm_mi2s_be_dai_links[] = {
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(pri_mi2s_rx),
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pri_mi2s_tx),
	},
	{
		.name = LPASS_BE_SEC_MI2S_RX,
		.stream_name = "Secondary MI2S Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(sec_mi2s_rx),
	},
	{
		.name = LPASS_BE_SEC_MI2S_TX,
		.stream_name = "Secondary MI2S Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sec_mi2s_tx),
	},
	{
		.name = LPASS_BE_TERT_MI2S_RX,
		.stream_name = "Tertiary MI2S Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(tert_mi2s_rx),
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(tert_mi2s_tx),
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(quat_mi2s_rx),
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quat_mi2s_tx),
	},
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(quin_mi2s_rx),
	},
	{
		.name = LPASS_BE_QUIN_MI2S_TX,
		.stream_name = "Quinary MI2S Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUINARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quin_mi2s_tx),
	},
	{
		.name = LPASS_BE_SENARY_MI2S_RX,
		.stream_name = "Senary MI2S Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SENARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		SND_SOC_DAILINK_REG(sen_mi2s_rx),
#ifndef ENABLE_WSA
		.init = &cs35l41_init,
#endif
	},
	{
		.name = LPASS_BE_SENARY_MI2S_TX,
		.stream_name = "Senary MI2S Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SENARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sen_mi2s_tx),
	},
};

#ifndef CONFIG_AUXPCM_DISABLE
static struct snd_soc_dai_link msm_auxpcm_be_dai_links[] = {
	/* Primary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(auxpcm_rx),
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(auxpcm_tx),
	},
	/* Secondary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_SEC_AUXPCM_RX,
		.stream_name = "Sec AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sec_auxpcm_rx),
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "Sec AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sec_auxpcm_tx),
	},
	/* Tertiary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_TERT_AUXPCM_RX,
		.stream_name = "Tert AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_TERT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(tert_auxpcm_rx),
	},
	{
		.name = LPASS_BE_TERT_AUXPCM_TX,
		.stream_name = "Tert AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TERT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(tert_auxpcm_tx),
	},
	/* Quaternary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_AUXPCM_RX,
		.stream_name = "Quat AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quat_auxpcm_rx),
	},
	{
		.name = LPASS_BE_QUAT_AUXPCM_TX,
		.stream_name = "Quat AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quat_auxpcm_tx),
	},
	/* Quinary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_QUIN_AUXPCM_RX,
		.stream_name = "Quin AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUIN_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quin_auxpcm_rx),
	},
	{
		.name = LPASS_BE_QUIN_AUXPCM_TX,
		.stream_name = "Quin AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUIN_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(quin_auxpcm_tx),
	},
	/* Senary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_SEN_AUXPCM_RX,
		.stream_name = "Sen AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SEN_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sen_auxpcm_rx),
	},
	{
		.name = LPASS_BE_SEN_AUXPCM_TX,
		.stream_name = "Sen AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_SEN_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &lahaina_aux_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(sen_auxpcm_tx),
	},
};
#endif

static struct snd_soc_dai_link msm_wsa_cdc_dma_be_dai_links[] = {
	/* WSA CDC DMA Backend DAI Links */
	{
		.name = LPASS_BE_WSA_CDC_DMA_RX_0,
		.stream_name = "WSA CDC DMA0 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(wsa_dma_rx0),
		.init = &msm_int_wsa_init,
	},
	{
		.name = LPASS_BE_WSA_CDC_DMA_RX_1,
		.stream_name = "WSA CDC DMA1 Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(wsa_dma_rx1),
	},
	{
		.name = LPASS_BE_WSA_CDC_DMA_TX_1,
		.stream_name = "WSA CDC DMA1 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(wsa_dma_tx1),
	},
	{
		.name = LPASS_BE_WSA_CDC_DMA_TX_0_VI,
		.stream_name = "WSA CDC DMA0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_cdc_dma_be_ops,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(wsa_dma_tx0_vi),
	},
};

static struct snd_soc_dai_link msm_rx_tx_cdc_dma_be_dai_links[] = {
	/* RX CDC DMA Backend DAI Links */
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_0,
		.stream_name = "RX CDC DMA0 Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(rx_dma_rx0),
		.init = &msm_rx_tx_codec_init,
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_1,
		.stream_name = "RX CDC DMA1 Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(rx_dma_rx1),
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_2,
		.stream_name = "RX CDC DMA2 Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_2,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(rx_dma_rx2),
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_3,
		.stream_name = "RX CDC DMA3 Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_3,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(rx_dma_rx3),
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_5,
		.stream_name = "RX CDC DMA5 Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_5,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(rx_dma_rx5),
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_6,
		.stream_name = "RX CDC DMA6 Playback",
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.dynamic_be = 1,
#endif /* CONFIG_AUDIO_QGKI */
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_6,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(rx_dma_rx6),
	},
	/* TX CDC DMA Backend DAI Links */
	{
		.name = LPASS_BE_TX_CDC_DMA_TX_3,
		.stream_name = "TX CDC DMA3 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TX_CDC_DMA_TX_3,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(tx_dma_tx3),
	},
	{
		.name = LPASS_BE_TX_CDC_DMA_TX_4,
		.stream_name = "TX CDC DMA4 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TX_CDC_DMA_TX_4,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(tx_dma_tx4),
	},
};

static struct snd_soc_dai_link msm_va_cdc_dma_be_dai_links[] = {
	{
		.name = LPASS_BE_VA_CDC_DMA_TX_0,
		.stream_name = "VA CDC DMA0 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_VA_CDC_DMA_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(va_dma_tx0),
	},
	{
		.name = LPASS_BE_VA_CDC_DMA_TX_1,
		.stream_name = "VA CDC DMA1 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_VA_CDC_DMA_TX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(va_dma_tx1),
	},
	{
		.name = LPASS_BE_VA_CDC_DMA_TX_2,
		.stream_name = "VA CDC DMA2 Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_VA_CDC_DMA_TX_2,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
		SND_SOC_DAILINK_REG(va_dma_tx2),
	},
};

static struct snd_soc_dai_link msm_afe_rxtx_lb_be_dai_link[] = {
	{
		.name = LPASS_BE_AFE_LOOPBACK_TX,
		.stream_name = "AFE Loopback Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_AFE_LOOPBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(afe_loopback_tx),
	},
};

static struct snd_soc_dai_link msm_lahaina_dai_links[
			ARRAY_SIZE(msm_common_dai_links) +
			ARRAY_SIZE(msm_bolero_fe_dai_links) +
			ARRAY_SIZE(msm_common_misc_fe_dai_links) +
			ARRAY_SIZE(msm_common_be_dai_links) +
			ARRAY_SIZE(msm_tdm_be_dai_links) +
			ARRAY_SIZE(msm_mi2s_be_dai_links) +
#ifndef CONFIG_AUXPCM_DISABLE
			ARRAY_SIZE(msm_auxpcm_be_dai_links) +
#endif
			ARRAY_SIZE(msm_wsa_cdc_dma_be_dai_links) +
			ARRAY_SIZE(msm_rx_tx_cdc_dma_be_dai_links) +
			ARRAY_SIZE(msm_va_cdc_dma_be_dai_links) +
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
			ARRAY_SIZE(ext_disp_be_dai_link) +
#endif
			ARRAY_SIZE(msm_wcn_be_dai_links) +
			ARRAY_SIZE(msm_afe_rxtx_lb_be_dai_link) +
			ARRAY_SIZE(msm_wcn_btfm_be_dai_links)];

static int msm_populate_dai_link_component_of_node(
					struct snd_soc_card *card)
{
	int i, j, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *np = NULL;
	int codecs_enabled = 0;
	struct snd_soc_dai_link_component *codecs_comp = NULL;

	if (!cdev) {
		dev_err(cdev, "%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platforms->of_node && dai_link[i].cpus->of_node)
			continue;

		/* populate platform_of_node for snd card dai links */
		if (dai_link[i].platforms->name &&
		    !dai_link[i].platforms->of_node) {
			index = of_property_match_string(cdev->of_node,
						"asoc-platform-names",
						dai_link[i].platforms->name);
			if (index < 0) {
				dev_err(cdev, "%s: No match found for platform name: %s\n",
					__func__, dai_link[i].platforms->name);
				ret = index;
				goto err;
			}
			np = of_parse_phandle(cdev->of_node, "asoc-platform",
					      index);
			if (!np) {
				dev_err(cdev, "%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platforms->name,
					index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platforms->of_node = np;
			dai_link[i].platforms->name = NULL;
		}

		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpus->dai_name && !dai_link[i].cpus->of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-cpu-names",
						 dai_link[i].cpus->dai_name);
			if (index >= 0) {
				np = of_parse_phandle(cdev->of_node, "asoc-cpu",
						index);
				if (!np) {
					dev_err(cdev, "%s: retrieving phandle for cpu dai %s failed\n",
						__func__,
						dai_link[i].cpus->dai_name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].cpus->of_node = np;
				dai_link[i].cpus->dai_name = NULL;
			}
		}

		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].num_codecs > 0) {
			for (j = 0; j < dai_link[i].num_codecs; j++) {
				if (dai_link[i].codecs[j].of_node ||
						!dai_link[i].codecs[j].name)
					continue;

				index = of_property_match_string(cdev->of_node,
						"asoc-codec-names",
						dai_link[i].codecs[j].name);
				if (index < 0)
					continue;
				np = of_parse_phandle(cdev->of_node,
						      "asoc-codec",
						      index);
				if (!np) {
					dev_err(cdev, "%s: retrieving phandle for codec %s failed\n",
						__func__,
						dai_link[i].codecs[j].name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].codecs[j].of_node = np;
				dai_link[i].codecs[j].name = NULL;
			}
		}
	}

	/* In multi-codec scenario, check if codecs are enabled for this platform */
	for (i = 0; i < card->num_links; i++) {
		codecs_enabled = 0;
		if (dai_link[i].num_codecs > 1) {
			for (j = 0; j < dai_link[i].num_codecs; j++) {
				if (!dai_link[i].codecs[j].of_node)
					continue;

				np = dai_link[i].codecs[j].of_node;
                                if (!of_device_is_available(np)) {
                                        dev_dbg(cdev, "%s: codec is disabled: %s\n",
						__func__,
						np->full_name);
					dai_link[i].codecs[j].of_node = NULL;
					continue;
                                }

				codecs_enabled++;
			}
			if (codecs_enabled > 0 &&
				    codecs_enabled < dai_link[i].num_codecs) {
				codecs_comp = devm_kzalloc(cdev,
				    sizeof(struct snd_soc_dai_link_component)
				    * codecs_enabled, GFP_KERNEL);
				if (!codecs_comp) {
					dev_err(cdev, "%s: %s dailink codec component alloc failed\n",
						__func__, dai_link[i].name);
					ret = -ENOMEM;
					goto err;
				}
				index = 0;
				for (j = 0; j < dai_link[i].num_codecs; j++) {
					if(dai_link[i].codecs[j].of_node) {
						codecs_comp[index].of_node =
						  dai_link[i].codecs[j].of_node;
						codecs_comp[index].dai_name =
						  dai_link[i].codecs[j].dai_name;
						codecs_comp[index].name = NULL;
						index++;
					}
				}
				dai_link[i].codecs = codecs_comp;
				dai_link[i].num_codecs = codecs_enabled;
			}
		}
	}

err:
	return ret;
}

static int msm_audrx_stub_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = -EINVAL;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "msm-stub-codec");

	if (!component) {
		pr_err("* %s: No match for msm-stub-codec component\n", __func__);
		return ret;
	}

	ret = snd_soc_add_component_controls(component, msm_snd_controls,
			ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		dev_err(component->dev,
			"%s: add_codec_controls failed, err = %d\n",
			__func__, ret);
		return ret;
	}

	return ret;
}

static int msm_snd_stub_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_ops msm_stub_be_ops = {
	.hw_params = msm_snd_stub_hw_params,
};

struct snd_soc_card snd_soc_card_stub_msm = {
	.name		= "lahaina-stub-snd-card",
};

static struct snd_soc_dai_link msm_stub_fe_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSMSTUB Media1",
		.stream_name = "MultiMedia1",
		.dynamic = 1,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
#endif /* CONFIG_AUDIO_QGKI */
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA1,
		SND_SOC_DAILINK_REG(multimedia1),
	},
};

static struct snd_soc_dai_link msm_stub_be_dai_links[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_AUXPCM_RX,
		.init = &msm_audrx_stub_init,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
		SND_SOC_DAILINK_REG(auxpcm_rx),
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
		SND_SOC_DAILINK_REG(auxpcm_tx),
	},
};

static struct snd_soc_dai_link msm_stub_dai_links[
			 ARRAY_SIZE(msm_stub_fe_dai_links) +
			 ARRAY_SIZE(msm_stub_be_dai_links)];

static const struct of_device_id lahaina_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,lahaina-asoc-snd",
	  .data = "codec"},
	{ .compatible = "qcom,lahaina-asoc-snd-stub",
	  .data = "stub_codec"},
	{},
};

static int msm_snd_card_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_component *component = NULL;
	const char *be_dl_name = LPASS_BE_RX_CDC_DMA_RX_0;
	struct snd_soc_pcm_runtime *rtd;
	struct msm_asoc_mach_data *pdata;
	int ret = 0;
	void *mbhc_calibration;
	bool is_wcd937x = false;

	pdata = snd_soc_card_get_drvdata(card);
	if (!pdata)
		return -EINVAL;

	if (pdata->wcd_disabled)
		return 0;

	rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
	if (!rtd) {
		dev_err(card->dev,
			"%s: snd_soc_get_pcm_runtime for %s failed!\n",
			__func__, be_dl_name);
		return -EINVAL;
	}

	component = snd_soc_rtdcom_lookup(rtd, WCD938X_DRV_NAME);
	if (!component) {
		component = snd_soc_rtdcom_lookup(rtd, WCD937X_DRV_NAME);
		if (!component) {
			pr_err("%s component is NULL\n", __func__);
			return -EINVAL;
		} else {
			is_wcd937x = true;
		}
	}

	mbhc_calibration = def_wcd_mbhc_cal();
	if (!mbhc_calibration)
		return -ENOMEM;
	wcd_mbhc_cfg.calibration = mbhc_calibration;
	if (!is_wcd937x)
		ret = wcd938x_mbhc_hs_detect(component, &wcd_mbhc_cfg);
	else
		ret = wcd937x_mbhc_hs_detect(component, &wcd_mbhc_cfg);
	if (ret) {
		dev_err(component->dev, "%s: mbhc hs detect failed, err:%d\n",
			__func__, ret);
		goto err_hs_detect;
	}
	return 0;

err_hs_detect:
	kfree(mbhc_calibration);
	return ret;
}

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink = NULL;
	int len_1 = 0;
	int len_2 = 0;
	int total_links = 0;
	int rc = 0;
	u32 mi2s_audio_intf = 0;
	u32 val = 0;
	u32 wcn_btfm_intf = 0;
	const struct of_device_id *match;
	u32 wsa_max_devs = 0;

	match = of_match_node(lahaina_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "codec")) {
		card = &snd_soc_card_lahaina_msm;

		memcpy(msm_lahaina_dai_links + total_links,
		       msm_common_dai_links,
		       sizeof(msm_common_dai_links));
		total_links += ARRAY_SIZE(msm_common_dai_links);

		rc = of_property_read_u32(dev->of_node,
				"qcom,wsa-max-devs", &wsa_max_devs);
		if (rc) {
			dev_info(dev,
				"%s: wsa-max-devs property missing in DT %s, ret = %d\n",
				 __func__, dev->of_node->full_name, rc);
			wsa_max_devs = 0;
		}
		if (!wsa_max_devs) {
			memcpy(msm_lahaina_dai_links + total_links,
				msm_bolero_fe_stub_dai_links,
				sizeof(msm_bolero_fe_stub_dai_links));
			total_links +=
				ARRAY_SIZE(msm_bolero_fe_stub_dai_links);
		} else {
			memcpy(msm_lahaina_dai_links + total_links,
				msm_bolero_fe_dai_links,
				sizeof(msm_bolero_fe_dai_links));
			total_links +=
				ARRAY_SIZE(msm_bolero_fe_dai_links);
		}
		memcpy(msm_lahaina_dai_links + total_links,
		       msm_common_misc_fe_dai_links,
		       sizeof(msm_common_misc_fe_dai_links));
		total_links += ARRAY_SIZE(msm_common_misc_fe_dai_links);

		memcpy(msm_lahaina_dai_links + total_links,
		       msm_common_be_dai_links,
		       sizeof(msm_common_be_dai_links));
		total_links += ARRAY_SIZE(msm_common_be_dai_links);

		memcpy(msm_lahaina_dai_links + total_links,
		       msm_tdm_be_dai_links,
		       sizeof(msm_tdm_be_dai_links));
		total_links += ARRAY_SIZE(msm_tdm_be_dai_links);

		memcpy(msm_lahaina_dai_links + total_links,
		       msm_rx_tx_cdc_dma_be_dai_links,
		       sizeof(msm_rx_tx_cdc_dma_be_dai_links));
		total_links +=
			ARRAY_SIZE(msm_rx_tx_cdc_dma_be_dai_links);

		if (wsa_max_devs) {
			memcpy(msm_lahaina_dai_links + total_links,
				msm_wsa_cdc_dma_be_dai_links,
				sizeof(msm_wsa_cdc_dma_be_dai_links));
			total_links +=
				ARRAY_SIZE(msm_wsa_cdc_dma_be_dai_links);
		}
		memcpy(msm_lahaina_dai_links + total_links,
		       msm_va_cdc_dma_be_dai_links,
		       sizeof(msm_va_cdc_dma_be_dai_links));
		total_links +=
			ARRAY_SIZE(msm_va_cdc_dma_be_dai_links);

		rc = of_property_read_u32(dev->of_node, "qcom,mi2s-audio-intf",
					  &mi2s_audio_intf);
		if (rc) {
			dev_dbg(dev, "%s: No DT match MI2S audio interface\n",
				__func__);
		} else {
			if (mi2s_audio_intf) {
				memcpy(msm_lahaina_dai_links + total_links,
					msm_mi2s_be_dai_links,
					sizeof(msm_mi2s_be_dai_links));
				total_links +=
					ARRAY_SIZE(msm_mi2s_be_dai_links);
			}
		}
#ifndef CONFIG_AUXPCM_DISABLE
		rc = of_property_read_u32(dev->of_node,
					  "qcom,auxpcm-audio-intf",
					  &val);
		if (rc) {
			dev_dbg(dev, "%s: No DT match Aux PCM interface\n",
				__func__);
		} else {
			if (val) {
				memcpy(msm_lahaina_dai_links + total_links,
					msm_auxpcm_be_dai_links,
					sizeof(msm_auxpcm_be_dai_links));
				total_links +=
					ARRAY_SIZE(msm_auxpcm_be_dai_links);
			}
		}
#endif

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		rc = of_property_read_u32(dev->of_node,
					   "qcom,ext-disp-audio-rx", &val);
		if (!rc && val) {
			dev_dbg(dev, "%s(): ext disp audio support present\n",
				__func__);
			memcpy(msm_lahaina_dai_links + total_links,
			       ext_disp_be_dai_link,
			       sizeof(ext_disp_be_dai_link));
			total_links += ARRAY_SIZE(ext_disp_be_dai_link);
		}
#endif

		rc = of_property_read_u32(dev->of_node, "qcom,wcn-bt", &val);
		if (!rc && val) {
			dev_dbg(dev, "%s(): WCN BT support present\n",
				__func__);
			memcpy(msm_lahaina_dai_links + total_links,
			       msm_wcn_be_dai_links,
			       sizeof(msm_wcn_be_dai_links));
			total_links += ARRAY_SIZE(msm_wcn_be_dai_links);
		}

		rc = of_property_read_u32(dev->of_node, "qcom,afe-rxtx-lb",
				&val);
		if (!rc && val) {
			memcpy(msm_lahaina_dai_links + total_links,
				msm_afe_rxtx_lb_be_dai_link,
				sizeof(msm_afe_rxtx_lb_be_dai_link));
			total_links +=
				ARRAY_SIZE(msm_afe_rxtx_lb_be_dai_link);
		}

		rc = of_property_read_u32(dev->of_node, "qcom,wcn-btfm",
					  &wcn_btfm_intf);
		if (rc) {
			dev_dbg(dev, "%s: No DT match wcn btfm interface\n",
				__func__);
		} else {
			if (wcn_btfm_intf) {
				memcpy(msm_lahaina_dai_links + total_links,
					msm_wcn_btfm_be_dai_links,
					sizeof(msm_wcn_btfm_be_dai_links));
				total_links +=
					ARRAY_SIZE(msm_wcn_btfm_be_dai_links);
			}
		}
		dailink = msm_lahaina_dai_links;
	} else if(!strcmp(match->data, "stub_codec")) {
		card = &snd_soc_card_stub_msm;
		len_1 = ARRAY_SIZE(msm_stub_fe_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_stub_be_dai_links);

		memcpy(msm_stub_dai_links,
		       msm_stub_fe_dai_links,
		       sizeof(msm_stub_fe_dai_links));
		memcpy(msm_stub_dai_links + len_1,
		       msm_stub_be_dai_links,
		       sizeof(msm_stub_be_dai_links));

		dailink = msm_stub_dai_links;
		total_links = len_2;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
		card->late_probe = msm_snd_card_late_probe;
	}

	return card;
}

static int msm_int_wsa_init(struct snd_soc_pcm_runtime *rtd)
{
	u8 spkleft_ports[WSA883X_MAX_SWR_PORTS] = {0, 1, 2, 3};
	u8 spkright_ports[WSA883X_MAX_SWR_PORTS] = {0, 1, 2, 3};
	u8 spkleft_port_types[WSA883X_MAX_SWR_PORTS] = {SPKR_L, SPKR_L_COMP,
						SPKR_L_BOOST, SPKR_L_VI};
	u8 spkright_port_types[WSA883X_MAX_SWR_PORTS] = {SPKR_R, SPKR_R_COMP,
						SPKR_R_BOOST, SPKR_R_VI};
	unsigned int ch_rate[WSA883X_MAX_SWR_PORTS] = {SWR_CLK_RATE_2P4MHZ, SWR_CLK_RATE_0P6MHZ,
							SWR_CLK_RATE_0P3MHZ, SWR_CLK_RATE_1P2MHZ};
	unsigned int ch_mask[WSA883X_MAX_SWR_PORTS] = {0x1, 0xF, 0x3, 0x3};
	struct snd_soc_component *component = NULL;
	struct snd_soc_dapm_context *dapm = NULL;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);
	int wsa_active_devs = 0;

        if (pdata->wsa_max_devs > 0) {
		component = snd_soc_rtdcom_lookup(rtd, "wsa-codec.1");
		if (component) {
			dapm = snd_soc_component_get_dapm(component);

			wsa883x_set_channel_map(component, &spkleft_ports[0],
					WSA883X_MAX_SWR_PORTS, &ch_mask[0],
					&ch_rate[0], &spkleft_port_types[0]);

			wsa883x_codec_info_create_codec_entry(pdata->codec_root,
								component);
			wsa_active_devs++;
		} else {
			pr_info("%s: wsa-codec.1 component is NULL\n", __func__);
		}
	}

        /* If current platform has more than one WSA */
        if (pdata->wsa_max_devs > wsa_active_devs) {
		component = snd_soc_rtdcom_lookup(rtd, "wsa-codec.2");
		if (!component) {
			pr_err("%s: wsa-codec.2 component is NULL\n", __func__);
			pr_err("%s: %d WSA is found. Expect %d WSA.",
				__func__, wsa_active_devs, pdata->wsa_max_devs);
			return -EINVAL;
		}

		dapm = snd_soc_component_get_dapm(component);

		wsa883x_set_channel_map(component, &spkright_ports[0],
				WSA883X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0], &spkright_port_types[0]);

		wsa883x_codec_info_create_codec_entry(pdata->codec_root,
							component);
	}

	return 0;
}

static int msm_rx_tx_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	struct snd_soc_component *bolero_component = NULL;
	struct snd_soc_dapm_context *dapm = NULL;
	int ret = 0;
	int codec_variant = -1;
	struct snd_info_entry *entry;
	struct snd_card *card = NULL;
	struct msm_asoc_mach_data *pdata;

	pdata = snd_soc_card_get_drvdata(rtd->card);
	if(!pdata)
		return -EINVAL;

	component = snd_soc_rtdcom_lookup(rtd, "bolero_codec");
	if (!component) {
		pr_err("%s: could not find component for bolero_codec\n",
			__func__);
		return ret;
	} else {
		bolero_component = component;
	}

	dapm = snd_soc_component_get_dapm(component);

	ret = snd_soc_add_component_controls(component, msm_int_snd_controls,
				ARRAY_SIZE(msm_int_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_component_controls failed: %d\n",
			__func__, ret);
		return ret;
	}
	ret = snd_soc_add_component_controls(component, msm_common_snd_controls,
				ARRAY_SIZE(msm_common_snd_controls));
	if (ret < 0) {
		pr_err("%s: add common snd controls failed: %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_int_dapm_widgets,
				ARRAY_SIZE(msm_int_dapm_widgets));

	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic7");

	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic5");

	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "WSA AIF VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT_WSA");

	snd_soc_dapm_sync(dapm);

	card = rtd->card->snd_card;

	if (!pdata->codec_root) {
		entry = msm_snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			return 0;
		}
		pdata->codec_root = entry;
	}
	bolero_info_create_codec_entry(pdata->codec_root, component);
	bolero_register_wake_irq(component, false);

	if (pdata->wcd_disabled) {
		bolero_set_port_map(bolero_component,
			ARRAY_SIZE(sm_port_map), sm_port_map);
		codec_reg_done = true;
		return 0;
	}
	component = snd_soc_rtdcom_lookup(rtd, WCD938X_DRV_NAME);
	if (!component) {
		component = snd_soc_rtdcom_lookup(rtd, WCD937X_DRV_NAME);
		if (!component) {
			pr_err("%s component is NULL\n", __func__);
			return -EINVAL;
		}
	}
	dapm = snd_soc_component_get_dapm(component);
	card = component->card->snd_card;

	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "AUX");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_sync(dapm);

	if (!strncmp(component->driver->name, WCD937X_DRV_NAME,
	    strlen(WCD937X_DRV_NAME))) {
		wcd937x_info_create_codec_entry(pdata->codec_root, component);
		codec_variant = wcd937x_get_codec_variant(component);
		dev_dbg(component->dev, "%s: variant %d\n",
			 __func__, codec_variant);
		if (codec_variant == WCD9370_VARIANT)
			ret = snd_soc_add_component_controls(component,
				msm_int_wcd9370_snd_controls,
				ARRAY_SIZE(msm_int_wcd9370_snd_controls));
		else if (codec_variant == WCD9375_VARIANT)
			ret = snd_soc_add_component_controls(component,
				msm_int_wcd9375_snd_controls,
				ARRAY_SIZE(msm_int_wcd9375_snd_controls));
		bolero_set_port_map(bolero_component,
			ARRAY_SIZE(sm_port_map_wcd937x), sm_port_map_wcd937x);
	} else if (!strncmp(component->driver->name, WCD938X_DRV_NAME,
		   strlen(WCD938X_DRV_NAME))) {
		wcd938x_info_create_codec_entry(pdata->codec_root, component);

		codec_variant = wcd938x_get_codec_variant(component);
		dev_dbg(component->dev, "%s: variant %d\n",
			 __func__, codec_variant);
		if (codec_variant == WCD9380)
			ret = snd_soc_add_component_controls(component,
				msm_int_wcd9380_snd_controls,
				ARRAY_SIZE(msm_int_wcd9380_snd_controls));
		else if (codec_variant == WCD9385)
			ret = snd_soc_add_component_controls(component,
				msm_int_wcd9385_snd_controls,
				ARRAY_SIZE(msm_int_wcd9385_snd_controls));

		if ((strnstr(rtd->card->name, "shima", strlen(rtd->card->name))
		    != NULL) || (strnstr(rtd->card->name, "yupik",
		    strlen(rtd->card->name)) != NULL))
			bolero_set_port_map(bolero_component,
				ARRAY_SIZE(sm_port_map_shima),
				sm_port_map_shima);
		else
			bolero_set_port_map(bolero_component,
				ARRAY_SIZE(sm_port_map), sm_port_map);
	} else {
		bolero_set_port_map(bolero_component, ARRAY_SIZE(sm_port_map),
				 sm_port_map);
	}

	if (ret < 0) {
		dev_err(component->dev, "%s: add codec specific snd controls failed: %d\n",
			__func__, ret);
		return ret;
	}

	codec_reg_done = true;
	return 0;
}

static void msm_i2s_auxpcm_init(struct platform_device *pdev)
{
	int count = 0;
	u32 mi2s_master_slave[MI2S_MAX];
	int ret = 0;

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_init(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
		mi2s_intf_conf[count].audio_core_vote = false;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-mi2s-master",
			mi2s_master_slave, MI2S_MAX);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: no qcom,msm-mi2s-master in DT node\n",
			__func__);
	} else {
		for (count = 0; count < MI2S_MAX; count++) {
			mi2s_intf_conf[count].msm_is_mi2s_master =
				mi2s_master_slave[count];
		}
	}
}

static void msm_i2s_auxpcm_deinit(void)
{
	int count = 0;

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_destroy(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
		mi2s_intf_conf[count].msm_is_mi2s_master = 0;
		mi2s_intf_conf[count].audio_core_vote = false;
	}
}

static int lahaina_ssr_enable(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	int ret = 0;

	if (!card) {
		dev_err(dev, "%s: card is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if (!strcmp(card->name, "lahaina-stub-snd-card")) {
		/* TODO */
		dev_dbg(dev, "%s: TODO \n", __func__);
	}

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
	snd_soc_card_change_online_state(card, 1);
#endif /* CONFIG_AUDIO_QGKI */
	dev_dbg(dev, "%s: setting snd_card to ONLINE\n", __func__);

err:
	return ret;
}

static void lahaina_ssr_disable(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	if (!card) {
		dev_err(dev, "%s: card is NULL\n", __func__);
		return;
	}

	dev_dbg(dev, "%s: setting snd_card to OFFLINE\n", __func__);
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
	snd_soc_card_change_online_state(card, 0);
#endif /* CONFIG_AUDIO_QGKI */

	if (!strcmp(card->name, "lahaina-stub-snd-card")) {
		/* TODO */
		dev_dbg(dev, "%s: TODO \n", __func__);
	}
}

static const struct snd_event_ops lahaina_ssr_ops = {
	.enable = lahaina_ssr_enable,
	.disable = lahaina_ssr_disable,
};

static int msm_audio_ssr_compare(struct device *dev, void *data)
{
	struct device_node *node = data;

	dev_dbg(dev, "%s: dev->of_node = 0x%p, node = 0x%p\n",
		__func__, dev->of_node, node);
	return (dev->of_node && dev->of_node == node);
}

static int msm_audio_ssr_register(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct snd_event_clients *ssr_clients = NULL;
	struct device_node *node = NULL;
	int ret = 0;
	int i = 0;

	for (i = 0; ; i++) {
		node = of_parse_phandle(np, "qcom,msm_audio_ssr_devs", i);
		if (!node)
			break;
		snd_event_mstr_add_client(&ssr_clients,
					msm_audio_ssr_compare, node);
	}

	ret = snd_event_master_register(dev, &lahaina_ssr_ops,
					ssr_clients, NULL);
	if (!ret)
		snd_event_notify(dev, SND_EVENT_UP);

	return ret;
}

static void parse_cps_configuration(struct platform_device *pdev,
			struct msm_asoc_mach_data *pdata)
{
	int ret = 0;
	int i = 0, j = 0;
	u32 dt_values[MAX_CPS_LEVELS];

	if (!pdev || !pdata || !pdata->wsa_max_devs)
		return;

	pdata->get_wsa_dev_num = wsa883x_codec_get_dev_num;
	pdata->cps_config.hw_reg_cfg.num_spkr = pdata->wsa_max_devs;

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,cps_reg_phy_addr", dt_values,
				sizeof(dt_values)/sizeof(dt_values[0]));
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,cps_reg_phy_addr");
	} else {
		pdata->cps_config.hw_reg_cfg.lpass_wr_cmd_reg_phy_addr =
								dt_values[0];
		pdata->cps_config.hw_reg_cfg.lpass_rd_cmd_reg_phy_addr =
								dt_values[1];
		pdata->cps_config.hw_reg_cfg.lpass_rd_fifo_reg_phy_addr =
								dt_values[2];
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,cps_threshold_levels", dt_values,
				sizeof(dt_values)/sizeof(dt_values[0]) - 1);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,cps_threshold_levels");
	} else {
		pdata->cps_config.hw_reg_cfg.vbatt_lower2_threshold =
								dt_values[0];
		pdata->cps_config.hw_reg_cfg.vbatt_lower1_threshold =
								dt_values[1];
	}

	pdata->cps_config.spkr_dep_cfg = devm_kzalloc(&pdev->dev,
				    sizeof(struct lpass_swr_spkr_dep_cfg_t)
				    * pdata->wsa_max_devs, GFP_KERNEL);
	if (!pdata->cps_config.spkr_dep_cfg) {
		dev_err(&pdev->dev, "%s: spkr dep cfg alloc failed\n", __func__);
		return;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,cps_wsa_vbatt_temp_reg_addr", dt_values,
				sizeof(dt_values)/sizeof(dt_values[0]) - 1);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,cps_wsa_vbatt_temp_reg_addr");
	} else {
		for (i = 0; i < pdata->wsa_max_devs; i++) {
			pdata->cps_config.spkr_dep_cfg[i].vbatt_pkd_reg_addr =
								dt_values[0];
			pdata->cps_config.spkr_dep_cfg[i].temp_pkd_reg_addr =
								dt_values[1];
		}
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,cps_normal_values", dt_values,
				sizeof(dt_values)/sizeof(dt_values[0]));
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,cps_normal_values");
	} else {
		for (i = 0; i < pdata->wsa_max_devs; i++) {
			for (j = 0; j < MAX_CPS_LEVELS; j++) {
				pdata->cps_config.spkr_dep_cfg[i].
					value_normal_thrsd[j] = dt_values[j];
			}
		}
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,cps_lower1_values", dt_values,
				sizeof(dt_values)/sizeof(dt_values[0]));
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,cps_lower1_values");
	} else {
		for (i = 0; i < pdata->wsa_max_devs; i++) {
			for (j = 0; j < MAX_CPS_LEVELS; j++) {
				pdata->cps_config.spkr_dep_cfg[i].
					value_low1_thrsd[j] = dt_values[j];
			}
		}
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,cps_lower2_values", dt_values,
				sizeof(dt_values)/sizeof(dt_values[0]));
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,cps_lower2_values");
	} else {
		for (i = 0; i < pdata->wsa_max_devs; i++) {
			for (j = 0; j < MAX_CPS_LEVELS; j++) {
				pdata->cps_config.spkr_dep_cfg[i].
					value_low2_thrsd[j] = dt_values[j];
			}
		}
	}
}

#ifndef ENABLE_WSA
static struct snd_soc_codec_conf msm_codec_conf[] = {
	{ "cs35l41.3-0040", NULL, "L" },
	{ "cs35l41.3-0041", NULL, "R" },
};
#endif

static int msm_parse_ext_mclk_gpios(struct snd_soc_card *card,
				    struct ext_mclk_gpio_info **ext_mclk_gpios)
{
	int ret = 0;
	uint32_t len = 0;
	uint32_t num_gpios = 0;
	struct device_node *np = NULL;
	struct ext_mclk_gpio_info *gpio_info = NULL;
	const char *ext_mclk_muxsel_str = NULL;
	u32 ext_mclk_muxsel_val = 0;
	int i = 0;

	if (!card || !card->dev || !card->dev->of_node)
		return -EINVAL;

	np = card->dev->of_node;

	if (!of_get_property(np, "qcom,ext-mclk-gpios", &len)) {
		dev_err(card->dev, "%s: ext mclk gpios not found in DT\n", __func__);
		return -EINVAL;
	}

	if (!len) {
		dev_err(card->dev, "%s: invalid ext mclk gpios configuration in DT\n",
		       __func__);
		return -EINVAL;
	}

	num_gpios = len / sizeof(uint32_t);
	dev_dbg(card->dev, "%d ext mclk gpios found\n", num_gpios);

	gpio_info = devm_kzalloc(card->dev,
				 num_gpios * sizeof(struct ext_mclk_gpio_info),
				 GFP_KERNEL);
	if (!gpio_info)
		return -ENOMEM;

	for (i = 0; i < num_gpios; i++) {
		mutex_init(&gpio_info[i].lock);
		gpio_info[i].ref_cnt = 0;
		gpio_info[i].gpio_p = of_parse_phandle(np, "qcom,ext-mclk-gpios", i);
		if (!gpio_info[i].gpio_p) {
			dev_err(card->dev, "ext mclk gpio %d device node is NULL", i);
			ret = -EINVAL;
			goto free_gpio_info;
		}

		/* aud_ref_mux is present only for LPI GPIOs on lahaina.
		 * Hence we mandate parsing of mux config only for LPI GPIOs.
		 * Review the existence of aud_ref_mux for LPI/TLMM GPIOs 
		 * while porting this change to other platforms
		 */
		if (of_property_read_bool(gpio_info[i].gpio_p, "qcom,lpi-gpios")) {
			ret = of_property_read_string(gpio_info[i].gpio_p,
					"qcom,ext-mclk-muxsel-str", &ext_mclk_muxsel_str);
			if (ret) {
				dev_err(card->dev, "%s: qcom,ext-mclk-muxsel-str not found in DT\n",
						__func__);
				ret = -EINVAL;
				goto free_gpio_info;
			} else {
				ret = of_property_read_u32(gpio_info[i].gpio_p,
						"qcom,ext-mclk-muxsel-val", &ext_mclk_muxsel_val);
				if (ret) {
					dev_err(card->dev, "%s: qcom,ext-mclk-muxsel-val not found in DT\n",
						__func__);
					ret = -EINVAL;
					goto free_gpio_info;
				}
			}
			dev_dbg(card->dev, "%s: gpio_info[%d] - muxsel= %s,  val= %u\n",
					__func__, i, ext_mclk_muxsel_str, ext_mclk_muxsel_val);
			gpio_info[i].ext_mclk_muxsel_str = ext_mclk_muxsel_str;
			gpio_info[i].ext_mclk_muxsel_val = ext_mclk_muxsel_val;
		}
	}

	num_ext_mclk_gpios = num_gpios;
	*ext_mclk_gpios = gpio_info;
	dev_dbg(card->dev, "%s: ext mclk gpios probe successful\n", __func__);
	return 0;

free_gpio_info:
	for (; i >= 0; i--) {
		mutex_destroy(&gpio_info[i].lock);
		of_node_put(gpio_info[i].gpio_p);
	}
	devm_kfree(card->dev, gpio_info);
	gpio_info = NULL;

	*ext_mclk_gpios = NULL;
	return ret;
}

static int msm_parse_ext_mclk_srcs(struct snd_soc_card *card,
				   struct ext_mclk_src_cfg **ext_mclk_src,
				   uint32_t *n_srcs)
{
	int ret = 0;
	struct ext_mclk_src_cfg *src_cfg = NULL;
	struct device_node *np = NULL;
	uint32_t len = 0;
	uint32_t num_srcs = 0;
	uint32_t cells = 0;

	if (!card || !card->dev || !card->dev->of_node)
		return -EINVAL;

	np = card->dev->of_node;

	if (!of_get_property(np, "qcom,ext-mclk-srcs", &len)) {
		dev_err(card->dev, "%s: ext mclk srcs cfg not found in DT\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "#ext-mclk-srcs-cells", &cells);
	if (ret) {
		dev_err(card->dev, "%s: ext mclk srcs cells not found in DT\n", __func__);
		return ret;
	}

	if (!len || (len % (cells * sizeof(uint32_t))) ||
					(cells != MCLK_SRCS_CELLS)) {
		dev_err(card->dev, "%s: invalid ext mclk srcs cfg in DT\n", __func__);
		return -EINVAL;
	};

	num_srcs = len / (cells * sizeof(uint32_t));

	src_cfg = devm_kzalloc(card->dev,
			       num_srcs * sizeof(struct ext_mclk_src_cfg),
			       GFP_KERNEL);
	if (!src_cfg)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "qcom,ext-mclk-srcs", (u32 *)src_cfg,
					 cells * num_srcs);
	if (ret) {
		dev_err(card->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,ext-mclk-srcs");
		ret = -EINVAL;
		goto free_mclk_array;
	}

	*ext_mclk_src = src_cfg;
	*n_srcs = num_srcs;
	dev_dbg(card->dev, "%s: ext mclk srcs probe successful\n", __func__);
	return 0;

free_mclk_array:
	devm_kfree(card->dev, src_cfg);
	src_cfg = NULL;

	*ext_mclk_src = NULL;
	*n_srcs = 0;
	return ret;
}

static int msm_parse_dt_ext_mclk_cfg(struct snd_soc_card *card,
							  enum afe_mclk_freq freq)
{
	int ret = 0;
	struct ext_mclk_freq_cfg *mclk_cfg = NULL;
	uint32_t len = 0;
	uint32_t num_cfg = 0;
	uint32_t cells = 0;
	int i = 0;
	struct device_node *np = NULL;
	uint32_t *freq_cfg_tbl = NULL;

	if (!card || !card->dev || !card->dev->of_node)
		return -EINVAL;

	np = card->dev->of_node;

	if (!of_get_property(np, ext_mclk_freq_info[freq].prop, &len)) {
		dev_dbg(card->dev, "%s: ext mclk cfg for %s not found in DT\n",
			 __func__, ext_mclk_freq_info[freq].prop);
		return 0;
	}

	ret = of_property_read_u32(np, "#ext-mclk-cfg-cells", &cells);
	if (ret) {
		dev_err(card->dev, "%s: ext mclk cfg cells not found in DT\n", __func__);
		return ret;
	}

	if (!len || (len % (cells * sizeof(uint32_t))) ||
					(cells != MCLK_CFG_CELLS)) {
		dev_err(card->dev, "%s: invalid mclk configuration in DT\n", __func__);
		return -EINVAL;
	};

	num_cfg = len / (cells * sizeof(uint32_t));
	mclk_cfg = devm_kzalloc(card->dev, num_cfg * sizeof(struct ext_mclk_freq_cfg),
				GFP_KERNEL);
	if (!mclk_cfg)
		return -ENOMEM;

	freq_cfg_tbl = devm_kzalloc(card->dev, cells * num_cfg * sizeof(uint32_t),
				    GFP_KERNEL);
	if (!freq_cfg_tbl) {
		ret = -ENOMEM;
		goto free_mclk_cfg;
	}

	ret = of_property_read_u32_array(np, ext_mclk_freq_info[freq].prop,
					 freq_cfg_tbl, cells * num_cfg);
	if (ret)
		goto free_freq_cfg_tbl;

	dev_dbg(card->dev, "table for %u freq\n",
		ext_mclk_freq_info[freq].mclk_freq);

	for (i = 0; i < num_cfg; i++) {
		memcpy(&mclk_cfg[i], &freq_cfg_tbl[i * cells],
		       sizeof(uint32_t) * cells);
		dev_dbg(card->dev,
			"clk freq (Hz): %u, div2x: %u, m: %u, n: %u, d: %u\n",
			mclk_cfg[i].clk_freq, mclk_cfg[i].div2x, mclk_cfg[i].m,
			mclk_cfg[i].n, mclk_cfg[i].d);
	}

	ext_mclk_freq_info[freq].mclk_cfg = mclk_cfg;
	ext_mclk_freq_info[freq].num_mclk_cfg = num_cfg;

	devm_kfree(card->dev, freq_cfg_tbl);
	freq_cfg_tbl = NULL;

	return 0;

free_freq_cfg_tbl:
	devm_kfree(card->dev, freq_cfg_tbl);
	freq_cfg_tbl = NULL;
free_mclk_cfg:
	devm_kfree(card->dev, mclk_cfg);
	mclk_cfg = NULL;

	return ret;
}

static int msm_parse_ext_mclk_freq_cfg(struct snd_soc_card *card)
{
	int ret = 0;
	int i = MCLK_FREQ_MIN;

	if (!card || !card->dev || !card->dev->of_node)
		return -EINVAL;

	for (i = MCLK_FREQ_MIN; i < MCLK_FREQ_MAX; i++) {
		ret = msm_parse_dt_ext_mclk_cfg(card, i);
		if (ret < 0)
			return ret;
	}

	dev_dbg(card->dev, "%s: ext mclk freq probe successful!\n", __func__);
	return ret;
}

static void lahaina_ext_mclk_cfg_deinit(struct snd_soc_card *card)
{
	struct msm_asoc_mach_data *pdata = NULL;
	int i = 0;
	enum afe_mclk_freq freq = MCLK_FREQ_MIN;

	if (!card || !card->dev)
		return;

	pdata = (struct msm_asoc_mach_data *) snd_soc_card_get_drvdata(card);
	if (!pdata)
		return;

	afe_unregister_ext_mclk_cb();

	for (i = 0; i < pdata->num_ext_mclk_srcs; i++)
		pdata->ext_mclk_srcs[i].gpio_info = NULL;

	pdata->num_ext_mclk_srcs = 0;

	if (ext_mclk_gpio_info) {
		for (i = 0; i < num_ext_mclk_gpios; i++) {
			mutex_destroy(&ext_mclk_gpio_info[i].lock);
			of_node_put(ext_mclk_gpio_info[i].gpio_p);
		}
		devm_kfree(card->dev, ext_mclk_gpio_info);
		ext_mclk_gpio_info = NULL;
	}

	if (ext_mclk_src_info) {
		devm_kfree(card->dev, ext_mclk_src_info);
		ext_mclk_src_info = NULL;
	}

	for (freq = MCLK_FREQ_MIN; freq < MCLK_FREQ_MAX; freq++) {
		if (ext_mclk_freq_info[freq].mclk_cfg)
			devm_kfree(card->dev, ext_mclk_freq_info[freq].mclk_cfg);
		ext_mclk_freq_info[freq].mclk_cfg = NULL;
		ext_mclk_freq_info[freq].num_mclk_cfg = 0;
	}
}

static int lahaina_ext_mclk_init_controls(struct snd_soc_card *card)
{
	int ret = 0;

	if (!card) {
		pr_err("%s: sound card is NULL\n", __func__);
		return -EINVAL;
	}

	ret = snd_soc_add_card_controls(card, msm_ext_mclk_controls,
					ARRAY_SIZE(msm_ext_mclk_controls));
	if (ret < 0) {
		dev_err(card->dev, "%s: add_card_controls failed for ext mclk ctls: %d\n",
			__func__, ret);
		return ret;
	}

	return ret;
}

static int lahaina_ext_mclk_cfg_init(struct snd_soc_card *card)
{
	int ret = 0;
	struct msm_asoc_mach_data *pdata = NULL;
	uint32_t num_ext_mclk_srcs = 0;
	int i = 0;

	if (!card || !card->dev || !card->dev->of_node)
		return -EINVAL;

	pdata = (struct msm_asoc_mach_data *) snd_soc_card_get_drvdata(card);
	if (!pdata)
		return -EINVAL;

	ret = msm_parse_ext_mclk_gpios(card, &ext_mclk_gpio_info);
	if (ret) {
		dev_err(card->dev, "%s: unable to parse ext mclk gpios, ret: %d\n",
		       __func__, ret);
		goto deinit;
	}

	ret = msm_parse_ext_mclk_srcs(card, &ext_mclk_src_info,
				      &num_ext_mclk_srcs);
	if (ret) {
		dev_err(card->dev, "%s: unable to parse ext mclk srcs, ret: %d\n",
		       __func__, ret);
		goto deinit;
	}

	ret = msm_parse_ext_mclk_freq_cfg(card);
	if (ret) {
		dev_err(card->dev, "%s: unable to parse ext mclk freq cfg, ret: %d\n",
		       __func__, ret);
		goto deinit;
	}

	pdata->num_ext_mclk_srcs = num_ext_mclk_srcs;

	if (pdata->num_ext_mclk_srcs) {
		pdata->ext_mclk_srcs = devm_kzalloc(card->dev,
			num_ext_mclk_srcs * sizeof(struct ext_mclk_src_info),
			GFP_KERNEL);
		if (!pdata->ext_mclk_srcs) {
			pdata->num_ext_mclk_srcs = 0;
			ret = -ENOMEM;
			goto deinit;
		}

		for (i = 0; i < num_ext_mclk_srcs; i++) {
			pdata->ext_mclk_srcs[i].clk_id = ext_mclk_src_info[i].clk_id;
			pdata->ext_mclk_srcs[i].clk_root = ext_mclk_src_info[i].clk_root;
			pdata->ext_mclk_srcs[i].gpio_info = &ext_mclk_gpio_info[ext_mclk_src_info[i].gpio_idx];

			dev_dbg(card->dev, "%s: clk id: 0x%x clk root: 0x%x gpio idx: %d\n",
				__func__, pdata->ext_mclk_srcs[i].clk_id,
				pdata->ext_mclk_srcs[i].clk_root,
				ext_mclk_src_info[i].gpio_idx);
		}

		ret = lahaina_ext_mclk_init_controls(card);
		if (ret) {
			dev_err(card->dev, "%s: Could not init ext mclk mixer ctls, ret: %d\n",
			       __func__, ret);
			goto deinit;
		}

		ret = afe_register_ext_mclk_cb(lahaina_enable_and_get_mclk_cfg,
					       (void *)card);
		if (ret) {
			dev_err(card->dev, "%s: Could not register afe ext mclk cb, ret: %d\n",
			       __func__, ret);
			goto deinit;
		}
	}

	dev_dbg(card->dev, "%s: ext mclk init successful!\n", __func__);

	return 0;

deinit:
	lahaina_ext_mclk_cfg_deinit(card);
	return ret;
}

static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL;
	struct msm_asoc_mach_data *pdata = NULL;
	const char *mbhc_audio_jack_type = NULL;
	int ret = 0;
	uint index = 0;
	struct clk *lpass_audio_hw_vote = NULL;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "%s: No platform supplied from device tree\n", __func__);
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node,
				"qcom,lito-is-v2-enabled",
				&pdata->lito_v2_enabled);

	of_property_read_u32(pdev->dev.of_node,
				"qcom,wcd-disabled",
				&pdata->wcd_disabled);

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "%s: parse card name failed, err:%d\n",
			__func__, ret);
		goto err;
	}

	ret = snd_soc_of_parse_audio_routing(card, "qcom,audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "%s: parse audio routing failed, err:%d\n",
			__func__, ret);
		goto err;
	}

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	/* Get maximum WSA device count for this platform */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,wsa-max-devs", &pdata->wsa_max_devs);
	if (ret) {
		dev_info(&pdev->dev,
			 "%s: wsa-max-devs property missing in DT %s, ret = %d\n",
			 __func__, pdev->dev.of_node->full_name, ret);
		pdata->wsa_max_devs = 0;
	}

#ifndef ENABLE_WSA
	card->codec_conf = msm_codec_conf;
	card->num_configs = sizeof(msm_codec_conf) / sizeof(msm_codec_conf[0]);
#endif

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret == -EPROBE_DEFER) {
		if (codec_reg_done)
			ret = -EINVAL;
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "%s: snd_soc_register_card failed (%d)\n",
			__func__, ret);
		goto err;
	}
	dev_info(&pdev->dev, "%s: Sound card %s registered\n",
		 __func__, card->name);

	/* Enable ext clk support ONLY after sound card enumeration and
	 * registration has occurred with the internal clock
	 */
	if (of_property_read_bool(pdev->dev.of_node, "qcom,supports-ext-mclk")) {
		ret = lahaina_ext_mclk_cfg_init(card);
		if (ret) {
			dev_err(&pdev->dev, "%s: ext mclk cfg init from DT failed: %d\n",
					__func__, ret);
			goto err;
		}
		pdata->supports_ext_mclk = 1;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,tdm-max-slots",
				   &pdata->tdm_max_slots);
	if (ret) {
		dev_err(&pdev->dev, "%s: No DT match for tdm max slots\n",
			__func__);
	}
	if ((pdata->tdm_max_slots <= 0) || (pdata->tdm_max_slots >
	    TDM_MAX_SLOTS)) {
		pdata->tdm_max_slots = TDM_MAX_SLOTS;
		dev_err(&pdev->dev, "%s: Using default tdm max slot: %d\n",
			__func__, pdata->tdm_max_slots);
	}

	pdata->hph_en1_gpio_p = of_parse_phandle(pdev->dev.of_node,
						"qcom,hph-en1-gpio", 0);
	if (!pdata->hph_en1_gpio_p) {
		dev_dbg(&pdev->dev, "%s: property %s not detected in node %s\n",
			__func__, "qcom,hph-en1-gpio",
			pdev->dev.of_node->full_name);
	}

	pdata->hph_en0_gpio_p = of_parse_phandle(pdev->dev.of_node,
						"qcom,hph-en0-gpio", 0);
	if (!pdata->hph_en0_gpio_p) {
		dev_dbg(&pdev->dev, "%s: property %s not detected in node %s\n",
			__func__, "qcom,hph-en0-gpio",
			pdev->dev.of_node->full_name);
	}

	ret = of_property_read_string(pdev->dev.of_node,
		"qcom,mbhc-audio-jack-type", &mbhc_audio_jack_type);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: Looking up %s property in node %s failed\n",
			__func__, "qcom,mbhc-audio-jack-type",
			pdev->dev.of_node->full_name);
		dev_dbg(&pdev->dev, "Jack type properties set to default\n");
	} else {
		if (!strcmp(mbhc_audio_jack_type, "4-pole-jack")) {
			wcd_mbhc_cfg.enable_anc_mic_detect = false;
			dev_dbg(&pdev->dev, "This hardware has 4 pole jack");
		} else if (!strcmp(mbhc_audio_jack_type, "5-pole-jack")) {
			wcd_mbhc_cfg.enable_anc_mic_detect = true;
			dev_dbg(&pdev->dev, "This hardware has 5 pole jack");
		} else if (!strcmp(mbhc_audio_jack_type, "6-pole-jack")) {
			wcd_mbhc_cfg.enable_anc_mic_detect = true;
			dev_dbg(&pdev->dev, "This hardware has 6 pole jack");
		} else {
			wcd_mbhc_cfg.enable_anc_mic_detect = false;
			dev_dbg(&pdev->dev, "Unknown value, set to default\n");
		}
	}
	/*
	 * Parse US-Euro gpio info from DT. Report no error if us-euro
	 * entry is not found in DT file as some targets do not support
	 * US-Euro detection
	 */
	pdata->us_euro_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,us-euro-gpios", 0);
	if (!pdata->us_euro_gpio_p) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,us-euro-gpios", pdev->dev.of_node->full_name);
	} else {
		dev_dbg(&pdev->dev, "%s detected\n",
			"qcom,us-euro-gpios");
		wcd_mbhc_cfg.swap_gnd_mic = msm_swap_gnd_mic;
	}

	if (wcd_mbhc_cfg.enable_usbc_analog)
		wcd_mbhc_cfg.swap_gnd_mic = msm_usbc_swap_gnd_mic;

	pdata->fsa_handle = of_parse_phandle(pdev->dev.of_node,
					"fsa4480-i2c-handle", 0);
	if (!pdata->fsa_handle)
		dev_dbg(&pdev->dev, "property %s not detected in node %s\n",
			"fsa4480-i2c-handle", pdev->dev.of_node->full_name);

	msm_i2s_auxpcm_init(pdev);
	pdata->dmic01_gpio_p = of_parse_phandle(pdev->dev.of_node,
					      "qcom,cdc-dmic01-gpios",
					       0);
	pdata->dmic23_gpio_p = of_parse_phandle(pdev->dev.of_node,
					      "qcom,cdc-dmic23-gpios",
					       0);
	pdata->dmic45_gpio_p = of_parse_phandle(pdev->dev.of_node,
					      "qcom,cdc-dmic45-gpios",
					       0);
	if (pdata->dmic01_gpio_p)
		msm_cdc_pinctrl_set_wakeup_capable(pdata->dmic01_gpio_p, false);
	if (pdata->dmic23_gpio_p)
		msm_cdc_pinctrl_set_wakeup_capable(pdata->dmic23_gpio_p, false);
	if (pdata->dmic45_gpio_p)
		msm_cdc_pinctrl_set_wakeup_capable(pdata->dmic45_gpio_p, false);

	pdata->mi2s_gpio_p[PRIM_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,pri-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[SEC_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,sec-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[TERT_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,tert-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[QUAT_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,quat-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[QUIN_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,quin-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[SEN_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,sen-mi2s-gpios", 0);
	for (index = PRIM_MI2S; index < MI2S_MAX; index++)
		atomic_set(&(pdata->mi2s_gpio_ref_count[index]), 0);

	/* parse cps configuration from dt */
	parse_cps_configuration(pdev, pdata);

	/* Register LPASS audio hw vote */
	lpass_audio_hw_vote = devm_clk_get(&pdev->dev, "lpass_audio_hw_vote");
	if (IS_ERR(lpass_audio_hw_vote)) {
		ret = PTR_ERR(lpass_audio_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_audio_hw_vote", ret);
		lpass_audio_hw_vote = NULL;
		ret = 0;
	}
	pdata->lpass_audio_hw_vote = lpass_audio_hw_vote;
	pdata->core_audio_vote_count = 0;

	ret = msm_audio_ssr_register(&pdev->dev);
	if (ret)
		pr_err("%s: Registration with SND event FWK failed ret = %d\n",
			__func__, ret);

	is_initial_boot = true;

	/* Add QoS request for audio tasks */
	msm_audio_add_qos_request();

	return 0;
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(card);
	if (pdata && pdata->supports_ext_mclk)
		lahaina_ext_mclk_cfg_deinit(card);
	snd_event_master_deregister(&pdev->dev);
	snd_soc_unregister_card(card);
	msm_i2s_auxpcm_deinit();
	msm_audio_remove_qos_request();

	return 0;
}

static struct platform_driver lahaina_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = lahaina_asoc_machine_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};
module_platform_driver(lahaina_asoc_machine_driver);

MODULE_SOFTDEP("pre: bt_fm_slim");
MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, lahaina_asoc_machine_of_match);
