/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (ADXL345 + USART2 for ESP32)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "NanoEdgeAI.h"
#include "knowledge.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct { int16_t x, y, z; } vector3_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADXL345_ADDR              (0x53 << 1)
#define ADXL345_REG_POWER_CTL     0x2D
#define ADXL345_REG_DATA_FORMAT   0x31
#define ADXL345_REG_DATA_X0       0x32
#define ACC_BUFFER_SIZE           300
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
float acc_buffer[ACC_BUFFER_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void uart_printf(const char *fmt, ...);
HAL_StatusTypeDef Read_ADXL345(vector3_t *accel);
void fill_accelerometer_buffer(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
void uart_printf(const char *fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
    HAL_Delay(5);
}

HAL_StatusTypeDef Read_ADXL345(vector3_t *accel) {
    uint8_t buf[6];
    if (HAL_I2C_Mem_Read(&hi2c1, ADXL345_ADDR, ADXL345_REG_DATA_X0, 1, buf, 6, 100) != HAL_OK)
        return HAL_ERROR;

    accel->x = (int16_t)((buf[1] << 8) | buf[0]);
    accel->y = (int16_t)((buf[3] << 8) | buf[2]);
    accel->z = (int16_t)((buf[5] << 8) | buf[4]);

    return HAL_OK;
}

void fill_accelerometer_buffer(void) {
    vector3_t acc;
    for (int i = 0; i < ACC_BUFFER_SIZE / 3; i++) {
        while (Read_ADXL345(&acc) != HAL_OK);
        acc_buffer[i * 3 + 0] = acc.x * 0.0039f;
        acc_buffer[i * 3 + 1] = acc.y * 0.0039f;
        acc_buffer[i * 3 + 2] = acc.z * 0.0039f;
        HAL_Delay(1);  // 1kHz sampling
    }
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();

  HAL_Delay(500);
  uart_printf("STM32 UART Ready\r\n");

  /* === ADXL345 INIT === */
  uint8_t cmd;
  cmd = 0x08; // Measure mode
  HAL_I2C_Mem_Write(&hi2c1, ADXL345_ADDR, ADXL345_REG_POWER_CTL, 1, &cmd, 1, 100);
  cmd = 0x08; // ±2g full resolution
  HAL_I2C_Mem_Write(&hi2c1, ADXL345_ADDR, ADXL345_REG_DATA_FORMAT, 1, &cmd, 1, 100);
  HAL_Delay(100);

  /* === NanoEdge AI Initialization === */
  enum neai_state error_code = neai_anomalydetection_init();
  uart_printf("NanoEdgeAI init: %s\r\n", error_code == NEAI_OK ? "OK" : "ERROR");

  /* === Learning Phase === */
  for (uint16_t i = 0; i < 20; i++) {
    uart_printf("Learning iteration %d/20\r\n", i + 1);
    fill_accelerometer_buffer();
    neai_anomalydetection_learn(acc_buffer);
  }

  uart_printf("Learning finished\r\n");

  /* === Inference Phase === */
  uint8_t similarity = 0;

  while (1)
  {
    fill_accelerometer_buffer();
    neai_anomalydetection_detect(acc_buffer, &similarity);

    if (similarity >= 90) {
        uart_printf("NOMINAL,%d\r\n", similarity);
    } else {
        uart_printf("ANOMALY,%d\r\n", similarity);
    }

    HAL_Delay(500);
  }
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    Error_Handler();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
