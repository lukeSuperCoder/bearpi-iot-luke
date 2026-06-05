/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : A15 SD Card Setup - format SD and prepare emoji directory
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "i2c.h"
#include "usart.h"
#include "quadspi.h"
#include "sdmmc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LCD.h"
#include "fatfs.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
FATFS   fs;
FIL     file;
UINT    fnum;
FRESULT f_res;

static const char *emoji_names[] = {
    "Grinning", "Tears of Joy", "Winking", "Heart Eyes", "Cool",
    "Neutral", "Unamused", "Disappointed", "Angry", "Crying",
    "Surprised", "Screaming", "Flushed", "Sleeping", "Dizzy",
    "Money Mouth", "Nerd", "Thinking", "Hugging", "Exploding Head",
};
#define NAME_COUNT (sizeof(emoji_names) / sizeof(emoji_names[0]))
BYTE format_work[8192];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LPUART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_QUADSPI_Init();
  MX_SDMMC1_SD_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */
  MX_FATFS_Init();
  LCD_Init();
  LCD_Clear(BLACK);
  POINT_COLOR = WHITE;
  BACK_COLOR = BLACK;
  LCD_ShowString(20, 5, 200, 16, 16, "SD Card Setup");
  printf("A15 SD Card Setup\r\n");

  /* Try mounting, format if needed */
  f_res = f_mount(&fs, (TCHAR const *)SDPath, 1);
  if (f_res != FR_OK) {
      printf("Mount failed (%d), formatting...\r\n", f_res);
      LCD_ShowString(10, 30, 220, 16, 16, "Formatting SD...");
      f_res = f_mkfs((TCHAR const *)SDPath, FM_FAT | FM_SFD, 0, format_work, sizeof(format_work));
      if (f_res != FR_OK) {
          printf("FAT format failed (%d), trying FAT32...\r\n", f_res);
          f_res = f_mkfs((TCHAR const *)SDPath, FM_FAT32, 0, format_work, sizeof(format_work));
      }
      if (f_res != FR_OK) {
          printf("Format failed: %d\r\n", f_res);
          LCD_ShowString(10, 50, 220, 16, 16, "Format FAILED!");
          while (1) { HAL_Delay(1000); }
      }
      f_res = f_mount(&fs, (TCHAR const *)SDPath, 1);
      if (f_res != FR_OK) {
          printf("Remount failed: %d\r\n", f_res);
          LCD_ShowString(10, 50, 220, 16, 16, "Remount FAILED!");
          while (1) { HAL_Delay(1000); }
      }
      printf("Format and mount OK\r\n");
  } else {
      printf("Mount OK\r\n");
  }

  /* Create emoji directory */
  f_res = f_mkdir("emoji");
  if (f_res == FR_OK) {
      printf("Created emoji/ directory\r\n");
  } else if (f_res == FR_EXIST) {
      printf("emoji/ already exists\r\n");
  }

  /* Write names.txt */
  f_res = f_open(&file, "emoji/names.txt", FA_CREATE_ALWAYS | FA_WRITE);
  if (f_res == FR_OK) {
      for (int i = 0; i < NAME_COUNT; i++) {
          f_puts(emoji_names[i], &file);
          f_puts("\n", &file);
      }
      f_close(&file);
      printf("Wrote emoji/names.txt (%d names)\r\n", NAME_COUNT);
  } else {
      printf("Write names.txt failed: %d\r\n", f_res);
  }

  LCD_ShowString(10, 50, 220, 16, 16, "Done! See serial");
  POINT_COLOR = 0x07E0;
  LCD_ShowString(10, 70, 220, 16, 16, "Now copy emoji");
  LCD_ShowString(10, 88, 220, 16, 16, "files from PC");

  printf("\r\nSD card ready!\r\n");
  printf("Now insert SD into PC and run:\r\n");
  printf("  python3 tools/emoji_sd_prepare.py /path/to/sd\r\n");
  printf("Then flash A14 firmware.\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(500);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE
                              |RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1|RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_MSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 16;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK|RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
