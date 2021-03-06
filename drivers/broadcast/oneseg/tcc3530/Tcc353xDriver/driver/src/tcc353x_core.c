/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_core.c                                          */
/*    Description : core Function                                           */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include "tcpal_os.h"
#include "tcpal_i2c.h"
#include "tcpal_spi.h"
#include "tcc353x_mailbox.h"
#include "tcc353x_core.h"
#include "tcc353x_register_control.h"
#include "tcc353x_rf.h"
#include "tcc353x_isdb.h"
#include "tcc353x_command_control.h"

/* static functions */
static I32S Tcc353xColdbootParserUtil(I08U * pData, I32U size,
				      Tcc353xBoot_t * pBOOTBin);
static void Tcc353xSetGpio(Tcc353xHandle_t * _handle);
static I32S Tcc353xSetPll(I32S _moduleIndex, I32S _deviceIndex, 
			  I32S _flagInterfaceLock, I16U _pllValue);
static I32S Tcc353xChangePll(I32S _moduleIndex, I16U _pllValue);
static void Tcc353xSaveOption(I32S _moduleIndex, I32S _diversityIndex,
			      Tcc353xOption_t * _Tcc353xOption);
static void Tcc353xConnectCommandInterface(I32S _moduleIndex, 
					   I32S _diversityIndex,
					   I08S _commandInterface);
static void Tcc353xSetInterruptControl(Tcc353xHandle_t * _handle);
static void Tcc353xOutBufferConfig(Tcc353xHandle_t * _handle);
static void Tcc353xSetStreamControl(Tcc353xHandle_t * _handle);
static I32S Tcc353xCodeDownload(I32S _moduleIndex, I08U * _coldbootData,
				I32S _codeSize);
static I32S Tcc353xCodeCrcCheck(I32S _moduleIndex, I08U * _coldbootData,
				Tcc353xBoot_t * _boot);
static void Tcc353xGetOpconfigValues(I32S _moduleIndex,
				     I32S _diversityIndex,
				     Tcc353xTuneOptions * _tuneOption,
				     I32U * _opConfig,
				     I32U _frequencyInfo);
static I32U Tcc353xDspRestart(I32S _moduleIndex, I32S _diversityIndex);
static I32S Tcc353xInitBroadcasting(I32S _moduleIndex,
				    I08U * _coldbootData, I32S _codeSize);
static I32S Tcc353xAttach(I32S _moduleIndex,
			  Tcc353xOption_t * _Tcc353xOption);
extern I64U Tcc353xDiv64(I64U x, I64U y);
extern I32S Tcc353xRfInit(I32S _moduleIndex, I32S _diversityIndex);
static I32S Tcc353xStreamStartPrepare(I32S _moduleIndex);
static I32S Tcc353xStreamOn (I32S _moduleIndex);
static I32S Tcc353xDspEpReopenForStreamStart(I32S _moduleIndex, 
					     I32S _diversityIndex);
static I32S Tcc353xSetOpConfig(I32S _moduleIndex, I32S _diversityIndex,
			I32U * op_cfg, I32U _firstFlag);
static I32U Tcc353xSearchDpllValue (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I32U _defaultPll);
static I32U Tcc353xSearchDpllTable (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I64U *_rcStep, 
				    I32U *_adcClkCfg, I32U _defaultPll);
static I32S Tcc353xSendStoppingCommand(I32S _moduleIndex);

#ifndef Bit0
#define Bit31       0x80000000
#define Bit30       0x40000000
#define Bit29       0x20000000
#define Bit28       0x10000000
#define Bit27       0x08000000
#define Bit26       0x04000000
#define Bit25       0x02000000
#define Bit24       0x01000000
#define Bit23       0x00800000
#define Bit22       0x00400000
#define Bit21       0x00200000
#define Bit20       0x00100000
#define Bit19       0x00080000
#define Bit18       0x00040000
#define Bit17       0x00020000
#define Bit16       0x00010000
#define Bit15       0x00008000
#define Bit14       0x00004000
#define Bit13       0x00002000
#define Bit12       0x00001000
#define Bit11       0x00000800
#define Bit10       0x00000400
#define Bit9        0x00000200
#define Bit8        0x00000100
#define Bit7        0x00000080
#define Bit6        0x00000040
#define Bit5        0x00000020
#define Bit4        0x00000010
#define Bit3        0x00000008
#define Bit2        0x00000004
#define Bit1        0x00000002
#define Bit0        0x00000001
#define BitNONE     0x00000000
#endif

#ifndef BITSET
#define	BITSET(X, MASK)				( (X) |= (I32U)(MASK) )
#endif
#ifndef BITCLR
#define	BITCLR(X, MASK)				( (X) &= ~((I32U)(MASK)) )
#endif

#define SCALE       22
#define FIXED(x)    (x<<SCALE)
#define MUL(A,B)    ((A*B)>>SCALE)
#define DIV(A,B)    (Tcc353xDiv64((A<<SCALE), B))

#define PLL_ISDB_T_FULLSEG              0xB08E  /* 117.6MHz */
#define PLL_ISDB_T_FULLSEG_31           0xB18E  /* 120.0MHz */
#define PLL_ISDB_T_FULLSEG_30           0xB08E  /* 117.6MHz */
#define PLL_ISDB_T_FULLSEG_2F           0xAF8E  /* 115.2MHz */
#define PLL_ISDB_T_FULLSEG_2E           0xAE8E  /* 112.8MHz */
#define PLL_ISDB_T_FULLSEG_2D           0xAD8E  /* 110.4MHz */
#define PLL_ISDB_T_FULLSEG_2C           0xAC8E  /* 108.0MHz */
#define PLL_ISDB_T_FULLSEG_2B           0xAB8E  /* 105.6MHz */
#define PLL_ISDB_T_FULLSEG_2A           0xAA8E  /* 103.2MHz */

#define PLL_ISDB_T_PARTIAL_1_SEG        0x9816  /*  40.0MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_384    0x8f0E  /*  38.4MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_408    0x900E  /*  40.8MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_416    0x9916  /*  41.6MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_432    0x910E  /*  43.2MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_456    0x920E  /*  45.6MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_464    0x9c16  /*  46.4MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_496    0x9e16  /*  49.6MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_512    0x9f16  /*  51.2MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_528    0xa016  /*  52.8MHz */

#define PLL_ISDB_TMM_FULLSEG            0xAA16  /*  68.8MHz */

#define PLL_ISDB_TMM_PARTIAL_1_SEG      0x8f0E  /*  38.4MHz */
#define PLL_ISDB_TMM_PARTIAL_1_SEG_384  0x8f0E  /*  38.4MHz */
#define PLL_ISDB_TMM_PARTIAL_1_SEG_456  0x920E  /*  45.6MHz */

#define PLL_ISDB_TSB                    0x9816  /*  40.0MHz */

/* Driver core version 8bit.8bit.8bit */
I32U Tcc353xCoreVersion = ((0<<16) | (1<<8) | (43));	/* version 8bit.8bit.8bit */

Tcc353xHandle_t Tcc353xHandle[TCC353X_MAX][TCC353X_DIVERSITY_MAX];// = { {0, }, {0, } };

TcpalSemaphore_t Tcc353xInterfaceSema;
I32U *pTcc353xInterfaceSema = NULL;

I32U Tcc353xCurrentDiversityCount[TCC353X_MAX];

