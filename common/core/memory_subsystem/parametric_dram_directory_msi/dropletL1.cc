#include "dropletL1.h"
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
This conservative scheme takes the first 3 misses to be trained before starting to prefetch
*/ 
DROPLETL1::DROPLETL1(String configName, core_id_t _core_id, UInt32 _shared_cores)
   : core_id(_core_id)
   , shared_cores(_shared_cores)
   , n_streams(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/baseline_stream/streams", core_id))
   , streams_per_core(Sim()->getCfg()->getBoolArray("perf_model/" + configName + "/prefetcher/baseline_stream/streams_per_core", core_id))
   , prefetch_degree(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/baseline_stream/prefetch_degree", core_id))
   , prefetch_distance(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/baseline_stream/prefetch_distance", core_id))
   , stop_at_page(Sim()->getCfg()->getBoolArray("perf_model/" + configName + "/prefetcher/baseline_stream/stop_at_page_boundary", core_id))
   , stream_confirm_distance(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/baseline_stream/stream_confirm_distance", core_id)) 
   , init_distance(Sim()->getCfg()->getIntArray("perf_model/" + configName + "/prefetcher/baseline_stream/init_distance", core_id))    
   , next_stream_to_allocate(-1)   
   , m_tracking_address(streams_per_core ? shared_cores : 1)
   , m_monitor_startAddress(streams_per_core ? shared_cores : 1)
   , m_monitor_endAddress(streams_per_core ? shared_cores : 1)       
{     
   for(UInt32 idx = 0; idx < (streams_per_core ? shared_cores : 1); ++idx){
       m_tracking_address.at(idx).resize(n_streams, INVALID_ADDRESS); 
       m_monitor_startAddress.at(idx).resize(n_streams, INVALID_ADDRESS);
       m_monitor_endAddress.at(idx).resize(n_streams, INVALID_ADDRESS);       
   } 

   m_confirm.resize(n_streams, -1);     
}

DROPLETL1::~DROPLETL1(){    
}

// This function returns prefetch addresses and updates the memory monitor region
std::vector<IntPtr> DROPLETL1::getPrefetchAddress(UInt32 selected_stream, std::vector<IntPtr> &prev_monitor_startAddress,
                                                         std::vector<IntPtr> &prev_monitor_endAddress, IntPtr current_address){
    std::vector<IntPtr> addresses; // starting addresses of cache blocks 
    
    // prefetch N lines along that stream but stop at page boundary    
    for(unsigned int i = 0; i < prefetch_degree; ++i){
           IntPtr prefetch_address = prev_monitor_endAddress[selected_stream] + (i + 1)* CACHE_BLOCK_SIZE;           
           if (!stop_at_page || ((prefetch_address & PAGE_MASK) == (current_address & PAGE_MASK)))
                addresses.push_back(prefetch_address);
    }      
    
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
DROPLETL1::getNextAddress(IntPtr current_address, core_id_t _core_id)
{     
   std::vector<IntPtr> addresses;
   
   std::vector<IntPtr> &prev_tracking_address = m_tracking_address.at(streams_per_core ? _core_id - core_id : 0);
   std::vector<IntPtr> &prev_monitor_startAddress = m_monitor_startAddress.at(streams_per_core ? _core_id - core_id : 0);
   std::vector<IntPtr> &prev_monitor_endAddress = m_monitor_endAddress.at(streams_per_core ? _core_id - core_id : 0);
   
   // find which stream the address belongs to. If a match is found, we prefetch if the match is in the memory monitor region 
   UInt32 selected_stream = 0;       
   bool found = false;  
   for(UInt32 i=0; i < n_streams; ++i){
       bool ready_to_check = (prev_tracking_address[i] != INVALID_ADDRESS) && (prev_monitor_startAddress[i] != INVALID_ADDRESS) &&
                             (prev_monitor_endAddress[i] != INVALID_ADDRESS);
       if(ready_to_check){           
           if(current_address >= prev_monitor_startAddress[i] && current_address <= prev_monitor_endAddress[i]){
               selected_stream = i;               
               found = true;
               
               break;
           }
       }
   }

   if(found){
       addresses = getPrefetchAddress(selected_stream, prev_monitor_startAddress, prev_monitor_endAddress, current_address);  
   }else{
       //find if it is part of a stream but the stream is not streaming 
       for(UInt32 i=0; i < n_streams; ++i){
           bool ready_to_check = (prev_tracking_address[i] != INVALID_ADDRESS) && (prev_monitor_startAddress[i] != INVALID_ADDRESS) &&
                             (prev_monitor_endAddress[i] != INVALID_ADDRESS);
           if(ready_to_check){
               if(current_address >= prev_tracking_address[i] && current_address < prev_monitor_startAddress[i]){
                   found = true;                                  
                   break;
               }
           }
       }
   }
   // if(found) we do nothing, we do not prefetch, so push nothing to addresses  
   if(!found){          
       // First check if we have a stream to confirm
       assert(m_confirm.size() == n_streams);
       for(UInt32 i = 0; i < n_streams; ++i){
           if((m_confirm[i] !=-1) && (m_confirm[i] !=2)){// this is a confirmation candidate               
               assert((prev_tracking_address[i] != INVALID_ADDRESS) && (prev_monitor_startAddress[i] != INVALID_ADDRESS) && 
                         (prev_monitor_endAddress[i] == INVALID_ADDRESS));
               assert((m_confirm[i] == 0) || (m_confirm[i] == 1));
               if(((current_address - prev_tracking_address[i])/CACHE_BLOCK_SIZE  + 1) <= stream_confirm_distance){
                   ++m_confirm[i];
                   if(m_confirm[i] == 2){
                       prev_monitor_endAddress[i] = current_address + init_distance * CACHE_BLOCK_SIZE;                  
                       addresses = getPrefetchAddress(i, prev_monitor_startAddress, prev_monitor_endAddress, current_address); 
                   }
                   found = true;
                   break; // once we confirmed one stream, we are good
               }
           }
       }     

       if(!found){                    
           // We know it's a brand new miss, allocate an entry -> assign the first free tracking entry 
           // if there are no free streams, do round robin deallocation and re-allocation (confirmed this with Seth Pugsley)
           // also initiate the monitor_start address but not the monitor end address 
           // because that will be set once the stream has been trained/confirmed           
           next_stream_to_allocate = (next_stream_to_allocate + 1) % n_streams; 
           prev_tracking_address[next_stream_to_allocate] = current_address;
           prev_monitor_startAddress[next_stream_to_allocate] = current_address;      
           prev_monitor_endAddress[next_stream_to_allocate] = INVALID_ADDRESS;            
           m_confirm[next_stream_to_allocate] = 0; // update confirmation                              
       }
   }
   return addresses; 
}

std::vector<IntPtr> DROPLETL1::trainPrefetcherForProperty(IntPtr address){

}

void DROPLETL1::printCurrentState(IntPtr current_address, std::vector<IntPtr> &prev_tracking_address,
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