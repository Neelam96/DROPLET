#ifndef __DROPLETL1_H
#define __DROPLETL1_H

#include "prefetcher.h"
#include <fstream>

// THINGS NEEDED FOR DROPLET
// address identification capability 
// if the address is a structure demand, start streaming 
// if the address is a structure prefetch, trainForProperty address 
class DROPLETL1 : public Prefetcher
{
   public:
      DROPLETL1(String configName, core_id_t core_id, UInt32 shared_cores);
      virtual std::vector<IntPtr> getNextAddress(IntPtr current_address, core_id_t core_id);            
      ~DROPLETL1();
   private:     
      const core_id_t core_id;
      const UInt32 shared_cores;      
      const UInt32 n_streams;   
      const bool streams_per_core;   
      const UInt32 prefetch_degree; 
      const UInt32 prefetch_distance; 
      const bool stop_at_page;
      const UInt32 stream_confirm_distance;
      const UInt32 init_distance;
      int next_stream_to_allocate; // next stream to allocate   
      
      std::vector<std::vector<IntPtr> > m_tracking_address;
      std::vector<std::vector<IntPtr> > m_monitor_startAddress;
      std::vector<std::vector<IntPtr> > m_monitor_endAddress; 
      std::vector<int> m_confirm; // book-keeping of which stream remains to be confirmed and stream confirm step
      

      std::vector<IntPtr> getPrefetchAddress(UInt32 selected_stream, std::vector<IntPtr> &prev_monitor_startAddress,
                                                         std::vector<IntPtr> &prev_monitor_endAddress, IntPtr current_address);  
      std::vector<IntPtr> trainPrefetcherForProperty(IntPtr address);  
        
      void printCurrentState(IntPtr current_address, std::vector<IntPtr> &prev_tracking_address,
                                         std::vector<IntPtr> &prev_monitor_startAddress,
                                         std::vector<IntPtr> &prev_monitor_endAddress);          
};

#endif // __DROPLETL1_H