TcpalSemaphore_t Tcc353xMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
TcpalSemaphore_t Tcc353xOpMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
Tcc353xRegisterConfig_t
    Tcc353xRegisterOptions[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

I32U OriginalOpConfig[TCC353X_MAX][TCC353X_DIVERSITY_MAX][16];

/* spur suppression tables */
#define _MAX_TMM_1SEG_FREQ_NUM_    22
I32U DpllTable_TMM_1SEG [_MAX_TMM_1SEG_FREQ_NUM_ * 5] = {
	/* please align low frequency to high frequency */
	/* start frequency, Pll, RC STEP_H, RC_STEP_L, ADC Clk */
	/* Pattern-A */
	207857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	208286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	208714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	209143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	209571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	210000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	210429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	
	/* Pattern-B */
	213429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	213857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	214286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	214714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	215143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	215571, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	216000, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	
	/* Pattern-C */
	219000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	219429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	219857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	220286, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	220714, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	221143, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	221571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	
	0,	PLL_ISDB_TMM_PARTIAL_1_SEG,	0x00,	0x00000000,	0x00
};

#define _MAX_TMM_13SEG_FREQ_NUM_    7
I32U DpllTable_TMM_13SEG [_MAX_TMM_13SEG_FREQ_NUM_ * 5] = {
	/* please align low frequency to high frequency */
	/* start frequency, Pll, RC STEP_H, RC_STEP_L, ADC Clk */
	/* Pattern-A */
	213429, PLL_ISDB_TMM_FULLSEG,   0x0E,   0x1499D87F,     0x02,
	219000, PLL_ISDB_TMM_FULLSEG,   0x0E,   0x1499D87F,     0x02,

	/* Pattern-B */
	210429, PLL_ISDB_TMM_FULLSEG,   0x0E,   0x1499D87F,     0x02,
	219000, PLL_ISDB_TMM_FULLSEG,   0x0E,   0x1499D87F,     0x02,

	/* Pattern-C */
	210429, PLL_ISDB_TMM_FULLSEG,   0x0E,   0x1499D87F,     0x02,
	216000, PLL_ISDB_TMM_FULLSEG,   0x0E,   0x1499D87F,     0x02,

	0,	PLL_ISDB_TMM_FULLSEG,   0x00,   0x00000000,     0x00
};

#define _MAX_TMM_USER_1SEG_FREQ_NUM_    34
I32U DpllTable_TMM_USER_1SEG [_MAX_TMM_USER_1SEG_FREQ_NUM_ * 5] = {
	/* please align low frequency to high frequency */
	/* start frequency, Pll, RC STEP_H, RC_STEP_L, ADC Clk */
	207857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	208286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	208714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	209143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	209571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	210000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	210429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	210857, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	211286, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	211714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	212143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	212571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	213000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	213429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	213857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	214286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	214714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	215143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	215571, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	216000, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	216429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	216857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	217286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	217714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	218143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	218571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	219000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	219429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	219857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	220286, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	220714, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	221143, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B,	0xDFC6F7F1,	0x0A,
	221571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27,	0x47C9D1F2,	0x28,
	0,	PLL_ISDB_TMM_FULLSEG,	0x00,	0x00000000,	0x00
};

#define _MAX_TMM_USER_13SEG_FREQ_NUM_    34
I32U DpllTable_TMM_USER_13SEG [_MAX_TMM_USER_13SEG_FREQ_NUM_ * 5] = {
	/* please align low frequency to high frequency */
	/* start frequency, Pll, RC STEP_H, RC_STEP_L, ADC Clk */
	207857, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	208286, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	208714, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	209143, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	209571, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	210000, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	210429, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	210857, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	211286, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	211714, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	212143, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	212571, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	213000, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	213429, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	213857, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	214286, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	214714, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	215143, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	215571, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	216000, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	216429, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	216857, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	217286, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	217714, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	218143, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	218571, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	219000, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	219429, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	219857, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	220286, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	220714, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	221143, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	221571, PLL_ISDB_TMM_FULLSEG,	0x0E,	0x1499D87F,	0x02,
	0,	PLL_ISDB_TMM_FULLSEG,	0x00,	0x00000000,	0x00
};

#define _MAX_PARTIAL_1SEG_FREQ_NUM_	93
I32U DpllTable_Partial1Seg [_MAX_PARTIAL_1SEG_FREQ_NUM_ * 5] = {
	/* please align low frequency to high frequency */
	/* start frequency, Pll, RC STEP_H, RC_STEP_L, ADC Clk */

	/* isdb-tmm 1seg */
	207857, PLL_ISDB_TMM_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	208286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	208714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	209143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	209571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	210000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	210429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	213429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	213857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	214286, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	214714, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	215143, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	215571, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B, 0xDFC6F7F1, 0x0A,
	216000, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B, 0xDFC6F7F1, 0x0A,
	219000, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	219429, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	219857, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	220286, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B, 0xDFC6F7F1, 0x0A,
	220714, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B, 0xDFC6F7F1, 0x0A,
	221143, PLL_ISDB_TMM_PARTIAL_1_SEG_456, 0x1B, 0xDFC6F7F1, 0x0A,
	221571, PLL_ISDB_TMM_PARTIAL_1_SEG_384, 0x27, 0x47C9D1F2, 0x28,
	/* ISDB-T 1-seg, VHF japan only */
	93143,	PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	99143,	PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	105143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	173143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	179143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	185143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	191143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	/* ISDB-T 1-seg, VHF brazil only */
	177143, PLL_ISDB_T_PARTIAL_1_SEG_496,	0x2E, 0x4578FCB9, 0x0A,
	183143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	189143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	/* ISDB-T 1-seg, VHF */
	195143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	201143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	207143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	213143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	219143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	/* ISDB-T 1-seg, UHF */
	473143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	479143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	485143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	491143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	497143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	503143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	509143, PLL_ISDB_T_PARTIAL_1_SEG_496,	0x2E, 0x4578FCB9, 0x0A,
	515143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	521143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	527143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	533143, PLL_ISDB_T_PARTIAL_1_SEG_432,	0x0F, 0x335205B8, 0x0A,
	539143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	545143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	551143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	557143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	563143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	569143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	575143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	581143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	587143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	593143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	599143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	605143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	611143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	617143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	623143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	629143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	635143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	641143, PLL_ISDB_T_PARTIAL_1_SEG_512,	0x34, 0xD34D34D3, 0x0A,
	647143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	653143, PLL_ISDB_T_PARTIAL_1_SEG_496,	0x2E, 0x4578FCB9, 0x0A,
	659143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	665143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	671143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	677143, PLL_ISDB_T_PARTIAL_1_SEG_496,	0x2E, 0x4578FCB9, 0x0A,
	683143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	689143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	695143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	701143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	707143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	713143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	719143, PLL_ISDB_T_PARTIAL_1_SEG_432,	0x0F, 0x335205B8, 0x0A,
	725143, PLL_ISDB_T_PARTIAL_1_SEG_416,	0x05, 0xF05F05F0, 0x0A,
	731143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	737143, PLL_ISDB_T_PARTIAL_1_SEG_496,	0x2E, 0x4578FCB9, 0x0A,
	743143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	749143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	755143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	761143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	767143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	773143, PLL_ISDB_T_PARTIAL_1_SEG_464,	0x1F, 0xCEAD7815, 0x0A,
	779143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	785143, PLL_ISDB_T_PARTIAL_1_SEG_408,	0x01, 0x0929ABB3, 0x0A,
	791143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	797143, PLL_ISDB_T_PARTIAL_1_SEG_432,	0x0F, 0x335205B8, 0x0A,
	803143, PLL_ISDB_T_PARTIAL_1_SEG_384,	0x27, 0x47C9D1F2, 0x28,
	0,	PLL_ISDB_TMM_FULLSEG,		0x00, 0x00000000, 0x00
};

#define _MAX_FULLSEG_FREQ_NUM_	129
I32U DpllTable_FullSeg [_MAX_FULLSEG_FREQ_NUM_ * 5] = {
	/* please align low frequency to high frequency */
	/* start frequency, Pll, RC STEP_H, RC_STEP_L, ADC Clk */

	/* isdb-tmm */
	213429, PLL_ISDB_TMM_FULLSEG,	0x0E, 0x1499D87F, 0x02,
	219000, PLL_ISDB_TMM_FULLSEG,	0x0E, 0x1499D87F, 0x02,
	210429, PLL_ISDB_TMM_FULLSEG,	0x0E, 0x1499D87F, 0x02,
	219000, PLL_ISDB_TMM_FULLSEG,	0x0E, 0x1499D87F, 0x02,
	210429, PLL_ISDB_TMM_FULLSEG,	0x0E, 0x1499D87F, 0x02,
	216000, PLL_ISDB_TMM_FULLSEG,	0x0E, 0x1499D87F, 0x02,

	/* isdbt full seg - VHF - Japan only */
	93143,	PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	99143,	PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	105143, PLL_ISDB_T_FULLSEG_31,	0x27, 0x47C9D1F2, 0x21,
	173143,	PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	179143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	185143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	191143, PLL_ISDB_T_FULLSEG_31,	0x2F, 0xF2FF2FF2, 0x03,
	/* isdbt full seg - VHF - Brazil only */
	177143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	183143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	189143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	/* isdbt full seg - VHF */
	195143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	201143, PLL_ISDB_T_FULLSEG_31,	0x27, 0x47C9D1F2, 0x21,
	207143, PLL_ISDB_T_FULLSEG_31,	0x27, 0x47C9D1F2, 0x21,
	213143, PLL_ISDB_T_FULLSEG_31,	0x2F, 0xF2FF2FF2, 0x03,
	219143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	/* JAPAN_BAND_MID_TABLE */
	111143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x03,
	117143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	123143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	129143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x21,
	135143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	141143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	147143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	153143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	159143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	167143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	/* JAPAN_BAND_SHB_TABLE */
	225143, PLL_ISDB_T_FULLSEG_2C, 	0x18, 0xD51B8A9C, 0x03,
	231143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	237143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	243143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x21,
	249143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	255143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	261143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	267143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	273143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	279143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	285143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	291143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	297143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x21,
	303143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	309143, PLL_ISDB_T_FULLSEG_31,	0x2F, 0xF2FF2FF2, 0x03,
	315143, PLL_ISDB_T_FULLSEG_2E,	0x27, 0x47C9D1F2, 0x21,
	321143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x03,
	327143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	333143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	339143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	345143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	351143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	357143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	363143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	369143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	375143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	381143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	387143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	393143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	399143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	405143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	411143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x21,
	417143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x03,
	423143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	429143, PLL_ISDB_T_FULLSEG_31,	0x27, 0x47C9D1F2, 0x21,
	435143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	441143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	447143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	453143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	459143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	465143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	/* isdbt full seg - UHF */
	473143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	479143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	485143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	491143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	497143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	503143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	509143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	515143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	521143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	527143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	533143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x21,
	539143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	545143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	551143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	557143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	563143, PLL_ISDB_T_FULLSEG_2A,	0x0E, 0x1499D87F, 0x03,
	569143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	575143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	581143, PLL_ISDB_T_FULLSEG_31,	0x27, 0x47C9D1F2, 0x21,
	587143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	593143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	599143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	605143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	611143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	617143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	623143, PLL_ISDB_T_FULLSEG_2F,	0x27, 0x47C9D1F2, 0x03,
	629143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	635143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	641143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	647143, PLL_ISDB_T_FULLSEG_2B,	0x27, 0x47C9D1F2, 0x21,
	653143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	659143, PLL_ISDB_T_FULLSEG_2E,	0x27, 0x47C9D1F2, 0x21,
	665143, PLL_ISDB_T_FULLSEG_30,	0x27, 0x47C9D1F2, 0x21,
	671143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	677143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	683143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	689143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	695143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	701143, PLL_ISDB_T_FULLSEG_2E,	0x27, 0x47C9D1F2, 0x21,
	707143, PLL_ISDB_T_FULLSEG_2D,	0x1D, 0xDB9AF156, 0x03,
	713143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	719143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	725143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	731143, PLL_ISDB_T_FULLSEG_30,	0x2B, 0xB4099EA4, 0x03,
	737143, PLL_ISDB_T_FULLSEG_2E,	0x27, 0x47C9D1F2, 0x21,
	743143, PLL_ISDB_T_FULLSEG_2E,	0x22, 0xAB5BBB2E, 0x03,
	749143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	755143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	761143, PLL_ISDB_T_FULLSEG_2A,	0x27, 0x47C9D1F2, 0x21,
	767143, PLL_ISDB_T_FULLSEG_2C,	0x18, 0xD51B8A9C, 0x03,
	773143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	779143, PLL_ISDB_T_FULLSEG_2C,	0x27, 0x47C9D1F2, 0x21,
	785143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	791143, PLL_ISDB_T_FULLSEG_2D,	0x27, 0x47C9D1F2, 0x21,
	797143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	803143, PLL_ISDB_T_FULLSEG_2B,	0x13, 0x9421FC4E, 0x03,
	     0, PLL_ISDB_T_FULLSEG_2E,	0x00, 0x00000000, 0x00
};

I08S *MailSemaphoreName[4][4] = { 
	{"MailboxSemaphore00", "MailboxSemaphore01",
	"MailboxSemaphore02", "MailboxSemaphore03"},
	{"MailboxSemaphore10", "MailboxSemaphore11",
	"MailboxSemaphore12", "MailboxSemaphore13"},
	{"MailboxSemaphore20", "MailboxSemaphore21",
	"MailboxSemaphore22", "MailboxSemaphore23"},
	{"MailboxSemaphore30", "MailboxSemaphore31",
	"MailboxSemaphore32", "MailboxSemaphore33"}
};
I08S *OPMailboxSemaphoreName[4][4] = { 
	{"OpMailboxSemaphore00", "OpMailboxSemaphore01",
	"OpMailboxSemaphore02", "OpMailboxSemaphore03"},
	{"OpMailboxSemaphore10", "OpMailboxSemaphore11",
	"OpMailboxSemaphore12", "OpMailboxSemaphore13"},
	{"OpMailboxSemaphore20", "OpMailboxSemaphore21",
	"OpMailboxSemaphore22", "OpMailboxSemaphore23"},
	{"OpMailboxSemaphore30", "OpMailboxSemaphore31",
	"OpMailboxSemaphore32", "OpMailboxSemaphore33"}
};

I32S Tcc353xOpen(I32S _moduleIndex, Tcc353xOption_t * _Tcc353xOption)
{
	return (Tcc353xAttach(_moduleIndex, _Tcc353xOption));
}

I32S Tcc353xClose(I32S _moduleIndex)
{
	I32U i;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	/* interrupt clr and disable */
	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt) {
		Tcc353xSetRegIrqEnable(&Tcc353xHandle[_moduleIndex][0], 0);
		Tcc353xIrqClear(_moduleIndex, TC3XREG_IRQ_STATCLR_ALL);
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xPeripheralOnOff(&Tcc353xHandle[_moduleIndex][i], 0);
	}
	return (Tcc353xDetach(_moduleIndex));
}

I32S Tcc353xInit(I32S _moduleIndex, I08U * _coldbootData, I32S _codeSize)
{
	I32U i = 0;
	I32S ret = TCC353X_RETURN_SUCCESS;
	mailbox_t MailBox;
	I08U progId;
	I08U version0, version1, version2;
	I08U year, month, date;
	I32U temp;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;


	/* asm download and addressing */
	ret =
	    Tcc353xInitBroadcasting(_moduleIndex, _coldbootData,
				    _codeSize);
	if (ret != TCC353X_RETURN_SUCCESS)
		return ret;

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		I08U xtalbias_value = 7;

		/* stream / int / gpio / etc settings */
		Tcc353xSetStreamControl(&Tcc353xHandle[_moduleIndex][i]);
		Tcc353xSetInterruptControl(&Tcc353xHandle[_moduleIndex]
					   [i]);
		Tcc353xSetGpio(&Tcc353xHandle[_moduleIndex][i]);

		/* Restart System for stablility */
		TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i], 
				       TC3XREG_SYS_EN_OPCLK);
		Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex][i],
				      TC3XREG_SYS_RESET_DSP, _LOCK_);
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i],
				       TC3XREG_SYS_EN_EP |
				       TC3XREG_SYS_EN_DSP |
				       TC3XREG_SYS_EN_OPCLK |
				       TC3XREG_SYS_EN_RF);
		Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][i]);
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

		/* display code binary version */
		/* Get ASM Version */
		Tcc353xMailboxTxRx(&Tcc353xHandle[_moduleIndex][i],
				   &MailBox, MBPARA_SYS_ASM_VER, NULL, 0);

		/* option - change ldo volatage 1.2 to 1.8v */
		/*
		Tcc353xSetRegLdoConfig (&Tcc353xHandle[_moduleIndex][i],
				      0x1C);
		*/

		/* write values to sdram */
		/* sdram controller config */
		if(i==0) {
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 9, 0x56);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 5, 0x8000);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 6, 0x10000f);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0, 0x47482400);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 3, 0xf0);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 2, 0x72);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04,0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x20);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x460);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 13, 0x80000E00);
		}

		/* Xtal Bias Key Setup */
		/*
		if (Tcc353xCurrentDiversityCount[_moduleIndex] == 1)
			xtalbias_value = 0;
		else
			xtalbias_value = 1;
		*/

		Tcc353xSetRegXtalBias(&Tcc353xHandle[_moduleIndex][i],
				      xtalbias_value);
		Tcc353xSetRegXtalBiasKey(&Tcc353xHandle[_moduleIndex][i],
					 0x5e);

		/* get program id, code version */
		Tcc353xGetRegProgramId(&Tcc353xHandle[_moduleIndex][0],
				       &progId);
		Tcc353xHandle[_moduleIndex][i].dspCodeVersion =
		    MailBox.data_array[0];
		temp = MailBox.data_array[0];

		TcpalPrintLog((I08S *)
			      "[TCC353X] ----------------------\n");
		TcpalPrintLog((I08S *)
			"[TCC353X] Code VersionForm(%d) : 0x%08X\n", i,
			temp);
		version0 = (I08U)((temp>>28) & 0x0F);
		version1 = (I08U)((temp>>24) & 0x0F);
		version2 = (I08U)((temp>>16) & 0xFF);
		year = (I08U)((temp>>9) & 0x3F);
		month = (I08U)((temp>>5) & 0x0F);
		date = (I08U)(temp & 0x1F);

		TcpalPrintLog((I08S *)
			      "[TCC353X] Version (%d.%d.%d)\n", version0, 
			      version1, version2);
		TcpalPrintLog((I08S *)
			      "[TCC353X] Date (%d.%d.%d)\n", year, 
			      month, date);
		TcpalPrintLog((I08S *)
			      "[TCC353X] ----------------------\n");
	}

	/* rf init */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		ret = Tcc353xRfInit(_moduleIndex, i);
		if(ret!=TCC353X_RETURN_SUCCESS)
			return ret;
	}

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xChangeToDiversityMode (I32S _mergeIndex, 
				   Tcc353xOption_t * _Tcc353xOption)
{
	return TCC353X_RETURN_FAIL;
}

