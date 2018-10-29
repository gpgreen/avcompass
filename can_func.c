#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include "defs.h"
#include "globals.h"
#include "can_func.h"
#include "timer.h"
#include "register.h"

#include <canard_avr.h>         /* CAN backend driver for avr, distributed with libcanard */

/*
 * Some useful constants defined by the UAVCAN specification.
 * Data type signature values can be easily obtained with the script show_data_type_info.py
 */
#define UAVCAN_EQUIPMENT_AIR_DATA_RAW_DATA_TYPE_ID                  1027
#define UAVCAN_EQUIPMENT_AIR_DATA_RAW_DATA_TYPE_SIGNATURE           0xc77df38ba122f5da
#define UAVCAN_EQUIPMENT_AIR_DATA_RAW_MAX_SIZE                      (397 / 8) + 1

#define UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID                      1
#define UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_SIGNATURE               0x0b2a812620a11d40
#define UAVCAN_NODE_ID_ALLOCATION_RANDOM_TIMEOUT_RANGE_USEC         400000UL
#define UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC         600000UL

#define UAVCAN_NODE_STATUS_MESSAGE_SIZE                             7
#define UAVCAN_NODE_STATUS_DATA_TYPE_ID                             341
#define UAVCAN_NODE_STATUS_DATA_TYPE_SIGNATURE                      0x0f0868d0c1a7c6f1

#define UAVCAN_GET_NODE_INFO_RESPONSE_MAX_SIZE                      ((3015 + 7) / 8)
#define UAVCAN_GET_NODE_INFO_DATA_TYPE_SIGNATURE                    0xee468a8121c46a9e
#define UAVCAN_GET_NODE_INFO_DATA_TYPE_ID                           1

/*
 * Library instance.
 * In simple applications it makes sense to make it static, but it is not necessary.
 */
static CanardInstance canard;                       ///< The library instance
static uint8_t canard_memory_pool[1024];            ///< Arena for memory allocation, used by the library

/*
 * Variables used for dynamic node ID allocation.
 * RTFM at http://uavcan.org/Specification/6._Application_level_functions/#dynamic-node-id-allocation
 */
static uint64_t send_next_node_id_allocation_request_at;    ///< When the next node ID allocation request should be sent
static uint8_t node_id_allocation_unique_id_offset;         ///< Depends on the stage of the next request

int initializeCanard(uint32_t bus_speed)
{
    set_register(COMPASS_STATE_REG, COMPASS_STATE_SHIFT, UAVCAN_NODE_MODE_INITIALIZATION);
    set_register(UAVCAN_NODE_HEALTH_REG, UAVCAN_NODE_HEALTH_SHIFT, UAVCAN_NODE_HEALTH_OK);
    
    /* initializing the CAN backend driver */
    int err = canardAVRInit(bus_speed);
    if (err)
        return err;

    /* initialize the libcanard instance */
    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               NULL);

    /* get the id stored in registers */
    uint8_t nodeid = get_register(UAVCAN_NODE_ID_REG, UAVCAN_NODE_ID_SHIFT);
    if (nodeid != CANARD_BROADCAST_NODE_ID)
        canardSetLocalNodeID(&canard, nodeid);
    
    return 0;
}

    

static uint32_t getMonotonicTimestampUSec(void)
{
    return jiffie();
}


/**
 * Returns a pseudo random float in the range [0, 1].
 */
static float getRandomFloat(void)
{
    static bool initialized = false;
    if (!initialized)                   // This is not thread safe, but a race condition here is not harmful.
    {
        initialized = true;
        srand((uint32_t) time(NULL));
    }
    // coverity[dont_call]
    return (float)rand() / (float)RAND_MAX;
}


