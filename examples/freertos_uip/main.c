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
 *  \page freertos_uip FreeRTOS uIP Web Server Example
 *
 *  \section Purpose
 *
 *  This project implements a webserver example of the uIP TCP/IP stack.
 *  It enables the device to act as a web server base on FreeRTOS, displaying 
 *  network information through an HTML browser.
 *
 *  \section Requirements
 *
 *  - On-board ethernet interface.
 *  - Make sure the IP adress of the device(the board) and the computer are in 
 *    the same network.
 *
 *  \section Description
 *
 *  Please refer to the uIP documentation for more information
 *  about the TCP/IP stack, the webserver example.
 *
 * By default, the example does not use DHCP.
 * If you want to use DHCP, please:
 * - Open file uip-conf.h and don't comment the line "#define UIP_DHCP_on".
 * - Include uip/apps/dhcps to compile.
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
 *  -# Connect an Ethernet cable between the evaluation board and the network.
 *      The board may be connected directly to a computer; in this case,
 *      make sure to use a cross/twisted wired cable such as the one provided
 *      with SAMA5D2-XPLAINED / SAMA5D3-EK / SAMA5D3-XULT / SAMA5D4-EK / SAMA5D4-XULT.
 *  -# Start the application. It will display the following message on the terminal:
 *    \code
 *    -- FreeRTOS uIP Web Server Example xxx --
 *    -- xxxxxx-xx
 *    -- Compiled: xxx xx xxxx xx:xx:xx --
 *    - MAC 3a:1f:34:08:54:05
 *    - Host IP 192.168.1.3
 *    - Router IP 192.168.1.1
 *    - Net Mask 255.255.255.0
 *    \endcode
 * -# Type the IP address (Host IP in the debug log) of the %device in a web browser:
 *    \code
 *    http://192.168.1.3
 *    \endcode
 *    The page generated by uIP will appear in the browser.
 *
 *
 */

/** \file
 *
 *  This file contains all the specific code for the eth_uip_webserver example.
 *
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "board.h"
#include "board_eth.h"
#include "trace.h"
#include "timer.h"
#include "led/led.h"

#include "network/ethd.h"

#include "serial/console.h"

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "uip/uip.h"
#include "uip/uip_arp.h"
#include "uip/clock.h"
#include "eth_tapdev.h"
#include "webserver.h"

/* Demo application includes. */
#include "timers.h"

#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 *         Variables
 *---------------------------------------------------------------------------*/

/* uIP buffer : The ETH header */
#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

/* The LED used by the check task to indicate the system status. */
#define mainCHECK_LED 				( 3 )

/* The rate at which data is sent to the queue.  The 1000 value is converted
to ticks using the portTICK_PERIOD_MS constant. */
#define mainTIMER_PERIOD_MS			( 500 / portTICK_PERIOD_MS )

/* The LED toggled by the Rx task. */
#define mainTIMER_LED				( 2 )

/* A block time of zero just means "don't block". */
#define mainDONT_BLOCK				( 0 )

/* The MAC address used for demo */
static struct uip_eth_addr _mac_addr;

/* The IP address used for demo (ping ...) */
static const uint8_t _host_ip_addr[4] = {192,168,1,3};

/* Set the default router's IP address. */
static const uint8_t _route_ip_addr[4] = {192,168,1,1};

/* The _netmask address */
static const uint8_t _netmask[4] = {255, 255, 255, 0};

/* The semaphore used by the ISR to wake the uIP task. */
SemaphoreHandle_t xSemaphore = NULL;

static uint8_t eth_port = 0;

/*----------------------------------------------------------------------------
 *        Local functions
 *----------------------------------------------------------------------------*/

/**
 * Initialize demo application
 */
static void _app_init(void)
{
	printf("P: webserver application init\n\r");
	httpd_init();

#ifdef __DHCPC_H__
	printf("P: DHCPC Init\n\r");
	dhcpc_init(_mac_addr.addr, 6);
#endif
}

static void prvLEDToggleTimer( TimerHandle_t pxTimer )
{
	/* Prevent compiler warnings. */
	( void ) pxTimer;

	/* Just toggle an LED to show the application is running. */
	led_toggle( mainTIMER_LED );
}

/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/

/**
 * uip_log: Global function for uIP to use.
 * \param m Pointer to string that logged
 */
void uip_log(char *m)
{
	trace_info("-uIP log- %s\n\r", m);
}

#ifdef __DHCPC_H__
/**
 * dhcpc_configured: Global function for uIP DHCPC to use,
 * notification of DHCP configuration.
 * \param s Pointer to DHCP state instance
 */
void dhcpc_configured(const struct dhcpc_state *s)
{
	u8_t * pAddr;

	printf("\n\r");
	printf("=== DHCP Configurations ===\n\r");
	pAddr = (u8_t *)s->ipaddr;
	printf("- IP   : %d.%d.%d.%d\n\r",
			pAddr[0], pAddr[1], pAddr[2], pAddr[3]);
	pAddr = (u8_t *)s->netmask;
	printf("- Mask : %d.%d.%d.%d\n\r",
			pAddr[0], pAddr[1], pAddr[2], pAddr[3]);
	pAddr = (u8_t *)s->default_router;
	printf("- GW   : %d.%d.%d.%d\n\r",
			pAddr[0], pAddr[1], pAddr[2], pAddr[3]);
	pAddr = (u8_t *)s->dnsaddr;
	printf("- DNS  : %d.%d.%d.%d\n\r",
			pAddr[0], pAddr[1], pAddr[2], pAddr[3]);
	printf("===========================\n\r\n");
	uip_sethostaddr(s->ipaddr);
	uip_setnetmask(s->netmask);
	uip_setdraddr(s->default_router);

#ifdef __RESOLV_H__
	resolv_conf(s->dnsaddr);
#else
	printf("DNS NOT enabled in the demo\n\r");
#endif
}
#endif

