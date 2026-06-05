/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ESP8266 WiFi AT command test + TCP server via LPUART1
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
#include "stdio.h"
#include "string.h"
#include "LCD.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ESP_RX_BUF_SIZE   1024
#define WIFI_SSID         "HeyBro"
#define WIFI_PASS         "123456798"
#define TCP_SERVER_PORT   8080

#define AT_TIMEOUT_MS     5000
#define AP_SCAN_TIMEOUT   15000
#define CONNECT_TIMEOUT   20000

#define LCD_RESP_Y        60
#define LCD_LINE_W        28
#define LCD_LINES_MAX     10

#define KEY_DEBOUNCE_MS   200
#define STEP_DELAY_MS     500

#define MSG_BUF_SIZE      256
#define SEND_BUF_SIZE     128
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef enum {
    STEP_IDLE = 0,
    STEP_TEST_COMM,
    STEP_QUERY_FW,
    STEP_SET_MODE,
    STEP_SCAN_AP,
    STEP_CONNECT,
    STEP_GET_IP,
    STEP_TCP_SERVER,
    STEP_TOTAL
} test_step_t;

static const char *step_desc[] = {
    "Press KEY1 to start",
    "1: AT test comm",
    "2: AT+GMR firmware",
    "3: CWMODE=1 station",
    "4: CWLAP scan WiFi",
    "5: CWJAP connect AP",
    "6: CIFSR get IP",
    "7: TCP Server mode",
};

uint8_t esp_rdata;
uint8_t esp_rx_buf[ESP_RX_BUF_SIZE];
volatile uint16_t esp_rx_len = 0;
volatile uint32_t esp_rx_last_tick = 0;
volatile uint8_t esp_response_ready = 0;

uint8_t debug_rdata;
volatile uint8_t passthrough_mode = 0;
volatile uint32_t detected_baud = 0;
volatile uint8_t esp_it_active = 0;

test_step_t current_step = STEP_IDLE;
uint8_t wifi_connected = 0;
volatile uint8_t server_mode = 0;
volatile uint8_t client_connected = 0;
volatile uint8_t client_link_id = 0;

char board_ip[20] = {0};

/* Message receive buffer for +IPD parsing */
char msg_buf[MSG_BUF_SIZE];
volatile uint16_t msg_buf_len = 0;
volatile uint8_t msg_ready = 0;

/* Send buffer for typing messages via serial */
char send_buf[SEND_BUF_SIZE];
volatile uint16_t send_buf_len = 0;
volatile uint8_t send_ready = 0;

char lcd_lines[LCD_LINES_MAX][LCD_LINE_W + 1];
uint8_t lcd_line_count = 0;
uint8_t lcd_cur_pos = 0;
volatile uint8_t flag_lcd_update = 0;

volatile uint32_t last_key1_tick = 0;
volatile uint32_t last_key2_tick = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static void LPUART1_Reconfigure(uint32_t baud);
static void ESP_StartIT(void);
static HAL_StatusTypeDef ESP8266_AutoDetectBaud(void);
static void ESP8266_FlushBuf(void);
static HAL_StatusTypeDef ESP8266_SendCmd(const char *cmd, uint32_t timeout_ms);
static HAL_StatusTypeDef ESP8266_SendAndCheck(const char *cmd, const char *expected, uint32_t timeout_ms);
static void ESP8266_SendTCP(uint8_t link_id, const char *data, uint16_t len);

static void LCD_DrawHeader(void);
static void LCD_DrawHelp(void);
static void LCD_DrawStep(test_step_t step);
static void LCD_UpdateResponse(void);
static void LCD_ShowStatus(const char *msg, uint16_t color);
static void LCD_AddLine(const char *line);
static void LCD_ClearLines(void);

