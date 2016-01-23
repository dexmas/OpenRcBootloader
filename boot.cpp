/****************************************************************************
*  Copyright (c) 2014 by Michael Blandford. All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*  1. Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*  2. Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*  3. Neither the name of the author nor the names of its contributors may
*     be used to endorse or promote products derived from this software
*     without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
*  THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
*  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
*  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
*  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
*  SUCH DAMAGE.
*
****************************************************************************
* Other Authors:
 * - Andre Bernet
 * - Bertrand Songis
 * - Bryan J. Rentoul (Gruvin)
 * - Cameron Weeks
 * - Erez Raviv
 * - Jean-Pierre Parisy
 * - Karl Szmutny
 * - Michal Hlavinka
 * - Pat Mackenzie
 * - Philip Moss
 * - Rob Thomson
 * - Romolo Manfredini
 * - Thomas Husterer
*
****************************************************************************/


/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/
 
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef PCBSKY
#include "AT91SAM3S4.h"
#include "core_cm3.h"
#endif

#ifdef PCBX9D
#include "stm32f2xx.h"
#include "stm32f2xx_flash.h"
#include "i2c_ee.h"
#include "hal.h"
#include "timers.h"

extern "C" {
#include "usb_dcd_int.h"
#include "usb_bsp.h"
#include "usbd_desc.h"
#include "usbd_msc_core.h"
#include "usbd_usr.h"
}

#endif

#ifdef PCB9XT
#include "stm32f2xx.h"
#include "stm32f2xx_flash.h"
#include "hal.h"
#include "timers.h"
#include "mega64.h"

extern "C" {
#include "usb_dcd_int.h"
#include "usb_bsp.h"
#include "usbd_desc.h"
#include "usbd_msc_core.h"
#include "usbd_usr.h"
}

#endif



#include "radio.h"
#include "lcd.h"
#include "ff.h"
#include "diskio.h"
#include "drivers.h"
#include "logicio.h"

__attribute__ ((section(".version"), used))
const uint8_t Version[] =
{
	'B', 'O', 'O', 'T', '1', '9'
} ;

__attribute__ ((section(".text"), used))

extern void usbPluggedIn( uint16_t allowSD ) ;
#ifdef PCBSKY
extern uint16_t usbLunStat() ;
#endif

#ifdef PCBSKY
extern void usbMassStorage( void ) ;
#endif

void createFat( uint32_t flashSize ) ;

#if ( defined(PCBSKY) || defined(PCB9XT) )
#define BOOT_KEY_UP			KEY_UP
#define BOOT_KEY_DOWN		KEY_DOWN
#define BOOT_KEY_LEFT		KEY_LEFT
#define BOOT_KEY_RIGHT	KEY_RIGHT
#define BOOT_KEY_MENU		KEY_MENU
#define BOOT_KEY_EXIT		KEY_EXIT
#define DISPLAY_CHAR_WIDTH	21
#endif

#ifdef PCBX9D
#define BOOT_KEY_UP			KEY_PLUS
#define BOOT_KEY_DOWN		KEY_MINUS
#define BOOT_KEY_LEFT		KEY_MENU
#define BOOT_KEY_RIGHT	KEY_PAGE
#define BOOT_KEY_MENU		KEY_ENTER
#define BOOT_KEY_EXIT		KEY_EXIT
#define DISPLAY_CHAR_WIDTH	35
#endif

// states
#define ST_START					0
#define ST_DIR_CHECK			1
#define ST_OPEN_DIR				2
#define ST_FILE_LIST			3
#define ST_FLASH_CHECK		4
#define ST_FLASHING				5
#define ST_FLASH_DONE			6
#define ST_USB						10
#define ST_REBOOT					11






/*----------------------------------------------------------------------------
 *        Local variables
 *----------------------------------------------------------------------------*/

uint32_t FirmwareSize ;

uint32_t Master_frequency ;
volatile uint8_t  Tenms ;
uint8_t EE_timer ;
uint8_t USBcounter ;
uint8_t SDcardDisabled ;
volatile uint16_t BlinkCounter ;
extern uint32_t sd_card_ready( void ) ;

TCHAR FlashFilename[60] ;
FATFS g_FATFS ;
FIL FlashFile ;
DIR Dj ;
FILINFO Finfo ;

TCHAR Filenames[8][50] ;
uint32_t FileSize[20] ;
uint32_t FnStartIndex ;
uint32_t Valid ;

uint32_t FlashSize ;


uint32_t FlashBlocked = 1 ;
uint32_t LockBits ;

uint32_t Block_buffer[1024] ;
UINT BlockCount ;

#if ( defined(PCBSKY) || defined(PCB9XT) )
extern int32_t EblockAddress ;
#endif
extern uint32_t EepromBlocked ;

extern void init_spi( void ) ;
extern void writeBlock( void ) ;


/*----------------------------------------------------------------------------
 *         Global functions
 *----------------------------------------------------------------------------*/