I32S Tcc353xChangeToDualMode (I32S _devideIndex, 
			      Tcc353xOption_t * _Tcc353xOption)
{
	return TCC353X_RETURN_FAIL;
}

static I32S Tcc353xShiftCenterFrequency(I32S _moduleIndex, I32S _frequency,
					Tcc353xTuneOptions * _tuneOption)
{
	I32S inputFrequency = 0;
	I32S outputFrequency = 0;
	I32S frequencyOffset = 0;

	inputFrequency = _frequency;
	outputFrequency = inputFrequency;

	switch (_tuneOption->tmmSet) {
	case A_1st_1Seg:
		frequencyOffset = -6857;
		break;
	case A_2nd_1Seg:
		frequencyOffset = -6428;
		break;
	case A_3rd_1Seg:
		frequencyOffset = -6000;
		break;
	case A_4th_1Seg:
		frequencyOffset = -5571;
		break;
	case A_5th_1Seg:
		frequencyOffset = -5143;
		break;
	case A_6th_1Seg:
		frequencyOffset = -4714;
		break;
	case A_7th_1Seg:
		frequencyOffset = -4285;
		break;
	case A_1st_13Seg:
		frequencyOffset = -1285;
		break;
	case A_2nd_13Seg:
		frequencyOffset = 4286;
		break;

	case B_1st_13Seg:
		frequencyOffset = -4285;
		break;
	case B_1st_1Seg:
		frequencyOffset = -1285;
		break;
	case B_2nd_1Seg:
		frequencyOffset = -857;
		break;
	case B_3rd_1Seg:
		frequencyOffset = -428;
		break;
	case B_4th_1Seg:
		frequencyOffset = 0;
		break;
	case B_5th_1Seg:
		frequencyOffset = 429;
		break;
	case B_6th_1Seg:
		frequencyOffset = 857;
		break;
	case B_7th_1Seg:
		frequencyOffset = 1286;
		break;
	case B_2nd_13Seg:
		frequencyOffset = 4286;
		break;

	case C_1st_13Seg:
		frequencyOffset = -4285;
		break;
	case C_2nd_13Seg:
		frequencyOffset = 1286;
		break;
	case C_1st_1Seg:
		frequencyOffset = 4286;
		break;
	case C_2nd_1Seg:
		frequencyOffset = 4715;
		break;
	case C_3rd_1Seg:
		frequencyOffset = 5143;
		break;
	case C_4th_1Seg:
		frequencyOffset = 5572;
		break;
	case C_5th_1Seg:
		frequencyOffset = 6000;
		break;
	case C_6th_1Seg:
		frequencyOffset = 6429;
		break;
	case C_7th_1Seg:
		frequencyOffset = 6857;
		break;
	default:
		frequencyOffset = 0;
		break;
	}

	outputFrequency += frequencyOffset;
	return outputFrequency;
}


static void Tcc353xRfSwitching(I32S _moduleIndex, I32S _diversityIndex,
			       I32S _frequency, Tcc353xOption_t * _option)
{
	I32U gpioNum = 0;
	I08U muxValue = 0x00;
	I08U gpioIdx = 0;
	I08U gpioLRoriginal = 0x00;
	I08U gpioDRoriginal = 0x00;
	I08U MaskIdx[8] =
	    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

#ifndef BITSET
#define	BITSET(X, MASK)				( (X) |= (I32U)(MASK) )
#endif
#ifndef BITCLR
#define	BITCLR(X, MASK)				( (X) &= ~((I32U)(MASK)) )
#endif

	if (_option->useRFSwitchingGpioN != -1) {
		gpioNum = _option->useRFSwitchingGpioN;

		if (gpioNum < 8) {
			gpioIdx = (I08U) (gpioNum);
			muxValue = 0x00;
		} else if (gpioNum < 16) {
			gpioIdx = (I08U) (gpioNum - 8);
			muxValue = 0x01;
		} else if (gpioNum < 24) {
			gpioIdx = (I08U) (gpioNum - 16);
			muxValue = 0x02;
		} else {
			return;
		}

		Tcc353xSetRegIoCfgMux(&Tcc353xHandle[_moduleIndex]
				      [_diversityIndex], muxValue);

		Tcc353xGetRegGpioDR(&Tcc353xHandle[_moduleIndex]
				    [_diversityIndex], &gpioDRoriginal);

		Tcc353xGetRegGpioLR(&Tcc353xHandle[_moduleIndex]
				    [_diversityIndex], &gpioLRoriginal);

		BITSET(gpioDRoriginal, MaskIdx[gpioIdx]);

		if (_frequency < 300000) {
			BITCLR(gpioLRoriginal, MaskIdx[gpioIdx]);	/* vhf */
		} else {
			BITSET(gpioLRoriginal, MaskIdx[gpioIdx]);	/* uhf */
		}

		Tcc353xSetRegGpioDR(&Tcc353xHandle[_moduleIndex]
				    [_diversityIndex], gpioDRoriginal);
		Tcc353xSetRegGpioLR(&Tcc353xHandle[_moduleIndex]
				    [_diversityIndex], gpioLRoriginal);
	} else {
		return;
	}
}

static I32U Tcc353xSearchDpllValue (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I32U _defaultPll)
{
	I32U i;
	I32U index;
	I32U pll;

	pll = _defaultPll;

	for(i = 0; i<_maxFreqNum; i++)	{
		index = (i*5);
		if(_tables[index] == 0) {
			/* last search, can't search frequency */
			pll = _tables[index+1];
			break;
		}
		if(_tables[index] == _frequencyInfo) {
			pll = _tables[index+1];
			break;
		}
	}

	return pll;
}

