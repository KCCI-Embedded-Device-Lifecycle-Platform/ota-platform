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
#include "rng.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "firmware_update_service.h"
#include "wifi_credentials.local.h"
#include "esp01_uart.h"
#include "esp01_modem.h"
#include "esp01_coap_download_transport.h"
#include "f429zi_firmware_backend.h"
#include "f429zi_lwm2m_client.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LWM2M_ENDPOINT_NAME "stm32-f429zi-01"
#define LWM2M_SERVER_HOST   "10.10.16.58"
#define LWM2M_SERVER_PORT   5683U
#define LWM2M_LOCAL_PORT    56830U
#define ARTIFACT_LOCAL_PORT 56831U
#define ARTIFACT_LINK_ID    1U

#ifndef F429ZI_APP_VERSION
#define F429ZI_APP_VERSION "unknown"
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static firmware_update_service_t firmware_service;
static firmware_update_backend_t firmware_backend;
static f429zi_firmware_backend_t firmware_backend_context;
static firmware_download_transport_t firmware_download_transport;
static esp01_coap_download_transport_t firmware_download_context;
static f429zi_lwm2m_client_t lwm2m_client;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  bool wifi_connected = false;
  int previous_lwm2m_error = COAP_NO_ERROR;

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
  MX_RNG_Init();
  /* USER CODE BEGIN 2 */

  printf("[OTA APP] version=%s\r\n", F429ZI_APP_VERSION);

  if (!esp01_uart_start(&huart6))
  {
      printf("[ESP] UART RX interrupt start failed\r\n");
      Error_Handler();
  }

  esp01_modem_status_t modem_status;

  esp01_modem_init();

  modem_status = esp01_modem_reset();

  if (modem_status != ESP01_MODEM_STATUS_OK)
  {
      printf(
          "[ESP] Modem reset failed: %u\r\n",
          (unsigned)modem_status);
  }
  else
  {
      modem_status = esp01_modem_join_wifi(
          WIFI_SSID,
          WIFI_PASSWORD);

      if (modem_status != ESP01_MODEM_STATUS_OK)
      {
          printf(
              "[ESP] Wi-Fi connection failed: %u\r\n",
              (unsigned)modem_status);
      }
      else
      {
          printf("[ESP] Wi-Fi connected\r\n");
          wifi_connected = true;
      }
  }

  printf(
      "[ESP] UART overflows=%lu, packet drops=%lu\r\n",
      (unsigned long)esp01_uart_overflow_count(),
      (unsigned long)esp01_modem_dropped_packet_count());

  if (!f429zi_firmware_backend_init(
          &firmware_backend_context,
          &firmware_backend))
  {
      Error_Handler();
  }

  if (!firmware_update_service_init(
          &firmware_service,
          &firmware_backend))
  {
      Error_Handler();
  }

  firmware_update_service_recover_after_boot(
      &firmware_service);

  printf(
      "[OTA KIT] state=%u, updateResult=%u\r\n",
      (unsigned)firmware_service.state,
      (unsigned)firmware_service.update_result);

  if (wifi_connected)
  {
      if (!esp01_coap_download_transport_init(
              &firmware_download_context,
              ARTIFACT_LINK_ID,
              ARTIFACT_LOCAL_PORT,
              &firmware_download_transport))
      {
          printf("[OTA] download transport initialization failed\r\n");
          Error_Handler();
      }

      if (!f429zi_lwm2m_client_init(
              &lwm2m_client,
              LWM2M_ENDPOINT_NAME,
              LWM2M_SERVER_HOST,
              LWM2M_SERVER_PORT,
              LWM2M_LOCAL_PORT,
              &firmware_service,
              &firmware_download_transport))
      {
          printf("[LwM2M] initialization failed\r\n");
          Error_Handler();
      }
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (wifi_connected)
    {
        int lwm2m_error =
            f429zi_lwm2m_client_step(&lwm2m_client);

        if (lwm2m_error != COAP_NO_ERROR &&
            lwm2m_error != previous_lwm2m_error)
        {
            printf(
                "[LwM2M] step failed: %d\r\n",
                lwm2m_error);
        }

        previous_lwm2m_error = lwm2m_error;

        firmware_download_transport_status_t download_status =
            esp01_coap_download_transport_process(
                &firmware_download_context);

        if (download_status !=
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK)
        {
            printf(
                "[OTA] download transport failed: %u\r\n",
                (unsigned)download_status);
        }

        f429zi_firmware_backend_process(
            &firmware_backend_context);
    }

    HAL_Delay(1U);
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
