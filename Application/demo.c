/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
Provide MFS file system on external SD card.
*
*
*END************************************************************************/


#include <mqx.h>
#include <bsp.h>
#include <fio.h>
#include <mfs.h>
#include <shell.h>
#include <sdcard.h>
#include <spi.h>
#include <part_mgr.h>
#include "Temp.h"

#if ! SHELLCFG_USES_MFS
#error "This application requires SHELLCFG_USES_MFS defined non-zero in user_config.h. Please recompile libraries with this option."
#endif

#if defined BSP_SDCARD_ESDHC_CHANNEL
#if ! BSPCFG_ENABLE_ESDHC
#error "This application requires BSPCFG_ENABLE_ESDHC defined non-zero in user_config.h. Please recompile libraries with this option."
#else
#define SDCARD_COM_CHANNEL BSP_SDCARD_ESDHC_CHANNEL
#endif

#elif defined BSP_SDCARD_SDHC_CHANNEL
#if ! BSPCFG_ENABLE_SDHC
#error "This application requires BSPCFG_ENABLE_SDHC defined non-zero in user_config.h. Please recompile libraries with this option."
#else
#define SDCARD_COM_CHANNEL BSP_SDCARD_SDHC_CHANNEL
#endif

#elif defined (BSP_SDCARD_SPI_CHANNEL)
#define SDCARD_COM_CHANNEL BSP_SDCARD_SPI_CHANNEL
#else
#error "SDCARD low level communication device not defined!"
#endif

#define TD_STRUCT_PTR void *

static LWGPIO_STRUCT stLedRed;
static LWGPIO_STRUCT stLedGreen;
static LWGPIO_STRUCT stLedBlue;

#define BSP_RGBRED                    (GPIO_PORT_B | GPIO_PIN22)
#define BSP_RGBRED_MUX_GPIO           (LWGPIO_MUX_B22_GPIO)

#define BSP_RGBGREEN                  (GPIO_PORT_E | GPIO_PIN26)
#define BSP_RGBGREEN_MUX_GPIO         (LWGPIO_MUX_E26_GPIO)

#define BSP_RGBBLUE                   (GPIO_PORT_B | GPIO_PIN21)
#define BSP_RGBBLUE_MUX_GPIO          (LWGPIO_MUX_B21_GPIO)

#define	LED_OFF	(LWGPIO_VALUE_HIGH)
#define	LED_ON	(LWGPIO_VALUE_LOW)

_task_id motorId, readId, SdCardId;

static LWGPIO_STRUCT stLedRed;
static LWGPIO_STRUCT stLedGreen;
static LWGPIO_STRUCT stLedBlue;

bool boBlueInit;

void init_task(uint32_t);
void motor_task(uint32_t);
void read_task(uint32_t);
void sdcard_task(uint32_t);

const TASK_TEMPLATE_STRUCT  MQX_template_list[] =
{
   /* Task Index,   Function,     Stack,  Priority, Name,     Attributes,          Param, Time Slice */
	{ 4,            init_task,    1500,    10,     "Init",  MQX_AUTO_START_TASK, 0,     0 },
	{ 3,            motor_task,   3000,    12,     "Motor",  0, 0,     0 },
    { 2,            read_task,    3000,    11,     "Read",   0, 0,     0 },
    { 1,            sdcard_task,  2000,    12,     "SDcard", 0, 0,     0 },
    { 0 }
};


/*TASK*-----------------------------------------------------------------
*
* Function Name  : shell_task
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
void init_task(uint32_t temp)
{
	_task_id init_taskId;
    (void)temp; /* suppress 'unused variable' warning */
    Adc_Init();
    
	if (!lwgpio_init(&stLedBlue, BSP_RGBBLUE, LWGPIO_DIR_OUTPUT, LED_OFF))
	{
		printf("...RED LED failed!!\n");
	}else
	{
		boBlueInit= TRUE;
		printf("R");
		lwgpio_set_functionality(&stLedBlue, BSP_RGBBLUE_MUX_GPIO);
	}
    
    init_taskId=_task_get_id();
    /* Run the shell on the serial port */
    printf("Demo Initialized\n");
    
    motorId =  _task_create_blocked(0, 3, 0);
    SdCardId = _task_create_blocked(0, 1, 0);
    readId  =  _task_create(0, 2, 0);
    _task_destroy(init_taskId);
    
}

