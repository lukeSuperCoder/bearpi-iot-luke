/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : C3 WiFi LLM Emoji - same transport as C2 (ESP8266 TCP
  *                   server on port 8080 receiving "N\\n" with N in 1..20),
  *                   but driven by a Claude Code Stop hook that calls ZhiPu
  *                   GLM to compress each assistant reply into one emoji.
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
#include "string.h"
#include "LCD.h"
#include "emoji_data.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WIFI_SSID         "HeyBro"
#define WIFI_PASS         "123456798"
#define TCP_SERVER_PORT   8080

#define ESP_RX_BUF_SIZE   1024
#define MSG_BUF_SIZE      32

#define AT_TIMEOUT_MS     5000
#define CONNECT_TIMEOUT   20000

#define EMOJI_X           ((240 - EMOJI_WIDTH) / 2)
#define EMOJI_Y           48
#define LABEL_Y           (EMOJI_Y + EMOJI_HEIGHT + 8)
#define STATUS_Y          (LABEL_Y + 24)
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
uint8_t esp_rdata;
uint8_t esp_rx_buf[ESP_RX_BUF_SIZE];
volatile uint16_t esp_rx_len = 0;
volatile uint8_t esp_response_ready = 0;
volatile uint8_t esp_it_active = 0;

char msg_buf[MSG_BUF_SIZE];
volatile uint16_t msg_buf_len = 0;
volatile uint8_t msg_ready = 0;
volatile uint8_t client_link_id = 0;

char board_ip[20] = {0};
uint8_t wifi_connected = 0;
uint8_t server_started = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static void LPUART1_Reconfigure(uint32_t baud);
static void ESP_StartIT(void);
static HAL_StatusTypeDef ESP8266_AutoDetectBaud(void);
static void ESP8266_FlushBuf(void);
static uint8_t ESP_CheckEndMarker(void);
static HAL_StatusTypeDef ESP8266_SendCmd(const char *cmd, uint32_t timeout_ms);
static HAL_StatusTypeDef ESP8266_SendAndCheck(const char *cmd, const char *expected, uint32_t timeout_ms);
static void ESP8266_SendTCP(uint8_t link_id, const char *data, uint16_t len);
static void ParseIPD(void);
static void ExtractIP(void);

static void display_emoji(uint8_t index);
static void process_command(const char *cmd);
static void LCD_ShowStatus(const char *msg, uint16_t color);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- LPUART1 reconfigure for ESP8266 ---- */
static void LPUART1_Reconfigure(uint32_t baud)
{
    hlpuart1.Init.BaudRate = baud;
    hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits = UART_STOPBITS_1;
    hlpuart1.Init.Parity = UART_PARITY_NONE;
    hlpuart1.Init.Mode = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_DeInit(&hlpuart1);
    HAL_UART_Init(&hlpuart1);
}

static void ESP_StartIT(void)
{
    if (!esp_it_active) {
        HAL_UART_Receive_IT(&hlpuart1, &esp_rdata, 1);
        esp_it_active = 1;
    }
}

/* ---- ESP8266 driver ---- */
static void ESP8266_FlushBuf(void)
{
    esp_rx_len = 0;
    esp_response_ready = 0;
    esp_rx_buf[0] = '\0';
}

static uint8_t ESP_CheckEndMarker(void)
{
    if (esp_rx_len < 2) return 0;
    if (strstr((char *)esp_rx_buf, "\r\nOK\r\n")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nERROR")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nFAIL")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nSEND OK")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nbusy")) return 1;
    return 0;
}

static HAL_StatusTypeDef ESP8266_SendCmd(const char *cmd, uint32_t timeout_ms)
{
    HAL_UART_StateTypeDef rx_state = HAL_UART_GetState(&hlpuart1);
    if (rx_state == HAL_UART_STATE_BUSY_RX || rx_state == HAL_UART_STATE_BUSY_TX_RX) {
        HAL_UART_AbortReceive(&hlpuart1);
        esp_it_active = 0;
    }
    ESP8266_FlushBuf();
    HAL_Delay(50);
    ESP_StartIT();
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)cmd, strlen(cmd), 1000);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (esp_response_ready) {
            return HAL_OK;
        }
    }
    if (esp_rx_len > 0) return HAL_OK;
    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef ESP8266_SendAndCheck(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    HAL_StatusTypeDef ret = ESP8266_SendCmd(cmd, timeout_ms);
    if (ret != HAL_OK) return ret;
    if (strstr((char *)esp_rx_buf, expected) != NULL) return HAL_OK;
    return HAL_ERROR;
}

static void ESP8266_SendTCP(uint8_t link_id, const char *data, uint16_t len)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u,%u\r\n", link_id, len);
    ESP8266_SendCmd(cmd, AT_TIMEOUT_MS);
    HAL_Delay(100);
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)data, len, 2000);
    HAL_Delay(100);
}

