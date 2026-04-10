/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

#include <limits.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */

/* ================== 如果在CubeMX中未定义硬件复位引脚，这里提供默认值 ================== */
#ifndef RST_Pin
#define RST_Pin       GPIO_PIN_2
#define RST_GPIO_Port GPIOA
#endif

/* ================== ADS1256 宏定义 ================== */
/* 寄存器地址 */
#define ADS1256_REG_STATUS      0x00U
#define ADS1256_REG_MUX         0x01U
#define ADS1256_REG_ADCON       0x02U
#define ADS1256_REG_DRATE       0x03U
#define ADS1256_REG_IO          0x04U

/* SPI 命令 */
#define ADS1256_CMD_WAKEUP      0x00U
#define ADS1256_CMD_RDATA       0x01U
#define ADS1256_CMD_RDATAC      0x03U
#define ADS1256_CMD_SDATAC      0x0FU
#define ADS1256_CMD_SYNC        0xFCU
#define ADS1256_CMD_STANDBY     0xFDU
#define ADS1256_CMD_RESET       0xFEU
#define ADS1256_CMD_SELFCAL     0xF0U

/* 放大倍数 (PGA) */
#define ADS1256_GAIN_1          0x00U
#define ADS1256_GAIN_2          0x01U
#define ADS1256_GAIN_4          0x02U
#define ADS1256_GAIN_8          0x03U
#define ADS1256_GAIN_16         0x04U
#define ADS1256_GAIN_32         0x05U
#define ADS1256_GAIN_64         0x06U

/* ADCON 其它位设置 */
#define ADS1256_CLKOUT_OFF      0x00U /* CLK1:CLK0 = 00 */
#define ADS1256_SDCS_OFF        0x00U /* SDCS1:SDCS0 = 00 */

/* 采样率 (Data Rate) */
#define ADS1256_DRATE_30000SPS  0xF0U
#define ADS1256_DRATE_15000SPS  0xE0U
#define ADS1256_DRATE_7500SPS   0xD0U
#define ADS1256_DRATE_3750SPS   0xC0U
#define ADS1256_DRATE_2000SPS   0xB0U
#define ADS1256_DRATE_1000SPS   0xA1U
#define ADS1256_DRATE_500SPS    0x92U
#define ADS1256_DRATE_100SPS    0x82U
#define ADS1256_DRATE_60SPS     0x72U
#define ADS1256_DRATE_50SPS     0x63U
#define ADS1256_DRATE_30SPS     0x53U
#define ADS1256_DRATE_25SPS     0x43U
#define ADS1256_DRATE_15SPS     0x33U
#define ADS1256_DRATE_10SPS     0x23U
#define ADS1256_DRATE_5SPS      0x13U
#define ADS1256_DRATE_2_5SPS    0x03U

/* 通道定义 */
#define ADS1256_AIN0            0x00U
#define ADS1256_AIN1            0x01U
#define ADS1256_AIN2            0x02U
#define ADS1256_AIN3            0x03U
#define ADS1256_AIN4            0x04U
#define ADS1256_AIN5            0x05U
#define ADS1256_AIN6            0x06U
#define ADS1256_AIN7            0x07U
#define ADS1256_AINCOM          0x08U

/* 延时时间参数 (基于 fCLKIN = 7.68MHz) */
/* t6 >= 50 * tCLKIN (约 6.51us) */
#define ADS1256_DELAY_T6_US     8U
/* t11 对 SYNC/RDATAC 场景需 >= 24 * tCLKIN (约 3.13us) */
#define ADS1256_DELAY_T11_US    4U

/* 基础底层函数 */
static HAL_StatusTypeDef ADS1256_WriteReg(uint8_t reg, uint8_t value);
static HAL_StatusTypeDef ADS1256_SendCmd(uint8_t cmd);
static HAL_StatusTypeDef ADS1256_ReadReg(uint8_t reg, uint8_t *value);
static HAL_StatusTypeDef ADS1256_WaitDRDY(uint32_t timeout_ms);
static void ADS1256_DelayUs(uint32_t us);
static void ADS1256_EnableGPIOClock(GPIO_TypeDef *port);
static HAL_StatusTypeDef ADS1256_SetPGAValue(uint8_t pga);
static HAL_StatusTypeDef ADS1256_SetDataRate(uint8_t drate);
static uint32_t ADS1256_DrdyTimeoutMsByDrate(uint8_t drate);
static void ADS1256_ProcessPendingCommand(void);

/* 功能 API */
static HAL_StatusTypeDef ADS1256_Init(void);
static HAL_StatusTypeDef ADS1256_SetChannel(uint8_t pos_ch, uint8_t neg_ch);
static HAL_StatusTypeDef ADS1256_Read_ADC(uint8_t pos_ch, uint8_t neg_ch, int32_t *adc_out);
static HAL_StatusTypeDef ADS1256_Read_ADC_Scan8(int32_t adc_out[8], uint8_t *failed_ch);

/* 连续模式 (RDATAC) 功能 API */
static HAL_StatusTypeDef ADS1256_StartContinuousMode(void);
static HAL_StatusTypeDef ADS1256_StopContinuousMode(void);
static HAL_StatusTypeDef ADS1256_Read_ADC_Continuous(int32_t *adc_out);
static HAL_StatusTypeDef ADS1256_RecoverContinuousStream(void);

