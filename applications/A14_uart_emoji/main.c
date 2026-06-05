/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : A14 UART Emoji - display emoji on LCD via UART + SD card
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
#define EMOJI_W       64
#define EMOJI_H       64
#define EMOJI_SIZE    (EMOJI_W * EMOJI_H * 2)
#define MAX_EMOJIS    200
#define EMOJI_X       ((240 - EMOJI_W) / 2)
#define EMOJI_Y       ((240 - EMOJI_H) / 2)
#define LABEL_Y       (EMOJI_Y + EMOJI_H + 8)
#define RX_BUF_SIZE   16
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t  rx_byte;
char     rx_line[RX_BUF_SIZE + 1];
uint8_t  rx_pos = 0;
volatile uint8_t cmd_ready = 0;

FATFS    fs;
FIL      file;
UINT     fnum;
FRESULT  f_res;

uint8_t  emoji_buf[EMOJI_SIZE];
uint16_t emoji_count = 0;
char     emoji_names[MAX_EMOJIS][24];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void load_emoji_index(void)
{
    char line_buf[28];
    UINT br;

    f_res = f_mount(&fs, (TCHAR const *)SDPath, 1);
    if (f_res != FR_OK) {
        printf("SD mount failed: %d\r\n", f_res);
        LCD_ShowString(10, 50, 220, 16, 16, "SD card mount fail!");
        return;
    }

    f_res = f_open(&file, "emoji/names.txt", FA_READ);
    if (f_res != FR_OK) {
        printf("names.txt not found: %d\r\n", f_res);
        LCD_ShowString(10, 50, 220, 16, 16, "names.txt not found!");
        return;
    }

    while (emoji_count < MAX_EMOJIS) {
        if (f_gets(line_buf, sizeof(line_buf), &file) == NULL)
            break;
        /* strip trailing \r\n */
        int len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\r' || line_buf[len - 1] == '\n'))
            line_buf[--len] = '\0';
        if (len == 0)
            continue;
        strncpy(emoji_names[emoji_count], line_buf, 23);
        emoji_names[emoji_count][23] = '\0';
        emoji_count++;
    }

    f_close(&file);
    /* keep filesystem mounted for later file reads */
}

static void display_emoji(uint16_t index)
{
    char path[24];
    char buf[32];

    snprintf(path, sizeof(path), "emoji/%02d.rgb565", index + 1);

    f_res = f_open(&file, path, FA_READ);
    if (f_res != FR_OK) {
        printf("File open fail: %s (%d)\r\n", path, f_res);
        POINT_COLOR = RED;
        BACK_COLOR = BLACK;
        LCD_ShowString(20, LABEL_Y + 24, 200, 16, 16, "File not found!");
        return;
    }

    f_res = f_read(&file, emoji_buf, EMOJI_SIZE, &fnum);
    f_close(&file);

    if (f_res != FR_OK || fnum != EMOJI_SIZE) {
        printf("File read fail: %s\r\n", path);
        POINT_COLOR = RED;
        BACK_COLOR = BLACK;
        LCD_ShowString(20, LABEL_Y + 24, 200, 16, 16, "Read error!");
        return;
    }

    LCD_Clear(BLACK);
    LCD_Show_Image(EMOJI_X, EMOJI_Y, EMOJI_W, EMOJI_H, emoji_buf);

    POINT_COLOR = WHITE;
    BACK_COLOR = BLACK;
    snprintf(buf, sizeof(buf), "%d/%d %s", index + 1, emoji_count, emoji_names[index]);
    LCD_ShowString(40, LABEL_Y, 160, 16, 16, buf);
}

static void process_command(const char *cmd)
{
    int num = 0;

    if (emoji_count == 0) {
        printf("ERR: No emojis loaded\r\n");
        return;
    }

    if (sscanf(cmd, "%d", &num) != 1 || num < 1 || num > (int)emoji_count) {
        printf("ERR: Invalid '%s' (use 1-%d)\r\n", cmd, emoji_count);
        POINT_COLOR = RED;
        BACK_COLOR = BLACK;
        LCD_ShowString(20, LABEL_Y + 24, 200, 16, 16, "Invalid input!");
        return;
    }

    uint16_t idx = (uint16_t)(num - 1);
    display_emoji(idx);
    printf("OK: #%d %s\r\n", num, emoji_names[idx]);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (rx_byte == '\r' || rx_byte == '\n') {
            if (rx_pos > 0) {
                rx_line[rx_pos] = '\0';
                cmd_ready = 1;
            }
        } else if (rx_pos < RX_BUF_SIZE) {
            rx_line[rx_pos++] = (char)rx_byte;
        }
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

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
  LCD_ShowString(40, 5, 160, 16, 16, "UART Emoji");
  LCD_ShowString(10, 24, 220, 16, 16, "Loading SD card...");

  printf("A14 UART Emoji Demo (SD Card)\r\n");

  load_emoji_index();

  if (emoji_count > 0) {
      char info[32];
      snprintf(info, sizeof(info), "SD: %d emojis ready", emoji_count);
      LCD_ShowString(20, 44, 200, 16, 16, info);
      printf("Loaded %d emojis from SD card\r\n", emoji_count);
      printf("Send emoji number 1-%d\r\n", emoji_count);
  }

  HAL_Delay(800);
  LCD_Clear(BLACK);
  LCD_ShowString(30, 100, 180, 16, 16, "Send 1-N via UART");

  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    if (cmd_ready) {
        cmd_ready = 0;
        process_command(rx_line);
        rx_pos = 0;
        rx_line[0] = '\0';
    }

    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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
  /** Initializes the CPU, AHB and APB buses clocks
  */
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
  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
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

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