static I32S Tcc353xSendStoppingCommand(I32S _moduleIndex)
{
	I32U i;
	
	/* stop mail -> pause mail (for receiving ack) */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		I32U temp = 0;
		I16S j = 0;
		Tcc353xMiscRead(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, &temp);
		temp |= 0x100;
		Tcc353xMiscWrite(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, temp);

		for(j=0; j<300; j++)
		{
			I08U tmpdata = 0;
			Tcc353xGetRegProgramId(&Tcc353xHandle[_moduleIndex][i]
				, &tmpdata);
			if(tmpdata & 0x2)
				break;
			else
				TcpalmDelay(1);
		}
	}
	TcpalmDelay(2);	/* for stability */

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xTune(I32S _moduleIndex, I32S _frequency,
		 Tcc353xTuneOptions * _tuneOption, I32S _fastTune)
{
	I32U i;
	I16U newPll = 0;
	I32U opConfig[TCC353X_DIVERSITY_MAX][16];
	I32S Inputfrequency = _frequency;
	I32U firstOpconfigWrite = 0;
	TcpalTime_t CurrTime;
	I32U TotalTime;
	I08U bufferEndReg[2];
	I32U bufferEndAddress;

	CurrTime = TcpalGetCurrentTimeCount_ms();

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	/* stopping old channel stream output */

	/* stop mail -> pause mail (for receiving ack) */
	Tcc353xSendStoppingCommand(_moduleIndex);

	if (Tcc353xHandle[_moduleIndex][0].streamStarted) {
		/* stream stop */
		Tcc353xStreamStop(_moduleIndex);
	}

	if(Tcc353xHandle[_moduleIndex][0].options.streamInterface== 
						TCC353X_STREAM_IO_MAINIO) {
		I08U fifothr[2];
		
		/* change buffer A end size */
		switch (_tuneOption->segmentType) {
		case TCC353X_ISDBT_1_OF_13SEG:
		case TCC353X_ISDBTSB_1SEG:
		case TCC353X_ISDBTSB_1_OF_3SEG:
			bufferEndAddress = 0x00019F5B;
			break;
		case TCC353X_ISDBT_13SEG:
		case TCC353X_ISDBTSB_3SEG:
			bufferEndAddress = 0x00027F57;
			break;
		case TCC353X_ISDBTMM:
			if (_tuneOption->tmmSet == A_1st_13Seg ||
			    _tuneOption->tmmSet == A_2nd_13Seg ||
			    _tuneOption->tmmSet == B_1st_13Seg ||
			    _tuneOption->tmmSet == B_2nd_13Seg ||
			    _tuneOption->tmmSet == C_1st_13Seg ||
			    _tuneOption->tmmSet == C_2nd_13Seg ||
			    _tuneOption->tmmSet == UserDefine_Tmm13Seg) {
				/*13seg */
				bufferEndAddress = 0x00027F57;
			} else {
				bufferEndAddress = 0x00019F5B;
			}
			break;
		default:
			bufferEndAddress = 0x00027F57;
			break;
		}
		
		if(_tuneOption->userFifothr) {
			fifothr[0] = (I08U)(((_tuneOption->userFifothr>>2)>>8) 
				     & 0xFF);
			fifothr[1] = (I08U)((_tuneOption->userFifothr>>2) 
				     & 0xFF);
			Tcc353xSetRegOutBufferAFifoThr
				(&Tcc353xHandle[_moduleIndex][0], &fifothr[0]);
		}

		bufferEndReg[0] = (I08U)((bufferEndAddress >> 10) & 0xFF),
		bufferEndReg[1] = (I08U)((bufferEndAddress >> 2) & 0xFF),
		Tcc353xSetRegOutBufferEndAddressA(
			&Tcc353xHandle[_moduleIndex][0], &bufferEndReg[0]);
	}

	TcpalMemcpy(&Tcc353xHandle[_moduleIndex][0].TuneOptions,
		    _tuneOption, sizeof(Tcc353xTuneOptions));

	/* center frequency shift for isdb-tmm */
	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
	case TCC353X_ISDBT_13SEG:
		/* ISDB-T */
		TcpalPrintLog((I08S *)
			      "[TCC353X] Tune frequency [ISDB-T]: %d\n",
			      _frequency);
		break;

	case TCC353X_ISDBTSB_1SEG:
	case TCC353X_ISDBTSB_3SEG:
	case TCC353X_ISDBTSB_1_OF_3SEG:
		/* ISDB-Tsb */
		TcpalPrintLog((I08S *)
			      "[TCC353X] Tune frequency [ISDB-Tsb]: %d\n",
			      _frequency);
		break;

	case TCC353X_ISDBTMM:
		/* ISDB-TMM */
		Inputfrequency =
		    Tcc353xShiftCenterFrequency(_moduleIndex, _frequency,
						_tuneOption);
		if (Inputfrequency <= 0)
			return TCC353X_RETURN_FAIL;

		TcpalPrintLog((I08S *)
 			      "[TCC353X] Tune frequency [ISDB-TMM] (TMM center frequency shift : %d to %d)\n",
			      _frequency, Inputfrequency);
		break;
	default:
		/* ISDB-T */
		TcpalPrintLog((I08S *)
			      "[TCC353X] Tune frequency [ISDB-T]: %d\n",
			      _frequency);
		break;
	}

	/* check pll change need or not */
	/* change full seg to partial 1seg or isdb-t <-> tmm */
	if(Tcc353xHandle[_moduleIndex][0].useDefaultPLL == 1)
	{
		switch (_tuneOption->segmentType) {
		case TCC353X_ISDBT_1_OF_13SEG:
			if(_tuneOption->rfIfType==TCC353X_LOW_IF)
				newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_Partial1Seg[0],
						_MAX_PARTIAL_1SEG_FREQ_NUM_, 
						PLL_ISDB_T_PARTIAL_1_SEG));
			else
				newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_FullSeg[0],
						_MAX_PARTIAL_1SEG_FREQ_NUM_, 
						PLL_ISDB_T_PARTIAL_1_SEG));
			break;
		case TCC353X_ISDBT_13SEG:
			newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_FullSeg[0],
						_MAX_FULLSEG_FREQ_NUM_, 
						PLL_ISDB_T_FULLSEG));
			break;
		case TCC353X_ISDBTMM:
			if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
				newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_TMM_USER_13SEG[0],
						_MAX_TMM_USER_13SEG_FREQ_NUM_, 
						PLL_ISDB_TMM_FULLSEG));
			else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
				newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_TMM_USER_1SEG[0],
						_MAX_TMM_USER_1SEG_FREQ_NUM_, 
						PLL_ISDB_TMM_PARTIAL_1_SEG));
			else if (_tuneOption->tmmSet == A_1st_13Seg||
				 _tuneOption->tmmSet == A_2nd_13Seg||
				 _tuneOption->tmmSet == B_1st_13Seg||
				 _tuneOption->tmmSet == B_2nd_13Seg||
				 _tuneOption->tmmSet == C_1st_13Seg||
				 _tuneOption->tmmSet == C_2nd_13Seg)
				newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_TMM_13SEG[0],
						_MAX_TMM_13SEG_FREQ_NUM_, 
						PLL_ISDB_TMM_FULLSEG));
			else
				newPll = (I16U)(Tcc353xSearchDpllValue (
						(I32U)(Inputfrequency),
						&DpllTable_TMM_1SEG[0],
						_MAX_TMM_1SEG_FREQ_NUM_, 
						PLL_ISDB_TMM_PARTIAL_1_SEG));
			break;
		case TCC353X_ISDBTSB_1SEG:
			newPll = PLL_ISDB_TSB;
			break;
		case TCC353X_ISDBTSB_3SEG:
			newPll = PLL_ISDB_TSB;
			break;
		case TCC353X_ISDBTSB_1_OF_3SEG:
			newPll = PLL_ISDB_TSB;
			break;
		default:
			newPll = PLL_ISDB_T_FULLSEG;
			break;
		}

		/* change pll */
		#ifdef TCC79X	/* for 79x muse */
		Tcc353xChangePll(_moduleIndex, newPll);
		#else
		if (newPll != Tcc353xHandle[_moduleIndex][0].options.pll)
			Tcc353xChangePll(_moduleIndex, newPll);
		#endif
	}

	if(!Tcc353xHandle[_moduleIndex][0].tuned)
		firstOpconfigWrite = 1;

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xRfSwitching(_moduleIndex, i, Inputfrequency,
				   &Tcc353xHandle[_moduleIndex]
				   [i].options);

		Tcc353xRfTune(_moduleIndex, i, Inputfrequency, 6000,
			      Tcc353xHandle[_moduleIndex][i].
			      options.oscKhz, _tuneOption);

		Tcc353xGetOpconfigValues(_moduleIndex, i, _tuneOption,
					 &opConfig[i][0], 
					 (I32U)(Inputfrequency));

		/* op configure it need dsp disable->reset->enable */
		Tcc353xSetOpConfig(_moduleIndex, i,
				   &opConfig[i][0], firstOpconfigWrite);
	}

	Tcc353xHandle[_moduleIndex][0].tuned = 1;

	/* stream start & dsp reset & send start mail */
	Tcc353xInitIsdbProcess(&Tcc353xHandle[_moduleIndex][0]);
	Tcc353xStreamStartPrepare (_moduleIndex);

	/* dsp disable to enable, ep reset & peripheral enable */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xDspEpReopenForStreamStart(_moduleIndex, i);
		Tcc353xSendStartMail(&Tcc353xHandle[_moduleIndex][i]);
	}
	
	TotalTime = (I32U)((I64U)(TcpalGetTimeIntervalCount_ms(CurrTime)) 
				   & 0xFFFFFFFF);
	TcpalPrintStatus ((I08S *) "[TCC353X] SpendTime [%d]ms\n", TotalTime);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xSetStreamFormat(I32S _moduleIndex,
			    Tcc353xStreamFormat_t * _streamFormat)
{
	I32U data = 0;
	TcpalMemcpy(&Tcc353xHandle[_moduleIndex][0].streamFormat, 
		    _streamFormat, sizeof(Tcc353xStreamFormat_t));

	Tcc353xMiscRead(_moduleIndex, 0, MISC_OP_REG_CTRL,
			TC3XREG_OP_FILTER_CFG, &data);
	data = (data & 0xFFFFFC3F);

	if (_streamFormat->pidFilterEnable)
		BITSET(data, Bit6);
	else
		BITCLR(data, Bit6);

	if (_streamFormat->tsErrorFilterEnable)
		BITSET(data, Bit7);
	else
		BITCLR(data, Bit7);

	if (_streamFormat->syncByteFilterEnable)
		BITSET(data, Bit8);
	else
		BITCLR(data, Bit8);

	if (_streamFormat->tsErrorInsertEnable)
		BITSET(data, Bit9);
	else
		BITCLR(data, Bit9);

	Tcc353xMiscWrite(_moduleIndex, 0, MISC_OP_REG_CTRL,
			 TC3XREG_OP_FILTER_CFG, data);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc35xSelectLayer(I32S _moduleIndex, I32S _layer)
{
	return TCC353X_RETURN_SUCCESS;
}

I32U Tcc353xSendStartMail(Tcc353xHandle_t * _handle)
{
	/* start / stop mail (toggle) for dp stablility */
	mailbox_t mailbox;
	I32S retmail;

	/* any value */
	I32U data = 0x11;

	retmail =
	    Tcc353xMailboxTxRx(_handle, &mailbox, MBPARA_SYS_START, &data,
			       1);
	if (retmail == -1)
		TcpalPrintErr((I08S *)
			      "[TCC353X] Error - Sending Startmail\n");
	return retmail;
}

I32S Tcc353xStreamStopAll(I32S _moduleIndex)
{

	/* stopping stream */
	Tcc353xStreamStop(_moduleIndex);

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xStreamStop(I32S _moduleIndex)
{
	I08U streamDataConfig_0x1E;
	I08U bufferConfig0;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	/* interrupt clr and disable */
	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt) {
		Tcc353xSetRegIrqEnable(&Tcc353xHandle[_moduleIndex][0], 0);
		Tcc353xIrqClear(_moduleIndex, TC3XREG_IRQ_STATCLR_ALL);
	}

	/* disable stream data config */
	streamDataConfig_0x1E =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->streamDataConfig_0x1E;
	BITCLR(streamDataConfig_0x1E, TC3XREG_STREAM_DATA_ENABLE);
	Tcc353xSetRegStreamConfig0(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->streamDataConfig_0x1B);
	Tcc353xSetRegStreamConfig3(&Tcc353xHandle[_moduleIndex][0],
				   streamDataConfig_0x1E);

	/* disable buffer */
	bufferConfig0 =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->bufferConfig_0x4E;
	BITCLR(bufferConfig0,
	       TC3XREG_OBUFF_CFG_BUFF_A_EN | TC3XREG_OBUFF_CFG_BUFF_B_EN |
	       TC3XREG_OBUFF_CFG_BUFF_C_EN | TC3XREG_OBUFF_CFG_BUFF_D_EN);
	Tcc353xSetRegOutBufferConfig(&Tcc353xHandle[_moduleIndex][0],
				     bufferConfig0);

	/* peripheral disable clr */
	Tcc353xPeripheralOnOff(&Tcc353xHandle[_moduleIndex][0], 0);

	Tcc353xHandle[_moduleIndex][0].streamStarted = 0;

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xStreamStartPrepare(I32S _moduleIndex)
{
	I08U streamDataConfig_0x1B;
	I08U streamDataConfig_0x1E;
	I08U bufferConfig0;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	Tcc353xHandle[_moduleIndex][0].streamStarted = 1;

	/* buffer init & enable buffer */
	Tcc353xSetRegOutBufferInit(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->bufferConfig_0x4F);

	bufferConfig0 =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->bufferConfig_0x4E;
	BITSET(bufferConfig0, TC3XREG_OBUFF_CFG_BUFF_A_EN);

	Tcc353xSetRegOutBufferConfig(&Tcc353xHandle[_moduleIndex][0],
				     bufferConfig0);

	streamDataConfig_0x1B =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->streamDataConfig_0x1B;
	streamDataConfig_0x1E =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->streamDataConfig_0x1E;

	BITSET(streamDataConfig_0x1E, TC3XREG_STREAM_DATA_ENABLE);

	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt)
		BITSET(streamDataConfig_0x1E,
		       TC3XREG_STREAM_DATA_FIFO_INIT |
		       TC3XREG_STREAM_DATA_FIFO_EN);
	else
		BITSET(streamDataConfig_0x1E,
		       TC3XREG_STREAM_DATA_FIFO_INIT);

	Tcc353xSetRegStreamConfig0(&Tcc353xHandle[_moduleIndex][0],
				   streamDataConfig_0x1B);
	Tcc353xSetRegStreamConfig1(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->streamDataConfig_0x1C);
	Tcc353xSetRegStreamConfig2(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->streamDataConfig_0x1D);
	Tcc353xSetRegStreamConfig3(&Tcc353xHandle[_moduleIndex][0],
				   streamDataConfig_0x1E);

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xStreamOn (I32S _moduleIndex)
{
	/* peripheral enable clr */
	Tcc353xPeripheralOnOff(&Tcc353xHandle[_moduleIndex][0], 1);

	/* interrupt enable */
	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt)
		Tcc353xSetRegIrqEnable(&Tcc353xHandle[_moduleIndex][0],
				       TC3XREG_IRQ_EN_FIFOAINIT|
				       TC3XREG_IRQ_EN_FIFO_OVER
				       /*TC3XREG_IRQ_EN_DATAINT */ );
	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xDspEpReopenForStreamStart(I32S _moduleIndex, I32S _diversityIndex)
{
	TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][_diversityIndex]);
	
	/* DSP disable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex]
			       [_diversityIndex],
			       TC3XREG_SYS_EN_EP |
			       TC3XREG_SYS_EN_OPCLK |
			       TC3XREG_SYS_EN_RF);
	/* DSP reset */
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], TC3XREG_SYS_RESET_DSP, 
			      _LOCK_);
	TcpalmDelay(1);

	/* EP reset */
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], TC3XREG_SYS_RESET_EP, 
			      _LOCK_);

	/* peripheral enable */
	if(_diversityIndex==0)
		Tcc353xStreamOn (_moduleIndex);

	/* DSP Enable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex]
			       [_diversityIndex],
			       TC3XREG_SYS_EN_EP |
			       TC3XREG_SYS_EN_DSP |
			       TC3XREG_SYS_EN_OPCLK |
			       TC3XREG_SYS_EN_RF);

	Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][_diversityIndex]);
	TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex]
			     [_diversityIndex]);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xStreamStart(I32S _moduleIndex)
{
	I32S i;
	/* stream start & dsp reset & send start mail */
	Tcc353xStreamStartPrepare (_moduleIndex);

	/* dsp disable to enable, ep reset & peripheral enable */
	for (i = 0; i < (I32S)Tcc353xCurrentDiversityCount[_moduleIndex]; 
	    i++) {
		Tcc353xDspEpReopenForStreamStart(_moduleIndex, i);
		Tcc353xSendStartMail(&Tcc353xHandle[_moduleIndex][i]);
	}
	
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xInterruptBuffClr(I32S _moduleIndex)
{
	I32U i;

	/* send stop command */
	Tcc353xSendStoppingCommand (_moduleIndex);

	/* stopping stream */
	Tcc353xStreamStop(_moduleIndex);

	/* send opconfig */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		I32U temp = 0;
		Tcc353xMiscRead(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, &temp);
		temp &= ~((I32U)(0x100));
		Tcc353xMiscWrite(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, temp);
	}

	/* send start mail */
	Tcc353xStreamStart(_moduleIndex);
	return TCC353X_RETURN_SUCCESS;
}