/* 在 usbd_cdc_if.c 的 CDC_Receive_FS 中调用该函数传入接收数据。 */
void ADS1256_CDC_OnRxData(const uint8_t *buf, uint32_t len);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

#define ADS1256_CMD_MAX_LEN 120U
#define ADS1256_ZERO_STREAK_RECOVER 12U

typedef enum
{
  ADS1256_READ_RDATA = 0,
  ADS1256_READ_RDATAC = 1
} ADS1256_ReadMode;

typedef enum
{
  ADS1256_ACQ_STOP = 0,
  ADS1256_ACQ_CONTINUOUS = 1
} ADS1256_AcqState;

typedef enum
{
  ADS1256_SAMPLE_SINGLE = 0,
  ADS1256_SAMPLE_SCAN8 = 1
} ADS1256_SampleMode;

static volatile char g_rx_line[ADS1256_CMD_MAX_LEN];
static volatile char g_cmd_line[ADS1256_CMD_MAX_LEN];
static volatile uint16_t g_rx_idx = 0U;
static volatile uint8_t g_cmd_ready = 0U;

static uint8_t g_cfg_pos = ADS1256_AIN0;
static uint8_t g_cfg_neg = ADS1256_AINCOM;
static uint8_t g_cfg_pga = 1U;
static uint8_t g_cfg_drate = ADS1256_DRATE_100SPS;
static uint8_t g_scan_mask = 0xFFU;
static ADS1256_ReadMode g_read_mode = ADS1256_READ_RDATA;
static ADS1256_AcqState g_acq_state = ADS1256_ACQ_STOP;
static ADS1256_SampleMode g_sample_mode = ADS1256_SAMPLE_SINGLE;
static uint8_t g_rdatac_active = 0U;
static uint16_t g_zero_streak = 0U;
static int32_t g_scan_cache[8] = {0};
static uint8_t g_scan_cache_valid_mask = 0U;

static uint8_t ADS1256_PgaToCode(uint8_t pga)
{
  switch (pga)
  {
    case 1U:  return ADS1256_GAIN_1;
    case 2U:  return ADS1256_GAIN_2;
    case 4U:  return ADS1256_GAIN_4;
    case 8U:  return ADS1256_GAIN_8;
    case 16U: return ADS1256_GAIN_16;
    case 32U: return ADS1256_GAIN_32;
    case 64U: return ADS1256_GAIN_64;
    default:  return 0xFFU;
  }
}

static int ADS1256_StrEqNoCase(const char *a, const char *b)
{
  while ((*a != '\0') && (*b != '\0'))
  {
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
    {
      return 0;
    }
    a++;
    b++;
  }

  return (*a == '\0') && (*b == '\0');
}

static int ADS1256_ParseChannel(const char *text, uint8_t *ch)
{
  size_t n;

  if ((text == NULL) || (ch == NULL))
  {
    return 0;
  }

  n = strlen(text);

  if (ADS1256_StrEqNoCase(text, "AINCOM"))
  {
    *ch = ADS1256_AINCOM;
    return 1;
  }

  if ((n == 4U) &&
      (toupper((unsigned char)text[0]) == 'A') &&
      (toupper((unsigned char)text[1]) == 'I') &&
      (toupper((unsigned char)text[2]) == 'N') &&
      (text[3] >= '0') && (text[3] <= '7'))
  {
    *ch = (uint8_t)(text[3] - '0');
    return 1;
  }

  return 0;
}

static HAL_StatusTypeDef ADS1256_SetPGAValue(uint8_t pga)
{
  uint8_t code = ADS1256_PgaToCode(pga);
  if (code == 0xFFU)
  {
    return HAL_ERROR;
  }

  g_cfg_pga = pga;
  return ADS1256_WriteReg(ADS1256_REG_ADCON, (uint8_t)(ADS1256_CLKOUT_OFF | ADS1256_SDCS_OFF | code));
}

static HAL_StatusTypeDef ADS1256_SetDataRate(uint8_t drate)
{
  g_cfg_drate = drate;
  return ADS1256_WriteReg(ADS1256_REG_DRATE, drate);
}

static uint32_t ADS1256_DrdyTimeoutMsByDrate(uint8_t drate)
{
  switch (drate)
  {
    case ADS1256_DRATE_30000SPS:
    case ADS1256_DRATE_15000SPS:
    case ADS1256_DRATE_7500SPS:
    case ADS1256_DRATE_3750SPS:
    case ADS1256_DRATE_2000SPS:
    case ADS1256_DRATE_1000SPS:
    case ADS1256_DRATE_500SPS:
      return 20U;

    case ADS1256_DRATE_100SPS:
    case ADS1256_DRATE_60SPS:
    case ADS1256_DRATE_50SPS:
    case ADS1256_DRATE_30SPS:
    case ADS1256_DRATE_25SPS:
    case ADS1256_DRATE_15SPS:
      return 150U;

    case ADS1256_DRATE_10SPS:
      return 250U;

    case ADS1256_DRATE_5SPS:
      return 450U;

    case ADS1256_DRATE_2_5SPS:
      return 850U;

    default:
      return 200U;
  }
}

