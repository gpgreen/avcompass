#ifndef CAN_FUNC_H_
#define CAN_FUNC_H_

#include "defs.h"
#include "globals.h"
#include <stdint.h>

// we want to define CANARD_ASSERT before including canard.h
// use COMPASS_ASSERT from globals.h
# define CANARD_ASSERT  COMPASS_ASSERT

#include <canard.h>

extern CanardInstance* instance;

extern int initializeCanard(void);

extern bool shouldAcceptTransfer(const CanardInstance* ins,
                                 uint64_t* out_data_type_signature,
                                 uint16_t data_type_id,
                                 CanardTransferType transfer_type,
                                 uint8_t source_node_id);

extern void onTransferReceived(CanardInstance* ins, CanardRxTransfer* transfer);

extern void process1HzTasks(uint64_t timestamp_usec);

extern void processTxRxOnce(void);

extern void processNodeId(void);

extern void sendRawMagData(void);

#endif