#ifdef PCBSKY
// Starts TIMER0 at full speed (MCK/2) for delay timing
// @ 36MHz this is 18MHz
// This was 6 MHz, we may need to slow it to TIMER_CLOCK2 (MCK/8=4.5 MHz)
void start_timer0()
{
  register Tc *ptc ;

  PMC->PMC_PCER0 |= 0x00800000L ;		// Enable peripheral clock to TC0

  ptc = TC0 ;		// Tc block 0 (TC0-2)
	ptc->TC_BCR = 0 ;			// No sync
	ptc->TC_BMR = 2 ;
	ptc->TC_CHANNEL[0].TC_CMR = 0x00008001 ;	// Waveform mode MCK/8 for 36MHz osc.(Upset be write below)
	ptc->TC_CHANNEL[0].TC_RC = 0xFFF0 ;
	ptc->TC_CHANNEL[0].TC_RA = 0 ;
	ptc->TC_CHANNEL[0].TC_CMR = 0x00008040 ;	// 0000 0000 0000 0000 1000 0000 0100 0000, stop at regC, 18MHz
	ptc->TC_CHANNEL[0].TC_CCR = 5 ;		// Enable clock and trigger it (may only need trigger)
}

void stop_timer0( void )
{
	TC0->TC_CHANNEL[0].TC_CCR = TC_CCR0_CLKDIS ;		// Disable clock
}

void delay2ms()
{
	TC0->TC_CHANNEL[0].TC_CCR = 5 ;	// Enable clock and trigger it (may only need trigger)
	while ( TC0->TC_CHANNEL[0].TC_CV < 36000 )		// 2mS, Value depends on MCK/2 (used 18MHz)
	{
		// Wait
	}
}

void delayNus( uint16_t time )
{
	time *= 18 ;				// Value depends on MCK/2 (used 18MHz)
	TC0->TC_CHANNEL[0].TC_CCR = 5 ;	// Enable clock and trigger it (may only need trigger)
	while ( TC0->TC_CHANNEL[0].TC_CV < time )		// "time" uS, Value depends on MCK/2 (used 18MHz)
	{
		// Wait
	}
}
#endif

static uint32_t PowerUpDelay ;

static bool usbPlugged(void)
{
	if ( PowerUpDelay < 100 )	// 1000 mS
	{
		return 0 ;
	}
#ifdef PCBSKY
	return PIOC->PIO_PDSR & 0x02000000 ;
#endif
	
#if ( defined(PCBX9D) || defined(PCB9XT) )
	return GPIOA->IDR & 0x0200 ;
#endif
}

#if ( defined(PCBX9D) || defined(PCB9XT) )

extern "C" {
USB_OTG_CORE_HANDLE USB_OTG_dev;

void OTG_FS_IRQHandler(void)
{
  USBD_OTG_ISR_Handler (&USB_OTG_dev);
}
}

static void usbInit()
{
  USB_OTG_BSP_Init(&USB_OTG_dev);
}

static void usbStart()
{
  USBD_Init(&USB_OTG_dev, USB_OTG_FS_CORE_ID, &USR_desc, &USBD_MSC_cb, &USR_cb);
}

uint32_t isFirmwareStart( uint32_t *block )
{
	if ( ( block[0] & 0xFFFC0000 ) != 0x20000000 )
	{
		return 0 ;
	}
	if ( ( block[1] & 0xFFF00000 ) != 0x08000000 )
	{
		return 0 ;
	}
	if ( ( block[2] & 0xFFF00000 ) != 0x08000000 )
	{
		return 0 ;
	}
	return 1 ;	
}
#endif

#ifdef PCBSKY
uint32_t isFirmwareStart( uint32_t *block )
{
	if ( ( block[0] & 0xFFFE3000 ) != 0x20000000 )
	{
		return 0 ;
	}
	if ( ( block[1] & 0xFFF80000 ) != 0x00400000 )
	{
		return 0 ;
	}
	if ( ( block[2] & 0xFFF80000 ) != 0x00400000 )
	{
		return 0 ;
	}
	return 1 ;	
}
#endif


#ifdef PCBSKY

uint32_t (*IAP_Function)(uint32_t, uint32_t) ;


uint32_t program( uint32_t *address, uint32_t *buffer )	// size is 256 bytes
{
	uint32_t FlashSectorNum ;
	uint32_t flash_cmd = 0 ;
	uint32_t i ;
//	uint32_t flash_status = 0;
	//	uint32_t EFCIndex = 0; // 0:EEFC0, 1: EEFC1
	/* Initialize the function pointer (retrieve function address from NMI vector) */

	if ( (uint32_t) address == 0x00408000 )
	{
		if ( isFirmwareStart( buffer) )
		{
			FlashBlocked = 0 ;
		}
		else
		{
			FlashBlocked = 1 ;
		}
	}

	if ( FlashBlocked )
	{
		return 1 ;
	}
	// Always initialise this here, setting a default doesn't seem to work
	IAP_Function = (uint32_t (*)(uint32_t, uint32_t))  *(( uint32_t *)0x00800008) ;
	FlashSectorNum = (uint32_t) address ;
	FlashSectorNum >>= 8 ;		// page size is 256 bytes
	FlashSectorNum &= 2047 ;	// max page number
	
	/* Send data to the sector here */
	for ( i = 0 ; i < 64 ; i += 1 )
	{
		*address++ = *buffer++ ;		
	}

	/* build the command to send to EEFC */
	flash_cmd = (0x5A << 24) | (FlashSectorNum << 8) | 0x03 ; //AT91C_MC_FCMD_EWP ;
	
	__disable_irq() ;
	/* Call the IAP function with appropriate command */
	i = IAP_Function( 0, flash_cmd ) ;
	__enable_irq() ;
	return i ;
}


