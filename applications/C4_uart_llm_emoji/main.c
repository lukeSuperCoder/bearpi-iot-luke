/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : C4 UART LLM Emoji - same UX as C3 (Claude Stop hook calls
  *                   GLM to compress each assistant reply into one emoji 1..20),
  *                   but the transport is USART1 over ST-Link VCP instead of
  *                   WiFi/TCP. No ESP8266 needed - just a USB cable.
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
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "LCD.h"
#include "emoji_data.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EMOJI_X       ((240 - EMOJI_WIDTH) / 2)
#define EMOJI_Y       20
#define SUMMARY_Y     110
#define SUMMARY_YMAX  234
#define SUMMARY_CHARS_PER_LINE  30
#define SUMMARY_LINE_H          16

#define RX_BUF_SIZE   96
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
uint8_t  rx_byte;
char     rx_line[RX_BUF_SIZE + 1];
uint8_t  rx_pos = 0;
volatile uint8_t cmd_ready = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static void display_emoji(uint8_t index);
static void process_command(const char *cmd);
static void LCD_ShowSummary(const char *summary, uint16_t y_start, uint16_t y_max);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void display_emoji(uint8_t index)
{
    LCD_Show_Image(EMOJI_X, EMOJI_Y, EMOJI_WIDTH, EMOJI_HEIGHT, emoji_table[index]);
}

static void LCD_ShowSummary(const char *summary, uint16_t y_start, uint16_t y_max)
{
    size_t len = strlen(summary);
    uint16_t y = y_start;

    POINT_COLOR = WHITE;
    BACK_COLOR = BLACK;

    for (size_t i = 0; i < len; i += SUMMARY_CHARS_PER_LINE) {
        if (y + SUMMARY_LINE_H > y_max) break;

        char line[SUMMARY_CHARS_PER_LINE + 1];
        size_t n = (len - i < SUMMARY_CHARS_PER_LINE) ? (len - i) : SUMMARY_CHARS_PER_LINE;
        memcpy(line, summary + i, n);
        line[n] = '\0';

        LCD_ShowString(0, y, 240, SUMMARY_LINE_H, 16, line);
        y += SUMMARY_LINE_H;
    }
}

static void process_command(const char *cmd)
{
    const char *sep = strchr(cmd, '|');
    if (!sep) {
        printf("ERR: no '|' in '%s'\r\n", cmd);
        return;
    }

    int num = atoi(cmd);
    if (num < 1 || num > EMOJI_COUNT) {
        printf("ERR: bad num '%.*s'\r\n", (int)(sep - cmd), cmd);
        return;
    }

    uint8_t idx = (uint8_t)(num - 1);
    display_emoji(idx);

    LCD_Fill(0, SUMMARY_Y, 239, SUMMARY_YMAX, BLACK);
    LCD_ShowSummary(sep + 1, SUMMARY_Y, SUMMARY_YMAX);

    printf("OK: #%d %s | %s\r\n", num, emoji_names[idx], sep + 1);
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

  LCD_Init();
  LCD_Clear(BLACK);
  display_emoji(5);

  printf("\r\n=== C4_uart_llm_emoji ===\r\n");
  printf("USART1 @115200 8N1, frame: N|summary (1-%d)\r\n", EMOJI_COUNT);

  /* Start USART1 RX interrupt (byte-by-byte) */
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_MSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 16;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
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

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
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
  /* User can add his own implementation to report the file name and the line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
