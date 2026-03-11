/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bmp280.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RX_BUF_SIZE 128
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
BMP280_HandleTypedef bmp280;
float temperature, pressure, humidity;
int soil_perc = 0, light_perc = 0;

// --- DMA Circular Buffer ---
#define DMA_RX_BUF_SIZE 256
uint8_t dma_rx_buf[DMA_RX_BUF_SIZE];
uint16_t dma_read_ptr = 0;

#define CMD_QUEUE_SIZE 8
char cmd_queue[CMD_QUEUE_SIZE][16];
volatile uint8_t cmd_head = 0;
volatile uint8_t cmd_tail = 0;

uint16_t rx_index = 0;
uint8_t rx_temp_buffer[RX_BUF_SIZE]; // Used for parsing characters from DMA

// Actuator states
uint8_t pump_state = 0, fan_state = 0, light_state = 0;
uint8_t auto_mode = 1;
uint8_t is_daytime = 1; // 1 = Day, 0 = Night (Sleep mode for light)

// --- ADVANCED: Calibration Values ---
int dry_val = 4095; // Default for air
int wet_val = 1500; // Default for water

// Schmitt Trigger Thresholds
int soil_low_threshold = 30;
int soil_high_threshold = 45;
float temp_high_threshold = 30.0;
float temp_low_threshold = 28.0;
int light_low_threshold = 30;
int light_high_threshold = 50;

// --- ADVANCED: Failsafe Variables ---
uint32_t pump_start_tick = 0;
uint8_t pump_failsafe = 0; // 1 = Hardware error detected
int soil_last_check = 0;

int health_score = 100;
uint32_t last_telemetry_time = 0;
char last_cmd[16] = "WAIT";

