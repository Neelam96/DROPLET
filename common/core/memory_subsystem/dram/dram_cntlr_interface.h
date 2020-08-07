#ifndef __DRAM_CNTLR_INTERFACE_H
#define __DRAM_CNTLR_INTERFACE_H

#include "subsecond_time.h"
#include "hit_where.h"
#include "shmem_msg.h"
#include "fixed_types.h"
#include <iostream>
#include <fstream>
#include <deque>
#include "stats.h"

#include "boost/tuple/tuple.hpp"
using namespace std;
/*....................................CHANGE!!!........................................................................
For CC, PR: The trainPrefetcher works on a structure PREFETCH request from the processor side.
For BFS: trainPrefetcher works multiple times. 1) OffsetPrefetch upon getting a structure DEMAND request
                                               2) Structure prefetch upon getting the offset prefetch
                                               3) Poperty prefetch upon getting structure prefetch(this is like CC, PR)
//......................................................................................................................*/

//..................CHANGE!!!......................
#define MAX_PREFETCH_LIST_SIZE 128
#define DRAM_PREFETCH_INTERVAL SubsecondTime::NSfromFloat(0.5)
//..................................................
class MemoryManagerBase;
class ShmemPerfModel;
class ShmemPerf;

class DramCntlrInterface
{
   protected:
      MemoryManagerBase* m_memory_manager;
      ShmemPerfModel* m_shmem_perf_model;      
      UInt32 m_cache_block_size;      
           
      UInt32 getCacheBlockSize() { return m_cache_block_size; }
      MemoryManagerBase* getMemoryManager() { return m_memory_manager; }
      ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }   

      //................................CHANGE!!!....................................
      // All this stuff for prefetching from memory-side       
      void trainPrefetcherForProperty(IntPtr address, core_id_t intraTileRequester, SubsecondTime t_now);

      std::vector<IntPtr> calculatePropertyPrefetchAddress(IntPtr address);    
      
      class prefInfo{
            public:
              IntPtr m_prefetch_address;
              SubsecondTime m_prefetch_time;
              prefInfo(IntPtr address, SubsecondTime time):
              m_prefetch_address(address),
              m_prefetch_time(time)
              {}
      };

      std::vector<std::deque<prefInfo*>> m_prefetch_list;       
      void doInitialCheck(PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
      void doMemorySidePrefetch(PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
      UInt64 m_sent_prefetches;
      void sendMiss(PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
      bool isAllEmpty();    

      class book{
            public:
              core_id_t m_core_id;
              UInt32 m_num_pref;
              book(core_id_t core_id, UInt32 num_pref):
              m_core_id(core_id),
              m_num_pref(num_pref){}
      };

      // helper structure to choose a prefetch destination. This structure is updated 
      // when new prefetch addresses are calculated, prefetch is sent to the tag for tag check,
      // and also when prefetches are popped from the prefetch list
      std::deque<book*> priority_list;       
      void updatePriorityList(core_id_t core_id, UInt32 popped_pref);
      //.............................................................................

   public:
      typedef enum
      {
         READ = 0,
         WRITE,
         PREFETCH, // I added a new access type 
         NUM_ACCESS_TYPES
      } access_t;     
      
      DramCntlrInterface(MemoryManagerBase* memory_manager, ShmemPerfModel* shmem_perf_model, UInt32 cache_block_size)
         : m_memory_manager(memory_manager)
         , m_shmem_perf_model(shmem_perf_model)
         , m_cache_block_size(cache_block_size)                            
      {
            //.............CHANGE!!!...........................
            UInt32 shared_cores = 4; 
            //m_prefetch_next.resize(shared_cores);   
            m_prefetch_list.resize(shared_cores);  
            //m_temp_prefetch_list.resize(shared_cores);        
            m_sent_prefetches = 0;
            registerStatsMetric("dram-cntlr-interface", 0, "memory-prefetches-sent", &m_sent_prefetches);
            //...................................................
      }
      virtual ~DramCntlrInterface() {}

      virtual boost::tuple<SubsecondTime, HitWhere::where_t> getDataFromDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now, ShmemPerf *perf, bool isPrefetch=false) = 0;
      virtual boost::tuple<SubsecondTime, HitWhere::where_t> putDataToDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now) = 0;

      void handleMsgFromTagDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);     
};

#endif // __DRAM_CNTLR_INTERFACE_H
