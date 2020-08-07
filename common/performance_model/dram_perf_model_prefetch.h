#ifndef __DRAM_PERF_MODEL_PREFETCH_H__
#define __DRAM_PERF_MODEL_PREFETCH_H__

#include "dram_perf_model.h"
#include "queue_model.h"
#include "fixed_types.h"
#include "subsecond_time.h"
#include "dram_cntlr_interface.h"

class DramPerfModelPrefetch : public DramPerfModel
{
   private:
      QueueModel* m_queue_model_demand;
      QueueModel* m_queue_model_prefetch;
      SubsecondTime m_dram_access_cost;
      ComponentBandwidth m_dram_bandwidth;
      bool m_shared_demandpref;

      SubsecondTime m_total_demand_queueing_delay;
      SubsecondTime m_total_prefetch_queueing_delay;
      SubsecondTime m_total_access_latency;

   public:
      DramPerfModelPrefetch(core_id_t core_id,
            UInt32 cache_block_size);

      ~DramPerfModelPrefetch();

      SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf);
};

#endif /* __DRAM_PERF_MODEL_READWRITE_H__ */
