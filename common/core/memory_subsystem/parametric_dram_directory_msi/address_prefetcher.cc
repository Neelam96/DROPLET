#include "address_prefetcher.h"
#include "simulator.h"
#include "config.hpp"
#include <iostream>
#include <cstdlib>
#include "memory_manager_base.h"
using namespace std;

const IntPtr CACHE_BLOCK_SIZE = 64; // in bytes
const IntPtr OFFSET_ENTRY_SIZE = 8; // in bytes
const IntPtr STRUC_ENTRY_SIZE = 4; // in bytes

bool AddressPrefetcher::checkPresenceAndPush(std::vector<IntPtr>& addresses, IntPtr aligned_add){
    bool found = false;
    
    for(UInt32 i=0; i< addresses.size(); ++i){
        if(addresses[i] == aligned_add){
            found = true; break;
        }
    }
    
    if(!found) addresses.push_back(aligned_add);  
    return found;
}

std::vector<IntPtr>
AddressPrefetcher::getNextAddress(IntPtr address, core_id_t _core_id)
{  
   std::vector<IntPtr> addresses;   
   //bool off = isOffsetCacheline(address);  
   bool struc = isStructureCacheline(address, CACHE_BLOCK_SIZE);

   if(struc){      
       UInt32 numIDs = CACHE_BLOCK_SIZE/STRUC_ENTRY_SIZE; // each ID is 4B
       UInt32 prefetch_distance = 4;

       // check which offset to call -> inOffset or Out Offset     
       IntPtr startOffset = INVALID_ADDRESS;  
       if(address >= startOutNeigh && address <= endOutNeigh)
           startOffset = startOutOffset;          
       else
           startOffset = startInOffset;               

       // get indices of the offsets array 
       for(UInt32 i = prefetch_distance; i < numIDs; ++i){
            UInt32* addrp = (UInt32*)(address + STRUC_ENTRY_SIZE * i);                       
            IntPtr add = startOffset + OFFSET_ENTRY_SIZE * (*addrp); // deferencing gives actual neighbor ID     
            
            long* addrp1 = (long*)(add);  // start of the edge list
            long* addrp2 = (long*)(add + OFFSET_ENTRY_SIZE); // end of edge list, each entry is 8B
            IntPtr aligned_addrp1 = *addrp1 - (*addrp1 % CACHE_BLOCK_SIZE); 
            IntPtr aligned_addrp2 = *addrp2 - (*addrp2 % CACHE_BLOCK_SIZE);
            bool ok = (isStructureCacheline(aligned_addrp1, CACHE_BLOCK_SIZE) && isStructureCacheline(aligned_addrp2, CACHE_BLOCK_SIZE));

            if(ok){
                // how many structure cachelines to prefetch for the offset?
                UInt32 num_cachelines = ((aligned_addrp2 - aligned_addrp1)/CACHE_BLOCK_SIZE) + 1;
                //cout << "Beginning: " << aligned_addrp1 << " Ending: " << aligned_addrp2 << "  Number of structure cachelines: " << num_cachelines << endl;               
                if(num_cachelines > 4) num_cachelines = 4;
                // what are the addresses of each cacheline
                for(UInt32 i=0; i < num_cachelines; ++i){
                    IntPtr struc_aligned_add = aligned_addrp1 + (i * CACHE_BLOCK_SIZE);  
                    assert(isStructureCacheline(struc_aligned_add, CACHE_BLOCK_SIZE));
                    __attribute__((unused)) bool inserted = checkPresenceAndPush(addresses, struc_aligned_add);  
                }
            }             
       }
       cout << "Collected " << addresses.size() << " addresses " << endl;
   }
   return addresses;            
}

void AddressPrefetcher::updateOffsetListDuringPop(IntPtr address, core_id_t core_id){
    bool offset = isOffsetCacheline(address);  

    if(offset){
        for(UInt32 i=0; i < offset_list.size(); ++i){
           if (offset_list[i]->aligned_add == address){
               offset = offset_list[i]->offset;
               offset_list.erase(offset_list.begin() + i);
               break;
           }
       }
    }
}

void AddressPrefetcher::getStructureAddrRange(){
      if(!(Sim()->getMagicServer()->inROI())) return;
      
      ifstream in("StrucAddress.txt");      
      string line;
      getline(in, line);   
      if(line=="Undirected"){        
            getline(in, line);         
            IntPtr add = stol(line, nullptr, 0);         
            startOutNeigh = add;
            getline(in, line);            
            add = stol(line, nullptr, 0);
            endOutNeigh = add;      
            // get offsets
            getline(in, line);
            add = stol(line, nullptr, 0);
            startOutOffset = add;
            getline(in, line);
            add = stol(line, nullptr, 0);
            endOutOffset = add;      
      }else{
            directed = true;
            getline(in, line);
            IntPtr add = stol(line, nullptr, 0);        
            startOutNeigh = add;
            getline(in, line);
            add = stol(line, nullptr, 0);  
            endOutNeigh = add;
            getline(in, line);
            add = stol(line, nullptr, 0);
            startInNeigh = add;
            getline(in, line);
            add = stol(line, nullptr, 0);
            endInNeigh = add;         

            // get offsets
            getline(in, line);
            add = stol(line, nullptr, 0);
            startOutOffset = add;
            getline(in, line);
            add = stol(line, nullptr, 0);
            endOutOffset = add; 
            getline(in, line);
            add = stol(line, nullptr, 0);
            startInOffset = add;   
            getline(in, line);
            add = stol(line, nullptr, 0);
            endInOffset = add;   
      }     
                 
      addrRangeSet = true;
      in.close();      
}

bool AddressPrefetcher::isStructureCacheline(IntPtr ca_address, UInt32 cache_block_size){
      bool strucCacheline = false;
      if (addrRangeSet == false) getStructureAddrRange();
      if(addrRangeSet == true){
            if(directed==false){                  
                  //undirected, check only startOutNeigh and endOutNeigh
                  if((startOutNeigh<=ca_address) && (ca_address+cache_block_size <=endOutNeigh)) strucCacheline = true;
            }else{
                  // direct, check all the four address bounds 
                  if(((startOutNeigh<=ca_address) && (ca_address+cache_block_size <=endOutNeigh)) ||
                   ((startInNeigh<=ca_address) && (ca_address+cache_block_size <=endInNeigh))) strucCacheline = true;
            }
      }
      return strucCacheline;
}

bool AddressPrefetcher::isOffsetCacheline(IntPtr ca_address){
      bool offsetCacheline = false;
      if (addrRangeSet == false) getStructureAddrRange();
      if(addrRangeSet == true){
            if(directed==false){                  
                  //undirected, check only startOutNeigh and endOutNeigh
                  if((startOutOffset<=ca_address) && (ca_address <=endOutOffset)) offsetCacheline = true;
            }else{
                  // direct, check all the four address bounds 
                  if(((startOutOffset<=ca_address) && (ca_address <=endOutOffset)) ||
                   ((startInOffset<=ca_address) && (ca_address <=endInOffset))) offsetCacheline = true;
            }
      }
      return offsetCacheline;
}