static void makeNodeStatusMessage(uint8_t buffer[UAVCAN_NODE_STATUS_MESSAGE_SIZE])
{
    memset(buffer, 0, UAVCAN_NODE_STATUS_MESSAGE_SIZE);

    static uint32_t started_at_sec = 0;
    if (started_at_sec == 0)
    {
        started_at_sec = (uint32_t)(getMonotonicTimestampUSec() / 1000000U);
    }

    const uint32_t uptime_sec = (uint32_t)((getMonotonicTimestampUSec() / 1000000U) - started_at_sec);

    /*
     * Here we're using the helper for demonstrational purposes; in this simple case it could be preferred to
     * encode the values manually.
     */
    canardEncodeScalar(buffer,  0, 32, &uptime_sec);
    uint8_t* addr = (uint8_t*)(&registers[UAVCAN_NODE_HEALTH_REG]) + UAVCAN_NODE_HEALTH_SHIFT;
    canardEncodeScalar(buffer, 32,  2, addr);
    addr = (uint8_t*)(&registers[COMPASS_STATE_REG]) + COMPASS_STATE_SHIFT;
    canardEncodeScalar(buffer, 34,  3, addr);
}


/*-----------------------------------------------------------------------*/
/**
 * This callback is invoked by the library when it detects beginning of a new transfer on the bus that can be received
 * by the local node.
 * If the callback returns true, the library will receive the transfer.
 * If the callback returns false, the library will ignore the transfer.
 * All transfers that are addressed to other nodes are always ignored.
 */
bool shouldAcceptTransfer(const CanardInstance* ins,
                          uint64_t* out_data_type_signature,
                          uint16_t data_type_id,
                          CanardTransferType transfer_type,
                          uint8_t source_node_id)
{
    (void)source_node_id;

    if (canardGetLocalNodeID(ins) == CANARD_BROADCAST_NODE_ID)
    {
        /*
         * If we're in the process of allocation of dynamic node ID, accept only relevant transfers.
         */
        if ((transfer_type == CanardTransferTypeBroadcast) &&
            (data_type_id == UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID))
        {
            *out_data_type_signature = UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_SIGNATURE;
            return true;
        }
    }
    else
    {
        if ((transfer_type == CanardTransferTypeRequest) &&
            (data_type_id == UAVCAN_GET_NODE_INFO_DATA_TYPE_ID))
        {
            *out_data_type_signature = UAVCAN_GET_NODE_INFO_DATA_TYPE_SIGNATURE;
            return true;
        }
    }

    return false;
}

/*-----------------------------------------------------------------------*/