static uint8_t Key_Poll(uint16_t pin, GPIO_TypeDef *port, volatile uint32_t *last_tick);
static void RunTestStep(test_step_t step);
static void ServerMode_Run(void);
static void ParseIPD(void);
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
    if (strstr((char *)esp_rx_buf, "\r\nOK\n")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nERROR")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nFAIL")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nSEND OK")) return 1;
    if (strstr((char *)esp_rx_buf, "\r\nbusy")) return 1;
    if (esp_rx_len >= 4 && strncmp((char *)&esp_rx_buf[esp_rx_len - 2], "OK", 2) == 0) return 1;
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
            printf("[ESP] cmd got %d bytes\r\n", esp_rx_len);
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
    printf("[TCP] Sent %d bytes to link %u\r\n", len, link_id);
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
                detected_baud = bauds[i];
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
    /* Look for +IPD in esp_rx_buf */
    char *ipd = strstr((char *)esp_rx_buf, "+IPD,");
    if (!ipd) return;

    /* Format: +IPD,<link_id>,<len>:<data> */
    int link_id = 0, data_len = 0;
    char *colon = NULL;
    if (sscanf(ipd, "+IPD,%d,%d", &link_id, &data_len) == 2) {
        colon = strchr(ipd, ':');
    }
    if (!colon || data_len <= 0 || data_len > MSG_BUF_SIZE - 1) return;

    char *data_start = colon + 1;
    uint16_t available = esp_rx_len - (data_start - (char *)esp_rx_buf);
    if (available < data_len) return; /* not all data received yet */

    /* Copy message */
    uint16_t copy_len = (data_len > MSG_BUF_SIZE - 1) ? MSG_BUF_SIZE - 1 : data_len;
    memcpy(msg_buf, data_start, copy_len);
    msg_buf[copy_len] = '\0';
    msg_buf_len = copy_len;
    msg_ready = 1;
    client_link_id = link_id;
    client_connected = 1;

    /* Remove processed data from buffer */
    char *processed_end = data_start + data_len;
    uint16_t remaining = esp_rx_len - (processed_end - (char *)esp_rx_buf);
    if (remaining > 0) {
        memmove(esp_rx_buf, processed_end, remaining);
    }
    esp_rx_len = remaining;
    esp_rx_buf[esp_rx_len] = '\0';
    esp_response_ready = 0;
}

/* ---- LCD display helpers ---- */
static void LCD_DrawHeader(void)
{
    LCD_Fill(0, 0, 239, 18, BLUE);
    POINT_COLOR = WHITE;
    BACK_COLOR = BLUE;
    if (server_mode) {
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "Srv %s:%d", board_ip, TCP_SERVER_PORT);
        LCD_ShowString(3, 2, 240, 16, 16, hdr);
    } else {
        LCD_ShowString(5, 2, 150, 16, 16, "WiFi8266 Test");
        if (detected_baud) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%lu", detected_baud);
            POINT_COLOR = YELLOW;
            LCD_ShowString(160, 2, 80, 16, 16, buf);
        }
    }
    BACK_COLOR = BLACK;
}

static void LCD_DrawHelp(void)
{
    LCD_Fill(0, 20, 239, 38, BLACK);
    POINT_COLOR = YELLOW;
    BACK_COLOR = BLACK;
    if (server_mode) {
        LCD_ShowString(3, 22, 240, 16, 16, "K1:Exit K2:SendMsg");
    } else {
        LCD_ShowString(3, 22, 240, 16, 16, "K1:Next K2:PassThru");
    }
}

static void LCD_DrawStep(test_step_t step)
{
    LCD_Fill(0, 40, 239, 56, BLACK);
    POINT_COLOR = GREEN;
    BACK_COLOR = BLACK;
    if (step < STEP_TOTAL) {
        LCD_ShowString(5, 42, 240, 16, 16, (char *)step_desc[step]);
    }
}

static void LCD_ClearLines(void)
{
    memset(lcd_lines, 0, sizeof(lcd_lines));
    lcd_line_count = 0;
    lcd_cur_pos = 0;
    LCD_Fill(0, LCD_RESP_Y, 239, 239, BLACK);
}

static void LCD_AddLine(const char *line)
{
    if (lcd_line_count >= LCD_LINES_MAX) {
        memmove(lcd_lines[0], lcd_lines[1], (LCD_LINES_MAX - 1) * (LCD_LINE_W + 1));
        lcd_line_count = LCD_LINES_MAX - 1;
    }
    strncpy(lcd_lines[lcd_line_count], line, LCD_LINE_W);
    lcd_lines[lcd_line_count][LCD_LINE_W] = '\0';
    lcd_line_count++;
    flag_lcd_update = 1;
}