void vuIP_Task( void *pvParameters )
{
	portBASE_TYPE i;
	uip_ipaddr_t ipaddr;
	struct _timeout periodic_timer, arp_timer;

	/* Create the semaphore used by the ISR to wake this task. */
	vSemaphoreCreateBinary( xSemaphore );
	timer_start_timeout(&periodic_timer, 500);
	timer_start_timeout(&arp_timer, 10000);

	/* Init uIP */
	uip_init();
	
	#ifdef __DHCPC_H__
	printf("P: DHCP Supported\n\r");
	uip_ipaddr(ipaddr, 0, 0, 0, 0);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 0, 0, 0, 0);
	uip_setdraddr(ipaddr);
	uip_ipaddr(ipaddr, 0, 0, 0, 0);
	uip_setnetmask(ipaddr);
#else
	/* Set the IP address of this host */
	uip_ipaddr(ipaddr, _host_ip_addr[0], _host_ip_addr[1], _host_ip_addr[2], _host_ip_addr[3]);
	uip_sethostaddr(ipaddr);

	uip_ipaddr(ipaddr, _route_ip_addr[0], _route_ip_addr[1], _route_ip_addr[2], _route_ip_addr[3]);
	uip_setdraddr(ipaddr);

	uip_ipaddr(ipaddr, _netmask[0], _netmask[1], _netmask[2], _netmask[3]);
	uip_setnetmask(ipaddr);
#endif

	uip_setethaddr(_mac_addr);
	
	_app_init();

	while(1) {
		uip_len = eth_tapdev_read(eth_port);
		if(uip_len > 0) {
			if(BUF->type == htons(UIP_ETHTYPE_IP)) {
				uip_arp_ipin();
				uip_input();
				/* If the above function invocation resulted in data that
				should be sent out on the network, the global variable
				uip_len is set to a value > 0. */
				if(uip_len > 0) {
					uip_arp_out();
					eth_tapdev_send(eth_port);
				}
			} else if(BUF->type == htons(UIP_ETHTYPE_ARP)) {
				uip_arp_arpin();
				/* If the above function invocation resulted in data that
				should be sent out on the network, the global variable
				uip_len is set to a value > 0. */
				if(uip_len > 0) {
					eth_tapdev_send(eth_port);
				}
			}
		} else if(timer_timeout_reached(&periodic_timer)) {
			timer_reset_timeout(&periodic_timer);
			for(i = 0; i < UIP_CONNS; i++) {
				uip_periodic(i);
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
					uip_arp_out();
					eth_tapdev_send(eth_port);
				}
			}
#if UIP_UDP
			for(i = 0; i < UIP_UDP_CONNS; i++) {
				uip_udp_periodic(i);
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
					uip_arp_out();
					eth_tapdev_send(eth_port);
				}
			}
#endif /* UIP_UDP */

			/* Call the ARP timer function every 10 seconds. */
			if(timer_timeout_reached(&arp_timer)) {
				timer_reset_timeout(&arp_timer);
				uip_arp_timer();
			}
		}
	}
}
/**
 *  \brief gmac_uip_webserver example entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main(void)
{
	TimerHandle_t xTimer = NULL;

	/* Output example information */
	console_example_info("FreeRTOS uIP Web Server Example");

	/* User select the port number for multiple eth */
	eth_port = select_eth_port();
	ethd_get_mac_addr(board_get_eth(eth_port), 0, _mac_addr.addr);

	/* Display MAC & IP settings */
	printf(" - MAC %02x:%02x:%02x:%02x:%02x:%02x\n\r",
	       _mac_addr.addr[0], _mac_addr.addr[1], _mac_addr.addr[2],
	       _mac_addr.addr[3], _mac_addr.addr[4], _mac_addr.addr[5]);
#ifndef __DHCPC_H__
	printf(" - Host IP  %d.%d.%d.%d\n\r",
	       _host_ip_addr[0], _host_ip_addr[1], _host_ip_addr[2], _host_ip_addr[3]);
	printf(" - Router IP  %d.%d.%d.%d\n\r",
	       _route_ip_addr[0], _route_ip_addr[1], _route_ip_addr[2], _route_ip_addr[3]);
	printf(" - Net Mask  %d.%d.%d.%d\n\r",
	       _netmask[0], _netmask[1], _netmask[2], _netmask[3]);
#endif


	
	xTaskCreate( vuIP_Task, "uIP", configMINIMAL_STACK_SIZE * 3, NULL, 1, NULL );

	/* A timer is used to toggle an LED just to show the application is executing. */

	xTimer = xTimerCreate( 	"LED", 					/* Text name to make debugging easier. */
							mainTIMER_PERIOD_MS, 	/* The timer's period. */
							pdTRUE,					/* This is an auto reload timer. */
							NULL,					/* ID is not used. */
							prvLEDToggleTimer );	/* The callback function. */
	
	/* Start the timer. */
	configASSERT( xTimer );
	xTimerStart( xTimer, mainDONT_BLOCK );

	/* Start the scheduler.

	NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
	The processor MUST be in supervisor mode when vTaskStartScheduler is
	called.  The demo applications included in the FreeRTOS.org download switch
	to supervisor mode prior to main being called.  If you are not using one of
	these demo application projects then ensure Supervisor mode is used here. */

	vTaskStartScheduler();

	/* We should never get here as control is now taken by the scheduler. */
	for( ;; );
}