void onTransferReceived(CanardInstance* ins,
                        CanardRxTransfer* transfer)
{
    /*
     * Dynamic node ID allocation protocol.
     * Taking this branch only if we don't have a node ID, ignoring otherwise.
     */
    if ((canardGetLocalNodeID(ins) == CANARD_BROADCAST_NODE_ID) &&
        (transfer->transfer_type == CanardTransferTypeBroadcast) &&
        (transfer->data_type_id == UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID))
    {
        // Rule C - updating the randomized time interval
        send_next_node_id_allocation_request_at =
            getMonotonicTimestampUSec() + UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC +
            (uint64_t)(getRandomFloat() * UAVCAN_NODE_ID_ALLOCATION_RANDOM_TIMEOUT_RANGE_USEC);

        if (transfer->source_node_id == CANARD_BROADCAST_NODE_ID)
        {
            uart_printf_P(PSTR("Allocation request from another allocatee\n"));
            node_id_allocation_unique_id_offset = 0;
            return;
        }

        // Copying the unique ID from the message
        static const uint8_t UniqueIDBitOffset = 8;
        uint8_t received_unique_id[UNIQUE_ID_LENGTH_BYTES];
        uint8_t received_unique_id_len = 0;
        for (; received_unique_id_len < (transfer->payload_len - (UniqueIDBitOffset / 8U)); received_unique_id_len++)
        {
            // BUGBUG: shouldn't have an assert but handle correctly
            CANARD_ASSERT(received_unique_id_len < UNIQUE_ID_LENGTH_BYTES);
            const uint8_t bit_offset = (uint8_t)(UniqueIDBitOffset + received_unique_id_len * 8U);
            (void) canardDecodeScalar(transfer, bit_offset, 8, false, &received_unique_id[received_unique_id_len]);
        }

        // Matching the received UID against the local one
        uint8_t* unique_id = (uint8_t*)&registers[UNIQUE_ID1_REG];
        if (memcmp(received_unique_id, unique_id, received_unique_id_len) != 0)
        {
            uart_printf_P(PSTR("Mismatching allocation response from %d:"), transfer->source_node_id);
            for (uint8_t i = 0; i < received_unique_id_len; i++)
            {
                uart_printf_P(PSTR(" %02x/%02x"), received_unique_id[i], unique_id[i]);
            }
            uart_printf("\n");
            node_id_allocation_unique_id_offset = 0;
            return;         // No match, return
        }

        if (received_unique_id_len < UNIQUE_ID_LENGTH_BYTES)
        {
            // The allocator has confirmed part of unique ID, switching to the next stage and updating the timeout.
            node_id_allocation_unique_id_offset = received_unique_id_len;
            send_next_node_id_allocation_request_at -= UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC;

            uart_printf_P(PSTR("Matching allocation response from %d offset %d\n"),
                   transfer->source_node_id, node_id_allocation_unique_id_offset);
        }
        else
        {
            // Allocation complete - copying the allocated node ID from the message
            uint8_t allocated_node_id = 0;
            (void) canardDecodeScalar(transfer, 0, 7, false, &allocated_node_id);
            CANARD_ASSERT(allocated_node_id <= 127);

            canardSetLocalNodeID(ins, allocated_node_id);
            uart_printf_P(PSTR("Node ID %d allocated by %d\n"), allocated_node_id, transfer->source_node_id);
        }
    }

    if ((transfer->transfer_type == CanardTransferTypeRequest) &&
        (transfer->data_type_id == UAVCAN_GET_NODE_INFO_DATA_TYPE_ID))
    {
        uart_printf_P(PSTR("GetNodeInfo request from %d\n"), transfer->source_node_id);

        uint8_t buffer[UAVCAN_GET_NODE_INFO_RESPONSE_MAX_SIZE];
        memset(buffer, 0, UAVCAN_GET_NODE_INFO_RESPONSE_MAX_SIZE);

        // NodeStatus
        makeNodeStatusMessage(buffer);

        // SoftwareVersion
        buffer[7] = APP_VERSION_MAJOR;
        buffer[8] = APP_VERSION_MINOR;

#if 0
        buffer[9] = 1;                          // Optional field flags, VCS commit is set
        uint32_t u32 = GIT_HASH;
        canardEncodeScalar(buffer, 80, 32, &u32);
#endif
        // Image CRC skipped

        // HardwareVersion
        // Major skipped
        // Minor skipped
        memcpy(&buffer[24], &registers[UNIQUE_ID1_REG], UNIQUE_ID_LENGTH_BYTES);
        // Certificate of authenticity skipped

        // Name
        const size_t name_len = strlen(APP_NODE_NAME);
        memcpy(&buffer[41], APP_NODE_NAME, name_len);

        const size_t total_size = 41 + name_len;

        /*
         * Transmitting; in this case we don't have to release the payload because it's empty anyway.
         */
        const int16_t resp_res = canardRequestOrRespond(ins,
                                                        transfer->source_node_id,
                                                        UAVCAN_GET_NODE_INFO_DATA_TYPE_SIGNATURE,
                                                        UAVCAN_GET_NODE_INFO_DATA_TYPE_ID,
                                                        &transfer->transfer_id,
                                                        transfer->priority,
                                                        CanardResponse,
                                                        &buffer[0],
                                                        (uint16_t)total_size);
        if (resp_res <= 0)
        {
            uart_printf_P(PSTR("Could not respond to GetNodeInfo; error %d\n"), resp_res);
        }
    }
}

/**
 * This function is called at 1 Hz rate from the main loop.
 */