static void LCD_UpdateResponse(void)
{
    if (!flag_lcd_update) return;
    flag_lcd_update = 0;
    LCD_Fill(0, LCD_RESP_Y, 239, 239, BLACK);
    POINT_COLOR = CYAN;
    BACK_COLOR = BLACK;
    for (uint8_t i = 0; i < lcd_line_count && i < LCD_LINES_MAX; i++) {
        LCD_ShowString(2, LCD_RESP_Y + i * 16, 236, 16, 16, lcd_lines[i]);
    }
}

static void LCD_ShowStatus(const char *msg, uint16_t color)
{
    LCD_Fill(0, LCD_RESP_Y, 239, LCD_RESP_Y + 16, BLACK);
    POINT_COLOR = color;
    BACK_COLOR = BLACK;
    LCD_ShowString(5, LCD_RESP_Y, 230, 16, 16, (char *)msg);
    lcd_line_count = 0;
}

static void LCD_ShowRawResponse(void)
{
    if (esp_rx_len == 0) return;
    char *p = (char *)esp_rx_buf;
    char *line = strtok(p, "\r\n");
    while (line && lcd_line_count < LCD_LINES_MAX) {
        if (strlen(line) > 0) LCD_AddLine(line);
        line = strtok(NULL, "\r\n");
    }
}

/* ---- Key polling ---- */
static uint8_t Key_Poll(uint16_t pin, GPIO_TypeDef *port, volatile uint32_t *last_tick)
{
    if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - *last_tick) > KEY_DEBOUNCE_MS) {
            *last_tick = HAL_GetTick();
            while (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET);
            return 1;
        }
    }
    return 0;
}

/* ---- Extract IP from CIFSR response ---- */
static void ExtractIP(void)
{
    char *p = strstr((char *)esp_rx_buf, "+CIFSR:STAIP,");
    if (p) {
        p += strlen("+CIFSR:STAIP,");
        /* Skip quote if present */
        if (*p == '"') p++;
        int i = 0;
        while (i < 19 && *p && *p != '"' && *p != '\r' && *p != '\n') {
            board_ip[i++] = *p++;
        }
        board_ip[i] = '\0';
    }
}