uint32_t readLockBits()
{
	// Always initialise this here, setting a default doesn't seem to work
	IAP_Function = (uint32_t (*)(uint32_t, uint32_t))  *(( uint32_t *)0x00800008) ;
	
	uint32_t flash_cmd = (0x5A << 24) | 0x0A ; //AT91C_MC_FCMD_GLB ;
	__disable_irq() ;
	(void) IAP_Function( 0, flash_cmd ) ;
	__enable_irq() ;
	return EFC->EEFC_FRR ;
}


void clearLockBits()
{
	uint32_t i ;
	uint32_t flash_cmd = 0 ;

	// Always initialise this here, setting a default doesn't seem to work
	IAP_Function = (uint32_t (*)(uint32_t, uint32_t))  *(( uint32_t *)0x00800008) ;
	for ( i = 0 ; i < 16 ; i += 1 )
	{
		flash_cmd = (0x5A << 24) | ((128*i) << 8) | 0x09 ; //AT91C_MC_FCMD_CLB ;
		__disable_irq() ;
		/* Call the IAP function with appropriate command */
		(void) IAP_Function( 0, flash_cmd ) ;
		__enable_irq() ;
	} 
}
#endif

void interrupt10ms()
{
	BlinkCounter += 7 ;
	Tenms |= 1 ;			// 10 mS has passed
 	per10ms() ;
}

#ifdef PCBSKY
void init10msTimer()
{
  register Tc *ptc ;
	register uint32_t timer ;

  PMC->PMC_PCER0 |= 0x02000000L ;		// Enable peripheral clock to TC2

	timer = Master_frequency / 12800  ;		// MCK/128 and 100 Hz

  ptc = TC0 ;		// Tc block 0 (TC0-2)
	ptc->TC_BCR = 0 ;			// No sync
	ptc->TC_BMR = 0 ;
	ptc->TC_CHANNEL[2].TC_CMR = 0x00008000 ;	// Waveform mode
	ptc->TC_CHANNEL[2].TC_RC = timer ;			// 10 Hz
	ptc->TC_CHANNEL[2].TC_RA = timer >> 1 ;
	ptc->TC_CHANNEL[2].TC_CMR = 0x0009C003 ;	// 0000 0000 0000 1001 1100 0000 0000 0011
																						// MCK/128, set @ RA, Clear @ RC waveform
	ptc->TC_CHANNEL[2].TC_CCR = 5 ;		// Enable clock and trigger it (may only need trigger)
	
	NVIC_EnableIRQ(TC2_IRQn) ;
	TC0->TC_CHANNEL[2].TC_IER = TC_IER0_CPCS ;
}

extern "C" void TC2_IRQHandler()
{
  register uint32_t dummy;

  /* Clear status bit to acknowledge interrupt */
  dummy = TC0->TC_CHANNEL[2].TC_SR;
	(void) dummy ;		// Discard value - prevents compiler warning

	interrupt10ms() ;
	
}
#endif

#if ( defined(PCBX9D) || defined(PCB9XT) )
void init10msTimer()
{
	// Timer14
	RCC->APB1ENR |= RCC_APB1ENR_TIM14EN ;		// Enable clock
	TIM14->ARR = 9999 ;	// 10mS
	TIM14->PSC = (Peri1_frequency*Timer_mult1) / 1000000 - 1 ;		// 1uS from 12MHz
	TIM14->CCER = 0 ;	
	TIM14->CCMR1 = 0 ;
	TIM14->EGR = 0 ;
	TIM14->CR1 = 5 ;
	TIM14->DIER |= 1 ;
	NVIC_EnableIRQ(TIM8_TRG_COM_TIM14_IRQn) ;
}

extern "C" void TIM8_TRG_COM_TIM14_IRQHandler()
{
	TIM14->SR &= ~TIM_SR_UIF ;
	interrupt10ms() ;
}

void init_hw_timer()
{
	// Timer13
	RCC->APB1ENR |= RCC_APB1ENR_TIM13EN ;		// Enable clock
	TIM13->ARR = 65535 ;
	TIM13->PSC = (Peri1_frequency*Timer_mult1) / 10000000 - 1 ;		// 0.1uS from 12MHz
	TIM13->CCER = 0 ;	
	TIM13->CCMR1 = 0 ;
	TIM13->EGR = 0 ;
	TIM13->CR1 = 1 ;
}


// delay in units of 0.1 uS up to 6.5535 mS
void hw_delay( uint16_t time )
{
	TIM13->CNT = 0 ;
	TIM13->EGR = 1 ;		// Re-start counter
	while ( TIM13->CNT < time )
	{
		// wait
	}
}

//After reset, write is not allowed in the Flash control register (FLASH_CR) to protect the
//Flash memory against possible unwanted operations due, for example, to electric
//disturbances. The following sequence is used to unlock this register:
//1. Write KEY1 = 0x45670123 in the Flash key register (FLASH_KEYR)
//2. Write KEY2 = 0xCDEF89AB in the Flash key register (FLASH_KEYR)
//Any wrong sequence will return a bus error and lock up the FLASH_CR register until the
//next reset.
//The FLASH_CR register can be locked again by software by setting the LOCK bit in the
//FLASH_CR register.
void unlockFlash()
{
	FLASH->KEYR = 0x45670123 ;
	FLASH->KEYR = 0xCDEF89AB ;
}