static I32S Tcc353xAttach(I32S _moduleIndex,
			  Tcc353xOption_t * _Tcc353xOption)
{
	I32U i;
	I32S ret = TCC353X_RETURN_FAIL;
	I08U chipId = 0;
	I08U progId = 0;

	/* init global values */
	switch (_Tcc353xOption[0].boardType) {
	case TCC353X_BOARD_SINGLE:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 1;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [Single Mode]\n");
		break;

	case TCC353X_BOARD_2DIVERSITY:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 2;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [2Diversity Mode]\n");
		break;

	case TCC353X_BOARD_3DIVERSITY:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 3;
		TcpalPrintLog((I08S *)
 			      "[TCC353X] TCC353X Attach Success! [3Diversity Mode]\n");
		break;

	case TCC353X_BOARD_4DIVERSITY:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 4;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [4Diversity Mode]\n");
		break;

	default:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 1;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [Single Mode]\n");
		break;
	}

	TcpalMemset(&Tcc353xHandle[_moduleIndex][0], 0,
		    sizeof(Tcc353xHandle_t) *
		    Tcc353xCurrentDiversityCount[_moduleIndex]);

	/* connect command interface function */
	/* set address */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xSaveOption(_moduleIndex, i, &_Tcc353xOption[i]);
		Tcc353xHandle[_moduleIndex][i].handleOpen = 1;
		Tcc353xConnectCommandInterface (_moduleIndex, i, 
			Tcc353xHandle[_moduleIndex][0].
			options.commandInterface);
	}

	/* interface semaphore only one semaphore */
	if (pTcc353xInterfaceSema == NULL) {
		TcpalCreateSemaphore(&Tcc353xInterfaceSema,
				     (I08S *) "InterfaceSemaphore", 1);
		pTcc353xInterfaceSema = &Tcc353xInterfaceSema;
	} else {
		TcpalPrintLog((I08S *)
			      "[TCC353X] - Already exist interface semaphore\n");
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		/* mailbox semaphore */
		TcpalCreateSemaphore(&Tcc353xMailboxSema[_moduleIndex][i],
				     MailSemaphoreName[_moduleIndex][i], 1);
		/* op & mailbox semaphore */
		TcpalCreateSemaphore(&Tcc353xOpMailboxSema[_moduleIndex]
				     [i], OPMailboxSemaphoreName[_moduleIndex][i],
				     1);

		Tcc353xGetRegChipId(&Tcc353xHandle[_moduleIndex][i],
				    &chipId);
		TcpalPrintLog((I08S *) "[TCC353X][%d][%d] ChipID 0x%02x\n",
			      _moduleIndex, i, chipId);
		Tcc353xGetRegProgramId(&Tcc353xHandle[_moduleIndex][i],
				       &progId);
		TcpalPrintLog((I08S *) "[TCC353X][%d][%d] progId 0x%02x\n",
			      _moduleIndex, i, progId);
	}

	if (chipId == 0x33)
		ret = TCC353X_RETURN_SUCCESS;
	return ret;
}

I32S Tcc353xDetach(I32S _moduleIndex)
{
	I32U i;
	/* link Dummy Function */
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xHandle[_moduleIndex][i].Read = DummyFunction0;
		Tcc353xHandle[_moduleIndex][i].Write = DummyFunction1;
	}

	/* Dealloc handles */
	TcpalMemset(&Tcc353xHandle[_moduleIndex][0], 0,
		    sizeof(Tcc353xHandle_t) *
		    Tcc353xCurrentDiversityCount[_moduleIndex]);

	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);

	/* interface semaphore only one semaphore */
	/* delete all drivers */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		/* mailbox semaphore */
		TcpalDeleteSemaphore(&Tcc353xMailboxSema[_moduleIndex][i]);
		/* op & mailbox semaphore */
		TcpalDeleteSemaphore(&Tcc353xOpMailboxSema[_moduleIndex]
				     [i]);
	}

	if (Tcc353xHandle[0][0].handleOpen == 0
	    && Tcc353xHandle[1][0].handleOpen == 0) {
		TcpalDeleteSemaphore(&Tcc353xInterfaceSema);
		pTcc353xInterfaceSema = NULL;

		/* init global values */
		Tcc353xCurrentDiversityCount[_moduleIndex] = 1;
	}

	TcpalPrintLog((I08S *) "[TCC353X] Detach Success!\n");

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xCodeDownload(I32S _moduleIndex, I08U * _coldbootData,
				I32S _codeSize)
{
	I32S coldsize;
	Tcc353xBoot_t boot;
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xColdbootParserUtil(_coldbootData, _codeSize, &boot) ==
	    TCC353X_RETURN_SUCCESS) {
		coldsize = boot.coldbootDataSize - 4;	/* Except CRC Size */
		Tcc353xDspAsmWrite(&Tcc353xHandle[_moduleIndex][0],
				   boot.coldbootDataPtr, coldsize);
		ret =
		    Tcc353xCodeCrcCheck(_moduleIndex, _coldbootData,
					&boot);
	} else {
		ret = TCC353X_RETURN_FAIL;
	}
	return ret;
}

static I32S Tcc353xCodeCrcCheck(I32S _moduleIndex, I08U * _coldbootData,
				Tcc353xBoot_t * _boot)
{
	I32S i;
	I32U destCrc, srcCrc;
	Tcc353xHandle_t *handle;
	I32S count = 1;
	I32S ret = TCC353X_RETURN_SUCCESS;
	I08U data[4];

	count = Tcc353xCurrentDiversityCount[_moduleIndex];

	for (i = count - 1; i >= 0; i--) {
		Tcc353xHandle[_moduleIndex][i].currentAddress =
		    Tcc353xHandle[_moduleIndex][i].originalAddress;
		handle = &Tcc353xHandle[_moduleIndex][i];

		Tcc353xGetRegDmaCrc32(handle, &data[0]);
		destCrc =
		    (data[0] << 24) | (data[1] << 16) | (data[2] << 8) |
		    (data[3]);
		srcCrc =
		    HTONL(GET4BYTES
			  (&_boot->coldbootDataPtr
			   [_boot->coldbootDataSize - 4]));

		if (destCrc == srcCrc) {
			TcpalPrintLog((I08S *)
				      "[TCC353X] [%d][%d] CRC Success!\n",
				      handle->moduleIndex,
				      handle->diversityIndex);
			TcpalPrintLog((I08S *)
				      "[TCC353X] [0x%x][0x%x]\n",
				      srcCrc, destCrc);
		} else {
			TcpalPrintErr((I08S *)
				      "[TCC353X] [%d][%d] CRC Fail!!!!!! \n",
				      _moduleIndex, i);
			TcpalPrintErr((I08S *)
				      "[TCC353X] [0x%x][0x%x]\n",
				      srcCrc, destCrc);
			ret = TCC353X_RETURN_FAIL;
		}
	}

	return ret;
}

static I32S Tcc353xInitBroadcasting(I32S _moduleIndex,
				    I08U * _coldbootData, I32S _codeSize)
{
	I32U i;
	I32S subret;
	I08U remapPc[3];
	I32S broadcastingFlag = 0;

	/* broad casting write */
	if (Tcc353xCurrentDiversityCount[_moduleIndex] > 1) {
		broadcastingFlag = 1;
		if (Tcc353xHandle[_moduleIndex][0].
		    options.commandInterface == TCC353X_IF_I2C)
			Tcc353xHandle[_moduleIndex][0].currentAddress =
			    0xA0;
		else if (Tcc353xHandle[_moduleIndex][0].
			 options.commandInterface == TCC353X_IF_TCCSPI)
			Tcc353xHandle[_moduleIndex][0].currentAddress =
			    (0xA0 >> 1);
		else
			Tcc353xHandle[_moduleIndex][0].currentAddress =
			    0xA0;
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++)
		TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	/* ALL Parts Disable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][0], 0);

	/* set pll */
	if(Tcc353xHandle[_moduleIndex][0].options.pll == 0)
		Tcc353xHandle[_moduleIndex][0].useDefaultPLL = 1;
	else
		Tcc353xHandle[_moduleIndex][0].useDefaultPLL = 0;
	Tcc353xSetPll(_moduleIndex, 0, 0, PLL_ISDB_T_FULLSEG);
	
	/* EP Reset */
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex][0],
			      TC3XREG_SYS_RESET_EP, _LOCK_);
	/* EP Enable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][0],
			       TC3XREG_SYS_EN_EP);

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++)
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	/* remap */
	Tcc353xSetRegRemap(&Tcc353xHandle[_moduleIndex][0], 0x00);

	/* asm code download and roll-back current address */
	if (_coldbootData == NULL) {
		subret = TCC353X_RETURN_SUCCESS;
		if (Tcc353xCurrentDiversityCount[_moduleIndex] > 1) {
			if (Tcc353xHandle[_moduleIndex][0].options.
			    commandInterface == TCC353X_IF_I2C)
				Tcc353xHandle[_moduleIndex]
				    [0].currentAddress =
				    Tcc353xHandle[_moduleIndex]
				    [0].originalAddress;
			else if (Tcc353xHandle[_moduleIndex][0].options.
				 commandInterface == TCC353X_IF_TCCSPI)
				Tcc353xHandle[_moduleIndex]
				    [0].currentAddress =
				    Tcc353xHandle[_moduleIndex]
				    [0].originalAddress;
			else
				Tcc353xHandle[_moduleIndex]
				    [0].currentAddress =
				    Tcc353xHandle[_moduleIndex]
				    [0].originalAddress;
		}
	} else {
		subret =
		    Tcc353xCodeDownload(_moduleIndex, _coldbootData,
					_codeSize);
	}

	if (subret != TCC353X_RETURN_SUCCESS)
		return TCC353X_RETURN_FAIL;

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		remapPc[0] =
		    Tcc353xHandle[_moduleIndex][i].options.Config->initRemap_0x0D;
		remapPc[1] =
		    Tcc353xHandle[_moduleIndex][i].options.Config->initPC_0x0E;
		remapPc[2] =
		    Tcc353xHandle[_moduleIndex][i].options.Config->initPC_0x0F;
		Tcc353xSetRegRemapPc(&Tcc353xHandle[_moduleIndex][i],
				     &remapPc[0], 3);
	}

	return TCC353X_RETURN_SUCCESS;
}

static void Tcc353xSetInterruptControl(Tcc353xHandle_t * _handle)
{
	/* init irq disable */
	Tcc353xSetRegIrqMode(_handle,
			     _handle->options.Config->irqMode_0x02);
	Tcc353xSetRegIrqClear(_handle, TC3XREG_IRQ_STATCLR_ALL);
	if (_handle->options.useInterrupt)
		Tcc353xSetRegIrqEnable(_handle, 0);
}