static HAL_StatusTypeDef ESP8266_AutoDetectBaud(void)
{
    uint32_t bauds[] = {115200, 9600};
    for (int i = 0; i < 2; i++) {
        LPUART1_Reconfigure(bauds[i]);
        esp_it_active = 0;
        ESP8266_FlushBuf();
        ESP_StartIT();
        HAL_Delay(100);
        ESP8266_FlushBuf();
        HAL_UART_Transmit(&hlpuart1, (uint8_t *)"AT\r\n", 4, 1000);
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 2000) {
            if (esp_response_ready && strstr((char *)esp_rx_buf, "OK")) {
                printf("ESP8266 detected at %lu baud\r\n", bauds[i]);
                return HAL_OK;
            }
        }
    }
    return HAL_TIMEOUT;
}

/* ---- +IPD parser ---- */
static void ParseIPD(void)
{
    char *ipd = strstr((char *)esp_rx_buf, "+IPD,");
    if (!ipd) return;

    int link_id = 0, data_len = 0;
    char *colon = NULL;
    if (sscanf(ipd, "+IPD,%d,%d", &link_id, &data_len) == 2) {
        colon = strchr(ipd, ':');
    }
    if (!colon || data_len <= 0 || data_len > MSG_BUF_SIZE - 1) return;

    char *data_start = colon + 1;
    uint16_t available = esp_rx_len - (data_start - (char *)esp_rx_buf);
    if (available < data_len) return;

    uint16_t copy_len = (data_len > MSG_BUF_SIZE - 1) ? MSG_BUF_SIZE - 1 : data_len;
    memcpy(msg_buf, data_start, copy_len);
    msg_buf[copy_len] = '\0';
    msg_buf_len = copy_len;
    msg_ready = 1;
    client_link_id = (uint8_t)link_id;

    char *processed_end = data_start + data_len;
    uint16_t remaining = esp_rx_len - (processed_end - (char *)esp_rx_buf);
    if (remaining > 0) {
        memmove(esp_rx_buf, processed_end, remaining);
    }
    esp_rx_len = remaining;
    esp_rx_buf[esp_rx_len] = '\0';
    esp_response_ready = 0;
}

static void ExtractIP(void)
{
    char *p = strstr((char *)esp_rx_buf, "+CIFSR:STAIP,");
    if (p) {
        p += strlen("+CIFSR:STAIP,");
        if (*p == '"') p++;
        int i = 0;
        while (i < 19 && *p && *p != '"' && *p != '\r' && *p != '\n') {
            board_ip[i++] = *p++;
        }
        board_ip[i] = '\0';
    }
}

/* ---- Emoji rendering (from A14) ---- */
static void display_emoji(uint8_t index)
{
    char buf[32];

    LCD_Show_Image(EMOJI_X, EMOJI_Y, EMOJI_WIDTH, EMOJI_HEIGHT, emoji_table[index]);

    POINT_COLOR = WHITE;
    BACK_COLOR = BLACK;
    snprintf(buf, sizeof(buf), "%d/%d %s", index + 1, EMOJI_COUNT, emoji_names[index]);
    LCD_ShowString(40, LABEL_Y, 160, 16, 16, buf);
}

static void process_command(const char *cmd)
{
    int num = 0;

    if (sscanf(cmd, "%d", &num) != 1 || num < 1 || num > EMOJI_COUNT) {
        printf("ERR: Invalid '%s' (use 1-%d)\r\n", cmd, EMOJI_COUNT);
        POINT_COLOR = RED;
        BACK_COLOR = BLACK;
        LCD_ShowString(20, STATUS_Y, 200, 16, 16, "Invalid input!");
        return;
    }

    uint8_t idx = (uint8_t)(num - 1);
    display_emoji(idx);
    printf("OK: #%d %s\r\n", num, emoji_names[idx]);
}

static void LCD_ShowStatus(const char *msg, uint16_t color)
{
    LCD_Fill(0, 24, 239, 40, BLACK);
    POINT_COLOR = color;
    BACK_COLOR = BLACK;
    LCD_ShowString(5, 26, 230, 16, 16, (char *)msg);
}

static void LCD_DrawHeader(void)
{
    LCD_Fill(0, 0, 239, 20, BLUE);
    POINT_COLOR = WHITE;
    BACK_COLOR = BLUE;
    if (server_started) {
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "Srv %s:%d", board_ip, TCP_SERVER_PORT);
        LCD_ShowString(3, 2, 240, 16, 16, hdr);
    } else {
        LCD_ShowString(5, 2, 200, 16, 16, "LLM Emoji");
    }
    BACK_COLOR = BLACK;
}

