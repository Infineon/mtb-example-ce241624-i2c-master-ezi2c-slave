/*******************************************************************************
* \file main.c
*
* Description: This code example demonstrates the communication between EZI2C
*              slave and I2C master. This is an integrated I2C master and EZI2C
*              slave code where in the master is configured to send command
*              packets to EZI2C slave to control a user LED on the board.
* 
* Related Document: See README.md
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
******************************************************************************/
#include "ezi2c_slave.h"
#include "cybsp.h"
#include "i2c_master.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define OFF                 (1UL)
#define ON                  (0UL)
#define CMD_TO_CMD_DELAY    (1000UL)

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Debug UART variables */
static cy_stc_scb_uart_context_t DEBUG_UART_context;
static mtb_hal_uart_t            DEBUG_UART_hal_obj;
/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/*******************************************************************************
* Function Definitions
*******************************************************************************/
/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function of the CE.
* It initializes the GPIO for LED, debug UART, EZI2C slave and the I2C mater and
* then prints the CE name through the UART terminal. In while loop, it sends the
* LED toggle commands to the EZI2C slave through I2C master and then reads its
* status. Also, it checks for the message received in the slave receive buffer
* and changes LED output based on the command received in the buffer.
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
    cy_rslt_t result;
    uint8_t cmd = ON;
    uint32_t status;
    uint8_t  buffer[WRITE_PACKET_SIZE] = {0};
    cy_en_scb_uart_status_t init_status;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure retarget-io to use the debug UART port */
    init_status = Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

    /* UART init failed. Stop program execution */
    if (init_status != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable UART */
    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);

    /* HAL UART setup failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget IO */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* retarget IO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: I2C master EZI2C slave\r\n");
    printf("************************************************************\r\n\n");

    /* Initialize I2C Slave */
    printf(">> Initializing EZI2C Slave..... ");
    status = init_ezi2c_slave();
    if(status != I2C_SUCCESS)
    {
        CY_ASSERT(0);
    }
    printf("Done\r\n");

    /* Initialize I2C Master */
    printf(">> Initializing I2C master..... ");
    status = init_i2c_master();
    if(status == I2C_FAILURE)
    {
        CY_ASSERT(0);
    }
    printf("Done\r\n");

    /* Enable interrupts */
    __enable_irq();

    for(;;)
    {
        /* Create packet to be sent to the slave.  */
        buffer[MESG_ADDR_POS] = EZI2C_BUFFER_ADDRESS;
        buffer[MESG_SOP_POS] = MESG_SOP;
        buffer[MESG_EOP_POS] = MESG_EOP;
        buffer[MESG_CMD_POS] = cmd;

        /* Send packet with LED toggle command to the slave. */
        uint8_t res = write_packet_to_ezi2c(buffer, WRITE_PACKET_SIZE);

        if (TRANSFER_CMPLT == res)
        {
            printf("I2C: Write complete\r\n");

            /* Read response packet from the slave. */
            if (TRANSFER_CMPLT == read_packet_from_ezi2c())
            {
                printf("I2C: Read Complete\r\n");

                /* Next command to be written. */
                cmd = (cmd == ON) ? OFF : ON;
            }

            /* Check the EZI2C Slave buffer for the received command. If
             * the received packet is valid, change the status of LED
             * based on the command received.
             */
            check_ezi2c_buffer();

            /* Give 1 Second delay between commands. */
            Cy_SysLib_Delay(CMD_TO_CMD_DELAY);
        }
        else
        {
            printf("Write failed with return val: %d\r\n", res);

            /* Give 1 Second delay between commands. */
            Cy_SysLib_Delay(CMD_TO_CMD_DELAY);
        }
    }
}
