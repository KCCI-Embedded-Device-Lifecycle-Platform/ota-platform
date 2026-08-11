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
#include "eth.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "firmware_update_service.h"
#include "wifi_credentials.local.h"
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
static firmware_update_service_t firmware_service;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int esp01_at_wait(
    const char *expected,
    uint32_t timeout_ms,
    int print_response)
{
    uint8_t response[256] = {0};
    size_t received = 0;
    uint32_t started_at = HAL_GetTick();

    while (received < sizeof(response) - 1U &&
           HAL_GetTick() - started_at < timeout_ms)
    {
        if (HAL_UART_Receive(
                &huart6,
                &response[received],
                1U,
                30U) == HAL_OK)
        {
            received++;
            response[received] = '\0';

            if (strstr(
                    (char *)response,
                    expected) != NULL)
            {
                if (print_response)
                    printf("[ESP] response:\r\n%s\r\n", response);

                return 1;
            }
        }
    }

    if (print_response)
        printf("[ESP] response timeout:\r\n%s\r\n", response);

    return 0;
}

static int esp01_at_command(
    const char *command,
    const char *expected_response,
    uint32_t timeout_ms,
    int print_response)
{
    uint8_t discarded;
    size_t command_length;

    if (command == NULL ||
        expected_response == NULL)
    {
        return 0;
    }

    command_length = strlen(command);

    if (command_length == 0U ||
        command_length > UINT16_MAX)
    {
        return 0;
    }

    __HAL_UART_CLEAR_OREFLAG(&huart6);

    /* 이전 명령의 남은 CR/LF 제거 */
    while (HAL_UART_Receive(
               &huart6,
               &discarded,
               1U,
               10U) == HAL_OK)
    {
    }

    if (HAL_UART_Transmit(
            &huart6,
            (const uint8_t *)command,
            (uint16_t)command_length,
            1000U) != HAL_OK)
    {
        return 0;
    }

    /* 수신은 이 함수에서 한 번만 수행 */
    return esp01_at_wait(
        expected_response,
        timeout_ms,
        print_response);
}

static int esp01_join_wifi(void)
{
    char command[160];

    int length = snprintf(
        command,
        sizeof(command),
        "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",
        WIFI_SSID,
        WIFI_PASSWORD);

    if (length <= 0 ||
        (size_t)length >= sizeof(command))
    {
        printf("[ESP] Wi-Fi command is too long\r\n");
        return 0;
    }

    /*
     * ESP가 명령을 echo하므로 이 명령의 전체 응답을
     * USART3 로그로 출력하면 비밀번호가 노출된다.
     */
    if (!esp01_at_command(
            command,
            "WIFI GOT IP",
            20000U,
            0))
    {
        printf("[ESP] Wi-Fi connection failed\r\n");
        return 0;
    }

    printf("[ESP] Wi-Fi connected\r\n");
    return 1;
}

static void esp01_udp_smoke_test(void)
{
    static const uint8_t payload[] = "PING";

    /* STM32가 재시작해도 ESP 소켓은 남아 있을 수 있다. */
    esp01_at_command(
        "AT+CIPCLOSE\r\n",
        "OK",
        1000U,
        0);

    if (!esp01_at_command(
            "AT+CIPMUX=0\r\n",
            "OK",
            1000U,
            1))
    {
        printf("[ESP] CIPMUX failed\r\n");
        return;
    }

    if (!esp01_at_command(
            "AT+CIPSTART=\"UDP\",\"10.10.16.58\",5685,56830,0\r\n",
            "OK",
            3000U,
            1))
    {
        printf("[ESP] UDP open failed\r\n");
        return;
    }

    if (!esp01_at_command(
            "AT+CIPSEND=4\r\n",
            ">",
            1000U,
            0))
    {
        printf("[ESP] CIPSEND prompt failed\r\n");
        return;
    }

    if (HAL_UART_Transmit(
            &huart6,
            payload,
            sizeof(payload) - 1U,
            1000U) != HAL_OK)
    {
        printf("[ESP] UDP payload transmit failed\r\n");
        return;
    }

    /*
    * SEND OK와 +IPD가 연속으로 올 수 있으므로
    * 중간에 printf하거나 수신을 중단하지 않는다.
    */
    if (!esp01_at_wait(
            "+IPD,4:PONG",
            3000U,
            1))
    {
        printf("[ESP] UDP exchange failed\r\n");
        return;
    }

    printf("[ESP] UDP smoke test passed\r\n");
}

static firmware_backend_status_t backend_prepare(
    void *context,
    size_t package_size)
{
    (void)context;
    (void)package_size;
    return FIRMWARE_BACKEND_STATUS_NO_STORAGE;
}

static firmware_backend_status_t backend_write_chunk(
    void *context,
    size_t offset,
    const uint8_t *data,
    size_t length)
{
    (void)context;
    (void)offset;
    (void)data;
    (void)length;
    return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
}

static firmware_backend_status_t backend_finish_download(void *context)
{
    (void)context;
    return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
}

static firmware_backend_status_t backend_install(void *context)
{
    (void)context;
    return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;
}

static firmware_backend_status_t backend_cancel(void *context)
{
    (void)context;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t backend_recover_after_boot(
    void *context,
    firmware_backend_recovery_result_t *result)
{
    (void)context;

    if (result == NULL)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    *result = FIRMWARE_BACKEND_RECOVERY_NONE;
    return FIRMWARE_BACKEND_STATUS_OK;
}

int __io_putchar(int ch)
{
    uint8_t byte = (uint8_t)ch;

    HAL_UART_Transmit(
        &huart3,
        &byte,
        1U,
        HAL_MAX_DELAY);

    return ch;
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

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ETH_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
firmware_update_backend_t backend = {
    .context = NULL,
    .prepare = backend_prepare,
    .write_chunk = backend_write_chunk,
    .finish_download = backend_finish_download,
    .install = backend_install,
    .cancel = backend_cancel,
    .recover_after_boot = backend_recover_after_boot
};

if (!firmware_update_service_init(
        &firmware_service,
        &backend))
{
    Error_Handler();
}

firmware_update_service_recover_after_boot(
    &firmware_service);

printf(
    "[OTA KIT] state=%u, updateResult=%u\r\n",
    (unsigned)firmware_service.state,
    (unsigned)firmware_service.update_result);

if (esp01_join_wifi())
{
    esp01_udp_smoke_test();
}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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