static void ADS1256_CopyPendingCommand(char *dst, uint32_t dst_size)
{
  uint32_t i;

  if ((dst == NULL) || (dst_size == 0U))
  {
    return;
  }

  __disable_irq();
  for (i = 0U; i < (dst_size - 1U); i++)
  {
    char c = g_cmd_line[i];
    dst[i] = c;
    if (c == '\0')
    {
      break;
    }
  }
  dst[dst_size - 1U] = '\0';
  g_cmd_ready = 0U;
  __enable_irq();
}

static void ADS1256_PrintConfig(void)
{
  char nsel_text[10];
  const char *mode_text = (g_read_mode == ADS1256_READ_RDATAC) ? "RDATAC" : "RDATA";
  const char *acq_text = (g_sample_mode == ADS1256_SAMPLE_SCAN8) ? "MULTI" : "SINGLE";

  if (g_cfg_neg == ADS1256_AINCOM)
  {
    (void)snprintf(nsel_text, sizeof(nsel_text), "AINCOM");
  }
  else
  {
    (void)snprintf(nsel_text, sizeof(nsel_text), "AIN%u", (unsigned int)g_cfg_neg);
  }

    printf("CFG PSEL=AIN%u NSEL=%s PGA=%u DRATE=0x%02X MODE=%s ACQ=%s CHMASK=0x%02X\r\n",
         (unsigned int)g_cfg_pos,
         nsel_text,
         (unsigned int)g_cfg_pga,
         (unsigned int)g_cfg_drate,
      mode_text,
      acq_text,
      (unsigned int)g_scan_mask);
}