void process1HzTasks(uint64_t timestamp_usec)
{
    /*
     * Purging transfers that are no longer transmitted. This will occasionally free up some memory.
     */
    canardCleanupStaleTransfers(&canard, timestamp_usec);

    /*
     * Printing the memory usage statistics.
     */
    {
        const CanardPoolAllocatorStatistics stats = canardGetPoolAllocatorStatistics(&canard);
        const uint16_t peak_percent = (uint16_t)(100U * stats.peak_usage_blocks / stats.capacity_blocks);

        uart_printf_P(PSTR("Memory pool stats: capacity %u blocks, usage %u blocks, peak usage %u blocks (%u%%)\n"),
               stats.capacity_blocks, stats.current_usage_blocks, stats.peak_usage_blocks, peak_percent);

        /*
         * The recommended way to establish the minimal size of the memory pool is to stress-test the application and
         * record the worst case memory usage.
         */
        if (peak_percent > 70)
        {
            uart_printf_P(PSTR("WARNING: ENLARGE MEMORY POOL\n"));
        }
    }

    /*
     * Transmitting the node status message periodically.
     */
    {
        uint8_t buffer[UAVCAN_NODE_STATUS_MESSAGE_SIZE];
        makeNodeStatusMessage(buffer);

        static uint8_t transfer_id;  // Note that the transfer ID variable MUST BE STATIC (or heap-allocated)!

        const int16_t bc_res = canardBroadcast(&canard,
                                               UAVCAN_NODE_STATUS_DATA_TYPE_SIGNATURE,
                                               UAVCAN_NODE_STATUS_DATA_TYPE_ID,
                                               &transfer_id,
                                               CANARD_TRANSFER_PRIORITY_LOWEST,
                                               buffer,
                                               UAVCAN_NODE_STATUS_MESSAGE_SIZE);
        if (bc_res <= 0)
        {
            uart_printf_P(PSTR("Could not broadcast node status; error %d\n"), bc_res);
        }
    }

    set_register(COMPASS_STATE_REG, COMPASS_STATE_SHIFT, UAVCAN_NODE_MODE_OPERATIONAL);
}


/**
 * Transmits all frames from the TX queue, receives up to one frame.
 */
void processTxRxOnce()
{
    static int tx_on = 0;
    
    // Transmitting
    for (const CanardCANFrame* txf = NULL; (txf = canardPeekTxQueue(&canard)) != NULL;)
    {
        const int16_t tx_res = canardAVRTransmit(txf);
        if (tx_res < 0)         // Failure - drop the frame and report
        {
            canardPopTxQueue(&canard);
            uart_printf_P(PSTR("Transmit error %d, frame dropped\n"), tx_res);
        }
        else if (tx_res > 0)    // Success - just drop the frame
        {
            canardPopTxQueue(&canard);
            if (tx_on)
                led4_off();
            else
                led4_on();
            tx_on = tx_on ? 0 : 1;
        }
        else                    // Timeout - just exit and try again later
        {
            break;
        }
    }

    static int rx_on = 0;
    
    // Receiving
    CanardCANFrame rx_frame;
    const uint64_t timestamp = getMonotonicTimestampUSec();
    const int16_t rx_res = canardAVRReceive(&rx_frame);
    if (rx_res < 0)             // Failure - report
    {
        uart_printf_P(PSTR("Receive error %d\n"), rx_res);
    }
    else if (rx_res > 0)        // Success - process the frame
    {
        canardHandleRxFrame(&canard, &rx_frame, timestamp);
        if (rx_on)
            led5_off();
        else
            led5_on();
        rx_on = rx_on ? 0 : 1;
    }
    else
    {
        ;                       // Timeout - nothing to do
    }
}