void waitFlashIdle()
{
	while (FLASH->SR & FLASH_FLAG_BSY)
	{
	 	wdt_reset() ;
	}
}

#define SECTOR_MASK               ((uint32_t)0xFFFFFF07)

void eraseSector( uint32_t sector )
{
	waitFlashIdle() ;

  FLASH->CR &= CR_PSIZE_MASK;
  FLASH->CR |= FLASH_PSIZE_WORD ;
  FLASH->CR &= SECTOR_MASK;
  FLASH->CR |= FLASH_CR_SER | (sector<<3) ;
  FLASH->CR |= FLASH_CR_STRT;
    
  /* Wait for operation to be completed */
	waitFlashIdle() ;
    
  /* if the erase operation is completed, disable the SER Bit */
  FLASH->CR &= (~FLASH_CR_SER);
  FLASH->CR &= SECTOR_MASK; 
}

uint32_t program( uint32_t *address, uint32_t *buffer )	// size is 256 bytes
{
	uint32_t i ;

	if ( (uint32_t) address == 0x08008000 )
	{
		if ( isFirmwareStart( buffer) )
		{
			FlashBlocked = 0 ;
		}
		else
		{
			FlashBlocked = 1 ;
		}
	}

	if ( FlashBlocked )
	{
		return 1 ;
	}

	if ( (uint32_t) address < 0x08008000 )
	{
		FlashBlocked = 1 ;
		return 1 ;
	}

	if ( (uint32_t) address == 0x08008000 )
	{
		eraseSector( 2 ) ;
	}
	if ( (uint32_t) address == 0x0800C000 )
	{
		eraseSector( 3 ) ;
	}
	if ( (uint32_t) address == 0x08010000 )
	{
		eraseSector( 4 ) ;
	}
	if ( (uint32_t) address == 0x08020000 )
	{
		eraseSector( 5 ) ;
	}
	if ( (uint32_t) address == 0x08040000 )
	{
		eraseSector( 6 ) ;
	}
	if ( (uint32_t) address == 0x08060000 )
	{
		eraseSector( 7 ) ;
	}

	// Now program the 256 bytes
	 
  for (i = 0 ; i < 64 ; i += 1 )
  {
    /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
       be done by word */ 
    
	  // Wait for last operation to be completed
		waitFlashIdle() ;
  
    FLASH->CR &= CR_PSIZE_MASK;
    FLASH->CR |= FLASH_PSIZE_WORD;
    FLASH->CR |= FLASH_CR_PG;
  
    *address = *buffer ;
        
    /* Wait for operation to be completed */
		waitFlashIdle() ;
    FLASH->CR &= (~FLASH_CR_PG);
		 
		 /* Check the written value */
    if ( *address != *buffer )
    {
      /* Flash content doesn't match SRAM content */
      return 2 ;
    }
    /* Increment FLASH destination address */
    address += 1 ;
		buffer += 1 ;
  }
  return 0 ;
}

#endif


uint8_t *cpystr( uint8_t *dest, uint8_t *source )
{
  while ( (*dest++ = *source++) )
    ;
  return dest - 1 ;
}

		
FRESULT readBinDir( DIR *dj, FILINFO *fno )
{
	FRESULT fr ;
	uint32_t loop ;
	do
	{
		loop = 0 ;
		fr = f_readdir ( dj, fno ) ;		// First entry

		if ( fr != FR_OK || fno->fname[0] == 0 )
		{
			break ;
		}
		if ( *fno->lfname == 0 )
		{
			cpystr( (uint8_t *)fno->lfname, (uint8_t *)fno->fname ) ;		// Copy 8.3 name
		}
		int32_t len = strlen(fno->lfname) - 4 ;
		if ( len < 0 )
		{
			loop = 1 ;
		}
		if ( fno->lfname[len] != '.' )
		{
			loop = 1 ;
		}
		if ( ( fno->lfname[len+1] & ~0x20 ) != 'B' )
		{
			loop = 1 ;
		}
		if ( ( fno->lfname[len+2] & ~0x20 ) != 'I' )
		{
			loop = 1 ;
		}
		if ( ( fno->lfname[len+3] & ~0x20 ) != 'N' )
		{
			loop = 1 ;
		}

	} while ( loop ) ;
	return fr ;
}


uint32_t fillNames( uint32_t index )
{
	uint32_t i ;
	FRESULT fr ;
	Finfo.lfname = Filenames[0] ;
	Finfo.lfsize = 48 ;
	fr = f_readdir ( &Dj, 0 ) ;					// rewind
	fr = f_readdir ( &Dj, &Finfo ) ;		// Skip .
	fr = f_readdir ( &Dj, &Finfo ) ;		// Skip ..
	i = 0 ;
	while ( i <= index )
	{
		fr = readBinDir( &Dj, &Finfo ) ;		// First entry
		FileSize[0] = Finfo.fsize ;
		i += 1 ;
		if ( fr == FR_NO_FILE)
		{
			return 0 ;
		}
	}
	for ( i = 1 ; i < 7 ; i += 1 )
	{
		Finfo.lfname = Filenames[i] ;
		fr = readBinDir( &Dj, &Finfo ) ;		// First entry
		FileSize[i] = Finfo.fsize ;
		if ( fr != FR_OK || Finfo.fname[0] == 0 )
		{
			break ;
		}
	}
	return i ;
}

