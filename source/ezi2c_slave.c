/******************************************************************************
* File Name:   ezi2c_slave.c
*
* Description: This file contains all the functions, MACROS and variables
*              required for the operation of the SCB in EZI2C slave mode and
*              to process the received message.
*
* Related Document: See README.md
*
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
*******************************************************************************/
/* Header file includes */
#include "ezi2c_slave.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* Macros for EZI2C slave interrupt */
#define EZI2C_INTR_NUM             EZI2C_SLAVE_IRQ
#define EZI2C_INTR_PRIORITY        (3UL)

/* EZI2C total buffer size */
#define EZI2C_BUFFER_SIZE          (0x08UL)

/* EZI2C slave send/receive packet start and end markers */
#define MESG_RPLY_SOP              (0x01UL)
#define MESG_RPLY_EOP              (0x17UL)

/* Status commands */
#define STS_CMD_DONE               (0x00UL)
#define STS_CMD_FAIL               (0xFFUL)

/* EZI2C slave receive packet index */
#define EZI2C_MESG_SOP_POS         (0x00UL)
#define EZI2C_MESG_CMD_POS         (0x01UL)
#define EZI2C_MESG_EOP_POS         (0x02UL)

/* EZI2C slave send packet index */
#define EZI2C_RPLY_SOP_POS         (0x05UL)
#define EZI2C_RPLY_STS_POS         (0x06UL)
#define EZI2C_RPLY_EOP_POS         (0x07UL)

#define ZERO                       (0UL)

/*******************************************************************************
* Global variables
*******************************************************************************/

/* Context structure for EZI2C Slave */
cy_stc_scb_ezi2c_context_t EZI2C_SLAVE_context;

/* sys interrupt configuration for EZI2C slave */
cy_stc_sysint_t EZI2C_SLAVE_SCB_IRQ_cfg =
{
        /*.intrSrc =*/ EZI2C_SLAVE_IRQ,
        /*.intrPriority =*/ 3u
};

/* EZI2C buffer */
uint8_t buffer_slave[EZI2C_BUFFER_SIZE] ;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void ezi2c_slave_interrupt_handler(void);

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: ezi2c_slave_interrupt_handler
********************************************************************************
* Summary:
*   Interrupt service routine for ezi2c slave.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void ezi2c_slave_interrupt_handler(void)
{
    /* ISR implementation for EZI2C. */
    Cy_SCB_EZI2C_Interrupt(EZI2C_SLAVE_HW, &EZI2C_SLAVE_context);
}

/*******************************************************************************
* Function Name: check_ezi2c_buffer
********************************************************************************
*
* Summary:
*   This function is for continuously checking for the received message. If a
*   message is received, it will verify it by checking start and end markers.
*   Then it will update the LED status based on the received command.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void check_ezi2c_buffer( void )
{
    volatile uint32_t ezi2c_state = 0;

    /* Disable the EZI2C interrupts so that ISR is not serviced while
     * checking for EZI2C status.
     */
    NVIC_DisableIRQ(EZI2C_SLAVE_SCB_IRQ_cfg.intrSrc);

    /* Read the EZi2C status */
    ezi2c_state = Cy_SCB_EZI2C_GetActivity(EZI2C_SLAVE_HW, &EZI2C_SLAVE_context);

    /* Write complete without errors: parse packets, otherwise ignore. */
    if((0u != (ezi2c_state & CY_SCB_EZI2C_STATUS_READ1)) && (0u == (ezi2c_state & CY_SCB_EZI2C_STATUS_ERR)))
    {
        /* Check buffer content to know any new packets are written from master. */
        if((buffer_slave[EZI2C_MESG_SOP_POS] == MESG_RPLY_SOP) && (buffer_slave[EZI2C_MESG_EOP_POS] == MESG_RPLY_EOP))
        {
            /* Update LED if a new packet is received. */
            Cy_GPIO_Write(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_NUM, buffer_slave[EZI2C_MESG_CMD_POS]);

            /* Clear the location so that any new packets written to buffer will be known. */
            buffer_slave[EZI2C_MESG_SOP_POS] = ZERO;
            buffer_slave[EZI2C_MESG_EOP_POS] = ZERO;

            /* Update the buffer with success message */
            buffer_slave[EZI2C_RPLY_SOP_POS] = MESG_RPLY_SOP;
            buffer_slave[EZI2C_RPLY_STS_POS] = STS_CMD_DONE;
            buffer_slave[EZI2C_RPLY_EOP_POS] = MESG_RPLY_EOP;
        }
        else
        {
            /* Update the buffer with failure message */
            buffer_slave[EZI2C_RPLY_SOP_POS] = MESG_RPLY_SOP;
            buffer_slave[EZI2C_RPLY_STS_POS] = STS_CMD_FAIL;
            buffer_slave[EZI2C_RPLY_EOP_POS] = MESG_RPLY_EOP;
        }
    }

    /* Enable interrupts for servicing ISR. */
    NVIC_EnableIRQ(EZI2C_SLAVE_SCB_IRQ_cfg.intrSrc);
}

/*******************************************************************************
* Function Name: init_ezi2c_slave
********************************************************************************
*
* Summary:
*   This function is for initializing the SCB as EZI2C slave and configure the
*   interrupt service routine.
*
* Return:
*   Status of initialization.
*
*******************************************************************************/
uint32_t init_ezi2c_slave(void)
{
    cy_en_scb_ezi2c_status_t ezi2c_init_status;
    cy_en_sysint_status_t    sysint_status;

    /*Initialize EZI2C slave.*/
    ezi2c_init_status = Cy_SCB_EZI2C_Init(EZI2C_SLAVE_HW, &EZI2C_SLAVE_config, &EZI2C_SLAVE_context);
    if(ezi2c_init_status!=CY_SCB_EZI2C_SUCCESS)
    {
        return I2C_FAILURE;
    }

    /* Init interrupt service routine and enable interrupt. */
    sysint_status = Cy_SysInt_Init(&EZI2C_SLAVE_SCB_IRQ_cfg, &ezi2c_slave_interrupt_handler);
    if(sysint_status!=CY_SYSINT_SUCCESS)
    {
        return I2C_FAILURE;
    }
    NVIC_EnableIRQ((IRQn_Type) EZI2C_SLAVE_SCB_IRQ_cfg.intrSrc);

    /* Configure buffer for communication with master. */
    Cy_SCB_EZI2C_SetBuffer1(EZI2C_SLAVE_HW, buffer_slave, EZI2C_BUFFER_SIZE, EZI2C_BUFFER_SIZE, &EZI2C_SLAVE_context);

    /* Enable SCB for the EZI2C operation. */
    Cy_SCB_EZI2C_Enable(EZI2C_SLAVE_HW);

    return I2C_SUCCESS;
}

