#include "dram_perf_model_prefetch.h"
#include "simulator.h"
#include "config.h"
#include "config.hpp"
#include "stats.h"
#include "shmem_perf.h"

DramPerfModelPrefetch::DramPerfModelPrefetch(core_id_t core_id,
      UInt32 cache_block_size):
   DramPerfModel(core_id, cache_block_size),
   m_queue_model_demand(NULL),
   m_queue_model_prefetch(NULL),
   m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")), // Convert bytes to bits
   m_shared_demandpref(Sim()->getCfg()->getBool("perf_model/dram/prefetch/shared")),
   m_total_demand_queueing_delay(SubsecondTime::Zero()),
   m_total_prefetch_queueing_delay(SubsecondTime::Zero()),
   m_total_access_latency(SubsecondTime::Zero())
{
   m_dram_access_cost = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/latency"))); // Operate in fs for higher precision before converting to uint64_t/SubsecondTime

   if (Sim()->getCfg()->getBool("perf_model/dram/queue_model/enabled"))
   {
      m_queue_model_demand = QueueModel::create("dram-queue-read", core_id, Sim()->getCfg()->getString("perf_model/dram/queue_model/type"),
                                              m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
      m_queue_model_prefetch = QueueModel::create("dram-queue-write", core_id, Sim()->getCfg()->getString("perf_model/dram/queue_model/type"),
                                               m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
   }

   registerStatsMetric("dram", core_id, "total-access-latency", &m_total_access_latency);
   registerStatsMetric("dram", core_id, "total-demand-queueing-delay", &m_total_demand_queueing_delay);
   registerStatsMetric("dram", core_id, "total-prefetch-queueing-delay", &m_total_prefetch_queueing_delay);
}

DramPerfModelPrefetch::~DramPerfModelPrefetch()
{
   if (m_queue_model_demand)
   {
      delete m_queue_model_demand;
      m_queue_model_demand = NULL;
      delete m_queue_model_prefetch;
      m_queue_model_prefetch = NULL;
   }
}

SubsecondTime
DramPerfModelPrefetch::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
   // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bits per clock cycle'
   if ((!m_enabled) ||
         (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
   {
      return SubsecondTime::Zero();
   }

   SubsecondTime processing_time = m_dram_bandwidth.getRoundedLatency(8 * pkt_size); // bytes to bits

   // Compute Queue Delay
   SubsecondTime queue_delay;
   if (m_queue_model_demand)
   {
      if (!(access_type == DramCntlrInterface::PREFETCH))
      {
         queue_delay = m_queue_model_demand->computeQueueDelay(pkt_time, processing_time, requester);
         if (m_shared_demandpref)
         {
            // Shared demand prefetch bandwidth, but demand requests are prioritized over prefetches.
            // With fluffy time, where we can't delay a prefetch because of an earlier (in simulated time) demand
            // that was simulated later (in wallclock time), we model this in the following way:
            // - demands are only delayed by other demands (through m_queue_model_demand), this assumes *all* prefetches
            //   can be moved out of the way if needed.
            // - prefetches see contention by both demands and other prefetches, i.e., m_queue_model_prefetch
            //   is updated on both demand and prefetch.
            m_queue_model_prefetch->computeQueueDelay(pkt_time, processing_time, requester);
         }
      }
      else
         queue_delay = m_queue_model_prefetch->computeQueueDelay(pkt_time, processing_time, requester);
   }
   else
   {
      queue_delay = SubsecondTime::Zero();
   }

   SubsecondTime access_latency = queue_delay + processing_time + m_dram_access_cost;


   perf->updateTime(pkt_time);
   perf->updateTime(pkt_time + queue_delay, ShmemPerf::DRAM_QUEUE);
   perf->updateTime(pkt_time + queue_delay + processing_time, ShmemPerf::DRAM_BUS);
   perf->updateTime(pkt_time + queue_delay + processing_time + m_dram_access_cost, ShmemPerf::DRAM_DEVICE);

   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += access_latency;
   if (!(access_type == DramCntlrInterface::PREFETCH))
      m_total_demand_queueing_delay += queue_delay;
   else
      m_total_prefetch_queueing_delay += queue_delay;

   return access_latency;
}