/* ---- Test step execution ---- */
static void RunTestStep(test_step_t step)
{
    HAL_StatusTypeDef ret;
    char cmd[128];

    LCD_ClearLines();
    LCD_DrawStep(step);
    HAL_Delay(STEP_DELAY_MS);

    switch (step) {
    case STEP_TEST_COMM:
        LCD_AddLine("> Sending AT...");
        LCD_UpdateResponse();
        ret = ESP8266_SendAndCheck("AT\r\n", "OK", AT_TIMEOUT_MS);
        LCD_AddLine(ret == HAL_OK ? "OK - Comm OK!" : "FAIL - No response");
        break;

    case STEP_QUERY_FW:
        LCD_AddLine("> AT+GMR...");
        LCD_UpdateResponse();
        ret = ESP8266_SendCmd("AT+GMR\r\n", AT_TIMEOUT_MS);
        if (ret == HAL_OK) LCD_ShowRawResponse();
        else LCD_AddLine("FAIL - Timeout");
        break;

    case STEP_SET_MODE:
        LCD_AddLine("> AT+CWMODE=1...");
        LCD_UpdateResponse();
        ret = ESP8266_SendCmd("AT+CWMODE=1\r\n", AT_TIMEOUT_MS);
        if (ret == HAL_OK && strstr((char *)esp_rx_buf, "OK"))
            LCD_AddLine("OK - Station mode");
        else {
            LCD_AddLine("Response:");
            LCD_ShowRawResponse();
        }
        break;

    case STEP_SCAN_AP:
        LCD_AddLine("> Scanning WiFi...");
        LCD_UpdateResponse();
        ret = ESP8266_SendCmd("AT+CWLAP\r\n", AP_SCAN_TIMEOUT);
        if (ret == HAL_OK) {
            char *p = (char *)esp_rx_buf;
            char *line = strtok(p, "\r\n");
            while (line && lcd_line_count < LCD_LINES_MAX) {
                if (strncmp(line, "+CWLAP:", 7) == 0) LCD_AddLine(line + 7);
                line = strtok(NULL, "\r\n");
            }
            if (lcd_line_count == 0) { LCD_AddLine("No AP found"); LCD_ShowRawResponse(); }
        } else LCD_AddLine("FAIL - Timeout");
        break;

    case STEP_CONNECT:
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
        printf("Connecting to: %s\r\n", WIFI_SSID);
        LCD_AddLine("> Connecting...");
        LCD_UpdateResponse();
        ret = ESP8266_SendCmd(cmd, CONNECT_TIMEOUT);
        if (ret == HAL_OK && strstr((char *)esp_rx_buf, "OK")) {
            wifi_connected = 1;
            LCD_AddLine("OK - WiFi connected!");
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        } else {
            wifi_connected = 0;
            LCD_AddLine("FAIL - Check SSID/PASS");
            LCD_ShowRawResponse();
        }
        break;

    case STEP_GET_IP:
        LCD_AddLine("> AT+CIFSR...");
        LCD_UpdateResponse();
        ret = ESP8266_SendCmd("AT+CIFSR\r\n", AT_TIMEOUT_MS);
        if (ret == HAL_OK) {
            ExtractIP();
            LCD_ShowRawResponse();
        } else LCD_AddLine("FAIL");
        break;

    case STEP_TCP_SERVER:
        LCD_AddLine("> Starting server...");
        LCD_UpdateResponse();

        /* Enable multi-connection */
        ret = ESP8266_SendAndCheck("AT+CIPMUX=1\r\n", "OK", AT_TIMEOUT_MS);
        if (ret != HAL_OK) {
            LCD_AddLine("CIPMUX failed");
            LCD_ShowRawResponse();
            break;
        }
        LCD_AddLine("CIPMUX=1 OK");
        LCD_UpdateResponse();
        HAL_Delay(200);

        /* Get IP if not yet */
        if (board_ip[0] == '\0') {
            if (ESP8266_SendCmd("AT+CIFSR\r\n", AT_TIMEOUT_MS) == HAL_OK) {
                ExtractIP();
            }
        }

        /* Start TCP server */
        snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%d\r\n", TCP_SERVER_PORT);
        ret = ESP8266_SendAndCheck(cmd, "OK", AT_TIMEOUT_MS);
        if (ret != HAL_OK) {
            LCD_AddLine("Server start FAIL");
            LCD_ShowRawResponse();
            break;
        }

        server_mode = 1;
        LCD_ClearLines();
        LCD_DrawHeader();
        LCD_DrawHelp();
        char info[32];
        snprintf(info, sizeof(info), "Listening %s:%d", board_ip, TCP_SERVER_PORT);
        LCD_AddLine(info);
        LCD_AddLine("Waiting client...");
        LCD_UpdateResponse();
        printf("TCP Server started on %s:%d\r\n", board_ip, TCP_SERVER_PORT);

        /* Re-arm IT for continuous monitoring */
        HAL_UART_AbortReceive(&hlpuart1);
        esp_it_active = 0;
        ESP8266_FlushBuf();
        ESP_StartIT();
        break;

    default:
        break;
    }
    LCD_UpdateResponse();
}