static void ADS1256_ExecuteCommand(const char *line)
{
  char local[ADS1256_CMD_MAX_LEN];
  char *token;

  uint8_t new_pos;
  uint8_t new_neg;
  uint8_t new_pga;
  uint8_t new_drate;
  uint8_t new_scan_mask;
  ADS1256_ReadMode new_mode;
  ADS1256_SampleMode new_acq;

  uint8_t has_pos = 0U;
  uint8_t has_neg = 0U;
  uint8_t has_pga = 0U;
  uint8_t has_drate = 0U;
  uint8_t has_scan_mask = 0U;
  uint8_t has_mode = 0U;
  uint8_t has_acq = 0U;

  HAL_StatusTypeDef st;

  if (line == NULL)
  {
    return;
  }

  (void)snprintf(local, sizeof(local), "%s", line);
  token = strtok(local, " ");
  if (token == NULL)
  {
    return;
  }

  if (ADS1256_StrEqNoCase(token, "START"))
  {
    g_acq_state = ADS1256_ACQ_CONTINUOUS;
    g_zero_streak = 0U;
    printf("ACQ START\r\n");
    return;
  }

  if (ADS1256_StrEqNoCase(token, "STOP"))
  {
    g_acq_state = ADS1256_ACQ_STOP;
    if (g_rdatac_active != 0U)
    {
      (void)ADS1256_StopContinuousMode();
      g_rdatac_active = 0U;
    }
    g_zero_streak = 0U;
    printf("ACQ STOP\r\n");
    return;
  }

  if (ADS1256_StrEqNoCase(token, "SINGLE"))
  {
    g_sample_mode = ADS1256_SAMPLE_SINGLE;
    printf("ACQ MODE SINGLE\r\n");
    return;
  }

  if (ADS1256_StrEqNoCase(token, "SCAN8") || ADS1256_StrEqNoCase(token, "MULTI"))
  {
    g_sample_mode = ADS1256_SAMPLE_SCAN8;
    if (g_rdatac_active != 0U)
    {
      (void)ADS1256_StopContinuousMode();
      g_rdatac_active = 0U;
    }
    g_read_mode = ADS1256_READ_RDATA;
    printf("ACQ MODE MULTI\r\n");
    return;
  }

  if (ADS1256_StrEqNoCase(token, "RESET"))
  {
    st = ADS1256_Init();
    if (st == HAL_OK)
    {
      g_rdatac_active = 0U;
      g_read_mode = ADS1256_READ_RDATA;
      g_acq_state = ADS1256_ACQ_STOP;
      g_cfg_pos = ADS1256_AIN0;
      g_cfg_neg = ADS1256_AINCOM;
      g_cfg_pga = 1U;
      g_cfg_drate = ADS1256_DRATE_100SPS;
      g_scan_mask = 0xFFU;
      g_scan_cache_valid_mask = 0U;
      g_sample_mode = ADS1256_SAMPLE_SINGLE;
      g_zero_streak = 0U;
      printf("RESET OK\r\n");
    }
    else
    {
      printf("RESET FAIL err=%d\r\n", (int)st);
    }
    return;
  }

  if (ADS1256_StrEqNoCase(token, "SELFCAL"))
  {
    st = ADS1256_SendCmd(ADS1256_CMD_SELFCAL);
    if (st == HAL_OK)
    {
      st = ADS1256_WaitDRDY(1000U);
    }
    if (st == HAL_OK)
    {
      printf("SELFCAL OK\r\n");
    }
    else
    {
      printf("SELFCAL FAIL err=%d\r\n", (int)st);
    }
    return;
  }

  if (ADS1256_StrEqNoCase(token, "SYNC"))
  {
    st = ADS1256_SendCmd(ADS1256_CMD_SYNC);
    if (st == HAL_OK)
    {
      printf("SYNC OK\r\n");
    }
    else
    {
      printf("SYNC FAIL err=%d\r\n", (int)st);
    }
    return;
  }

  if (ADS1256_StrEqNoCase(token, "WAKEUP"))
  {
    st = ADS1256_SendCmd(ADS1256_CMD_WAKEUP);
    if (st == HAL_OK)
    {
      printf("WAKEUP OK\r\n");
    }
    else
    {
      printf("WAKEUP FAIL err=%d\r\n", (int)st);
    }
    return;
  }

  if (ADS1256_StrEqNoCase(token, "RDATAC"))
  {
    if (g_sample_mode == ADS1256_SAMPLE_SCAN8)
    {
      printf("ERR RDATAC unsupported in MULTI mode\r\n");
      return;
    }
    g_read_mode = ADS1256_READ_RDATAC;
    g_acq_state = ADS1256_ACQ_CONTINUOUS;
    printf("MODE RDATAC\r\n");
    return;
  }

  if (ADS1256_StrEqNoCase(token, "SDATAC"))
  {
    if (g_rdatac_active != 0U)
    {
      (void)ADS1256_StopContinuousMode();
      g_rdatac_active = 0U;
    }
    g_read_mode = ADS1256_READ_RDATA;
    g_acq_state = ADS1256_ACQ_STOP;
    g_zero_streak = 0U;
    printf("MODE RDATA\r\n");
    return;
  }

  if (!ADS1256_StrEqNoCase(token, "CFG"))
  {
    printf("ERR unknown cmd: %s\r\n", token);
    return;
  }

  new_pos = g_cfg_pos;
  new_neg = g_cfg_neg;
  new_pga = g_cfg_pga;
  new_drate = g_cfg_drate;
  new_scan_mask = g_scan_mask;
  new_mode = g_read_mode;
  new_acq = g_sample_mode;

  while ((token = strtok(NULL, " ")) != NULL)
  {
    char *eq = strchr(token, '=');
    if (eq == NULL)
    {
      continue;
    }

    *eq = '\0';
    eq++;

    if (ADS1256_StrEqNoCase(token, "PSEL"))
    {
      if (ADS1256_ParseChannel(eq, &new_pos) != 0)
      {
        has_pos = 1U;
      }
    }
    else if (ADS1256_StrEqNoCase(token, "NSEL"))
    {
      if (ADS1256_ParseChannel(eq, &new_neg) != 0)
      {
        has_neg = 1U;
      }
    }
    else if (ADS1256_StrEqNoCase(token, "PGA"))
    {
      uint32_t val = strtoul(eq, NULL, 10);
      if ((val == 1U) || (val == 2U) || (val == 4U) || (val == 8U) ||
          (val == 16U) || (val == 32U) || (val == 64U))
      {
        new_pga = (uint8_t)val;
        has_pga = 1U;
      }
    }
    else if (ADS1256_StrEqNoCase(token, "DRATE"))
    {
      uint32_t val;
      if ((eq[0] == '0') && ((eq[1] == 'x') || (eq[1] == 'X')))
      {
        val = strtoul(eq + 2, NULL, 16);
      }
      else
      {
        val = strtoul(eq, NULL, 0);
      }

      if (val <= 0xFFU)
      {
        new_drate = (uint8_t)val;
        has_drate = 1U;
      }
    }
    else if (ADS1256_StrEqNoCase(token, "CHMASK"))
    {
      uint32_t val;
      if ((eq[0] == '0') && ((eq[1] == 'x') || (eq[1] == 'X')))
      {
        val = strtoul(eq + 2, NULL, 16);
      }
      else
      {
        val = strtoul(eq, NULL, 0);
      }

      if ((val <= 0xFFU) && (val != 0U))
      {
        new_scan_mask = (uint8_t)val;
        has_scan_mask = 1U;
      }
    }
    else if (ADS1256_StrEqNoCase(token, "MODE"))
    {
      if (ADS1256_StrEqNoCase(eq, "RDATAC"))
      {
        new_mode = ADS1256_READ_RDATAC;
        has_mode = 1U;
      }
      else if (ADS1256_StrEqNoCase(eq, "RDATA"))
      {
        new_mode = ADS1256_READ_RDATA;
        has_mode = 1U;
      }
    }
    else if (ADS1256_StrEqNoCase(token, "ACQ"))
    {
      if (ADS1256_StrEqNoCase(eq, "SINGLE"))
      {
        new_acq = ADS1256_SAMPLE_SINGLE;
        has_acq = 1U;
      }
      else if (ADS1256_StrEqNoCase(eq, "SCAN8") || ADS1256_StrEqNoCase(eq, "MULTI"))
      {
        new_acq = ADS1256_SAMPLE_SCAN8;
        has_acq = 1U;
      }
    }
  }

  st = HAL_OK;
  if (has_pga != 0U)
  {
    st = ADS1256_SetPGAValue(new_pga);
  }

  if ((st == HAL_OK) && (has_drate != 0U))
  {
    st = ADS1256_SetDataRate(new_drate);
  }

  if ((st == HAL_OK) && ((has_pos != 0U) || (has_neg != 0U)))
  {
    st = ADS1256_SetChannel(new_pos, new_neg);
    if (st == HAL_OK)
    {
      g_cfg_pos = new_pos;
      g_cfg_neg = new_neg;
    }
  }

  if ((st == HAL_OK) && (has_acq != 0U))
  {
    g_sample_mode = new_acq;
  }

  if ((st == HAL_OK) && (has_scan_mask != 0U))
  {
    g_scan_mask = new_scan_mask;
    g_scan_cache_valid_mask &= g_scan_mask;
  }

  if ((st == HAL_OK) && ((has_mode != 0U) || (has_acq != 0U)))
  {
    if ((new_acq == ADS1256_SAMPLE_SCAN8) && (new_mode == ADS1256_READ_RDATAC))
    {
      new_mode = ADS1256_READ_RDATA;
      printf("WARN MULTI forces MODE=RDATA\r\n");
    }

    if ((new_mode == ADS1256_READ_RDATA) && (g_rdatac_active != 0U))
    {
      (void)ADS1256_StopContinuousMode();
      g_rdatac_active = 0U;
    }
    g_read_mode = new_mode;
  }

  if (st == HAL_OK)
  {
    g_cfg_pga = new_pga;
    g_cfg_drate = new_drate;
    g_scan_mask = new_scan_mask;
    g_scan_cache_valid_mask &= g_scan_mask;
    ADS1256_PrintConfig();
    printf("CFG OK\r\n");
  }
  else
  {
    printf("CFG FAIL err=%d\r\n", (int)st);
  }
}