static void Tcc353xOutBufferConfig(Tcc353xHandle_t * _handle)
{
	I08U data[2];

	data[0] = _handle->options.Config->bufferConfig_0x50;
	data[1] = _handle->options.Config->bufferConfig_0x51;
	Tcc353xSetRegOutBufferStartAddressA(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x52;
	data[1] = _handle->options.Config->bufferConfig_0x53;
	Tcc353xSetRegOutBufferEndAddressA(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x58;
	data[1] = _handle->options.Config->bufferConfig_0x59;
	Tcc353xSetRegOutBufferStartAddressB(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x5A;
	data[1] = _handle->options.Config->bufferConfig_0x5B;
	Tcc353xSetRegOutBufferEndAddressB(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x60;
	data[1] = _handle->options.Config->bufferConfig_0x61;
	Tcc353xSetRegOutBufferStartAddressC(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x62;
	data[1] = _handle->options.Config->bufferConfig_0x63;
	Tcc353xSetRegOutBufferEndAddressC(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x68;
	data[1] = _handle->options.Config->bufferConfig_0x69;
	Tcc353xSetRegOutBufferStartAddressD(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x6A;
	data[1] = _handle->options.Config->bufferConfig_0x6B;
	Tcc353xSetRegOutBufferEndAddressD(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x54;
	data[1] = _handle->options.Config->bufferConfig_0x55;
	Tcc353xSetRegOutBufferAFifoThr(_handle, &data[0]);

	Tcc353xSetRegOutBufferConfig(_handle,
				     _handle->options.
				     Config->bufferConfig_0x4E);
	Tcc353xSetRegOutBufferInit(_handle,
				   _handle->options.
				   Config->bufferConfig_0x4F);
}

static void Tcc353xSetStreamControl(Tcc353xHandle_t * _handle)
{
	I08U data[4];
	I32U streamClkSpeed;
	I32U dlr;

	Tcc353xOutBufferConfig(_handle);

	data[0] = _handle->options.Config->streamDataConfig_0x1B;
	data[1] = _handle->options.Config->streamDataConfig_0x1C;
	data[2] = _handle->options.Config->streamDataConfig_0x1D;
	data[3] = _handle->options.Config->streamDataConfig_0x1E;
	Tcc353xSetRegStreamConfig(_handle, &data[0]);

	data[0] = _handle->options.Config->periConfig_0x30;
	data[1] = _handle->options.Config->periConfig_0x31;
	data[2] = _handle->options.Config->periConfig_0x32;
	data[3] = _handle->options.Config->periConfig_0x33;
	Tcc353xSetRegPeripheralConfig(_handle, &data[0]);

	if ((_handle->options.Config->periConfig_0x30 & 0x30) == 0x10) {
		/* spi ms */
		I64U temp;
		I64U temp2;
		dlr =
		    ((_handle->options.
		      Config->periConfig_0x31 & 0x1C) >> 2);
		temp2 = _handle->mainClkKhz;
		temp = (DIV(temp2, ((1 + dlr) << 1))) >> SCALE;
		streamClkSpeed = (I32U) (temp);
		TcpalPrintLog((I08S *)
			      "[TCC353X] SET SPI Clk : %d khz [DLR : %d]\n",
			      streamClkSpeed, dlr);
	} else if ((_handle->options.Config->periConfig_0x30 & 0x30) ==
		   0x20) {
		/* ts */
		I64U temp;
		I64U temp2;
		dlr = (_handle->options.Config->periConfig_0x31 & 0x07);
		temp2 = _handle->mainClkKhz;
		temp = (DIV(temp2, ((1 + dlr) << 1))) >> SCALE;
		streamClkSpeed = (I32U) (temp);
		TcpalPrintLog((I08S *)
			      "[TCC353X] SET TS Clk : %d khz [DLR : %d]\n",
			      streamClkSpeed, dlr);
	} else {
		;		/*none */
	}

}

void Tcc353xPeripheralOnOff(Tcc353xHandle_t * _handle, I32S _onoff)
{
	if (_onoff)
		Tcc353xSetRegPeripheralConfig0(_handle,
					       _handle->options.
					       Config->periConfig_0x30 |
					       TC3XREG_PERI_EN |
					       TC3XREG_PERI_INIT_AUTOCLR);
	else
		Tcc353xSetRegPeripheralConfig0(_handle,
					       _handle->options.
					       Config->periConfig_0x30 |
					       TC3XREG_PERI_INIT_AUTOCLR);
}

static void Tcc353xSetGpio(Tcc353xHandle_t * _handle)
{
	Tcc353xSetRegIoCfgMux(_handle, 0x00);
	Tcc353xSetRegGpioAlt(_handle,
			     _handle->options.Config->gpioAlt_0x10_07_00);
	Tcc353xSetRegGpioDR(_handle,
			    _handle->options.Config->gpioDr_0x11_07_00);
	Tcc353xSetRegGpioLR(_handle,
			    _handle->options.Config->gpioLr_0x12_07_00);
	Tcc353xSetRegGpioDRV(_handle,
			     _handle->options.Config->gpioDrv_0x13_07_00);
	Tcc353xSetRegGpioPE(_handle,
			    _handle->options.Config->gpioPe_0x14_07_00);
	Tcc353xSetRegGpiosDRV(_handle,
			      _handle->options.
			      Config->gpioSDrv_0x15_07_00);

	Tcc353xSetRegIoCfgMux(_handle, 0x01);
	Tcc353xSetRegGpioAlt(_handle,
			     _handle->options.Config->gpioAlt_0x10_15_08);
	Tcc353xSetRegGpioDR(_handle,
			    _handle->options.Config->gpioDr_0x11_15_08);
	Tcc353xSetRegGpioLR(_handle,
			    _handle->options.Config->gpioLr_0x12_15_08);
	Tcc353xSetRegGpioDRV(_handle,
			     _handle->options.Config->gpioDrv_0x13_15_08);
	Tcc353xSetRegGpioPE(_handle,
			    _handle->options.Config->gpioPe_0x14_15_08);
	Tcc353xSetRegGpiosDRV(_handle,
			      _handle->options.
			      Config->gpioSDrv_0x15_15_08);

	Tcc353xSetRegIoCfgMux(_handle, 0x02);
	Tcc353xSetRegGpioAlt(_handle,
			     _handle->options.Config->gpioAlt_0x10_23_16);
	Tcc353xSetRegGpioDR(_handle,
			    _handle->options.Config->gpioDr_0x11_23_16);
	Tcc353xSetRegGpioLR(_handle,
			    _handle->options.Config->gpioLr_0x12_23_16);
	Tcc353xSetRegGpioDRV(_handle,
			     _handle->options.Config->gpioDrv_0x13_23_16);
	Tcc353xSetRegGpioPE(_handle,
			    _handle->options.Config->gpioPe_0x14_23_16);
	Tcc353xSetRegGpiosDRV(_handle,
			      _handle->options.
			      Config->gpioSDrv_0x15_23_16);

	Tcc353xSetRegIoMISC(_handle, _handle->options.Config->ioMisc_0x16);
}

static I32S Tcc353xSetPll(I32S _moduleIndex, I32S _deviceIndex, 
			  I32S _flagInterfaceLock, I16U _pllValue)
{
	I08U PLL6, PLL7;
	I08U pll_f, pll_m, pll_r, pll_od;
	I32U fout, fvco;
	I08U lockFlag;
	Tcc353xHandle_t *h;

	if (_flagInterfaceLock == 0)
		lockFlag = _LOCK_;
	else
		lockFlag = _UNLOCK_;
	
	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][_deviceIndex]);

	if(Tcc353xHandle[_moduleIndex][0].useDefaultPLL == 1)
		h->options.pll = _pllValue;

	PLL6 = (h->options.pll >> 8) & 0x007f;
	PLL7 = ((h->options.pll) & 0xFF);

	/* for stablility */
	Tcc353xSetRegPll8(h, 0x28, lockFlag);
	Tcc353xSetRegPll9(h, 0x64, lockFlag);

	Tcc353xSetRegPll6(h, PLL6, lockFlag);
	Tcc353xSetRegPll7(h, PLL7, lockFlag);
	
	Tcc353xSetRegPll6(h, PLL6 | 0x80, lockFlag);
	TcpalmDelay(1);		/* 1ms (orig: 340us) */

	pll_m = ((PLL6 & 0x40) >> 6);
	pll_f = (PLL6 & 0x3f) + 1;
	pll_r = ((PLL7 >> 3)&0x0F) + 1;

	fvco = (I32U) (MUL(h->options.oscKhz,DIV(pll_f, pll_r)));
	pll_od = ((PLL7 & 0x06) >> 1);

	if (pll_od)
		fout = fvco >> pll_od;
	else
		fout = fvco;

	if (pll_m)
		fout = fout >> pll_m;

	h->mainClkKhz = fout;
	TcpalPrintLog((I08S *)"[TCC353X] PLLSet %dkHz\n", h->mainClkKhz);
	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xChangePll(I32S _moduleIndex, I16U _pllValue)
{
	I32S i;

	/* lock all interface */
	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--)
		TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	/* change pll */
	/* slave first for stability */
	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--) {
		/* ALL Parts Disable */
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i], 
				       TC3XREG_SYS_EN_OPCLK);
		TcpalmDelay(1);

		/* dsp reset */
		Tcc353xSetRegSysReset(
			&Tcc353xHandle[_moduleIndex][i], 
			TC3XREG_SYS_RESET_DSP, _UNLOCK_);
		TcpalmDelay(1);
		/* ep reset */
		Tcc353xSetRegSysReset(
			&Tcc353xHandle[_moduleIndex][i], 
			TC3XREG_SYS_RESET_EP, _UNLOCK_);
		/* change pll */
		Tcc353xSetPll(_moduleIndex, i, 1, _pllValue);

		/* DSP Enable */
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i],
				       TC3XREG_SYS_EN_EP |
				       TC3XREG_SYS_EN_DSP |
				       TC3XREG_SYS_EN_OPCLK |
				       TC3XREG_SYS_EN_RF);
	}

	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--)
		Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][i]);

	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--)
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	return TCC353X_RETURN_SUCCESS;
}

static void Tcc353xSaveOption(I32S _moduleIndex, I32S _diversityIndex,
			      Tcc353xOption_t * _Tcc353xOption)
{
	TcpalMemcpy(&Tcc353xHandle[_moduleIndex][_diversityIndex].options,
		    &_Tcc353xOption[0], sizeof(Tcc353xOption_t));
	TcpalMemcpy(&Tcc353xRegisterOptions[_moduleIndex][_diversityIndex],
		    _Tcc353xOption[0].Config,
		    sizeof(Tcc353xRegisterConfig_t));

	Tcc353xHandle[_moduleIndex][_diversityIndex].options.Config =
	    &Tcc353xRegisterOptions[_moduleIndex][_diversityIndex];
	Tcc353xHandle[_moduleIndex][_diversityIndex].moduleIndex =
	    (I08U) _moduleIndex;
	Tcc353xHandle[_moduleIndex][_diversityIndex].diversityIndex = 
	    (I08U)(_diversityIndex);

	switch (Tcc353xHandle[_moduleIndex][_diversityIndex].
		options.commandInterface) {
	case TCC353X_IF_I2C:
		Tcc353xHandle[_moduleIndex][_diversityIndex].currentAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		Tcc353xHandle[_moduleIndex][_diversityIndex].originalAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		break;

	case TCC353X_IF_TCCSPI:
		Tcc353xHandle[_moduleIndex][_diversityIndex].currentAddress =
		    (Tcc353xHandle[_moduleIndex][_diversityIndex].
		     options.address >> 1);
		Tcc353xHandle[_moduleIndex][_diversityIndex].originalAddress =
		    (Tcc353xHandle[_moduleIndex][_diversityIndex].
		     options.address >> 1);
		break;

	default:
		Tcc353xHandle[_moduleIndex][_diversityIndex].currentAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		Tcc353xHandle[_moduleIndex][_diversityIndex].originalAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		TcpalPrintErr((I08S *)
			      "[TCC353X] Driver Can't support your interface yet\n");
		break;
	}
}

static void Tcc353xConnectCommandInterface(I32S _moduleIndex, 
					   I32S _diversityIndex,
					   I08S _commandInterface)
{
	I32U i;
	i = (I32U)(_diversityIndex);

	switch (_commandInterface) {
	case TCC353X_IF_I2C:
		Tcc353xHandle[_moduleIndex][i].Read =
		    Tcc353xI2cRead;
		Tcc353xHandle[_moduleIndex][i].Write =
		    Tcc353xI2cWrite;
		Tcc353xHandle[_moduleIndex][i].currentAddress =
		    Tcc353xHandle[_moduleIndex][i].options.address;
		Tcc353xHandle[_moduleIndex][i].originalAddress =
		    Tcc353xHandle[_moduleIndex][i].options.address;
		TcpalPrintLog((I08S *)
			      "[TCC353X] Interface is I2C\n");
		break;
	
	case TCC353X_IF_TCCSPI:
		Tcc353xHandle[_moduleIndex][i].Read =
		    Tcc353xTccspiRead;
		Tcc353xHandle[_moduleIndex][i].Write =
		    Tcc353xTccspiWrite;
		Tcc353xHandle[_moduleIndex][i].currentAddress =
		    (Tcc353xHandle[_moduleIndex][i].
		     options.address >> 1);
		Tcc353xHandle[_moduleIndex][i].originalAddress =
		    (Tcc353xHandle[_moduleIndex][i].
		     options.address >> 1);
		TcpalPrintLog((I08S *)
			      "[TCC353X] Interface is Tccspi\n");
		break;
	
	default:
		TcpalPrintErr((I08S *)
			      "[TCC353X] Driver Can't support your interface yet\n");
		break;
	}
}


