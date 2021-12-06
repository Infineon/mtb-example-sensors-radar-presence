/* ***************************************************************************
** File name: main.c
**
** Description: This is the main file for presence detection example project.
**
** ===========================================================================
** Copyright (C) 2021 Infineon Technologies AG. All rights reserved.
** ===========================================================================
**
** ===========================================================================
** Infineon Technologies AG (INFINEON) is supplying this file for use
** exclusively with Infineon's sensor products. This file can be freely
** distributed within development tools and software supporting such
** products.
**
** THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
** OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
** INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES, FOR ANY REASON
** WHATSOEVER.
** ===========================================================================
*/

/* Header file includes */
#include "cy_retarget_io.h"
#include "cybsp.h"
#include "cyhal.h"

/* Header file for RTOS abstraction */
#include "cyabs_rtos.h"

/* Header file for local tasks */
#include "radar_task.h"

/*******************************************************************************
* Macros
*******************************************************************************/

/* LED blink timer clock value in Hz  */
#define LED_BLINK_TIMER_CLOCK_HZ          (10000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD            (9999)

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);
static void isr_timer(void *callback_arg, cyhal_timer_event_t event);

/*******************************************************************************
 * Global Variables
 *******************************************************************************/
/* Timer object used for blinking the LED */
cyhal_timer_t led_blink_timer;

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 * This is the main function for example project that demonstrates presence
 * detection use case of radar. It sets up board support package, global
 * interrupts and UART. Two tasks are then created: one for presence
 * detection application, another for terminal UI to configure presence detection
 * parameters. Scheduler is then started.
 *
 * Parameters:
 *  none
 *
 * Return:
 *  int
 *
 *******************************************************************************/

int main(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts. */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port. */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("============================================================\n"
           "Connected Sensor Kit: Radar Presence Application\n"
           "============================================================\n\n");

    printf("For more PSoC 6 MCU projects, "
           "visit our code examples repositories:\n\n");

    printf("https://github.com/Infineon/\n\n"
           "Code-Examples-for-ModusToolbox-Software\n\n");

    /* Initialize the User LED */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
                             CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize timer to toggle the CYBSP_USER_LED that indicates system start up status */
    timer_init();

    /* Create task that initializes context object of RadarSensing,        */
    /* initializes radar device configuration, sets parameters for         */
    /* presence detection, registers callback to handle presence detection */
    /* events and continuously processes data acquired from radar.         */
    cy_thread_t ifxradar_task;
    result = cy_rtos_create_thread(&ifxradar_task,
                                   radar_task,
                                   RADAR_TASK_NAME,
                                   NULL,
                                   RADAR_TASK_STACK_SIZE,
                                   RADAR_TASK_PRIORITY,
                                   (cy_thread_arg_t)NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);
}

/*******************************************************************************
* Function Name: timer_init
********************************************************************************
* Summary:
* This function creates and configures a Timer object. The timer ticks
* continuously and produces a periodic interrupt on every terminal count
* event. The period is defined by the 'period' and 'compare_value' of the
* timer configuration structure 'led_blink_timer_cfg'. This application is
* designed to produce an interrupt every 1 second.
*
* Parameters:
*  none
*
*******************************************************************************/
 void timer_init(void)
 {
    cy_rslt_t result;

    const cyhal_timer_cfg_t led_blink_timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value, not used */
        .period = LED_BLINK_TIMER_PERIOD,   /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = true,              /* Run timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&led_blink_timer, NC, NULL);

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure timer period and operation mode such as count direction,
       duration */
    result = cyhal_timer_configure(&led_blink_timer, &led_blink_timer_cfg);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Set the frequency of timer's clock source */
    result = cyhal_timer_set_frequency(&led_blink_timer, LED_BLINK_TIMER_CLOCK_HZ);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Assign the ISR to execute on timer interrupt */
    cyhal_timer_register_callback(&led_blink_timer, isr_timer, NULL);

    /* Set the event on which timer interrupt occurs and enable it */
    cyhal_timer_enable_event(&led_blink_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                             CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Start the timer with the configured settings */
    result = cyhal_timer_start(&led_blink_timer);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
 }

/*******************************************************************************
* Function Name: isr_timer
********************************************************************************
* Summary:
* This is the interrupt handler function for the timer interrupt.
*
* Parameters:
*    callback_arg    Arguments passed to the interrupt callback
*    event            Timer/counter interrupt triggers
*
*******************************************************************************/
static void isr_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Invert the USER LED state */
     cyhal_gpio_toggle(CYBSP_USER_LED);
}

/* [] END OF FILE */