static void ADS1256_ProcessPendingCommand(void)
{
  char cmd[ADS1256_CMD_MAX_LEN];

  if (g_cmd_ready == 0U)
  {
    return;
  }

  ADS1256_CopyPendingCommand(cmd, sizeof(cmd));
  ADS1256_ExecuteCommand(cmd);
}

void ADS1256_CDC_OnRxData(const uint8_t *buf, uint32_t len)
{
  uint32_t i;

  if (buf == NULL)
  {
    return;
  }

  for (i = 0U; i < len; i++)
  {
    char c = (char)buf[i];

    if (c == '\r')
    {
      continue;
    }

    if (c == '\n')
    {
      if (g_rx_idx > 0U)
      {
        uint16_t j;
        __disable_irq();
        g_rx_line[g_rx_idx] = '\0';
        if (g_cmd_ready == 0U)
        {
          for (j = 0U; j <= g_rx_idx; j++)
          {
            g_cmd_line[j] = g_rx_line[j];
          }
          g_cmd_ready = 1U;
        }
        g_rx_idx = 0U;
        __enable_irq();
      }
      continue;
    }

    if (g_rx_idx < (ADS1256_CMD_MAX_LEN - 1U))
    {
      g_rx_line[g_rx_idx++] = c;
    }
    else
    {
      g_rx_idx = 0U;
    }
  }
}

/* ================== 底层 SPI 通信 ================== */

/**
  * @brief  写单个寄存器
  * @note   要求 CS 在整个命令周期内保持低电平。
  */
static HAL_StatusTypeDef ADS1256_WriteReg(uint8_t reg, uint8_t value)
{
  uint8_t buf[3];
  HAL_StatusTypeDef st;

  buf[0] = 0x50U | (reg & 0x0FU); // WREG 命令
  buf[1] = 0x00U;                 // 写入 1 个寄存器 (0+1)
  buf[2] = value;                 // 寄存器值

  /* 规范：整个命令期间 CS 必须保持低 */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
  st = HAL_SPI_Transmit(&hspi1, buf, 3, 100);
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  ADS1256_DelayUs(ADS1256_DELAY_T11_US);
  return st;
}

/**
  * @brief  读单个寄存器
  * @note   规范要求：读命令(2字节) -> 等待t6 -> 读取数据(1字节)。整个过程CS不能拉高！
  */
static HAL_StatusTypeDef ADS1256_ReadReg(uint8_t reg, uint8_t *value)
{
  uint8_t tx[2];
  HAL_StatusTypeDef st;

  if (value == NULL) return HAL_ERROR;

  tx[0] = 0x10U | (reg & 0x0FU); // RREG 命令
  tx[1] = 0x00U;                 // 读取 1 个寄存器 (0+1)

  /* 规范：整个命令期间 CS 必须保持低 */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  st = HAL_SPI_Transmit(&hspi1, tx, 2, 100);
  if (st != HAL_OK) goto end;

  /* RREG 命令后必须等待 t6 时间才能开始产生SCLK去读数据 */
  ADS1256_DelayUs(ADS1256_DELAY_T6_US);

  st = HAL_SPI_Receive(&hspi1, value, 1, 100);

end:
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
  ADS1256_DelayUs(ADS1256_DELAY_T11_US);
  return st;
}

/**
  * @brief  发送单字节命令
  */
static HAL_StatusTypeDef ADS1256_SendCmd(uint8_t cmd)
{
  HAL_StatusTypeDef st;
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
  st = HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  ADS1256_DelayUs(ADS1256_DELAY_T11_US);
  return st;
}

static HAL_StatusTypeDef ADS1256_WaitDRDY(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  while (HAL_GPIO_ReadPin(DRDY_GPIO_Port, DRDY_Pin) == GPIO_PIN_SET)
  {
    if ((HAL_GetTick() - start) > timeout_ms) return HAL_TIMEOUT;
  }
  return HAL_OK;
}