/*TASK*-----------------------------------------------------------------
*
* Function Name  : shell_task
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
void read_task(uint32_t temp)
{
    (void)temp; /* suppress 'unused variable' warning */
    _task_id motor_Id, sdcard_Id;
    TD_STRUCT_PTR motor_ptr;
    TD_STRUCT_PTR sdcard_ptr;
    
    
    motor_Id = _task_get_id_from_name("Motor");
    sdcard_Id = _task_get_id_from_name("SDcard"); 
    
    motor_ptr = _task_get_td(motor_Id);
    sdcard_ptr = _task_get_td(sdcard_Id);
        
    for(;;){
    	
    	 lwgpio_toggle_value(&stLedBlue);
    	    
    	 /* Run the shell on the serial port */
    	 printf("Read Task\n");
    	
		_task_ready(motor_ptr);
		_task_ready(sdcard_ptr);
    	    
    	_time_delay (1000);
    }
    
}

/*TASK*-----------------------------------------------------------------
*
* Function Name  : shell_task
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
void motor_task(uint32_t temp)
{
    (void)temp; /* suppress 'unused variable' warning */
    
    /* Run the shell on the serial port */
    for(;;){
    printf("Motor Task\n");
    _task_block();
    }
}

/*TASK*-----------------------------------------------------------------
*
* Function Name  : sdcard_task
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
void sdcard_task(uint32_t temp)
{
    (void)          temp; /* suppress 'unused variable' warning */
    bool            inserted = TRUE, readonly = FALSE, last = FALSE;
    _mqx_int        error_code;
    _mqx_uint       param;
    MQX_FILE_PTR    com_handle, sdcard_handle, filesystem_handle, partition_handle;
    char            filesystem_name[] = "a:";
    char            partman_name[] = "pm:";
    char            partition_name[] = "pm:1";
#if defined BSP_SDCARD_GPIO_DETECT
    LWGPIO_STRUCT   sd_detect;
#endif
        
    /* Open low level communication device */
    com_handle = fopen(SDCARD_COM_CHANNEL, (void *)(SPI_FLAG_FULL_DUPLEX));

    if (NULL == com_handle)
    {
        printf("Error opening communication handle %s, check your user_config.h.\n", SDCARD_COM_CHANNEL);
        _task_block();
    }

#if defined BSP_SDCARD_GPIO_DETECT
    /* Init GPIO pins for other SD card signals */
    error_code = lwgpio_init(&sd_detect, BSP_SDCARD_GPIO_DETECT, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE);
       if (!error_code)
       {
           printf("Initializing GPIO with sdcard detect pin failed.\n");
           _task_block();
       }
       
       /*Set detect and protect pins as GPIO Function */
       lwgpio_set_functionality(&sd_detect,BSP_SDCARD_DETECT_MUX_GPIO);
#if defined(BSP_SDCARD_DETECT_ACTIVE_LOW)
       lwgpio_set_attribute(&sd_detect, LWGPIO_ATTR_PULL_DOWN, LWGPIO_AVAL_ENABLE);