/* ---- Server mode main loop ---- */
static void ServerMode_Run(void)
{
    /* Check for incoming TCP data */
    ParseIPD();

    if (msg_ready) {
        msg_ready = 0;

        /* Display received message on LCD */
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "> %u:", client_link_id);
        LCD_AddLine(prefix);

        /* Split long messages into multiple lines */
        char *p = msg_buf;
        while (*p && lcd_line_count < LCD_LINES_MAX) {
            char line_buf[LCD_LINE_W + 1];
            uint16_t len = strlen(p);
            if (len <= LCD_LINE_W) {
                LCD_AddLine(p);
                break;
            }
            memcpy(line_buf, p, LCD_LINE_W);
            line_buf[LCD_LINE_W] = '\0';
            LCD_AddLine(line_buf);
            p += LCD_LINE_W;
        }
        LCD_UpdateResponse();
        printf("[TCP recv] link%u: %s\r\n", client_link_id, msg_buf);

        /* Also echo back acknowledgment */
        ESP8266_SendTCP(client_link_id, "OK\r\n", 4);
    }

    /* Check for link connect/disconnect notifications */
    if (esp_rx_len > 0) {
        if (strstr((char *)esp_rx_buf, ",CONNECT")) {
            char *c = strstr((char *)esp_rx_buf, ",CONNECT");
            int id = 0;
            if (c > (char *)esp_rx_buf) id = *(c - 1) - '0';
            client_connected = 1;
            client_link_id = id;
            LCD_AddLine("Client connected!");
            LCD_UpdateResponse();
            printf("Client link %u connected\r\n", id);
            ESP8266_FlushBuf();
        }
        if (strstr((char *)esp_rx_buf, ",CLOSED")) {
            LCD_AddLine("Client disconnected");
            LCD_UpdateResponse();
            client_connected = 0;
            ESP8266_FlushBuf();
        }
        /* Flush stale data if buffer getting full */
        if (esp_rx_len > ESP_RX_BUF_SIZE - 64) {
            ESP8266_FlushBuf();
        }
    }

    /* Send message typed via serial terminal */
    if (send_ready) {
        send_ready = 0;
        ESP8266_SendTCP(client_link_id, send_buf, send_buf_len);
        LCD_AddLine("< Me:");
        char *p = send_buf;
        while (*p && lcd_line_count < LCD_LINES_MAX) {
            char line_buf[LCD_LINE_W + 1];
            uint16_t len = strlen(p);
            if (len <= LCD_LINE_W) { LCD_AddLine(p); break; }
            memcpy(line_buf, p, LCD_LINE_W);
            line_buf[LCD_LINE_W] = '\0';
            LCD_AddLine(line_buf);
            p += LCD_LINE_W;
        }
        LCD_UpdateResponse();
        send_buf_len = 0;
    }

    LCD_UpdateResponse();
}

/* USER CODE END 0 */