static I32S Tcc353xColdbootParserUtil(I08U * pData, I32U size,
				      Tcc353xBoot_t * pBOOTBin)
{
	I32U idx;
	I32U length;
	I08U *pBin;
	I08U *daguDataPtr=NULL;
	I08U *dintDataPtr=NULL;
	I08U *randDataPtr=NULL;
	I08U *colOrderDataPtr=NULL;
	I32U BootSize[5];

	/*
	 * coldboot         0x00000001
	 * dagu             0x00000002
	 * dint             0x00000003
	 * rand             0x00000004
	 * col_order        0x00000005
	 * sizebyte         4byte
	 * data             nbyte
	 */

	TcpalMemset(BootSize, 0, sizeof(I32U) * 5);

	/* cold boot */
	idx = 0;
	if (pData[idx + 3] != 0x01) {
		return TCC353X_RETURN_FAIL;
	}
	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	BootSize[0] = length;
	pBin = &pData[idx];
	idx += length;
	size -= (length + 8);

	/* dagu */
	if (pData[idx + 3] != 0x02) {
		return TCC353X_RETURN_FAIL;
	}
	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	if (length) {
		daguDataPtr = &pData[idx];
		BootSize[1] = length;
		idx += length;
	} else {
		BootSize[1] = 0;
	}
	size -= (length + 8);

	/* dint */
	if (pData[idx + 3] != 0x03) {
		return TCC353X_RETURN_FAIL;
	}
	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	if (length) {
		dintDataPtr = &pData[idx];
		BootSize[2] = length;
		idx += length;
	} else {
		dintDataPtr = NULL;
		BootSize[2] = 0;
	}
	size -= (length + 8);

	/* rand */
	if (pData[idx + 3] != 0x04) {
		return TCC353X_RETURN_FAIL;
	}

	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	if (length) {
		randDataPtr = &pData[idx];
		BootSize[3] = length;
		idx += length;
	} else {
		randDataPtr = NULL;
		BootSize[3] = 0;
	}
	size -= (length + 8);

	if (size >= 8) {
		if (pData[idx + 3] != 0x05) {
			return TCC353X_RETURN_FAIL;
		}

		idx += 4;
		length =
		    (pData[idx] << 24) + (pData[idx + 1] << 16) +
		    (pData[idx + 2] << 8) + (pData[idx + 3]);
		idx += 4;

		if (length) {
			colOrderDataPtr = &pData[idx];
			BootSize[4] = length;
			idx += length;
		} else {
			colOrderDataPtr = NULL;
			BootSize[4] = 0;
		}
		size -= (length + 8);
	}

	pBOOTBin->coldbootDataPtr = pBin;
	pBOOTBin->coldbootDataSize = BootSize[0];
	pBOOTBin->daguDataPtr = daguDataPtr;
	pBOOTBin->daguDataSize = BootSize[1];
	pBOOTBin->dintDataPtr = dintDataPtr;
	pBOOTBin->dintDataSize = BootSize[2];
	pBOOTBin->randDataPtr = randDataPtr;
	pBOOTBin->randDataSize = BootSize[3];
	pBOOTBin->colOrderDataPtr = colOrderDataPtr;
	pBOOTBin->colOrderDataSize = BootSize[4];

	return TCC353X_RETURN_SUCCESS;
}

I32U Tcc353xGetCoreVersion()
{
	return Tcc353xCoreVersion;
}

I32S Tcc353xMailboxWrite(I32S _moduleIndex, I32S _diversityIndex,
			 I32U _command, I32U * dataArray, I32S wordSize)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	ret = Tcc353xMailboxTxOnly(&Tcc353xHandle[_moduleIndex]
				   [_diversityIndex], _command, dataArray,
				   wordSize);
	return ret;
}

I32S Tcc353xMailboxRead(I32S _moduleIndex, I32S _diversityIndex,
			I32U _command, mailbox_t * _mailbox)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	ret = Tcc353xMailboxTxRx(&Tcc353xHandle[_moduleIndex]
				 [_diversityIndex], _mailbox, _command,
				 NULL, 0);
	return ret;
}

I32S Tcc353xGetOpStatus(I32S _moduleIndex, I32S _diversityIndex,
			   I32U* _opStatusData, I32U _dataSize)
{
	I08U datas[32];
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	Tcc353xGetRegOPStatus(&Tcc353xHandle[_moduleIndex]
				 [_diversityIndex], &datas[0], _dataSize,
				   _UNLOCK_);
	TcpalMemcpy ((void *)(_opStatusData), (void *)(&datas[0]), 32);
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);

	return TCC353X_RETURN_SUCCESS;
}


static I32S Tcc353xSetOpConfig(I32S _moduleIndex, I32S _diversityIndex,
			I32U * _opConfig, I32U _firstFlag)
{
	I32S i;
	I08U opconfigAddress[16];
	I32U opconfigValue[16];
	I32S count = 0;

	if(_firstFlag) {
		Tcc353xMiscWriteExIncrease(_moduleIndex, _diversityIndex,
				      MISC_OP_REG_CTRL, TC3XREG_OP_CFG00, 
				      &_opConfig[0], 16);
	}
	else {
		for(i = 0; i<16; i++)	{
			if((OriginalOpConfig[_moduleIndex][_diversityIndex][i]
			   != _opConfig[i]) || (i==6)) {
				opconfigAddress[count] = (I08U)(i);
				opconfigValue[count] = _opConfig[i];
				count++;
			}
		}
		Tcc353xMiscWriteEx(_moduleIndex, _diversityIndex,
				      MISC_OP_REG_CTRL, &opconfigAddress[0], 
				      &opconfigValue[0], count);
	}

	TcpalMemcpy (&OriginalOpConfig[_moduleIndex][_diversityIndex][0], 
		     _opConfig, sizeof(I32U)*16);
	return TCC353X_RETURN_SUCCESS;
}

static I32U Tcc353xSearchDpllTable (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I64U *_rcStep, 
				    I32U *_adcClkCfg, I32U _defaultPll)
{
	I32U i;
	I32U index;
	I64U data = 0;
	I32U pll;

	pll = _defaultPll;

	for(i = 0; i<_maxFreqNum; i++)	{
		index = (i*5);
		
		if(_tables[index] == 0) {
			/* last search, can't search frequency */
			pll = _tables[index+1];
			break;
		}
		if(_tables[index] == _frequencyInfo) {
			data = _tables[index+2];
			_rcStep[0] = ((data << 32) | _tables[index+3]);
			_adcClkCfg[0] = _tables[index+4];
			pll = _tables[index+1];
			break;
		}
	}

	return pll;
}

static I32S Tcc353xApplySpurSuppression (I32S _moduleIndex, 
					 Tcc353xTuneOptions *_tuneOption, 
					 I64U *_rcStep, I32U *_adcClkCfg,
					 I32U *_icic, I32U _frequencyInfo)
{
	I32U pllValue;
	/* 0 : not match, 1:partial 1seg match, 13:full seg match */
	I64U changeRcStep = _rcStep[0];
	I32U changeadcClkCfg = _adcClkCfg[0];

	pllValue = Tcc353xHandle[_moduleIndex][0].options.pll;

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		Tcc353xSearchDpllTable (_frequencyInfo, 
					&DpllTable_Partial1Seg[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					PLL_ISDB_TMM_PARTIAL_1_SEG);
		break;
	case TCC353X_ISDBT_13SEG:
		pllValue = Tcc353xSearchDpllTable (_frequencyInfo, 
					&DpllTable_FullSeg[0],
					_MAX_FULLSEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					PLL_ISDB_T_FULLSEG);
		
		if (pllValue == PLL_ISDB_TMM_FULLSEG) /* isdb-tmm 13 case */
			_icic[0] = 0; /* icic value change for isdb-tmm*/
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_USER_13SEG[0],
						_MAX_TMM_USER_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_USER_1SEG[0],
						_MAX_TMM_USER_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_13SEG[0],
						_MAX_TMM_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_1SEG[0],
						_MAX_TMM_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		break;
	default:
		return -1;
		break;
	}

	if(Tcc353xHandle[_moduleIndex][0].options.oscKhz != 38400)
		return -1;

	_rcStep[0] = changeRcStep;
	_adcClkCfg[0] = changeadcClkCfg;

	return 0;
}