void processNodeId()
{
        /*
     * Performing the dynamic node ID allocation procedure.
     */
    static const uint8_t PreferredNodeID = CANARD_BROADCAST_NODE_ID;    ///< This can be made configurable, obviously

    node_id_allocation_unique_id_offset = 0;

    uint8_t node_id_allocation_transfer_id = 0;

    while (canardGetLocalNodeID(&canard) == CANARD_BROADCAST_NODE_ID)
    {
        uart_printf_P(PSTR("Waiting for dynamic node ID allocation...\n"));

        send_next_node_id_allocation_request_at =
            getMonotonicTimestampUSec() + UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC +
            (uint64_t)(getRandomFloat() * UAVCAN_NODE_ID_ALLOCATION_RANDOM_TIMEOUT_RANGE_USEC);

        while ((getMonotonicTimestampUSec() < send_next_node_id_allocation_request_at) &&
               (canardGetLocalNodeID(&canard) == CANARD_BROADCAST_NODE_ID))
        {
            processTxRxOnce();
        }

        if (canardGetLocalNodeID(&canard) != CANARD_BROADCAST_NODE_ID)
        {
            break;
        }

        // Structure of the request is documented in the DSDL definition
        // See http://uavcan.org/Specification/6._Application_level_functions/#dynamic-node-id-allocation
        uint8_t allocation_request[CANARD_CAN_FRAME_MAX_DATA_LEN - 1];
        allocation_request[0] = (uint8_t)(PreferredNodeID << 1U);

        if (node_id_allocation_unique_id_offset == 0)
        {
            allocation_request[0] |= 1;     // First part of unique ID
        }

        static const uint8_t MaxLenOfUniqueIDInRequest = 6;
        uint8_t uid_size = (uint8_t)(UNIQUE_ID_LENGTH_BYTES - node_id_allocation_unique_id_offset);
        if (uid_size > MaxLenOfUniqueIDInRequest)
        {
            uid_size = MaxLenOfUniqueIDInRequest;
        }

        // Paranoia time
        CANARD_ASSERT(node_id_allocation_unique_id_offset < UNIQUE_ID_LENGTH_BYTES);
        CANARD_ASSERT(uid_size <= MaxLenOfUniqueIDInRequest);
        CANARD_ASSERT(uid_size > 0);
        CANARD_ASSERT((uid_size + node_id_allocation_unique_id_offset) <= UNIQUE_ID_LENGTH_BYTES);

        uint8_t* unique_id = (uint8_t*)&registers[UNIQUE_ID1_REG];
        memmove(&allocation_request[1], &unique_id[node_id_allocation_unique_id_offset], uid_size);

        // Broadcasting the request
        const int16_t bcast_res = canardBroadcast(&canard,
                                                  UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_SIGNATURE,
                                                  UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID,
                                                  &node_id_allocation_transfer_id,
                                                  CANARD_TRANSFER_PRIORITY_LOW,
                                                  &allocation_request[0],
                                                  (uint16_t) (uid_size + 1));
        if (bcast_res < 0)
        {
            uart_printf_P(PSTR("Could not broadcast dynamic node ID allocation request; error %d\n"),
                          bcast_res);
        }

        // Preparing for timeout; if response is received, this value will be updated from the callback.
        node_id_allocation_unique_id_offset = 0;
    }

    uart_printf_P(PSTR("Dynamic node ID allocation complete [%d]\n"), canardGetLocalNodeID(&canard));

}

void sendRawMagData(void)
{
    uint8_t buffer[UAVCAN_EQUIPMENT_AIR_DATA_RAW_MAX_SIZE];
    memset(buffer, 0, UAVCAN_EQUIPMENT_AIR_DATA_RAW_MAX_SIZE);
    // skip flags

    // pressure data
    //canardEncodeScalar(buffer, 8, 32, &g_bmp085_data[0].press);
    //canardEncodeScalar(buffer, 8+32, 32, &g_bmp085_data[1].press);

    // temperature data
    //uint16_t converted = canardConvertNativeFloatToFloat16(g_bmp085_data[0].temp);
    //canardEncodeScalar(buffer, 8+32+32, 16, &converted);
    //converted = canardConvertNativeFloatToFloat16(g_bmp085_data[1].temp);
    //canardEncodeScalar(buffer, 8+32+32+16, 16, &converted);

    // skip static air temperature
    // skip pitot temperature
    // skip covariance

    static uint8_t transfer_id;  // Note that the transfer ID variable MUST BE STATIC (or heap-allocated)!

    const int16_t bc_res = canardBroadcast(&canard,
                                           UAVCAN_EQUIPMENT_AIR_DATA_RAW_DATA_TYPE_SIGNATURE,
                                           UAVCAN_EQUIPMENT_AIR_DATA_RAW_DATA_TYPE_ID,
                                           &transfer_id,
                                           CANARD_TRANSFER_PRIORITY_HIGH,
                                           buffer,
                                           UAVCAN_EQUIPMENT_AIR_DATA_RAW_MAX_SIZE);
    if (bc_res <= 0)
    {
        uart_printf_P(PSTR("Could not broadcast raw air data; error %d\n"), bc_res);
    }
}

