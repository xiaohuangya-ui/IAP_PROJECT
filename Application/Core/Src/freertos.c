/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId UartRecvHandle;
osThreadId UpgradeCtrlHandle;
osThreadId BusinessHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void CubeStub_UartRecv(void const * argument);
void CubeStub_UpgradeCtrl(void const * argument);
void CubeStub_Business(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
__weak void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of UartRecv */
  osThreadDef(UartRecv, CubeStub_UartRecv, osPriorityNormal, 0, 256);
  UartRecvHandle = osThreadCreate(osThread(UartRecv), NULL);

  /* definition and creation of UpgradeCtrl */
  osThreadDef(UpgradeCtrl, CubeStub_UpgradeCtrl, osPriorityBelowNormal, 0, 192);
  UpgradeCtrlHandle = osThreadCreate(osThread(UpgradeCtrl), NULL);

  /* definition and creation of Business */
  osThreadDef(Business, CubeStub_Business, osPriorityLow, 0, 192);
  BusinessHandle = osThreadCreate(osThread(Business), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_CubeStub_UartRecv */
/**
  * @brief  Function implementing the UartRecv thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_CubeStub_UartRecv */
void CubeStub_UartRecv(void const * argument)
{
  /* USER CODE BEGIN CubeStub_UartRecv */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END CubeStub_UartRecv */
}

/* USER CODE BEGIN Header_CubeStub_UpgradeCtrl */
/**
* @brief Function implementing the UpgradeCtrl thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CubeStub_UpgradeCtrl */
void CubeStub_UpgradeCtrl(void const * argument)
{
  /* USER CODE BEGIN CubeStub_UpgradeCtrl */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END CubeStub_UpgradeCtrl */
}

/* USER CODE BEGIN Header_CubeStub_Business */
/**
* @brief Function implementing the Business thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CubeStub_Business */
void CubeStub_Business(void const * argument)
{
  /* USER CODE BEGIN CubeStub_Business */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END CubeStub_Business */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