#endif
#endif /* BSP_SDCARD_GPIO_DETECT */
    /* Install SD card device */
    error_code = _io_sdcard_install("sdcard:", (void *)&_bsp_sdcard0_init, com_handle);
    if ( error_code != MQX_OK )
    {
        printf("Error installing SD card device (0x%x)\n", error_code);
        _task_block();
    }

    for (;;)
    {
        /* Run the shell on the serial port */
        printf("Sdcard Task\n");
#if defined BSP_SDCARD_GPIO_DETECT
#if defined(BSP_SDCARD_DETECT_ACTIVE_LOW)
        inserted = lwgpio_get_value(&sd_detect);
#endif /* BSP_SDCARD_DETECT_ACTIVE_LOW */
#endif /* BSP_SDCARD_GPIO_DETECT */

        if (last != inserted)
        {
            last = inserted;
        	        
            if (inserted)
            {
                _time_delay (200);
                /* Open the device which MFS will be installed on */
                sdcard_handle = fopen("sdcard:", 0);
                if (sdcard_handle == NULL)
                {
                    printf("Unable to open SD card device.\n");
                    continue;
                }

                /* Set read only flag as needed */
                param = 0;
                if (readonly)
                {
                    param = IO_O_RDONLY;
                }
                if (IO_OK != ioctl(sdcard_handle, IO_IOCTL_SET_FLAGS, (char *) &param))
                {
                    printf("Setting device read only failed.\n");
                    continue;
                }

                /* Install partition manager over SD card driver */
                error_code = _io_part_mgr_install(sdcard_handle, partman_name, 0);
                if (error_code != MFS_NO_ERROR)
                {
                    printf("Error installing partition manager: %s\n", MFS_Error_text((uint32_t)error_code));
                    continue;
                }

                /* Open partition */
                partition_handle = fopen(partition_name, NULL);
                if (partition_handle != NULL)
                {
                    printf("Installing MFS over partition...\n");
                    
                    /* Validate partition */
                    error_code = _io_ioctl(partition_handle, IO_IOCTL_VAL_PART, NULL);
                    if (error_code != MFS_NO_ERROR)
                    {
                        printf("Error validating partition: %s\n", MFS_Error_text((uint32_t)error_code));
                        printf("Not installing MFS.\n");
                        continue;
                    }

                    /* Install MFS over partition */
                    error_code = _io_mfs_install(partition_handle, filesystem_name, 0);
                    if (error_code != MFS_NO_ERROR)
                    {
                        printf("Error initializing MFS over partition: %s\n", MFS_Error_text((uint32_t)error_code));
                    }

                } else {

                    printf("Installing MFS over SD card driver...\n");
                	
                    /* Install MFS over SD card driver */
                    error_code = _io_mfs_install(sdcard_handle, filesystem_name, (_file_size)0);
                    if (error_code != MFS_NO_ERROR)
                    {
                        printf("Error initializing MFS: %s\n", MFS_Error_text((uint32_t)error_code));
                    }
                }

                /* Open file system */
                if (error_code == MFS_NO_ERROR) {
                    filesystem_handle = fopen(filesystem_name, NULL);
                    error_code = ferror(filesystem_handle);
                    if (error_code == MFS_NOT_A_DOS_DISK)
                    {
                        printf("NOT A DOS DISK! You must format to continue.\n");
                    }
                    else if (error_code != MFS_NO_ERROR)
                    {
                        printf("Error opening filesystem: %s\n", MFS_Error_text((uint32_t)error_code));
                        continue;
                    }

                    printf ("SD card installed to %s\n", filesystem_name);
                    if (readonly)
                    {
                        printf ("SD card is locked (read only).\n");
                    }
                }
            }
            else
            {
                /* Close the filesystem */
                if ((filesystem_handle != NULL) && (MQX_OK != fclose(filesystem_handle)))
                {
                    printf("Error closing filesystem.\n");
                }
                filesystem_handle = NULL;

                /* Uninstall MFS  */
                error_code = _io_dev_uninstall(filesystem_name);
                if (error_code != MFS_NO_ERROR)
                {
                    printf("Error uninstalling filesystem.\n");
                }
                
                /* Close partition */
                if ((partition_handle != NULL) && (MQX_OK != fclose(partition_handle)))
                {
                    printf("Error closing partition.\n");
                }
                partition_handle = NULL;

                /* Uninstall partition manager  */
                error_code = _io_dev_uninstall(partman_name);
                if (error_code != MFS_NO_ERROR)
                {
                    printf("Error uninstalling partition manager.\n");
                }

                /* Close the SD card device */
                if ((sdcard_handle != NULL) && (MQX_OK != fclose(sdcard_handle)))
                {
                    printf("Unable to close SD card device.\n");
                }
                sdcard_handle = NULL;

                printf ("SD card uninstalled.\n");
            }
        }
        _task_block();
    }
}

/* EOF */