#ifndef __ADDRESS_PREFETCHER_H
#define __ADDRESS_PREFETCHER_H

#include "prefetcher.h"
#include <fstream>
#include "memory_manager_base.h"
#include "magic_server.h"
#include "simulator.h"

class AddressPrefetcher : public Prefetcher
{     
   private:
      const core_id_t core_id;
      const UInt32 shared_cores;   
         
      bool directed;
      bool addrRangeSet;     
      bool propRangeSet; 
      IntPtr startOutOffset;
      IntPtr endOutOffset;   
      IntPtr startInOffset;
      IntPtr endInOffset;     
      IntPtr startOutNeigh;
      IntPtr endOutNeigh;
      IntPtr startInNeigh;
      IntPtr endInNeigh;  
      void getStructureAddrRange();   
      bool isStructureCacheline(IntPtr ca_address, UInt32 cache_block_size);
      bool isOffsetCacheline(IntPtr ca_address);

      class offsetPrefetch{
            public:
              IntPtr aligned_add;
              IntPtr offset;
              offsetPrefetch(IntPtr _address, IntPtr _offset):
              aligned_add(_address),
              offset(_offset)
              {}
      };
      std::deque<offsetPrefetch*> offset_list;
      
      // returns true if we added to the prefetch queue 
      bool checkPresenceAndPush(std::vector<IntPtr>& addresses, IntPtr aligned_add);

   public:
        AddressPrefetcher(String configName, core_id_t _core_id, UInt32 _shared_cores)
        :core_id(_core_id), shared_cores(_shared_cores) {
                directed = false; addrRangeSet = false; propRangeSet = false;
                startOutOffset = 0; endOutOffset = 0; startInOffset = 0; endInOffset = 0;
                startOutNeigh = 0; endOutNeigh = 0; startInNeigh = 0; endInNeigh = 0;  
        }

        ~AddressPrefetcher(){}     

        virtual std::vector<IntPtr> getNextAddress(IntPtr address, core_id_t core_id);  
        virtual void updateOffsetListDuringPop(IntPtr address, core_id_t core_id);            
};
#endif // __ADDRESS_PREFETCHER_H