// --- Non-Blocking TX ---
char json_tx_buffer[250];
volatile uint8_t tx_complete = 1;// DMA hardware reads from here
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
uint16_t Read_ADC_Channel(uint32_t channel);
void Process_Command(char* cmd);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
const unsigned char plant_happy_32x32[] = {
        0x00, 0x1e, 0x78, 0x00, 0x00, 0x61, 0x86, 0x00, 0x01, 0x83, 0x03, 0x00, 0x00, 0xc3, 0x0c, 0x00,
        0x00, 0x67, 0x98, 0x00, 0x00, 0x3c, 0xf0, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x03, 0xff, 0xff, 0x00,
        0x07, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0xc0, 0x1f, 0x3f, 0xf3, 0xe0, 0x1e, 0x1f, 0xe1, 0xe0,
        0x3f, 0x3f, 0xf3, 0xf0, 0x3f, 0xff, 0xff, 0xf0, 0x3f, 0xff, 0xff, 0xf0, 0x3f, 0xef, 0xf7, 0xf0,
        0x3f, 0xe7, 0xe7, 0xf0, 0x1f, 0xf8, 0x1f, 0xe0, 0x1f, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xff, 0xc0,
        0x07, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00, 0x3c, 0xf0, 0x00,
        0x00, 0x78, 0x78, 0x00, 0x00, 0xf0, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char plant_sad_32x32[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00,
        0x00, 0xc0, 0xc0, 0x00, 0x01, 0x9f, 0x30, 0x00, 0x00, 0xe1, 0xc0, 0x00, 0x00, 0x3f, 0x00, 0x00,
        0x00, 0xff, 0xfc, 0x00, 0x03, 0xff, 0xff, 0x00, 0x07, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0xc0,
        0x1f, 0x3f, 0xf3, 0xe0, 0x1e, 0x1f, 0xe1, 0xe0, 0x3f, 0x3f, 0xf3, 0xf0, 0x3f, 0xff, 0xff, 0xf0,
        0x3f, 0xff, 0xff, 0xf0, 0x3f, 0xf0, 0x3f, 0xf0, 0x3f, 0xf3, 0x3f, 0xf0, 0x1f, 0xf0, 0x3f, 0xe0,
        0x1f, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x00,
        0x00, 0xff, 0xfc, 0x00, 0x00, 0x3c, 0xf0, 0x00, 0x00, 0x78, 0x78, 0x00, 0x00, 0xf0, 0x3c, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
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

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */



  ssd1306_Init();
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  bmp280_init_default_params(&bmp280.params);
  bmp280.addr = BMP280_I2C_ADDRESS_0; // Try _0 (0x76) or _1 (0x77) if it fails
  bmp280.i2c = &hi2c3;

    if (!bmp280_init(&bmp280, &bmp280.params)) {
        // Sensor not found!
    }

  // Start UART DMA circular reception
  HAL_UART_Receive_DMA(&huart1, dma_rx_buf, DMA_RX_BUF_SIZE);

  // THE ULTIMATE FIX: Disable the UART Error Interrupt (EIE).
  // This prevents the STM32 HAL from violently shutting down the DMA 
  // receiver if the water pump causes an electrical noise spike (Overrun Error).
  CLEAR_BIT(huart1.Instance->CR3, USART_CR3_EIE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
          // --- 0. PROCESS UART DMA BUFFER ---
          uint16_t dma_write_ptr = DMA_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
          while (dma_read_ptr != dma_write_ptr) {
              uint8_t c = dma_rx_buf[dma_read_ptr];
              dma_read_ptr = (dma_read_ptr + 1) % DMA_RX_BUF_SIZE;

              if (c == '\n' || c == '\r') {
                  if (rx_index > 0) {
                      rx_temp_buffer[rx_index] = '\0';
                      uint8_t next_head = (cmd_head + 1) % CMD_QUEUE_SIZE;
                      if (next_head != cmd_tail) { // if queue not full
                          strncpy(cmd_queue[cmd_head], (char*)rx_temp_buffer, 15);
                          cmd_queue[cmd_head][15] = '\0';
                          cmd_head = next_head;
                      }
                      rx_index = 0;
                  }
              } else {
                  // EMI Filter: Only accept numbers, colon, and period
                  if ((c >= '0' && c <= '9') || c == ':' || c == '.' || c == '-') {
                      if (rx_index < RX_BUF_SIZE - 1) {
                          rx_temp_buffer[rx_index++] = (uint8_t)c;
                      }
                  } else {
                      rx_index = 0; // Clear on noise
                  }
              }
          }

          while (cmd_tail != cmd_head) {
              Process_Command(cmd_queue[cmd_tail]);
              cmd_tail = (cmd_tail + 1) % CMD_QUEUE_SIZE;
              last_telemetry_time = HAL_GetTick() - 1000; // Force immediate screen update!
          }

          // --- 1. READ THE SENSORS ---
          uint16_t raw_moisture = Read_ADC_Channel(ADC_CHANNEL_5); // PA0
          uint16_t raw_light = Read_ADC_Channel(ADC_CHANNEL_6);    // PA1 (LDR)
          bmp280_read_float(&bmp280, &temperature, &pressure, &humidity);

          // --- 2. CALCULATE PERCENTAGES (Using Calibration) ---
          soil_perc = 100 - ((raw_moisture - wet_val) * 100 / (dry_val - wet_val));
          
          int bright_value = 4095;
          int dark_value = 100;
          light_perc = 100 - ((raw_light - bright_value) * 100 / (dark_value - bright_value));

          if (soil_perc > 100) { soil_perc = 100; }
          if (soil_perc < 0)   { soil_perc = 0; }
          if (light_perc > 100) { light_perc = 100; }
          if (light_perc < 0)   { light_perc = 0; }

          // --- 3. HEALTH SCORING LOGIC ---
          health_score = 100;
          if (soil_perc < soil_low_threshold) health_score -= 20; 
          if (soil_perc > soil_high_threshold) health_score -= 10; 
          if (temperature > temp_high_threshold) health_score -= 20; 
          if (temperature < temp_low_threshold) health_score -= 10;
          if (light_perc < light_low_threshold) health_score -= 10;
          if (light_perc > light_high_threshold) health_score -= 5;
          if (pump_failsafe) health_score = 0; 
          if (health_score < 0) health_score = 0;

          // --- 4. AUTOMATION & FAILSAFE LOGIC ---
          if (auto_mode && !pump_failsafe) {
              if (soil_perc < soil_low_threshold) {
                  if (pump_state == 0) {
                      pump_state = 1;
                      pump_start_tick = HAL_GetTick();
                      soil_last_check = soil_perc;
                  }
              } else if (soil_perc > soil_high_threshold) {
                  pump_state = 0;
              }

              if (pump_state == 1 && (HAL_GetTick() - pump_start_tick > 300000)) { // 5 Minute timeout for drip irrigation
                  if (soil_perc <= soil_last_check + 1) { // Only require 1% change over 5 mins
                      pump_failsafe = 1; 
                      pump_state = 0;
                  } else {
                      pump_start_tick = HAL_GetTick(); // Reset tick to check again in 5 mins
                      soil_last_check = soil_perc;
                  }
              }
              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, pump_state ? GPIO_PIN_SET : GPIO_PIN_RESET);

              if (temperature > temp_high_threshold) {
                  fan_state = 1;
              } else if (temperature < temp_low_threshold) {
                  fan_state = 0;
              }
              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, fan_state ? GPIO_PIN_SET : GPIO_PIN_RESET);

              if (is_daytime) {
                  if (light_perc < light_low_threshold && light_state == 0) {
                      light_state = 1;
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
                      HAL_Delay(100); // Short pulse to toggle
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
                  } else if (light_perc > light_high_threshold && light_state == 1) {
                      light_state = 0;
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
                      HAL_Delay(100); // Short pulse to toggle
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
                  }
              } else {
                  if (light_state == 1) {
                      light_state = 0;
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
                      HAL_Delay(100); // Short pulse to toggle
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
                  }
              }
          }

          // --- 5. TELEMETRY & DISPLAY ---
          if (HAL_GetTick() - last_telemetry_time >= 1000) {
              last_telemetry_time = HAL_GetTick();

              ssd1306_Fill(Black);
              char display_buffer[32];

              // Draw Pokemon Avatar on the left (x=0, y=16)
              if (pump_failsafe || health_score <= 70) {
                  ssd1306_DrawBitmap(0, 16, plant_sad_32x32, 32, 32, White);
              } else {
                  ssd1306_DrawBitmap(0, 16, plant_happy_32x32, 32, 32, White);
              }

              if (pump_failsafe) {
                  ssd1306_SetCursor(35, 0);
                  ssd1306_WriteString("CRITICAL", Font_7x10, White);
                  ssd1306_SetCursor(35, 15);
                  ssd1306_WriteString("CHECK PUMP", Font_7x10, White);
              } else {
                  ssd1306_SetCursor(35, 0);
                  sprintf(display_buffer, "H:%d%% %s", health_score, auto_mode ? "A" : "M");
                  ssd1306_WriteString(display_buffer, Font_7x10, White);

                  sprintf(display_buffer, "S:%d%% L:%d%%", soil_perc, light_perc);
                  ssd1306_SetCursor(35, 15);
                  ssd1306_WriteString(display_buffer, Font_7x10, White);
              }

              sprintf(display_buffer, "T:%.1fC P:%s", temperature, pump_state ? "ON" : "OFF");
              ssd1306_SetCursor(35, 30);
              ssd1306_WriteString(display_buffer, Font_7x10, White);

              sprintf(display_buffer, "F:%s L:%s", fan_state ? "ON" : "OFF", light_state ? "ON" : "OFF");
              ssd1306_SetCursor(35, 42);
              ssd1306_WriteString(display_buffer, Font_7x10, White);

              sprintf(display_buffer, "C:%s", last_cmd);
              ssd1306_SetCursor(35, 54);
              ssd1306_WriteString(display_buffer, Font_7x10, White);

              ssd1306_UpdateScreen();

              if (tx_complete) {
                  sprintf(json_tx_buffer, "{\"s\":%d,\"l\":%d,\"t\":%.1f,\"p\":%d,\"f\":%d,\"pl\":%d,\"am\":%d,\"h\":%d,\"sl\":%d,\"sh\":%d,\"th\":%.1f,\"tl\":%.1f,\"ll\":%d,\"lh\":%d,\"pf\":%d,\"dv\":%d,\"wv\":%d,\"day\":%d}\n", 
                          soil_perc, light_perc, temperature, pump_state, fan_state, light_state, auto_mode, health_score,
                          soil_low_threshold, soil_high_threshold, temp_high_threshold, temp_low_threshold, light_low_threshold, light_high_threshold, pump_failsafe, dry_val, wet_val, is_daytime);

                  tx_complete = 0; // Lock the flag

                  // Check if the DMA successfully started
                  if (HAL_UART_Transmit_DMA(&huart1, (uint8_t*)json_tx_buffer, strlen(json_tx_buffer)) != HAL_OK) {
                      // If it failed to start, unlock the flag so it can try again next loop
                      tx_complete = 1;
                  }
              }
          }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
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
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10D19CE4;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 799;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 152000;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
uint16_t Read_ADC_Channel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    uint32_t accumulator = 0;
    const int samples = 16; 
    for(int i = 0; i < samples; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK) {
            accumulator += HAL_ADC_GetValue(&hadc1);
        }
        for(volatile int j = 0; j < 1000; j++); 
    }
    return (uint16_t)(accumulator / samples);
}

