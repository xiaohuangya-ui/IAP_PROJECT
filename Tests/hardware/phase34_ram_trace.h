#ifndef PHASE34_RAM_TRACE_H
#define PHASE34_RAM_TRACE_H
#include <stdint.h>
typedef struct {
    uint32_t magic,abi,seq,init_calls,poll_calls,service_calls,start_calls,start_result;
    uint32_t context_bits,tick_ms,core_clock_hz,stats_valid;
    uint32_t rx_collected,rx_consumed,tx_accepted,tx_tc_completed;
    uint32_t discarded,errors_total,error_bits_seen,pending_errors;
    uint32_t recoveries,queued,rx_ready,tx_busy;
} TraceSnapshot;
extern volatile TraceSnapshot g_phase34_ram;
#endif