FRESULT openFirmwareFile( uint32_t index )
{
	cpystr( cpystr( (uint8_t *)FlashFilename, (uint8_t *)"\\firmware\\" ), (uint8_t *)Filenames[index] ) ;
	f_open( &FlashFile, FlashFilename, FA_READ ) ;
	f_lseek ( &FlashFile, 32768 ) ;
	return f_read( &FlashFile, (BYTE *)Block_buffer, 4096, &BlockCount ) ;
}



uint8_t flashFile( uint32_t index )
{
	FRESULT fr ;
	
	lcd_clear() ;
	lcd_puts_Pleft( 0, "\005Flash File" ) ;
	if ( Valid == 0 )
	{
		// Validate file here
		// return 3 if invalid
		fr = openFirmwareFile( index ) ;
		fr = f_close( &FlashFile ) ;
		
		Valid = (BlockCount == 4096 ) ? 1 : 2 ;
		if ( isFirmwareStart( Block_buffer ) == 0 )
		{
			Valid = 2 ;
		}
	}
	if ( Valid == 2 )
	{
	  lcd_puts_Pleft( 3*FH,"NOT A VALID FIRMWARE") ;
#ifdef PCBX9D
	  lcd_puts_Pleft( 6*FH,"\015[EXIT]") ;
#else
	  lcd_puts_Pleft( 6*FH,"\007[EXIT]") ;
#endif
		uint8_t event = getEvent() ;
		if ( event == EVT_KEY_LONG(BOOT_KEY_EXIT) )
		{
			return 3 ;
		}
		return 4 ;		// 
	}
	lcd_putsn_P( 0, 2*FH, Filenames[index], DISPLAY_CHAR_WIDTH ) ;

#ifdef PCBX9D
  lcd_puts_Pleft( 6*FH,"\010[ENTER]\021[EXIT]") ;
  lcd_puts_Pleft( 5*FH,"\010YES\021NO") ;
#else
  lcd_puts_Pleft( 6*FH,"\003[MENU]\013[EXIT]") ;
  lcd_puts_Pleft( 5*FH,"\003YES\013NO") ;
#endif
	
	uint8_t event = getEvent() ;

	if ( event == EVT_KEY_LONG(BOOT_KEY_MENU) )
	{
		fr = openFirmwareFile( index ) ;
		FirmwareSize = FileSize[index] ; 
		if ( fr != FR_OK )
		{
			return 4 ;		// File open error
		}
		return 2 ;
	}
	if ( event == EVT_KEY_LONG(BOOT_KEY_EXIT) )
	{
		return 1 ;
	}
	return 0 ;
}


extern Key keys[] ;


uint16_t statuses ;

uint16_t WriteCounter ;
extern uint32_t Breason ;

//#ifdef PCB9XT
//void console9xtInit()
//{
//	// Serial configure  
//	RCC->APB1ENR |= RCC_APB1ENR_UART4EN ;		// Enable clock
//	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN ; 		// Enable portA clock
//	configure_pins( 0x00000001, PIN_PERIPHERAL | PIN_PUSHPULL | PIN_OS25 | PIN_PORTA | PIN_PER_8 ) ;
//	configure_pins( 0x00000002, PIN_PERIPHERAL | PIN_PORTA | PIN_PER_8 | PIN_PULLUP ) ;
//	GPIOA->MODER = (GPIOA->MODER & 0xFFFFFFF0 ) | 0x0000000A ;	// Alternate func.
//	GPIOA->AFR[0] = (GPIOA->AFR[0] & 0xFFFFFF00 ) | 0x00000088 ;	// Alternate func.
//	UART4->BRR = Peri1_frequency / 115200 ;		// 97.625 divider => 19200 baud
////	UART4->CR1 = USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TE | USART_CR1_RE ;
//	UART4->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE ;
//	UART4->CR2 = 0 ;
//	UART4->CR3 = 0 ;
////	NVIC_SetPriority( UART4_IRQn, 4 ) ; // Lower priority interrupt
////  NVIC_EnableIRQ(UART4_IRQn) ;
//}

////extern "C" void UART4_IRQHandler()
////{
////	if ( ( g_model.com2Function == COM2_FUNC_SBUSTRAIN ) || ( g_model.com2Function == COM2_FUNC_SBUS57600 ) )
////	{
////		put_fifo64( &Sbus_fifo, UART4->DR ) ;	
////	}
////	else
////	{
////		put_fifo64( &Console_fifo, UART4->DR ) ;	
////	}	 
////}

//void txmit( uint8_t c )
//{
//	/* Wait for the transmitter to be ready */
//  while ( (UART4->SR & USART_SR_TXE) == 0 ) ;

//  /* Send character */
//	UART4->DR = c ;
//}

//#endif