static void ADS1256_DelayUs(uint32_t us)
{
  uint32_t i;
  volatile uint32_t count;
  for (i = 0; i < us; i++) { count = 12U; while (count-- > 0U) { __NOP(); } }
}

static void ADS1256_EnableGPIOClock(GPIO_TypeDef *port)
{
  if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
  else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
  else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
  else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
  else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
  else if (port == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
}

/* ================== 功能 API 实现 ================== */

static HAL_StatusTypeDef ADS1256_SetChannel(uint8_t pos_ch, uint8_t neg_ch)
{
  uint8_t mux = ((pos_ch & 0x0FU) << 4) | (neg_ch & 0x0FU);
  return ADS1256_WriteReg(ADS1256_REG_MUX, mux);
}

/**
  * @brief  初始化 ADS1256
  * @note   包含硬件复位控制、寄存器配置以及读回校验机制。
  */
static HAL_StatusTypeDef ADS1256_Init(void)
{
  HAL_StatusTypeDef st;
  uint8_t read_val;

  /* 1. 硬件复位 (控制 RST 引脚) */
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(5);
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);

  /* 硬件复位后，等待振荡器启动以及内部复位完成 (手册建议至少30ms) */
  HAL_Delay(50);

  st = ADS1256_WaitDRDY(200U);
  if (st != HAL_OK) return st;

  /* 2. 发送软件复位指令 (双重保险) */
  st = ADS1256_SendCmd(ADS1256_CMD_RESET);
  if (st != HAL_OK) return st;
  HAL_Delay(5);
  st = ADS1256_WaitDRDY(200U);
  if (st != HAL_OK) return st;

  /* 3. 停止连续读取模式 (上电默认在某些操作下可能处于 RDATAC) */
  st = ADS1256_SendCmd(ADS1256_CMD_SDATAC);
  if (st != HAL_OK) return st;

  /* 4. 配置寄存器: 关闭自动校准(ACAL=0), 关闭Buffer(BUFEN=0) */
  uint8_t config_status = 0x00U;
  st = ADS1256_WriteReg(ADS1256_REG_STATUS, config_status);

  /* 增益: 1倍 (PGA=0), 关闭时钟输出(CLKOUT=OFF), 关闭SensorDetect */
  uint8_t config_adcon = ADS1256_GAIN_1 | ADS1256_CLKOUT_OFF | ADS1256_SDCS_OFF;
  st |= ADS1256_WriteReg(ADS1256_REG_ADCON, config_adcon);

  /* 采样率: 100 SPS */
  uint8_t config_drate = ADS1256_DRATE_100SPS;
  st |= ADS1256_WriteReg(ADS1256_REG_DRATE, config_drate);

  if (st != HAL_OK) return HAL_ERROR;

  /* 5. 寄存器读回校验 */
  st = ADS1256_ReadReg(ADS1256_REG_STATUS, &read_val);
  if (st != HAL_OK || (read_val & 0x06U) != config_status) return HAL_ERROR;

  st = ADS1256_ReadReg(ADS1256_REG_ADCON, &read_val);
  if (st != HAL_OK || read_val != config_adcon) return HAL_ERROR;

  st = ADS1256_ReadReg(ADS1256_REG_DRATE, &read_val);
  if (st != HAL_OK || read_val != config_drate) return HAL_ERROR;

  /* 6. 执行一次彻底的自校准 */
  st = ADS1256_SendCmd(ADS1256_CMD_SELFCAL);
  if (st != HAL_OK) return st;

  /* 校准过程需要时间，阻塞等待 DRDY 变低表示校准完成 */
  return ADS1256_WaitDRDY(1000U);
}

/**
  * @brief  轮询读取 ADC 数据 (非 RDATAC 模式)
  * @note   严格遵循数据手册 "Cycling the ADS1256 Input Multiplexer" 时序。
  */
static HAL_StatusTypeDef ADS1256_Read_ADC(uint8_t pos_ch, uint8_t neg_ch, int32_t *adc_out)
{
  uint8_t buf[3];
  int32_t adc;
  HAL_StatusTypeDef st;
  const uint32_t drdy_timeout_ms = ADS1256_DrdyTimeoutMsByDrate(g_cfg_drate);

  if (adc_out == NULL) return HAL_ERROR;

  /* 1. 切换多路复用器 */
  st = ADS1256_SetChannel(pos_ch, neg_ch);
  if (st != HAL_OK) return st;

  /* 2. 重置数字滤波器：发送 SYNC 然后发送 WAKEUP */
  st = ADS1256_SendCmd(ADS1256_CMD_SYNC);
  if (st != HAL_OK) return st;

  st = ADS1256_SendCmd(ADS1256_CMD_WAKEUP);
  if (st != HAL_OK) return st;

  /*
   * 3. 核心机制：等待滤波器稳定 (Wait for t18)
   *    发出 SYNC -> WAKEUP 后，ADS1256 的内部滤波器会重新开始采样。
   *    此时等待 DRDY 引脚产生下降沿，就正好等效于渡过了 t18 滤波器建立时间。
   *    这是获取多通道切换后“干净且完全建立”的数据的最规范做法。
   */
  st = ADS1256_WaitDRDY(drdy_timeout_ms);
  if (st != HAL_OK) return st;

  /* 4. 发送 RDATA 指令并读回 24 位数据 (整个过程 CS 保持拉低) */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  buf[0] = ADS1256_CMD_RDATA;
  st = HAL_SPI_Transmit(&hspi1, buf, 1, 100);
  if (st != HAL_OK) goto end;

  /* 发送 RDATA 命令后，需等待 t6 才能读数据 */
  ADS1256_DelayUs(ADS1256_DELAY_T6_US);

  st = HAL_SPI_Receive(&hspi1, buf, 3, 100);

end:
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
  if (st != HAL_OK) return st;

  /* 5. 数据拼接与符号扩展 (24-bit 补码) */
  adc = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | (int32_t)buf[2];
  if ((adc & 0x800000L) != 0)
  {
    adc -= 0x1000000L;
  }

  *adc_out = adc;
  return HAL_OK;
}