static void Tcc353xGetOpconfigValues(I32S _moduleIndex,
				     I32S _diversityIndex,
				     Tcc353xTuneOptions * _tuneOption,
				     I32U * _opConfig, I32U _frequencyInfo)
{
	/* opconfig version higher than 0.0.15 */
	I32U LSEL, TDF_SEL, OU, DIV_CFG, AH, GMODE, TMODE, CT_OM,
	    START_SUB_CH, S_TYPE, S, ICIC, ASE;
	I32U ADC_CLK_CFG, FP_CLK_CFG, DIV_CLK_CFG;
	I32U FP_GLB_CFG, ADC_GLB_CFG;
	I32U DC_CFG;
	I32U OM_MODE, ME, TMCC_SEG_FLAG, CFO_SEG_FLAG;
	I32U AFC_STEP;
	I32U CFO_ER;
	I64U RC_STEP;
	I32U frequencyForm;
	I32U semiRfFlag = 0;
	I32U DIV_NUM_CFG = 0;
	I32U AGC_TR_SPEED = 3;

	semiRfFlag = 0;

	if(Tcc353xCurrentDiversityCount[_moduleIndex] > 2)
		DIV_NUM_CFG = 1;
	else
		DIV_NUM_CFG = 0;


	frequencyForm = ((_frequencyInfo >> 4) & 0xFFFF);
	S = 1;
	ME = 0;
	CT_OM = 1;
	TMODE = 0;
	GMODE = 0;
	AH = 1;
	TDF_SEL = 2;	/* 1seg - low if -> 1 */
	LSEL = 0;
	OM_MODE = 0;
	CFO_ER = 3;
	RC_STEP = 0x2747C9D1F2LL;
	ASE = 1;

	if (Tcc353xHandle[_moduleIndex][0].options.boardType ==
	    TCC353X_BOARD_SINGLE) {
		DIV_CFG = 0x00;
	} else {
		if (Tcc353xHandle[_moduleIndex][_diversityIndex].options.
		    diversityPosition == TCC353X_DIVERSITY_MASTER)
			DIV_CFG = 0x15E;
		else if (Tcc353xHandle[_moduleIndex]
			 [_diversityIndex].options.diversityPosition ==
			 TCC353X_DIVERSITY_MID)
			DIV_CFG = 0x11F;
		else
			DIV_CFG = 0x13D;
	}

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		S_TYPE = 0;
		START_SUB_CH = 21;
		ICIC = 0;	/*ICI cancellation */
		OU = 1;
		break;
	case TCC353X_ISDBT_13SEG:
		S_TYPE = 2;
		START_SUB_CH = 3;
		ICIC = 1;	/*ICI cancellation */
		OU = 0;
		break;
	case TCC353X_ISDBTSB_1SEG:
		S_TYPE = 0;
		START_SUB_CH = 3;
		ICIC = 0;	/*ICI cancellation */
		OU = 0;
		break;
	case TCC353X_ISDBTSB_3SEG:
		S_TYPE = 1;
		START_SUB_CH = 3;
		ICIC = 0;	/*ICI cancellation */
		OU = 0;
		break;
	case TCC353X_ISDBTSB_1_OF_3SEG:
		S_TYPE = 0;
		START_SUB_CH = 21;
		ICIC = 0;	/*ICI cancellation */
		OU = 0;
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == A_1st_13Seg ||
		    _tuneOption->tmmSet == A_2nd_13Seg ||
		    _tuneOption->tmmSet == B_1st_13Seg ||
		    _tuneOption->tmmSet == B_2nd_13Seg ||
		    _tuneOption->tmmSet == C_1st_13Seg ||
		    _tuneOption->tmmSet == C_2nd_13Seg ||
		    _tuneOption->tmmSet == UserDefine_Tmm13Seg) {
			/*13seg */
			OU = 0;
			S_TYPE = 2;
			START_SUB_CH = 3;
		} else {
			OU = 1;
			S_TYPE = 0;
			if(_tuneOption->tmmSet == UserDefine_Tmm1Seg)
				START_SUB_CH = 21;
			else if (_tuneOption->tmmSet <= A_7th_1Seg)
				START_SUB_CH =
				    (_tuneOption->tmmSet - A_1st_1Seg) * 3;
			else if (_tuneOption->tmmSet <= B_7th_1Seg)
				START_SUB_CH =
				    (_tuneOption->tmmSet - B_1st_1Seg) * 3;
			else
				START_SUB_CH =
				    (_tuneOption->tmmSet - C_1st_1Seg) * 3;
		}
		ICIC = 0;	/*ICI cancellation */
		break;
	default:
		S_TYPE = 2;
		START_SUB_CH = 3;
		ICIC = 0;	/*ICI cancellation */
		OU = 0;
		break;
	}

	if (S_TYPE == 0)
		ADC_CLK_CFG = 0x28;	/* 1seg, partial 1seg */
	else if (S_TYPE == 1)
		ADC_CLK_CFG = 0x24;	/* 3seg */
	else
		ADC_CLK_CFG = 0x21;	/* 13seg */

	if(Tcc353xHandle[_moduleIndex][0].options.streamInterface== 
						TCC353X_STREAM_IO_MAINIO) {
		if(S_TYPE == 0)
			OM_MODE = 0x00;
		else if(S_TYPE ==2 && _tuneOption->segmentType ==
							TCC353X_ISDBTMM)
			OM_MODE = 0x1c;
		else if(S_TYPE ==2 && _tuneOption->segmentType ==
							TCC353X_ISDBT_13SEG)
			OM_MODE = 0x1c;
		else
			OM_MODE = 0x00;
	}

	FP_CLK_CFG = 0x02;
	DIV_CLK_CFG = 0x02;

	if (Tcc353xHandle[_moduleIndex][0].TuneOptions.rfIfType ==
	    TCC353X_LOW_IF) {
		FP_GLB_CFG = 0x00C9;
		ADC_GLB_CFG = 0x00E1;
		AFC_STEP = 0x03040001;
	} else {
		FP_GLB_CFG = 0x0309;
		ADC_GLB_CFG = 0x00E2;
		AFC_STEP = 0x00000000;
	}

	DC_CFG = 0x0001969A;

	if (S_TYPE == 0) {
		/* 1seg, partial 1seg */
		TMCC_SEG_FLAG = 0x01;
		CFO_SEG_FLAG = 0x01;
		TDF_SEL = 1;
	} else if (S_TYPE == 1) {
		/* 3seg */
		TMCC_SEG_FLAG = 0x07;
		CFO_SEG_FLAG = 0x05;
	} else {
		/* 13seg */
		TMCC_SEG_FLAG = 0x1803;
		CFO_SEG_FLAG = 0x404;
	}

	/* Spur - ADC Clock Control */
	Tcc353xApplySpurSuppression(_moduleIndex, _tuneOption, &RC_STEP, 
				    &ADC_CLK_CFG, &ICIC, _frequencyInfo);

	_opConfig[0] =
	    (LSEL << 30) | (ASE << 29) | (ICIC << 28) | (TDF_SEL << 26) |
	    (OU << 25) | (DIV_CFG << 16) | (AH << 15) | (GMODE << 13) |
	    (TMODE << 11) | (CT_OM << 9) | (START_SUB_CH << 3) | (S_TYPE <<
								  1) | (S);
	_opConfig[1] = 0x368285E5;	/* layer - A only ts resync enable */
	_opConfig[2] =
	   (DIV_NUM_CFG<<31) | (AGC_TR_SPEED<<21) | (semiRfFlag<<20) | 
	   (CFO_ER<<18) | (ADC_CLK_CFG << 12) | 
	   (FP_CLK_CFG << 6) | DIV_CLK_CFG;
	_opConfig[3] = (FP_GLB_CFG << 16) | (ADC_GLB_CFG);
	_opConfig[4] = DC_CFG;
	_opConfig[5] =
	    (OM_MODE << 27) | (ME << 26) | (TMCC_SEG_FLAG << 13) |
	    (CFO_SEG_FLAG);
	_opConfig[6] = ((I32U)((RC_STEP>>32)&0xFF) | (frequencyForm << 16));
	_opConfig[7] = (I32U)(RC_STEP & 0xFFFFFFFF);
	_opConfig[8] = AFC_STEP;
	_opConfig[9] = 0x00000000;
	_opConfig[10] = 0x00000000;
	_opConfig[11] = 0x00000000;
	_opConfig[12] = 0x00000000;
	_opConfig[13] = 0xC2A8FF09;
	_opConfig[14] = 0x01BEFF16;

	if(Tcc353xHandle[_moduleIndex][0].options.basebandName 
	   == BB_TCC3531) {
		_opConfig[15] = 0x03BEFF43;
	} else if(Tcc353xHandle[_moduleIndex][0].options.basebandName 
		== BB_TCC3530) {
		if(S_TYPE==2)
			_opConfig[15] = 0x03BEFF42;
		else
			_opConfig[15] = 0x03BEFF43;
	} else {
		_opConfig[15] = 0x03BEFF23;
	}
}

I32S Tcc353xGetTMCCInfo(I32S _moduleIndex, I32S _diversityIndex,
			tmccInfo_t * _tmccInfo)
{
	mailbox_t mailbox;
	I32S ret = TCC353X_RETURN_SUCCESS;

	ret = Tcc353xMailboxRead(_moduleIndex, _diversityIndex,
				 MBPARA_TMCC_RESULT, &mailbox);

	if (ret != TCC353X_RETURN_SUCCESS)
		return ret;

	_tmccInfo->systemId =
	    (I08U) (((mailbox.data_array[0] >> 10) & 0x03));
	_tmccInfo->transParamSwitch =
	    (I08U) (((mailbox.data_array[0] >> 6) & 0x0F));
	_tmccInfo->startFlagEmergencyAlarm =
	    (I08U) (((mailbox.data_array[0] >> 5) & 0x01));

	_tmccInfo->currentInfo.partialReceptionFlag =
	    (I08U) (((mailbox.data_array[0] >> 4) & 0x01));
	_tmccInfo->currentInfo.transParammLayerA =
	    (I16U) (((mailbox.
		      data_array[1] >> 23) & 0x1FF) |
		    ((mailbox.data_array[0] & 0x0F)
		     << 9));
	_tmccInfo->currentInfo.transParammLayerB =
	    (I16U) (((mailbox.data_array[1]) >> 10) & 0x1FFF);
	_tmccInfo->currentInfo.transParammLayerC =
	    (I16U) (((mailbox.
		      data_array[2] >> 29) & 0x07) |
		    ((mailbox.data_array[1] & 0x3FF)
		     << 3));

	_tmccInfo->nextInfo.partialReceptionFlag =
	    (I08U) (((mailbox.data_array[2] >> 28) & 0x01));
	_tmccInfo->nextInfo.transParammLayerA =
	    (I16U) (((mailbox.data_array[2] >> 15) & 0x1FFF));
	_tmccInfo->nextInfo.transParammLayerB =
	    (I16U) (((mailbox.data_array[2] >> 2) & 0x1FFF));
	_tmccInfo->nextInfo.transParammLayerC =
	    (I16U) (((mailbox.
		      data_array[3] >> 21) & 0x7FF) |
		    ((mailbox.data_array[2] & 0x3)
		     << 11));

	_tmccInfo->phaseShiftCorrectionValue =
	    (I08U) (((mailbox.data_array[3] >> 18) & 0x07));

	return ret;
}

I32S Tcc353xCasOpen(I32S _moduleIndex, I32U _casRound, I08U * _systemKey)
{
	I08U i;
	I32U currentOpFilterConfig = 0x00;
	I32U systemKey[8];

	Tcc353xMiscRead(_moduleIndex, 0, MISC_OP_REG_CTRL,
			TC3XREG_OP_FILTER_CFG, &currentOpFilterConfig);
	currentOpFilterConfig = currentOpFilterConfig & 0x3FF;
	currentOpFilterConfig |= ((((_casRound << 3) - 1) & 0xFFFF) << 16);
	currentOpFilterConfig |= 0x6C00;
	currentOpFilterConfig |= 0x8000;   

	Tcc353xMiscWrite(_moduleIndex, 0, MISC_OP_REG_CTRL,
			 TC3XREG_OP_FILTER_CFG, currentOpFilterConfig);

	for (i = 0; i < 8; i++) {
		int idx;
		idx = (i << 2);
		systemKey[i] = 0;
		systemKey[i] |= (_systemKey[idx + 3] << 24);
		systemKey[i] |= (_systemKey[idx + 2] << 16);
		systemKey[i] |= (_systemKey[idx + 1] << 8);
		systemKey[i] |= (_systemKey[idx]);
	}
	Tcc353xMiscWriteExIncrease(_moduleIndex, 0,
			   MISC_OP_REG_CTRL, TC3XREG_OP_CAS_SYSTEM_KEY0, 
			   &systemKey[0], 8);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xCasSetPid(I32S _moduleIndex, I32U * _pids, I32U _numberOfPids)
{
	return TCC353X_RETURN_SUCCESS;	 
}

I32S Tcc353xCasSetKeyMulti2(I32S _moduleIndex, I32S _parity,
			    I08U * _key, I32S _keyLength,
			    I08U * _initVector, I32S _initVectorLength)
{
	I32U keyLow;
	I32U keyHigh;
	I32U inputKey[2];

	if (_parity != 0) {
		keyHigh =
		    (_key[3] << 24) | (_key[2] << 16) | (_key[1] <<
							 8) | _key[0];
		keyLow =
		    (_key[7] << 24) | (_key[6] << 16) | (_key[5] <<
							 8) | _key[4];;

		inputKey[0] = keyLow;
		inputKey[1] = keyHigh;
		Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
				   TC3XREG_OP_CAS_PID0O_EVEN_KEY_L, 
				   &inputKey[0], 2);
	} else {
		keyHigh =
		    (_key[3] << 24) | (_key[2] << 16) | (_key[1] <<
							 8) | _key[0];
		keyLow =
		    (_key[7] << 24) | (_key[6] << 16) | (_key[5] <<
							 8) | _key[4];;

		inputKey[0] = keyLow;
		inputKey[1] = keyHigh;
		Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
				   TC3XREG_OP_CAS_PID0O_ODD_KEY_L, 
				   &inputKey[0], 2);
	}

	if (_initVectorLength != 0) {
		keyHigh = (_initVector[3] << 24) | (_initVector[2] << 16)
		    | (_initVector[1] << 8) | _initVector[0];
		keyLow = (_initVector[7] << 24) | (_initVector[6] << 16)
		    | (_initVector[5] << 8) | _initVector[4];;

		inputKey[0] = keyLow;
		inputKey[1] = keyHigh;
		Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
				   TC3XREG_OP_CAS_IV0, 
				   &inputKey[0], 2);
	}

	return TCC353X_RETURN_SUCCESS;
}

static I32U Tcc353xDspRestart(I32S _moduleIndex, I32S _diversityIndex)
{
	TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex]
			   [_diversityIndex]);
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], 
			      TC3XREG_SYS_RESET_DSP, _LOCK_);
	Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][_diversityIndex]);
	TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex]
			     [_diversityIndex]);
	TcpalPrintLog((I08S *) "[TCC353X] SYS_RESET(DSP)\n");
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xGetFifoStatus(I32S _moduleIndex, I32U *_fifoSize)
{
	I08U data[2];
	Tcc353xGetRegFifoAStatus(&Tcc353xHandle[_moduleIndex][0], &data[0]);
	_fifoSize[0] = (((data[0]<<8) | data[1])<<2);
	return TCC353X_RETURN_SUCCESS;
}


I32S Tcc353xUserCommand(I32S _moduleIndex, I32S _diversityIndex,
			I32S _command, void *_param1,
			void *_param2, void *_param3, void *_param4)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	switch (_command) {
	case TCC353X_CMD_DSP_RESET:
		Tcc353xDspRestart(_moduleIndex, _diversityIndex);
		break;

	default:
		ret = TCC353X_RETURN_FAIL_UNKNOWN;
		break;
	}

	return ret;
}

/* Dummy functions */

I32S DummyFunction0(I32S _moduleIndex, I32S _chipAddress,
		    I08U _inputData, I08U * _outData, I32S _size)
{
	TcpalPrintLog((I08S *)
		      "[TCC353X] Access dummy function 0\n");
	return TCC353X_RETURN_SUCCESS;
}

I32S DummyFunction1(I32S _moduleIndex, I32S _chipAddress,
		    I08U _address, I08U * _inputData, I32S _size)
{
	TcpalPrintLog((I08S *)
		      "[TCC353X] Access dummy function 1\n");
	return TCC353X_RETURN_SUCCESS;
}