int main(void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    SystemClock_Config();
    PeriphCommonClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

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
    LCD_Init();
    LCD_Clear(BLACK);

    LCD_DrawHeader();
    LCD_DrawHelp();
    LCD_DrawStep(STEP_IDLE);
    LCD_ShowStatus("ESP8266 detecting...", YELLOW);
    LCD_UpdateResponse();

    printf("\r\n=== C1_wifi8266_basic ===\r\n");

    /* Auto-detect ESP8266 baud rate */
    if (ESP8266_AutoDetectBaud() == HAL_OK) {
        char msg[32];
        snprintf(msg, sizeof(msg), "ESP8266 @ %lu OK", detected_baud);
        LCD_ShowStatus(msg, GREEN);
        printf("ESP8266 detected at %lu baud\r\n", detected_baud);
        HAL_Delay(200);
        ESP8266_SendCmd("ATE0\r\n", AT_TIMEOUT_MS);
    } else {
        LCD_ShowStatus("ESP8266 NOT found!", RED);
        printf("ESP8266 not detected!\r\n");
    }
    LCD_DrawHeader();
    HAL_UART_Receive_IT(&huart1, &debug_rdata, 1);

    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        /* ---- Server mode: continuous message monitoring ---- */
        if (server_mode) {
            ServerMode_Run();

            /* KEY1: exit server mode */
            if (Key_Poll(KEY1_Pin, KEY1_GPIO_Port, &last_key1_tick)) {
                server_mode = 0;
                passthrough_mode = 0;
                ESP8266_SendCmd("AT+CIPSERVER=0\r\n", AT_TIMEOUT_MS);
                LCD_ClearLines();
                LCD_DrawHeader();
                LCD_DrawHelp();
                LCD_AddLine("Server stopped");
                LCD_UpdateResponse();
                current_step = STEP_GET_IP;
                LCD_DrawStep(current_step);
                /* Re-arm UART IT */
                HAL_UART_AbortReceive(&huart1);
                HAL_UART_Receive_IT(&huart1, &debug_rdata, 1);
                HAL_UART_AbortReceive(&hlpuart1);
                esp_it_active = 0;
                ESP_StartIT();
            }

            /* KEY2: enter send message mode via serial terminal */
            if (Key_Poll(KEY2_Pin, KEY2_GPIO_Port, &last_key2_tick)) {
                passthrough_mode = 1;
                send_buf_len = 0;
                LCD_AddLine("-- Type msg,End Enter --");
                LCD_UpdateResponse();
                printf("Send mode: type message, end with Enter\r\n");
                HAL_UART_AbortReceive(&huart1);
                HAL_UART_Receive_IT(&huart1, &debug_rdata, 1);
            }

            continue;
        }

        /* ---- Normal mode: KEY1 advance steps ---- */
        if (Key_Poll(KEY1_Pin, KEY1_GPIO_Port, &last_key1_tick)) {
            if (current_step < STEP_TOTAL - 1) {
                current_step++;
            } else {
                current_step = STEP_TEST_COMM;
            }
            printf("\r\n--- Step %d: %s ---\r\n", current_step, step_desc[current_step]);
            RunTestStep(current_step);
        }

        /* KEY2: toggle passthrough mode */
        if (Key_Poll(KEY2_Pin, KEY2_GPIO_Port, &last_key2_tick)) {
            passthrough_mode = !passthrough_mode;
            if (passthrough_mode) {
                LCD_ClearLines();
                LCD_AddLine("== Passthrough ON ==");
                LCD_AddLine("Type AT cmd via");
                LCD_AddLine("serial terminal");
                LCD_UpdateResponse();
            } else {
                LCD_ClearLines();
                LCD_AddLine("== Passthrough OFF ==");
                LCD_UpdateResponse();
            }
            HAL_UART_AbortReceive(&huart1);
            HAL_UART_Receive_IT(&huart1, &debug_rdata, 1);
            HAL_UART_AbortReceive(&hlpuart1);
            esp_it_active = 0;
            ESP_StartIT();
            LCD_DrawStep(current_step);
        }

        /* LED blink when not connected */
        if (!wifi_connected) {
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            HAL_Delay(200);
        }
    }
    /* USER CODE END 3 */
}

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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        if (esp_rx_len < ESP_RX_BUF_SIZE - 1) {
            esp_rx_buf[esp_rx_len++] = esp_rdata;
            esp_rx_buf[esp_rx_len] = '\0';
        }
        esp_rx_last_tick = HAL_GetTick();

        /* In server mode, just accumulate - ParseIPD handles it in main loop */
        if (!server_mode) {
            if (ESP_CheckEndMarker()) {
                esp_response_ready = 1;
            }
            if (esp_rx_len >= ESP_RX_BUF_SIZE - 1) {
                esp_response_ready = 1;
            }
        }

        /* In passthrough mode, echo to PC terminal */
        if (passthrough_mode) {
            HAL_UART_Transmit(&huart1, &esp_rdata, 1, 100);
        }

        HAL_UART_Receive_IT(&hlpuart1, &esp_rdata, 1);
    }

    if (huart->Instance == USART1) {
        if (server_mode && passthrough_mode) {
            /* In server send mode: accumulate typed message, send on Enter */
            if (debug_rdata == '\r' || debug_rdata == '\n') {
                if (send_buf_len > 0) {
                    send_ready = 1;
                    passthrough_mode = 0;
                }
            } else if (send_buf_len < SEND_BUF_SIZE - 1) {
                send_buf[send_buf_len++] = debug_rdata;
                send_buf[send_buf_len] = '\0';
                /* Echo character back */
                HAL_UART_Transmit(&huart1, &debug_rdata, 1, 100);
            }
        } else if (passthrough_mode) {
            /* Normal passthrough: forward to ESP8266 */
            HAL_UART_Transmit(&hlpuart1, &debug_rdata, 1, 100);
        }
        HAL_UART_Receive_IT(&huart1, &debug_rdata, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        HAL_UART_Receive_IT(&hlpuart1, &esp_rdata, 1);
    }
    if (huart->Instance == USART1) {
        HAL_UART_Receive_IT(&huart1, &debug_rdata, 1);
    }
}
/* USER CODE END 4 */

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
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