static HAL_StatusTypeDef ADS1256_Read_ADC_Scan8(int32_t adc_out[8], uint8_t *failed_ch)
{
  uint8_t ch;
  const uint8_t scan_mask = g_scan_mask;
  HAL_StatusTypeDef st;

  if (failed_ch != NULL)
  {
    *failed_ch = 0xFFU;
  }

  if (adc_out == NULL)
  {
    return HAL_ERROR;
  }

  if (scan_mask == 0U)
  {
    return HAL_ERROR;
  }

  for (ch = 0U; ch < 8U; ch++)
  {
    if ((scan_mask & (uint8_t)(1U << ch)) == 0U)
    {
      continue;
    }

    st = ADS1256_Read_ADC(ch, ADS1256_AINCOM, &g_scan_cache[ch]);
    if (st != HAL_OK)
    {
      if (failed_ch != NULL)
      {
        *failed_ch = ch;
      }
      return st;
    }

    g_scan_cache_valid_mask |= (uint8_t)(1U << ch);
  }

  for (ch = 0U; ch < 8U; ch++)
  {
    if ((g_scan_cache_valid_mask & (uint8_t)(1U << ch)) == 0U)
    {
      g_scan_cache[ch] = 0;
    }
    adc_out[ch] = g_scan_cache[ch];
  }

  return HAL_OK;
}

/* ================== RDATAC (连续读取模式) ================== */

static HAL_StatusTypeDef ADS1256_StartContinuousMode(void)
{
  HAL_StatusTypeDef st;
  const uint32_t drdy_timeout_ms = ADS1256_DrdyTimeoutMsByDrate(g_cfg_drate);

  /* 在进入 RDATAC 前显式退出一次，避免模式边界状态不一致。 */
  st = ADS1256_SendCmd(ADS1256_CMD_SDATAC);
  if (st != HAL_OK)
  {
    return st;
  }

  st = ADS1256_SendCmd(ADS1256_CMD_RDATAC);
  if (st != HAL_OK)
  {
    return st;
  }

  /* 等待首个 DRDY 下降沿，确保后续读到的是有效转换结果。 */
  return ADS1256_WaitDRDY(drdy_timeout_ms);
}

static HAL_StatusTypeDef ADS1256_StopContinuousMode(void)
{
  return ADS1256_SendCmd(ADS1256_CMD_SDATAC);
}

/**
  * @brief  在 RDATAC 模式下读取 ADC 数据
  * @note   该模式下不需要发送 RDATA 命令，DRDY 变低后直接给 24 个 SCLK 读取即可。
  */
static HAL_StatusTypeDef ADS1256_Read_ADC_Continuous(int32_t *adc_out)
{
  uint8_t buf[3];
  uint8_t tx_dummy[3] = {0xFFU, 0xFFU, 0xFFU};
  HAL_StatusTypeDef st;
  const uint32_t drdy_timeout_ms = ADS1256_DrdyTimeoutMsByDrate(g_cfg_drate);

  if (adc_out == NULL) return HAL_ERROR;

  /* 1. 监测 DRDY 下降沿 */
  st = ADS1256_WaitDRDY(drdy_timeout_ms);
  if (st != HAL_OK) return st;

  /* 2. DRDY 变低后，直接读出 24 位数据 */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
  st = HAL_SPI_TransmitReceive(&hspi1, tx_dummy, buf, 3, 100);
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  if (st != HAL_OK) return st;

  /* 3. 数据拼接 */
  int32_t adc = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | (int32_t)buf[2];
  if (adc & 0x800000L) adc -= 0x1000000L;

  *adc_out = adc;
  return HAL_OK;
}

static HAL_StatusTypeDef ADS1256_RecoverContinuousStream(void)
{
  HAL_StatusTypeDef st;
  const uint32_t drdy_timeout_ms = ADS1256_DrdyTimeoutMsByDrate(g_cfg_drate);

  if (g_rdatac_active != 0U)
  {
    st = ADS1256_StopContinuousMode();
    if (st != HAL_OK)
    {
      return st;
    }
    g_rdatac_active = 0U;
  }

  st = ADS1256_SetPGAValue(g_cfg_pga);
  if (st != HAL_OK)
  {
    return st;
  }

  st = ADS1256_SetDataRate(g_cfg_drate);
  if (st != HAL_OK)
  {
    return st;
  }

  st = ADS1256_SetChannel(g_cfg_pos, g_cfg_neg);
  if (st != HAL_OK)
  {
    return st;
  }

  st = ADS1256_SendCmd(ADS1256_CMD_SYNC);
  if (st != HAL_OK)
  {
    return st;
  }

  st = ADS1256_SendCmd(ADS1256_CMD_WAKEUP);
  if (st != HAL_OK)
  {
    return st;
  }

  st = ADS1256_WaitDRDY(drdy_timeout_ms);
  return st;
}