void Process_Command(char* cmd) {
    int command_id = 0;
    float value_f = 0.0;
    int value_i = 0;

    strncpy(last_cmd, cmd, 15);
    last_cmd[15] = '\0';

    char *colon = strchr(cmd, ':');
    if (colon != NULL) {
        *colon = '\0';
        command_id = atoi(cmd);
        value_f = atof(colon + 1);
        value_i = atoi(colon + 1);
    } else {
        command_id = atoi(cmd);
    }

    switch (command_id) {
        case 0: // AUTO OFF
            auto_mode = 0;
            break;

        case 1: // AUTO ON
            auto_mode = 1;
            break;

        case 2: // PUMP OFF
            auto_mode = 0;  // Stop auto logic
            pump_state = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
            break;

        case 3: // PUMP ON
            auto_mode = 0;  // Stop auto logic
            pump_state = 1;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
            break;

        case 4: // FAN OFF
            auto_mode = 0;
            fan_state = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
            break;

        case 5: // FAN ON
            auto_mode = 0;
            fan_state = 1;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            break;

        case 6: // LIGHT OFF
            auto_mode = 0;
            if (light_state == 1) {
                light_state = 0;
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
                HAL_Delay(100); // Short pulse to toggle
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
            }
            break;

        case 7: // LIGHT ON
            auto_mode = 0;
            if (light_state == 0) {
                light_state = 1;
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
                HAL_Delay(100); // Short pulse to toggle
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
            }
            break;

        case 8: // CAL DRY
            dry_val = Read_ADC_Channel(ADC_CHANNEL_5);
            break;

        case 9: // CAL WET
            wet_val = Read_ADC_Channel(ADC_CHANNEL_5);
            break;

        case 10: // RESET FAILSAFE
            pump_failsafe = 0;
            pump_state = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
            break;

        case 11: soil_low_threshold = value_i; break;
        case 12: soil_high_threshold = value_i; break;
        case 13: temp_low_threshold = value_f; break;
        case 14: temp_high_threshold = value_f; break;
        case 15: light_low_threshold = value_i; break;
        case 16: light_high_threshold = value_i; break;

        case 17: is_daytime = 1; break;
        case 18: is_daytime = 0; break;

        default:
            break;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        tx_complete = 1;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Clear hardware error flags
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);

        // Abort the locked state
        HAL_UART_AbortReceive(huart);

        // Restart the DMA circular listener
        HAL_UART_Receive_DMA(&huart1, dma_rx_buf, DMA_RX_BUF_SIZE);
        
        // Reset our parser pointers so we don't read corrupt data
        dma_read_ptr = 0;
        rx_index = 0;
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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
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
