/******************************************************************************
* File Name:   i2c_master.c
*
* Description: This file contains all the functions and variables required for
*              the operation of the SCB in I2C master mode and to send and
*              receive messages with EZI2C slave.
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
#include "i2c_master.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* I2C master interrupt macros */
#define I2C_INTR_NUM        I2C_MASTER_IRQ
#define I2C_INTR_PRIORITY   (3UL)

/* I2C slave address to communicate with */
#define I2C_SLAVE_ADDR      (12UL)

/* Buffer and packet size */
#define READ_PACKET_SIZE    (0x08UL)

/* Status commands */
#define STS_CMD_DONE        (0x00UL)
#define STS_CMD_FAIL        (0xFFUL)

/* Communication status */
#define TRANSFER_ERROR      (0xFFUL)
#define READ_ERROR          (TRANSFER_ERROR)

/* I2C master receive packet index */
#define RPLY_SOP_POS  (5UL)
#define RPLY_STS_POS  (6UL)
#define RPLY_EOP_POS  (7UL)

/* Combine master error statuses in single mask  */
#define MASTER_ERROR_MASK   (CY_SCB_I2C_MASTER_DATA_NAK | CY_SCB_I2C_MASTER_ADDR_NAK   | \
                            CY_SCB_I2C_MASTER_ARB_LOST | CY_SCB_I2C_MASTER_ABORT_START | \
                            CY_SCB_I2C_MASTER_BUS_ERR)

/*******************************************************************************
* Global variables
*******************************************************************************/
/* Structure for master transfer configuration */
cy_stc_scb_i2c_master_xfer_config_t master_transfer_cfg =
{
    .slaveAddress = I2C_SLAVE_ADDR,
    .buffer       = NULL,
    .bufferSize   = 0U,
    .xferPending  = false
};

/* Context structure for I2C Master */
cy_stc_scb_i2c_context_t I2C_MASTER_context;