/* USB 打印重定向 */
int __io_putchar(int ch)
{
  extern USBD_HandleTypeDef hUsbDeviceFS;
  USBD_CDC_HandleTypeDef *hcdc;
  uint32_t start;
  static uint8_t tx_byte;

  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return ch;

  hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (hcdc == NULL) return ch;

  start = HAL_GetTick();
  while (hcdc->TxState != 0U)
  {
    if ((HAL_GetTick() - start) > 10U) return ch;
  }

  tx_byte = (uint8_t)ch;
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, &tx_byte, 1);
  (void)USBD_CDC_TransmitPacket(&hUsbDeviceFS);

  return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  int32_t adc_val;
  int32_t adc8[8];
  HAL_StatusTypeDef st;

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USB_DEVICE_Init();

  st = ADS1256_Init();
  if (st != HAL_OK)
  {
    printf("ADS1256 init failed, err=%d\r\n", (int)st);
  }
  else
  {
    printf("ADS1256 init success.\r\n");
    printf("READY: send START/STOP/CFG/RESET/SELFCAL commands\r\n");
    ADS1256_PrintConfig();
  }

  while (1)
  {
    ADS1256_ProcessPendingCommand();

    if (g_acq_state == ADS1256_ACQ_STOP)
    {
      HAL_Delay(5);
      continue;
    }

    if (g_sample_mode == ADS1256_SAMPLE_SCAN8)
    {
      uint8_t failed_ch = 0xFFU;

      if (g_rdatac_active != 0U)
      {
        (void)ADS1256_StopContinuousMode();
        g_rdatac_active = 0U;
      }

      g_read_mode = ADS1256_READ_RDATA;
      st = ADS1256_Read_ADC_Scan8(adc8, &failed_ch);
      if (st == HAL_OK)
      {
        printf("AD8:%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\r\n",
               (long)adc8[0],
               (long)adc8[1],
               (long)adc8[2],
               (long)adc8[3],
               (long)adc8[4],
               (long)adc8[5],
               (long)adc8[6],
               (long)adc8[7]);
      }
      else
      {
        if (st == HAL_TIMEOUT)
        {
          printf("ADC8 read timeout, CH=%u MASK=0x%02X DRATE=0x%02X wait=%lu ms\r\n",
                 (unsigned int)failed_ch,
                 (unsigned int)g_scan_mask,
                 (unsigned int)g_cfg_drate,
                 (unsigned long)ADS1256_DrdyTimeoutMsByDrate(g_cfg_drate));
        }
        else
        {
          printf("ADC8 read failed, CH=%u err=%d\r\n", (unsigned int)failed_ch, (int)st);
        }
      }

      HAL_Delay(2);
      continue;
    }

    if (g_read_mode == ADS1256_READ_RDATAC)
    {
      if (g_rdatac_active == 0U)
      {
        st = ADS1256_StartContinuousMode();
        if (st != HAL_OK)
        {
          printf("RDATAC start failed, err=%d\r\n", (int)st);
          HAL_Delay(20);
          continue;
        }
        g_rdatac_active = 1U;
      }

      st = ADS1256_Read_ADC_Continuous(&adc_val);
    }
    else
    {
      if (g_rdatac_active != 0U)
      {
        (void)ADS1256_StopContinuousMode();
        g_rdatac_active = 0U;
      }

      st = ADS1256_Read_ADC(g_cfg_pos, g_cfg_neg, &adc_val);
    }

    if (st == HAL_OK)
    {
      uint32_t raw24 = ((uint32_t)adc_val & 0x00FFFFFFUL);

      if ((g_read_mode == ADS1256_READ_RDATAC) && (raw24 == 0U))
      {
        g_zero_streak++;
      }
      else
      {
        g_zero_streak = 0U;
      }

      if (g_zero_streak >= ADS1256_ZERO_STREAK_RECOVER)
      {
        st = ADS1256_RecoverContinuousStream();
        if (st == HAL_OK)
        {
          printf("WARN RDATAC stream stuck at 0, recovered\r\n");
        }
        else
        {
          printf("WARN RDATAC recover failed, err=%d\r\n", (int)st);
        }
        g_zero_streak = 0U;
        continue;
      }

      printf("AD:%ld HEX:0x%06lX\r\n",
             (long)adc_val,
             (unsigned long)raw24);
    }
    else
    {
      printf("ADC read failed, err=%d\r\n", (int)st);
    }

    if (g_read_mode == ADS1256_READ_RDATA)
    {
      HAL_Delay(10);
    }
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;

  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  ADS1256_EnableGPIOClock(CS_GPIO_Port);
  ADS1256_EnableGPIOClock(DRDY_GPIO_Port);
  ADS1256_EnableGPIOClock(RST_GPIO_Port); /* 启用硬件复位引脚时钟 */

  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RST_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = DRDY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DRDY_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