int main()
{
	uint32_t i ;
  uint8_t index = 0 ;

//#ifdef PCB9XT
//	console9xtInit() ;
//#endif

#if ( defined(PCBX9D) || defined(PCB9XT) )
  uint8_t TenCount = 2 ;
#endif			
  uint8_t maxhsize = DISPLAY_CHAR_WIDTH ;
	FRESULT fr ;
	uint32_t state = ST_START ;
	uint32_t nameCount = 0 ;
	uint32_t vpos = 0 ;
	uint32_t hpos = 0 ;
#if ( defined(PCBX9D) || defined(PCB9XT) )
	uint32_t firmwareAddress = 0x08000000 ;
#endif			
#ifdef PCBSKY
	uint32_t firmwareAddress = 0x00400000 ;
#endif			
	uint32_t firmwareWritten = 0 ;

#if ( defined(PCBX9D) || defined(PCB9XT) )
	wdt_reset() ;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN ; 		// Enable portA clock
#endif

	init_soft_power() ;

#ifdef PCB9XT
	initM64() ;
#endif

#ifdef PCBSKY
	MATRIX->CCFG_SYSIO |= 0x000000F0L ;		// Disable syspins, enable B4,5,6,7
#endif

#ifdef PCBSKY
 #ifdef REVX
	createFat( 512 ) ;
 #else
	createFat( 256 ) ;
 #endif
#else
	createFat( 512 ) ;
#endif

#ifdef PCBSKY
	init_SDcard() ;
	PIOC->PIO_PER = PIO_PC25 ;		// Enable bit C25 (USB-detect)
	start_timer0() ;
#endif

	lcd_init() ;

#ifdef PCBSKY
extern uint8_t OptrexDisplay ;
	OptrexDisplay = 1 ;
#endif
	lcd_clear() ;
#ifdef PCBX9D
	lcd_puts_Pleft( 0, "\006Boot Loader" ) ;
#endif
	refreshDisplay() ;
#ifdef PCBSKY
	OptrexDisplay = 0 ;
	refreshDisplay() ;
#endif

#ifdef PCBX9D	 
	init_keys() ;
	setup_switches() ;
	I2C_EE_Init() ;
	init_hw_timer()	;
#endif

#ifdef PCB9XT
//	init_keys() ;
//	setup_switches() ;
	init_hw_timer()	;
#endif

	__enable_irq() ;

#ifdef PCB9XT
	BlSetColour( 50, 2 ) ;	
#endif

	init10msTimer() ;

#if ( defined(PCBSKY) || defined(PCB9XT) )
	EblockAddress = -1 ;
	init_spi() ;
#endif

#ifdef PCBSKY
	uint32_t chip_id = CHIPID->CHIPID_CIDR ;

	FlashSize = ( (chip_id >> 8 ) & 0x000F ) == 9 ? 256 : 512 ; 
#endif

#if ( defined(PCBX9D) || defined(PCB9XT) )
	FlashSize = 512 ;
#endif

#ifdef PCBSKY
	LockBits = readLockBits() ;
	if ( LockBits )
	{
		clearLockBits() ;
	}
#endif

#if ( defined(PCBX9D) || defined(PCB9XT) )
	// SD card detect pin
#ifdef PCB9XT
	configure_pins( GPIO_Pin_CP, PIN_PORTC | PIN_INPUT | PIN_PULLUP ) ;
#else
	configure_pins( GPIO_Pin_CP, PIN_PORTD | PIN_INPUT | PIN_PULLUP ) ;
#endif
	disk_initialize( 0 ) ;
	sdInit() ;
	unlockFlash() ;

  usbInit() ;
  usbStart() ;
#endif

#ifdef PCB9XT
	i = 40 ;
	do
	{
		if ( Tenms )
		{
	    wdt_reset() ;  // Retrigger hardware watchdog
			Tenms = 0 ;
			i -= 1 ;
		}
	} while ( i ) ;
#endif

	for(;;)
	{
#ifdef PCBSKY
 		if ( PowerUpDelay > 100 )	// 1000 mS
		{
    	usbMassStorage() ;
		}
#endif
		
		wdt_reset() ;

#ifdef PCB9XT
	checkM64() ;
#endif


		if ( Tenms )
		{
	    wdt_reset() ;  // Retrigger hardware watchdog

			if ( EE_timer )
			{
				if ( --EE_timer  == 0)
				{
#if ( defined(PCBSKY) || defined(PCB9XT) )
					writeBlock() ;
#endif
				}
			}

			Tenms = 0 ;
			lcd_clear() ;
#if ( defined(PCBSKY) || defined(PCB9XT) )
			lcd_puts_Pleft( 0, "Boot Loader V ." ) ;
			lcd_putc( 13*FW, 0, Version[4] ) ;
			lcd_putc( 15*FW-2, 0, Version[5] ) ;

//extern uint8_t StartStopCounter ;
//extern uint16_t ReadCounter ;
//	lcd_outhex4( 0, FH, StartStopCounter ) ;
//	lcd_outhex4( 25, FH, ReadCounter ) ;
//	lcd_outhex4( 50, FH, statuses ) ;
//	lcd_outhex4( 75, FH, WriteCounter ) ;

#endif
#ifdef PCBX9D
			lcd_puts_Pleft( 0, "\006Boot Loader V ." ) ;
			lcd_putc( 19*FW, 0, Version[4] ) ;
			lcd_putc( 21*FW-2, 0, Version[5] ) ;
//extern uint8_t StartStopCounter ;
//extern uint16_t ReadCounter ;
//	lcd_outhex4( 0, FH, StartStopCounter ) ;
//	lcd_outhex4( 25, FH, ReadCounter ) ;
			
#endif
	 
			if ( SDcardDisabled )
			{
				if ( BlinkCounter & 512 )
				{
					lcd_puts_Pleft( FH, "SD Card OFF" ) ;
				}
			}

			if ( sd_card_ready() )
			{

#if ( defined(PCBSKY) || defined(PCB9XT) )
				lcd_puts_P( 16*FW-1, 0, "Ready" ) ;
#endif
#ifdef PCBX9D
				lcd_puts_P( 22*FW-1, 0, "Ready" ) ;
#endif

#ifdef PCBSKY
				statuses = usbLunStat() ;
#endif

				if ( state != ST_USB )
				{
					if ( usbPlugged() )
					{
						state = ST_USB ;
						usbPluggedIn( !SDcardDisabled ) ;
					}
				}
				
				if ( state == ST_USB )
				{
#if ( defined(PCBSKY) || defined(PCB9XT) )
					lcd_puts_Pleft( 3*FH, "\004Connecting.." ) ;
#endif
#ifdef PCBX9D
					lcd_puts_Pleft( 3*FH, "\012Connecting.." ) ;
#endif
					if ( usbPlugged() == 0 )
					{
						state = ST_START ;
					}
#ifdef PCBSKY
					lcd_putc( 0, 6*FH, 'F' ) ;
					lcd_putc( 6, 6*FH, '0' + FlashBlocked ) ;
					lcd_putc( 0, 7*FH, 'E' ) ;
					lcd_putc( 6, 7*FH, '0' + EepromBlocked ) ;
#endif
				}
				
				if ( state == ST_START )
				{
  				fr = f_mount(0, &g_FATFS) ;
				}
				else
				{
					fr = FR_OK ;
				}

				if ( fr == FR_OK)
				{
					if ( state == ST_START )
					{
						state = ST_DIR_CHECK ;
					}
				}
				if ( state == ST_DIR_CHECK )
				{
					fr = f_chdir( (TCHAR *)"\\firmware" ) ;
					if ( fr == FR_OK )
					{
						state = ST_OPEN_DIR ;
						index = 0 ;
					}
				}
				if ( state == ST_DIR_CHECK )
				{
					uint8_t event = getEvent() ;
#if ( defined(PCBSKY) || defined(PCB9XT) )
					lcd_puts_Pleft( 16, "\002No Firmware Files" ) ;
#endif
#ifdef PCBX9D
					lcd_puts_Pleft( 16, "\010No Firmware Files" ) ;
#endif
					if ( event == EVT_KEY_REPT( KEY_TRN ) )
					{
						killEvents( event ) ;
					}
					if ( event == EVT_KEY_FIRST( KEY_TRN ) )
					{
						SDcardDisabled = SDcardDisabled ? 0 : 1;
					}
				}
				if ( state == ST_OPEN_DIR )
				{
					fr = f_opendir( &Dj, (TCHAR *) "." ) ;
					if ( fr == FR_OK )
					{
						state = ST_FILE_LIST ;
						index = 0 ;
						nameCount = fillNames( 0 ) ;
						hpos = 0 ;
						vpos = 0 ;
					}
				}
				if ( state == ST_FILE_LIST )
				{
					uint32_t limit = 6 ;
					if ( nameCount < limit )
					{
						limit = nameCount ;						
					}
					maxhsize = 0 ;
					for ( i = 0 ; i < limit ; i += 1 )
					{
						uint32_t x ;
						x = strlen( Filenames[i] ) ;
						if ( x > maxhsize )
						{
							maxhsize = x ;							
						}
						if ( x > DISPLAY_CHAR_WIDTH )
						{
							if ( ( hpos + DISPLAY_CHAR_WIDTH ) > x )
							{
								x = x - DISPLAY_CHAR_WIDTH ;
							}
							else
							{
								x = hpos ;
							}
						}
						else
						{
							x = 0 ;
						}
						lcd_putsn_P( 0, 16+FH*i, &Filenames[i][x], DISPLAY_CHAR_WIDTH ) ;
					}
					{
						uint8_t event = getEvent() ;
						if ( event == EVT_KEY_FIRST( KEY_TRN ) )
						{
							SDcardDisabled = SDcardDisabled ? 0 : 1;
						}
						
						if ( ( event == EVT_KEY_REPT(BOOT_KEY_DOWN) ) || event == EVT_KEY_FIRST(BOOT_KEY_DOWN) )
						{
							if ( vpos < limit-1 )
							{
								vpos += 1 ;
							}
							else
							{
								if ( nameCount > limit )
								{
									index += 1 ;
									nameCount = fillNames( index ) ;
								}
							}
						}
						if ( ( event == EVT_KEY_REPT(BOOT_KEY_UP)) || ( event == EVT_KEY_FIRST(BOOT_KEY_UP) ) )
						{
							if ( vpos > 0 )
							{
								vpos -= 1 ;
							}
							else
							{
								if ( index )
								{
									index -= 1 ;
									nameCount = fillNames( index ) ;
								}
							}
						}
						if ( ( event == EVT_KEY_REPT(BOOT_KEY_RIGHT)) || ( event == EVT_KEY_FIRST(BOOT_KEY_RIGHT) ) )
						{
							if ( hpos + DISPLAY_CHAR_WIDTH < maxhsize )
							{
								hpos += 1 ;								
							}
						}
						if ( ( event == EVT_KEY_REPT(BOOT_KEY_LEFT)) || ( event == EVT_KEY_FIRST(BOOT_KEY_LEFT) ) )
						{
							if ( hpos )
							{
								hpos -= 1 ;								
							}
						}
						if ( event == EVT_KEY_LONG(BOOT_KEY_MENU) )
						{
							// Select file to flash
							state = ST_FLASH_CHECK ;
							Valid = 0 ;
						}
						if ( event == EVT_KEY_LONG(BOOT_KEY_EXIT) )
						{
							state = ST_REBOOT ;
						}
					}
					lcd_char_inverse( 0, 2*FH+FH*vpos, DISPLAY_CHAR_WIDTH*FW, 0 ) ;
				}
				if ( state == ST_FLASH_CHECK )
				{
					i = flashFile( vpos ) ;
					FirmwareSize = FileSize[vpos] - 32768 ;
					if ( i == 1 )
					{
						state = ST_FILE_LIST ;		// Canceled						
					}
					if ( i == 2 )
					{
#ifdef PCBSKY
						firmwareAddress = 0x00408000 ;
#endif
#if ( defined(PCBX9D) || defined(PCB9XT) )
						firmwareAddress = 0x08008000 ;
#endif
						firmwareWritten = 0 ;
						state = ST_FLASHING ;		 // confirmed
					}
					if ( i == 3 )
					{
						// Invalid file
						state = ST_FILE_LIST ;		// Canceled						
					}
				}
				if ( state == ST_FLASHING )
				{
					// Commit to flashing
					uint32_t blockOffset = 0 ;
					lcd_puts_Pleft( 3*FH, "Flashing" ) ;
					while ( BlockCount )
					{
						program( (uint32_t *)firmwareAddress, &Block_buffer[blockOffset] ) ;	// size is 256 bytes
						blockOffset += 64 ;		// 32-bit words (256 bytes)
						firmwareAddress += 256 ;
						if ( BlockCount > 256 )
						{
							BlockCount -= 256 ;							
						}
						else
						{
							BlockCount = 0 ;
						}
					}
					firmwareWritten += 1 ;
					uint32_t width = FirmwareSize / 4096 ;
					lcd_hline( 0, 5*FH-1, width+1 ) ;
					lcd_hline( 0, 6*FH, width+1 ) ;
					lcd_vline( width, 5*FH, 8 ) ;
					for ( i = 0 ; i < firmwareWritten ; i += 1 )
					{
						lcd_vline( i, 5*FH, 8 ) ;
					}
					fr = f_read( &FlashFile, (BYTE *)Block_buffer, 4096, &BlockCount ) ;
					if ( BlockCount == 0 )
					{
						state = ST_FLASH_DONE ;
					}
					if ( firmwareWritten > FlashSize/4 - 9 )	// (127-8, or 63-8) 4K blocks
					{
						state = ST_FLASH_DONE ;				// Backstop
					}
				}
				if ( state == ST_FLASH_DONE )
				{
					uint8_t event = getEvent() ;
					lcd_puts_Pleft( 3*FH, "Flashing Complete" ) ;
					if ( event == EVT_KEY_LONG(BOOT_KEY_EXIT) )
					{
						state = ST_FILE_LIST ;
					}
				}
			}

#if ( defined(PCBX9D) || defined(PCB9XT) )
			if ( --TenCount == 0 )
			{
				TenCount = 4 ;
#endif			
			refreshDisplay() ;
#if ( defined(PCBX9D) || defined(PCB9XT) )
			}
#endif			
			if ( PowerUpDelay < 200 )	// 2000 mS
			{
				PowerUpDelay += 1 ;
			}
			if ( PowerUpDelay >= 20 )	// 200 mS
//			if ( PowerUpDelay < 20 )	// 200 mS
//			{
//				PowerUpDelay += 1 ;
//			}
//			else
			{
#ifdef PCBSKY
				sd_poll_10mS() ;
#endif			
#if ( defined(PCBX9D) || defined(PCB9XT) )
				sdPoll10ms() ;
#endif			
			}
		}
		if ( ( state < ST_FLASH_CHECK ) || (state == ST_FLASH_DONE) )
		{
			if ( check_soft_power() == POWER_OFF )
			{
				soft_power_off() ;
				for(;;)
				{
					// Wait for power to go off
				}
			}
		}
		if ( state < ST_FILE_LIST )
		{
			if ( getEvent() == EVT_KEY_LONG(BOOT_KEY_EXIT) )
			{
				state = ST_REBOOT ;
			}
		}
		if ( state == ST_REBOOT )
		{
			if ( (~read_keys() & 0x7E) == 0 )
			{
		  	NVIC_SystemReset() ;
			}
		}

	}
//	stop_timer0() ;

  return 0;
}