/* ---- WiFi auto-connect ---- */
static uint8_t WiFi_ConnectAndStartServer(void)
{
    char cmd[128];
    HAL_StatusTypeDef ret;

    LCD_ShowStatus("ESP8266 detecting...", YELLOW);
    if (ESP8266_AutoDetectBaud() != HAL_OK) {
        LCD_ShowStatus("ESP8266 NOT found!", RED);
        printf("ESP8266 not detected!\r\n");
        return 0;
    }
    ESP8266_SendCmd("ATE0\r\n", AT_TIMEOUT_MS);

    LCD_ShowStatus("Set station mode...", CYAN);
    if (ESP8266_SendAndCheck("AT+CWMODE=1\r\n", "OK", AT_TIMEOUT_MS) != HAL_OK) {
        LCD_ShowStatus("CWMODE failed", RED);
        return 0;
    }

    LCD_ShowStatus("Connecting WiFi...", CYAN);
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
    printf("Connecting to: %s\r\n", WIFI_SSID);
    ret = ESP8266_SendCmd(cmd, CONNECT_TIMEOUT);
    if (ret != HAL_OK || !strstr((char *)esp_rx_buf, "OK")) {
        LCD_ShowStatus("WiFi connect FAIL", RED);
        printf("WiFi connect failed\r\n");
        return 0;
    }
    wifi_connected = 1;
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

    LCD_ShowStatus("Getting IP...", CYAN);
    if (ESP8266_SendCmd("AT+CIFSR\r\n", AT_TIMEOUT_MS) == HAL_OK) {
        ExtractIP();
    }
    printf("Board IP: %s\r\n", board_ip);

    LCD_ShowStatus("Enabling multi-conn...", CYAN);
    if (ESP8266_SendAndCheck("AT+CIPMUX=1\r\n", "OK", AT_TIMEOUT_MS) != HAL_OK) {
        LCD_ShowStatus("CIPMUX failed", RED);
        return 0;
    }

    LCD_ShowStatus("Starting TCP server...", CYAN);
    snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%d\r\n", TCP_SERVER_PORT);
    if (ESP8266_SendAndCheck(cmd, "OK", AT_TIMEOUT_MS) != HAL_OK) {
        LCD_ShowStatus("Server start FAIL", RED);
        return 0;
    }

    server_started = 1;

    /* Re-arm IT for continuous receive in server mode */
    HAL_UART_AbortReceive(&hlpuart1);
    esp_it_active = 0;
    ESP8266_FlushBuf();
    ESP_StartIT();
    return 1;
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
  LCD_DrawHeader();
  POINT_COLOR = WHITE;
  BACK_COLOR = BLACK;
  LCD_ShowString(30, EMOJI_Y, 180, 16, 16, "Boot...");

  printf("\r\n=== C3_wifi_llm_emoji ===\r\n");

  uint8_t server_ok = WiFi_ConnectAndStartServer();

  if (server_ok) {
      LCD_Clear(BLACK);
      LCD_DrawHeader();
      POINT_COLOR = GREEN;
      BACK_COLOR = BLACK;
      LCD_ShowString(20, EMOJI_Y, 200, 16, 16, "Waiting LLM...");
      POINT_COLOR = WHITE;
      LCD_ShowString(30, STATUS_Y, 180, 16, 16, "Hook sends 1-N");
      printf("TCP Server ready on %s:%d\r\n", board_ip, TCP_SERVER_PORT);
  } else {
      POINT_COLOR = RED;
      BACK_COLOR = BLACK;
      LCD_ShowString(20, STATUS_Y, 200, 16, 16, "Setup failed - reset");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */

    if (server_ok) {
        ParseIPD();

        if (msg_ready) {
            msg_ready = 0;
            process_command(msg_buf);
        }

        /* Flush stale data only if buffer is filling */
        if (esp_rx_len > ESP_RX_BUF_SIZE - 64) {
            ESP8266_FlushBuf();
        }
    }

    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(20);
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
  /** Initializes the CPU, AHB, and APB buses clocks
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
    if (huart->Instance == LPUART1) {
        if (esp_rx_len < ESP_RX_BUF_SIZE - 1) {
            esp_rx_buf[esp_rx_len++] = esp_rdata;
            esp_rx_buf[esp_rx_len] = '\0';
        }

        if (!server_started) {
            if (ESP_CheckEndMarker()) {
                esp_response_ready = 1;
            }
            if (esp_rx_len >= ESP_RX_BUF_SIZE - 1) {
                esp_response_ready = 1;
            }
        }

        HAL_UART_Receive_IT(&hlpuart1, &esp_rdata, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        HAL_UART_Receive_IT(&hlpuart1, &esp_rdata, 1);
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
