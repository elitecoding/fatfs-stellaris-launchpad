/*
 * main.c
 */
#define PART_TM4C1233H6PM
#include "driverlib/pin_map.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"

#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/ssi.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"

#include "ff.h"
#include <stdio.h>

#define RED GPIO_PIN_1
#define BLUE GPIO_PIN_2
#define GREEN GPIO_PIN_3

FATFS FatFs;		/* FatFs work area needed for each volume */
FIL Fil;			/* File object needed for each open file */

volatile int re;

void UARTPutString(char* s)
{
	while(*s)
		UARTCharPut(UART0_BASE, *s++);
}
void UARTPutNewLine()
{
	UARTCharPut(UART0_BASE,'\r');
	UARTCharPut(UART0_BASE,'\n');
}

void UARTGetString(char* buffer)
{
  uint8_t data;

  while( (data = (uint8_t)UARTCharGet(UART0_BASE)) != '\r')
  {
	  *buffer++ = data;
	  UARTCharPut(UART0_BASE, data);
  }
  UARTCharPut(UART0_BASE, '\r');
  UARTCharPut(UART0_BASE, '\n');
  //*buffer++='\r';
  //*buffer++='\n';
  *buffer='\0';

}

void dly_us (UINT n)	/* Delay n microseconds*/
{
	//3 cycles per unit
	SysCtlDelay((SysCtlClockGet()/3000000)*n);
}

volatile uint8_t second;
volatile uint8_t minute;
volatile uint8_t hour;
volatile uint32_t day;
volatile uint8_t systicks;

char UARTRecBuffer[128];
volatile uint32_t clockRate;
volatile bool displayTime = false;
char formattedTime[32];

void RTCIntHandler()
{
	systicks++;

	if(systicks == 4)
	{

		systicks = 0;

		if(second < 59)
		{
			second++;
			if(displayTime)
			{
				sprintf(formattedTime,"%.2u:%.2u:%.2u",hour,minute,second);
				UARTPutString(formattedTime);
				UARTCharPut(UART0_BASE,'\r');
			}

		}else
		{
			second = 0;
			minute++;
			if(displayTime)
			{
				sprintf(formattedTime,"%.2u:%.2u:%.2u",hour,minute,second);
				UARTPutString(formattedTime);
				UARTCharPut(UART0_BASE,'\r');
			}
		}

		if(minute == 60)
		{
			minute = 0;
			hour++;
		}

		if(hour == 24)
		{
			hour = 0;
			day++;
		}

	}
}

void initRTC()
{
	SysTickPeriodSet(10000000); //max 16mio
	SysTickIntRegister(RTCIntHandler);
	SysTickIntEnable();
	SysTickEnable();
}

int main(void) {

	SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
	clockRate = SysCtlClockGet();
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, RED|GREEN|BLUE);
	GPIOPinWrite(GPIO_PORTF_BASE, GREEN, GREEN);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

	GPIOPinConfigure(GPIO_PA0_U0RX);
	GPIOPinConfigure(GPIO_PA1_U0TX);
	GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
			(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

	UARTPutString("########## Stellaris Launchpad FatFs SD Card Test #########");
	UARTPutNewLine();

	initRTC();

	while(1)
	{
		UARTGetString(UARTRecBuffer);
		//UARTPutString(UARTRecBuffer);

		if(strcmp(UARTRecBuffer,"mountfs")==0)
		{

			if( ( re = f_mount(&FatFs, "", 1) ) ==FR_OK)	/* Give a work area to the default drive */
			{
				int i=0;
				for(i = 0;i<5;i++)
				{
					GPIOPinWrite(GPIO_PORTF_BASE, GREEN, 0);
					dly_us(90000);
					GPIOPinWrite(GPIO_PORTF_BASE, GREEN, GREEN);
					dly_us(90000);
				}

				UARTPutString("ok");
				UARTPutNewLine();
			}else
			{
				GPIOPinWrite(GPIO_PORTF_BASE, RED, RED);
				GPIOPinWrite(GPIO_PORTF_BASE, GREEN, 0);
				UARTPutString("error: could not mount file system!");
				UARTPutNewLine();
			}

		}

		if(strcmp(UARTRecBuffer,"mkfile")==0)
		{
			UARTPutString("enter file name");
			UARTPutNewLine();
			UARTGetString(UARTRecBuffer);

			UINT bw;

			if ( (re=f_open(&Fil, UARTRecBuffer, FA_WRITE | FA_CREATE_ALWAYS)) == FR_OK) {	/* Create a file */

				f_write(&Fil, "It works fast now!\r\n", sizeof("It works fast now!\r\n"), &bw);	/* Write data to the file */

				f_close(&Fil);								/* Close the file */

				UARTPutString("ok");
				UARTPutNewLine();
			}else
			{
				GPIOPinWrite(GPIO_PORTF_BASE, RED, RED);
				GPIOPinWrite(GPIO_PORTF_BASE, GREEN, 0);
				UARTPutString("error: could not create file!");
				UARTPutNewLine();
			}
		}

		if(strcmp(UARTRecBuffer,"ls")==0)
		{
			DIR d1;
			FRESULT res;
			FILINFO fno;

			f_opendir(&d1,"/");

			while(1)
			{
				res = f_readdir(&d1, &fno);
				if (res != FR_OK || fno.fname[0] == 0) break;
				UARTPutString(fno.fname);
			}
		}

		if(strcmp(UARTRecBuffer,"time")==0)
		{
			displayTime = !displayTime;
		}

	}

	//return 0;
}

/*---------------------------------------------------------*/
/* User Provided Timer Function for FatFs module           */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support a real time clock.          */

DWORD get_fattime (void)
{

    return    ((2014UL-1980) << 25)    // Year = 2007
            | (6UL << 21)            // Month = June
            | ((day+(1UL)) << 16)            // Day = 5
            | (hour << 11)            // Hour = 11
            | (minute << 5)            // Min = 38
            | (second >> 1)                // Sec = 0
            ;

}
