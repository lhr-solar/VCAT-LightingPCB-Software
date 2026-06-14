/**
 ******************************************************************************
 * @file    can_app.h
 * @brief   Application CAN layer: Lighting_Command RX and Lighting_*_Status TX.
 ******************************************************************************
 */
#ifndef CAN_APP_H
#define CAN_APP_H

/**
 * @brief Configure the RX filter (accept only Lighting_Command 0x660), start
 *        CAN1 and enable the RX FIFO0 pending notification. Call after bsp_init.
 */
void can_app_init(void);

/**
 * @brief Broadcast this board's Lighting_*_Status frame. Call at the status
 *        cadence (STATUS_TX_PERIOD_MS).
 */
void can_status_send(void);

#endif /* CAN_APP_H */
