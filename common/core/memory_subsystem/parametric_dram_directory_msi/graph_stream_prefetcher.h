#ifndef __GRAPH_STREAM_PREFETCHER_H
#define __GRAPH_STREAM_PREFETCHER_H

#include "prefetcher.h"
#include <fstream>

class GraphStreamPrefetcher : public Prefetcher
{
   public:
      GraphStreamPrefetcher(String configName, core_id_t core_id, UInt32 shared_cores);
      virtual std::vector<IntPtr> getNextAddress(IntPtr current_address, core_id_t core_id);                
      ~GraphStreamPrefetcher();
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
      int next_stream_to_allocate;
      int next_stream_to_confirm;      
      int stream_confirm_step; // takes values 0, 1, 2
      std::vector<std::vector<IntPtr> > m_tracking_address;
      std::vector<std::vector<IntPtr> > m_monitor_startAddress;
      std::vector<std::vector<IntPtr> > m_monitor_endAddress; 

      std::vector<IntPtr> getPrefetchAddress(UInt32 selected_stream, std::vector<IntPtr> &prev_monitor_startAddress,
                                                         std::vector<IntPtr> &prev_monitor_endAddress, IntPtr current_address);  
        
      void printCurrentState(IntPtr current_address, std::vector<IntPtr> &prev_tracking_address,
                                         std::vector<IntPtr> &prev_monitor_startAddress,
                                         std::vector<IntPtr> &prev_monitor_endAddress);          
};

#endif // __GRAPH_STREAM_PREFETCHER_H
