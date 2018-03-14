/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2018, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
 *  \page FreeRTOS-start FreeRTOS Start with sama5dx Microcontrollers
 *
 *  \section Purpose
 *
 *  The FreeRTOS Started example will help new users get familiar with Atmel's
 *  sama5dx microcontroller and FreeRTOS. This basic application shows the startup
 *  sequence of a chip and how to use its core peripherals and FreeRTOS basic API.
 *
 *  \section Requirements
 *
 *  This package can be used with SAMA5DX board.
 *
 *  \section Description
 *
 * The demonstration program makes one LED on the board blink at a fixed rate.
 * This rate is generated by using vTaskDelay API of FreeRTOS.
 *
 *
 *  \section Usage
 *
 *  -# Build the program and download it inside the evaluation board. Please
 *     refer to the
 *     <a href="http://www.atmel.com/dyn/resources/prod_documents/6421B.pdf">
 *     SAM-BA User Guide</a>, the
 *     <a href="http://www.atmel.com/dyn/resources/prod_documents/doc6310.pdf">
 *     GNU-Based Software Development</a>
 *     application note or to the
 *     <a href="ftp://ftp.iar.se/WWWfiles/arm/Guides/EWARM_UserGuide.ENU.pdf">
 *     IAR EWARM User Guide</a>,
 *     depending on your chosen solution.
 *  -# On the computer, open and configure a terminal application
 *     (e.g. HyperTerminal on Microsoft Windows) with these settings:
 *    - 115200 bauds
 *    - 8 bits of data
 *    - No parity
 *    - 1 stop bit
 *    - No flow control
 *  -# Start the application.
 *  -# One LED should start blinking on the board. In the terminal window, the
 *     following text should appear (values depend on the board and chip used):
 *     \code
 *      -- FreeRTOS Start Example xxx --
 *      -- SAMxxxxx-xx
 *      -- Compiled: xxx xx xxxx xx:xx:xx --
 *     \endcode
 *
 *  \section References
 *  - freertos-start/main.c
 *  - pio.h
 *  - pio_it.h
 *  - led.h
 *  - trace.h
 */

/** \file
 *
 *  This file contains all the specific code for the FreeRTOS-start example.
 *
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "board.h"
#include "chip.h"
#include "trace.h"
#include "irqflags.h"
#include "barriers.h"
#include "compiler.h"
#include "timer.h"

#include "irq/irq.h"
#include "gpio/pio.h"
#include "peripherals/pmc.h"
#include "peripherals/tcd.h"

#include "serial/console.h"
#include "led/led.h"

/* FreeRTOS head files */
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdio.h>

/* Priorities at which the tasks are created. */
#define mainLED_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )

/*----------------------------------------------------------------------------
 *        Local variables
 *----------------------------------------------------------------------------*/
volatile bool led_status[NUM_LEDS];

static const char *LedTaskName = "LedCtrl";

/*----------------------------------------------------------------------------
 *        Local functions
 *----------------------------------------------------------------------------*/

static void vLedTask(void *pvParameters);



static void vLedTask(void *pvParameters)
{
	while (1) {

		printf("LED task running\n\r");

		/* Wait for LED to be active */
		while (!led_status[0]);

		/* Toggle LED state if active */
		if (led_status[0]) {
			led_toggle(0);
			printf("0 ");
		}
		/* Simply toggle the LED every 500ms, blocking between each toggle. */
		vTaskDelay(500/portTICK_RATE_MS);

	}
}


/*----------------------------------------------------------------------------
 *        Global functions
 *----------------------------------------------------------------------------*/

/**
 *  \brief FreeRTOS-start Application entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main(void)
{
	int i = 0;

	console_example_info("FreeRTOS Start Example");

	led_status[0] = true;
	for (i = 1; i < NUM_LEDS; ++i) {
		led_status[i] = led_status[i-1];
	}

	xTaskCreate(vLedTask, LedTaskName, configMINIMAL_STACK_SIZE, NULL, 
										mainLED_TASK_PRIORITY, NULL);

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was either insufficient FreeRTOS heap memory available for the idle
	and/or timer tasks to be created, or vTaskStartScheduler() was called from
	User mode.  See the memory management section on the FreeRTOS web site for
	more details on the FreeRTOS heap http://www.freertos.org/a00111.html.  The
	mode from which main() is called is set in the C start up code and must be
	a privileged mode (not user mode). */

	while(1);

}