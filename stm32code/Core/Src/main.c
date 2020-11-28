/* USER CODE BEGIN Header */
/**
 *
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "net.h"
#include "enc28j60.h"
#include "usbd_cdc_if.h"
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
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint8_t net_buf[MAX_FRAMELEN];
uint8_t usb_buf[USB_BUFSIZE];
uint8_t packet_buf[MAX_FRAMELEN];


//current position in usb_buf
uint32_t pos_int = 0; //usb callback

//array of positions in usb_buf, when the packet is received,
//it is written into usb_buf circularly and new pointer to the next free pos to array_pos circularly
uint32_t array_pos[USB_POINTERS_ARRAY_SIZE];
uint32_t p_a = 0; //position in array of positions (updated in interrupt)
uint32_t pl_a = 0; //local position in array_pos

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t sign_buf[6];


/**
 * Copy usb buffer to packet buffer
 *
 */
void copy_buf(uint8_t *to, uint8_t *from_usb_buf, uint8_t *base_usb_buf, int16_t len){
	if(len <= 0)
		return;
	uint16_t offset = from_usb_buf - base_usb_buf;
	if(offset > USB_BUFSIZE) //should not be
		return;
	uint16_t new_pos = offset + len;
	uint8_t split = 0;
	if (new_pos > USB_BUFSIZE){
		split = 1;
	}
	if(split){
		int len1 = USB_BUFSIZE - offset;
		int len2 = len - len1;
		memcpy(to, from_usb_buf, len1);
		memcpy(to + len1, base_usb_buf, len2);
	}
	else
		memcpy(to, from_usb_buf, len);
}
/**
 * Read 32 bit data from circular usb buffer
 */
uint32_t read32(uint8_t *from_usb_buf, uint8_t *base_usb_buf){
	uint32_t value;
	copy_buf((uint8_t *)&value, from_usb_buf, base_usb_buf, 4);
	return value;
}
/**
 * Read 16 bit data from circular usb buffer
 */
uint16_t read16(uint8_t *from_usb_buf, uint8_t *base_usb_buf){
	uint16_t value;
	copy_buf((uint8_t *)&value, from_usb_buf, base_usb_buf, 2);
	return value;
}
/**
 * Calculate pointer into usb buffer len bytes from the current
 */
uint8_t *next_usb_ptr(uint8_t *from_usb_buf, uint8_t *base_usb_buf, int16_t len){
	uint8_t *limit = base_usb_buf + USB_BUFSIZE;
	uint8_t *p = from_usb_buf + len;
	if (p >= limit)
		return base_usb_buf + (p - limit);
	else
		return p;
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
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  *((uint32_t*)sign_buf) = PACKET_START_SIGN;
  net_ini();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */


  uint16_t packet_len = 0;
  uint16_t packet_size = 0;
  uint8_t *packet_next_ptr;
  uint8_t packet_start = 1;


  while (1)
  {
	  /* There are packets from usb buf to read and send to ethernet */
	  if(pl_a < p_a){
		  uint32_t prev = 0;
		  if(pl_a > 0)
			  prev = array_pos[(pl_a - 1) % USB_POINTERS_ARRAY_SIZE];
		  int32_t n = array_pos[pl_a % USB_POINTERS_ARRAY_SIZE] - prev;//usb frame size
		  uint8_t *from = usb_buf + prev % USB_BUFSIZE;
		  uint8_t right_n = 1;
		  if (n < 0 || n > MAX_FRAMELEN){
			  right_n = 0;
		  }

		  // new ethernet packet
		  if((packet_len == 0) && packet_start && (n > 5) && right_n){
			  uint32_t sign = read32(from,usb_buf); //4 bytes
			  uint8_t *next = next_usb_ptr(from,usb_buf,4);
			  packet_size = read16(next,usb_buf);// 2 bytes after sign is packet length

			  if (packet_size > MAX_FRAMELEN || sign != PACKET_START_SIGN){
				  packet_size = 0;
			  }
			  else{
				  next = next_usb_ptr(from,usb_buf,6);
				  copy_buf(packet_buf, next, usb_buf, n - 6);
				  packet_len = n - 6;
				  packet_next_ptr = packet_buf + packet_len;
				  packet_start = 0;
			  }

		  }
		  else if(packet_len < packet_size && right_n){
			  //make sure packet_len + n < packet_buf size!!!
			  copy_buf(packet_next_ptr, from, usb_buf, n);
			  packet_len += n;
			  packet_next_ptr = packet_buf + packet_len;
		  }
		  else if (packet_len > packet_size){
			  packet_len = 0;
			  packet_start = 1;
		  }

		  /* Send ready packet to ethernet */
		  if(packet_len == packet_size && packet_size > 0){
			  enc28j60_packetSend(packet_buf, packet_size);
			  packet_len = 0;
			  packet_start = 1;
		  }

		  pl_a++;

	  }

	  uint16_t len=enc28j60_packetReceive(net_buf,sizeof(net_buf));

	  /* There are data from ethernet, send it to USB */
	  if (len>0)
	  {
		  *((uint16_t*)(sign_buf + 4)) = len;
		  while(CDC_Transmit_FS(sign_buf, sizeof(sign_buf)) == USBD_BUSY_CDC_TRANSMIT);
		  while(CDC_Transmit_FS(net_buf, len) == USBD_BUSY_CDC_TRANSMIT);
		  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