/* sys interrupt configuration for I2C master */
cy_stc_sysint_t I2C_MASTER_SCB_IRQ_cfg =
{
        /*.intrSrc =*/ I2C_MASTER_IRQ,
        /*.intrPriority =*/ 3u
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void i2c_master_interrupt(void);

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: i2c_master_interrupt
****************************************************************************//**
*
* Summary:
*   Invokes the Cy_SCB_I2C_MasterInterrupt() PDL driver function.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void i2c_master_interrupt(void)
{
    Cy_SCB_I2C_MasterInterrupt(I2C_MASTER_HW, &I2C_MASTER_context);
}

/*******************************************************************************
* Function Name: write_packet_to_ezi2c
********************************************************************************
*
* Summary:
*         This function is for sending packets to the EZI2C slave. Out of the
*         two arguments the first one is the packet and the next one is its size.
*         After sending the packet it gets the write status and then returns the
*         status.
*
* Parameters:
*   uint8_t* write_buffer Command packet buffer pointer
*   uint32_t buffer_size  Size of the packet buffer
*
* Return:
*   Status after command is written to slave.
*   TRANSFER_ERROR is returned if any error occurs.
*   TRANSFER_CMPLT is returned if write is successful.
*
*******************************************************************************/
uint8_t write_packet_to_ezi2c(uint8_t* write_buffer, uint32_t buffer_size)
{
    uint8_t                 return_status = TRANSFER_ERROR;
    uint32_t                master_write_status;
    cy_en_scb_i2c_status_t  error_status;

    /* Timeout 1 sec (one unit is us) */
    uint32_t timeout = 1000000UL;

    /* Setup transfer specific parameters */
    master_transfer_cfg.buffer     = write_buffer;
    master_transfer_cfg.bufferSize = buffer_size;

    /* Initiate write transaction */
    error_status = Cy_SCB_I2C_MasterWrite(I2C_MASTER_HW, &master_transfer_cfg, &I2C_MASTER_context);
    if(error_status == CY_SCB_I2C_SUCCESS)
    {
        /* Wait until master complete read transfer or time out has occurred */
        do
        {
            /* Read current I2C master status. */
            master_write_status  = Cy_SCB_I2C_MasterGetStatus(I2C_MASTER_HW, &I2C_MASTER_context);
            Cy_SysLib_DelayUs(CY_SCB_WAIT_1_UNIT);
            timeout--;

        } while ((0UL != (master_write_status & CY_SCB_I2C_MASTER_BUSY)) && (timeout > 0));

        if (timeout <= 0)
        {
            /* Timeout recovery */
            Cy_SCB_I2C_Disable(I2C_MASTER_HW, &I2C_MASTER_context);
            Cy_SCB_I2C_Enable(I2C_MASTER_HW);
        }
        else
        {
            /* Set write completion success when there is no error and the complete packet is sent. */
            if ((0u == (MASTER_ERROR_MASK & master_write_status)) &&
                (WRITE_PACKET_SIZE == Cy_SCB_I2C_MasterGetTransferCount(I2C_MASTER_HW, &I2C_MASTER_context)))
            {
                return_status = TRANSFER_CMPLT;
            }
        }
    }

    return (return_status);
}

/*******************************************************************************
* Function Name: read_packet_from_ezi2c
****************************************************************************//**
*
* Summary:
*         This function is for reading the status from the EZI2C slave. It reads
*         the entire buffer from the slave device and verifies the status of
*         processing the message received in the slave.
*
*  Parameters:
*  void
*
* Return:
*   Status of the transfer by checking packets read.
*   Note that if the status packet read is correct function returns TRANSFER_CMPLT
*   and if status packet is incorrect function returns TRANSFER_ERROR.
*
*******************************************************************************/
uint8_t read_packet_from_ezi2c(void)
{
    uint8_t                return_status = TRANSFER_ERROR;
    uint32_t               master_read_status;
    cy_en_scb_i2c_status_t i2c_init_status;

    /* Timeout 1 sec (one unit is us) */
    uint32_t timeout = 1000000UL;
    uint8_t buffer[READ_PACKET_SIZE];

    /* Setup transfer specific parameters */
    master_transfer_cfg.buffer     = buffer;
    master_transfer_cfg.bufferSize = READ_PACKET_SIZE;

    /* Initiate read transaction */
    i2c_init_status = Cy_SCB_I2C_MasterRead(I2C_MASTER_HW, &master_transfer_cfg, &I2C_MASTER_context);
    if(i2c_init_status == CY_SCB_I2C_SUCCESS)
    {
        /* Wait until master complete read transfer or time out has occurred */
        do
        {
            /* Read current I2C master status. */
            master_read_status  = Cy_SCB_I2C_MasterGetStatus(I2C_MASTER_HW, &I2C_MASTER_context);
            Cy_SysLib_DelayUs(CY_SCB_WAIT_1_UNIT);
            timeout--;

        } while ((0UL != (master_read_status & CY_SCB_I2C_MASTER_BUSY)) && (timeout > 0));

        if (timeout <= 0)
        {
            /* Timeout recovery */
            Cy_SCB_I2C_Disable(I2C_MASTER_HW, &I2C_MASTER_context);
            Cy_SCB_I2C_Enable(I2C_MASTER_HW);
        }
        else
        {
            /* Check transfer status */
            if (0u == (MASTER_ERROR_MASK & master_read_status))
            {
                /* Check packet structure and status */
                if((MESG_SOP     == buffer[RPLY_SOP_POS]) &&
                   (MESG_EOP     == buffer[RPLY_EOP_POS]) &&
                   (STS_CMD_DONE == buffer[RPLY_STS_POS]) )
                {
                    return_status = TRANSFER_CMPLT;
                }
            }
        }
    }
    return (return_status);

}

/*******************************************************************************
* Function Name: init_i2c_master
********************************************************************************
*
* Summary:
*   This function initiates and enables SCB in I2C master mode. It also
*   configures the interrupt service routine.
*
* Return:
*   Status of initialization
*
*******************************************************************************/
uint32_t init_i2c_master(void)
{
    cy_en_scb_i2c_status_t i2c_init_status;
    cy_en_sysint_status_t sysint_status;

    /*Initialize and enable the I2C in master mode*/
    i2c_init_status = Cy_SCB_I2C_Init(I2C_MASTER_HW, &I2C_MASTER_config, &I2C_MASTER_context);
    if(i2c_init_status != CY_SCB_I2C_SUCCESS)
    {
        return I2C_FAILURE;
    }

    /* Init interrupt service routine and enable interrupt. */
    sysint_status = Cy_SysInt_Init(&I2C_MASTER_SCB_IRQ_cfg, &i2c_master_interrupt);
    if(sysint_status != CY_SYSINT_SUCCESS)
    {
        return I2C_FAILURE;
    }
    NVIC_EnableIRQ((IRQn_Type) I2C_MASTER_SCB_IRQ_cfg.intrSrc);

    /* Enable SCB I2C */
    Cy_SCB_I2C_Enable(I2C_MASTER_HW);

    return I2C_SUCCESS;
}

