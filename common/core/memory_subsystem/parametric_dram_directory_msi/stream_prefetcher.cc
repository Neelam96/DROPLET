#include "stream_prefetcher.h"
#include "simulator.h"
#include "config.hpp"
#include <iostream>
#include <cstdlib>
using namespace std;
const IntPtr PAGE_SIZE = 4096;
const IntPtr PAGE_MASK = ~(PAGE_SIZE-1);
const IntPtr CACHE_BLOCK_SIZE = 64; // in bytes

/* Prefetch algorithm implemented from the paper:
Feedback Directed Prefetch, HPCA 2007 
All addresses are cache aligned addresses 
*/ 

StreamPrefetcher::StreamPrefetcher(String configName, core_id_t _core_id, UInt32 _shared_cores)
   : core_id(_core_id)
   , shared_cores(_shared_cores)
   , n_streams(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/stream/streams", core_id))
   , streams_per_core(Sim()->getCfg()->getBoolArray("perf_model/" + configName + "/prefetcher/stream/streams_per_core", core_id))
   , prefetch_degree(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/stream/prefetch_degree", core_id))
   , prefetch_distance(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/stream/prefetch_distance", core_id))
   , stop_at_page(Sim()->getCfg()->getBoolArray("perf_model/" + configName + "/prefetcher/stream/stop_at_page_boundary", core_id))   
   , init_distance(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/stream/init_distance", core_id)) 
   , next_stream_to_allocate(-1)   
   , m_tracking_address(streams_per_core ? shared_cores : 1)
   , m_monitor_startAddress(streams_per_core ? shared_cores : 1)
   , m_monitor_endAddress(streams_per_core ? shared_cores : 1)     
{     
   for(UInt32 idx = 0; idx < (streams_per_core ? shared_cores : 1); ++idx){
       m_tracking_address.at(idx).resize(n_streams); 
       m_monitor_startAddress.at(idx).resize(n_streams);
       m_monitor_endAddress.at(idx).resize(n_streams);
   }      
}

StreamPrefetcher::~StreamPrefetcher(){    
}

// This function returns prefetch addresses and updates the memory monitor region
std::vector<IntPtr> StreamPrefetcher::getPrefetchAddress(UInt32 selected_stream, std::vector<IntPtr> &prev_monitor_startAddress,
                                                         std::vector<IntPtr> &prev_monitor_endAddress, IntPtr current_address){
    std::vector<IntPtr> addresses; // starting addresses of cache blocks 
    
    // prefetch N lines along that stream but stop at page boundary    
    for(unsigned int i = 0; i < prefetch_degree; ++i){
           IntPtr prefetch_address = prev_monitor_endAddress[selected_stream] + (i + 1)* CACHE_BLOCK_SIZE;           
           if (!stop_at_page || ((prefetch_address & PAGE_MASK) == (current_address & PAGE_MASK)))
                addresses.push_back(prefetch_address);
    }      
    //if(addresses.empty()) cout << "No prefetch req sent    ";
    // uppdate the monitor memory region (no of cache blocks between two pointers last cacheline inclusive) 
    // Goal is that monitor memory region size <= prefetch distance. Do this update only if the addresses is 
    // not empty and increment by number of lines prefetched 
       
    if(!addresses.empty()){        
        IntPtr monitor_region_size = (prev_monitor_endAddress[selected_stream] - prev_monitor_startAddress[selected_stream])/CACHE_BLOCK_SIZE + 1;
        if(monitor_region_size < prefetch_distance){
            prev_monitor_endAddress[selected_stream] += addresses.size()*CACHE_BLOCK_SIZE;
         
            // check if the monitor region size has exceeded the prefetch distance
            // If yes, increment the start address accordingly
            monitor_region_size = (prev_monitor_endAddress[selected_stream] - prev_monitor_startAddress[selected_stream])/CACHE_BLOCK_SIZE + 1;
            if(monitor_region_size > prefetch_distance)
                prev_monitor_startAddress[selected_stream] += (monitor_region_size-prefetch_distance)*CACHE_BLOCK_SIZE;
        }else{
            prev_monitor_startAddress[selected_stream] += addresses.size() * CACHE_BLOCK_SIZE;
            prev_monitor_endAddress[selected_stream] += addresses.size() * CACHE_BLOCK_SIZE;
        }
    }
    return addresses;
}

std::vector<IntPtr>
StreamPrefetcher::getNextAddress(IntPtr current_address, core_id_t _core_id)
{  
   std::vector<IntPtr> addresses;
   
   std::vector<IntPtr> &prev_tracking_address = m_tracking_address.at(streams_per_core ? _core_id - core_id : 0);
   std::vector<IntPtr> &prev_monitor_startAddress = m_monitor_startAddress.at(streams_per_core ? _core_id - core_id : 0);
   std::vector<IntPtr> &prev_monitor_endAddress = m_monitor_endAddress.at(streams_per_core ? _core_id - core_id : 0);
   
   // find which stream the address belongs to. If a match is found, we prefetch if the match is in the memory 
   // monitor region 
   UInt32 selected_stream = 0;       
   bool found = false;  
   for(UInt32 i=0; i < n_streams; ++i){
       if(prev_tracking_address[i] != 0){
           if(current_address >= prev_monitor_startAddress[i] && current_address <= prev_monitor_endAddress[i]){
               selected_stream = i;               
               found = true;
               break;
           }
       }
   }

   if(found) {
       addresses = getPrefetchAddress(selected_stream, prev_monitor_startAddress, prev_monitor_endAddress, current_address);  
   }else{
       //find if it is part of a stream but the stream is not streaming 
       for(UInt32 i=0; i < n_streams; ++i){
           if(prev_tracking_address[i] != 0){
               if(current_address >= prev_tracking_address[i] && current_address < prev_monitor_startAddress[i]){
                   found = true;                  
                   break;
               }
           }
       }
   }
   // if(found) we do nothing, we do not prefetch, so push nothing to addresses  
   if(!found){    
       // We alloate a new stream and start prefetching, skip "init_distance" prefetches        
       next_stream_to_allocate = (next_stream_to_allocate + 1) % n_streams; 
       prev_tracking_address[next_stream_to_allocate] = current_address;
       prev_monitor_startAddress[next_stream_to_allocate] = current_address;  
       prev_monitor_endAddress[next_stream_to_allocate] = current_address + init_distance * CACHE_BLOCK_SIZE;       
       addresses = getPrefetchAddress(next_stream_to_allocate, prev_monitor_startAddress, prev_monitor_endAddress, current_address);        
   }
   return addresses; 
}

void StreamPrefetcher::printCurrentState(IntPtr current_address, std::vector<IntPtr> &prev_tracking_address,
                                         std::vector<IntPtr> &prev_monitor_startAddress,
                                         std::vector<IntPtr> &prev_monitor_endAddress){
    cout << "Current address is " << current_address << endl;

    cout << "Prev_tracking_address      ";
    for(UInt32 y = 0; y < prev_tracking_address.size(); ++y)
         cout << prev_tracking_address[y] << "  ";
    cout << endl;

    cout << "Prev_monitor_startAddress   ";
    for(UInt32 y = 0; y < prev_monitor_startAddress.size(); ++y)
         cout << prev_monitor_startAddress[y] << "  ";
    cout << endl;

    cout << "Prev_monitor_endAddress     ";
    for(UInt32 y = 0; y < prev_monitor_endAddress.size(); ++y)
         cout << prev_monitor_endAddress[y] << "  ";
    cout << endl << endl;     
}