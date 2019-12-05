/*******************************************************************************
* File Name     : ballbot_uart.c
* Version       : 1.0
* Device(s)     : RX63N
* Tool-Chain    : Renesas RX Standard Toolchain 1.0.0
* OS            : None
* H/W Platform  : YRDKRX63N
* Description   : Library for using UART p
*******************************************************************************/
/*******************************************************************************
* History : DD.MM.YYYY     Version     Description
*         : 29.11.2019     1.00        First release
*         : 01.12.2019     1.00		   Added comments
*******************************************************************************/
/******************************************************************************
Includes   <System Includes> , "Project Includes"
******************************************************************************/
#include <stdbool.h>
#include <stdio.h>
#include <machine.h>
#include "platform.h"
#include "ballbot_uart.h"

/******************************************************************************
Private global variables
******************************************************************************/
/* Rx and Tx queue element used for managing the reception data buffer and transmission data buffer */
queue rx_queue;
queue tx_queue;

/* Rx and Tx queue pointers */
queue* tx_queue_pointer;
queue* rx_queue_pointer;


/******************************************************************************
* Function name : uart_init
* Description   : Set up all the registers for enabling UART communication
* Arguments     : none
* Return Value  : none
******************************************************************************/
void uart_init()
{
	 /* Enable the UART peripheral to operate */
		/* (the peripheral must be enabled before being used).*/
		/* The SCI8 channel is used because the TxD8 and RxD8 register are
		 * accessible by JN1's pins. (ref. Hardware Manual Chapt.22 page 704)*/
	    /* To enable  SCIc (c=0..11)  */
		/* the register MSTP(SCIc) must be activated, because*/
		/* this register is protected on writing by PRCR protection register, */
		/* before activate MSTP(SCIc) it is necessary:*/
		/* a) to set off the PRCR, */
		/* b) enable desired SCIc SCIc (c=0..11) channel (in this case SCI8, c=8)*/
		/* c) to set on the PRCR. */

		#ifdef PLATFORM_BOARD_RDKRX63N
			SYSTEM.PRCR.WORD = 0xA50B; /* Protect off */
		#endif

		MSTP(SCI8)=0; /* cancel stop state of SCI8 peripheral to enable writing to it */

		#ifdef PLATFORM_BOARD_RDKRX63N
			SYSTEM.PRCR.WORD=0xA500; /* Protect off*/

		/* Protect set on  (ref. Hardware Manual Chapt.13.1.1)*/
									   /* write A5 (in hexadecimal) to the eight higher-order */
									   /* bits and 0B (in hexadecimal) to the eight lower-order */
									   /* where B in hexadecimal is equivalent to 1011 in Binary */
									   /* therefore it sets PRC3, PRC1 and PRC0 to 1 */


		#endif
		/* clear bits TIE, RE and TEIE in SCR to 0. Set CKE to internal */
		SCI8.SCR.BYTE=0x00;

		/* Set up the UART I/O port and pins */
		//MPC.PC6PFS.BYTE=0x4A;	/*P20 is RxD8*/
		MPC.PC6PFS.BIT.ISEL=1;		//alternative code: ISEL is the interrupt
		MPC.PC6PFS.BIT.PSEL=0xA;	//corrisponde a RxD8
		MPC.PC7PFS.BYTE=0x4A;  /*P21 is TxD8*/

		PORTC.PDR.BIT.B6=1;	  /* TxD2 is output */
		PORTC.PDR.BIT.B7=0;	  /* RxD2 is input */
		PORTC.PMR.BIT.B6=1;	  /*TxD2 is pheripheral */
		PORTC.PMR.BIT.B7=1;  /* RxD8 is a pheripheral */

		/* Set data transfer format in Serial Mode Register (SMR)  -  HW 35.2.5
		 * -Asynchronous Mode
		 * -8 bits
		 * -no parity
		 * -1 stop bit
		 * PCLK clock (n=0)
		 */
		SCI8.SMR.BYTE=0x00;
		SCI8.SCMR.BIT.SMIF=0;  /* Set to 0 for serial communications interface mode */
		SCI8.BRR=480000000 / (( 64/2) * 9600)-1; /*

		/* Clear IR bits (of the interrupt) for SCI8 registers: TIE, RIE, TEIE */
		IR(SCI8,RXI8)=0;	//Clear any pending ISR
		IR(SCI8,TXI8)=0;
		IR(SCI8,TEI8)=0;

		IPR(SCI8,RXI8)=4;/* Set interrupt priority, not using the transmit end interrupt */
		IPR(SCI8,TXI8)=4;
		//IPR(SCI8, TEI2) = 4;


		IEN(SCI8,TXI8)=1;	// Disable interrupt sources (al posto della funzioni gli abilito
		IEN(SCI8,RXI8)=1;	//questi due
		IEN(SCI8,TEI8)=0;

		/* Enable RXI and TXI interrupts in SCI peripheral */
		SCI8.SCR.BIT.RIE=1;  //RXI interrupt enabled, RX buffer full
		SCI8.SCR.BIT.TIE=1;	//TXI intterupt disable, data register empty
		SCI8.SCR.BIT.TEIE=1;	//Trasmit end interrupt disable

		/* Enable Rx only */
		SCI8.SCR.BIT.RE=1;  //Serial reception enabled

		/* Initializing the Tx and Rx queues */
		rx_queue_init();
		tx_queue_init();

		IR(SCI8,TXI8)=1;	//Enable interrupt reception


/*******************************************************************************
* Function name: SCI8_TXI8_int
* Description  : Interrupt Service Routine for TXD8 DA COMPLETERE SU QUANDO SI ATTIVA
* Argument     : none
* Return value : none
*******************************************************************************/
#pragma interrupt SCI8_TXI8_int (vect = VECT_SCI8_TXI8, enable)
static void SCI8_TXI8_int()
{
	LED13=LED_ON; //a caso
	if (!queue_is_empty(tx_queue_pointer))
	{
		LED14=LED_ON;  //a caso
		SCI8.TDR=tx_queue_pointer->data[tx_queue_pointer->head];	/* putting into the data buffer
		register (TDR) of the SCI8 channel the first element of the data buffer trasmission */

		tx_queue_pointer->head++;	/* Now the head of the buffer will be the next element */

		if (tx_queue_pointer->head>=DIM_BUFFER)	/* Beucase of the circular nature of the buffer, if
		the next element is the last of the array, it will be setted to the first element of the
		array (index=0) */
			tx_queue_pointer->head=0;
	}
}

/******************************************************************************
* Function name : rx_queue_init
* Description   : Initialize the rx queue setting the pointer to the head and the tail of the
* rx data buffer
* Arguments     : none
* Return Value  : none
******************************************************************************/
void rx_queue_init()
{
	rx_queue_pointer=&rx_queue;
	rx_queue_pointer->head=0;
	rx_queue_pointer->tail=0;
	/* Empty queue conditions: head points to the same element pointed by tail */

}

/******************************************************************************
* Function name : tx_queue_init
* Description   : Initialize the tx queue setting the pointer to the head and the tail of the
* tx data buffer
* Arguments     : none
* Return Value  : none
******************************************************************************/
void tx_queue_init()
{
	tx_queue_pointer=&tx_queue;
	tx_queue_pointer->head=0;
	tx_queue_pointer->tail=0;
	/* Empty queue conditions: head points to the same element pointed by tail */

}


/******************************************************************************
* Function name : queue_is_full
* Description   : Check if a queue is full
* Arguments     : queue pointer to check
* Return Value  : int value --> 1 the queue is full, 0 the queue is not full
******************************************************************************/
int queue_is_full(queue* queue_pointer)
{
	unsigned int tail=queue_pointer->tail; /*Saving the index of the item pointed by the tail*/
	tail++;
	if (tail>=DIM_BUFFER) tail=0;
	/* Because of the incremention (tail++) the var tail contains the index where the next element
	 * will be inserted into the data buffer. If this index is equal to the index pointed by the
	 * head this means that the queue is full
	 */
	if (tail==queue_pointer->head)
		return 1;
	else
		return 0;
}


/******************************************************************************
* Function name : queue_is_empty
* Description   : Check if a queue is empty
* Arguments     : queue pointer to check
* Return Value  : int value --> 1 the queue is empty, 0 the queue is not empty
******************************************************************************/
int queue_is_empty(queue* queue_pointer)
{
	/*The queue is empty when the head and the tail points to the same item*/
	if (queue_pointer->tail == queue_pointer->head)
		return 1;
		else
		return 0;
}

