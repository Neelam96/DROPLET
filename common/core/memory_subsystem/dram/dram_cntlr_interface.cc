#include "dram_cntlr_interface.h"
#include "memory_manager.h"
#include "shmem_msg.h"
#include "shmem_perf.h"
#include "log.h"
#include<fstream>

const IntPtr STRUC_ENTRY_SIZE = 4; // in bytes
const IntPtr PROP_ENTRY_SIZE = 8; // in bytes
const uint64_t kBitsPerWord = 64; // For the bitmap in BFS 

void DramCntlrInterface::handleMsgFromTagDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg)
{
   PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t shmem_msg_type = shmem_msg->getMsgType();
   SubsecondTime msg_time = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD);
   
   shmem_msg->getPerf()->updateTime(msg_time);
   
   switch (shmem_msg_type)
   {
      case PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_READ_REQ:     
      {
         IntPtr address= shmem_msg->getAddress();     
        
         Byte data_buf[getCacheBlockSize()];
         SubsecondTime dram_latency;
         HitWhere::where_t hit_where;

         boost::tie(dram_latency, hit_where) = getDataFromDram(address, shmem_msg->getRequester(), data_buf, msg_time, shmem_msg->getPerf());
         getShmemPerfModel()->incrElapsedTime(dram_latency, ShmemPerfModel::_SIM_THREAD);
         
         //...........................CHANGE!!!...............................................   
         /*if(shmem_msg->isPrefetch()){                    
               bool struc = getMemoryManager()->isStructureCacheline(address, m_cache_block_size);
               if(struc){                    
                     assert(shmem_msg->getIntraTileRequester() != INVALID_CORE_ID);                                            
                     trainPrefetcherForProperty(address, shmem_msg->getIntraTileRequester(), getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD));                                                                    
               }                           
         }*/
         //.........................................................................................

         shmem_msg->getPerf()->updateTime(getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD),
            hit_where == HitWhere::DRAM_CACHE ? ShmemPerf::DRAM_CACHE : ShmemPerf::DRAM);      

         getMemoryManager()->sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_READ_REP,
               MemComponent::DRAM, MemComponent::TAG_DIR,
               shmem_msg->getRequester() /* requester */,
               sender /* receiver */,
               address,
               data_buf, getCacheBlockSize(),
               hit_where, shmem_msg->getPerf(), ShmemPerfModel::_SIM_THREAD,
               shmem_msg->getIntraTileRequester(), shmem_msg->isPrefetch());
         
         break;
      }

      case PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_WRITE_REQ:
      {  
         putDataToDram(shmem_msg->getAddress(), shmem_msg->getRequester(), shmem_msg->getDataBuf(), msg_time);
         // DRAM latency is ignored on write
         break;
      }
      //.......................CHANGE!!!.......................................................     
      case PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_PREFETCH_REQ:
      {
            doInitialCheck(shmem_msg);
            break;
      }

      case PrL1PrL2DramDirectoryMSI::ShmemMsg::TAG_CHECK_REP:
      {
            doMemorySidePrefetch(shmem_msg);
            break;
      }
      //.......................................................................................

      default:
         LOG_PRINT_ERROR("Unrecognized Shmem Msg Type: %u", shmem_msg_type);
         break;
   }
}

//......................................CHANGE!!!..........................................................
std::vector<IntPtr> DramCntlrInterface::calculatePropertyPrefetchAddress(IntPtr address){       
      std::vector<UInt32> index;
      UInt32 numIDs = m_cache_block_size/STRUC_ENTRY_SIZE; // each ID is 4B for unweighted, 8B for weighted
      UInt32 prefetch_distance = 0;

      // get indices of the property array 
      for(UInt32 i = prefetch_distance; i < numIDs; ++i){
            UInt32* addrp = (UInt32*)(address+STRUC_ENTRY_SIZE*i);
            index.push_back(*addrp); // dereferencing gives the neighbr IDs            
      }
            
      // for now just do the propStart1, add to get absolute address then 
      // adjust to get aligned cache block address 
      std::vector<IntPtr> addresses;
      for(UInt32 i=0; i<index.size(); ++i){            
            //cout << "PropStart1 " << getMemoryManager()->propStart1 << endl; 
            //cout << "index " << index[i] << endl;            
            IntPtr new_add = getMemoryManager()->propStart1 + PROP_ENTRY_SIZE * (index[i]/kBitsPerWord); 
            //cout << "New add " << new_add << endl;    
            assert(getMemoryManager()->isPropertyCacheline(new_add));        
            IntPtr aligned_add = new_add - (new_add % m_cache_block_size);    
            bool prop = getMemoryManager()->isPropertyCacheline(aligned_add);
            if(prop){
                  // check if this aligned address is already present in the prefetch address list 
                  bool found = false;
                  for(UInt32 i=0; i< addresses.size(); ++i){
                        if(addresses[i] == aligned_add){
                              found = true; break;
                        }
                  }
                  if(!found) addresses.push_back(aligned_add);    
            }                                   
      }     

      return addresses;       
}

void DramCntlrInterface::trainPrefetcherForProperty(IntPtr address, core_id_t intraTileRequester, SubsecondTime t_now){      
      std::vector<IntPtr> prefetchList = calculatePropertyPrefetchAddress(address);   
      if(prefetchList.empty()) return;      
      std::deque <prefInfo*> &prefetch_list = m_prefetch_list.at(intraTileRequester);     
           
      UInt32 i = 0; UInt32 popped_prefetches = 0;
      for(std::vector<IntPtr>::iterator it = prefetchList.begin(); it != prefetchList.end(); ++it){                  
            assert(getMemoryManager()->isPropertyCacheline((*it)));         
            // if prefetch list size is larger drop older prefetches 
            if(prefetch_list.size()>=MAX_PREFETCH_LIST_SIZE){
                  prefetch_list.pop_front();
                  ++popped_prefetches;                  
            }
            
            prefInfo* p = new prefInfo(*it, t_now + i*DRAM_PREFETCH_INTERVAL); 
            prefetch_list.push_back(p);    
            ++i;        
      }

      /*for(UInt32 i= 0; i<prefetch_list.size(); ++i){
                  cout << prefetch_list[i]->m_prefetch_address << "(" << prefetch_list[i]->m_prefetch_time << ")"<< "  ";
      }cout << endl;*/

      updatePriorityList(intraTileRequester, popped_prefetches);

      book * x = new book(intraTileRequester, i);
      priority_list.push_back(x);       

      /*cout << "After changing popping things: ";
      cout << priority_list.front()->m_core_id << "  " << priority_list.front()->m_num_pref << endl;
      if(priority_list.front()->m_num_pref == 0){
            cout << "number of pops " << popped_prefetches << endl;
      }*/        
}

void DramCntlrInterface::updatePriorityList(core_id_t core_id, UInt32 popped_pref){       
      if(popped_pref == 0) return; 

      cout << "Popped from core " << core_id << endl;
      UInt32 i = 0;
      while (i < priority_list.size()){
            if(priority_list[i]->m_core_id == core_id){ 
                  if(popped_pref <= priority_list[i]->m_num_pref){                       
                        priority_list[i]->m_num_pref = priority_list[i]->m_num_pref - popped_pref;
                        if(priority_list[i]->m_num_pref == 0){
                              priority_list.erase(priority_list.begin() + i); 
                        }
                        break;
                  }else{
                        UInt32 dec = priority_list[i]->m_num_pref;
                        priority_list[i]->m_num_pref = 0;
                        priority_list.erase(priority_list.begin() + i); 
                        i--;
                        popped_pref = popped_pref - dec;
                  }
            }
            i++;
      }      
     /*for(UInt32 i =0 ; i< priority_list.size(); ++i){
            cout << priority_list[i]->m_core_id << " " << priority_list[i]->m_num_pref << "    ";            
      
      }cout << endl;*/     
}

bool DramCntlrInterface::isAllEmpty(){
      bool empty = true;
      for (UInt32 i = 0; i<m_prefetch_list.size(); i++){
            if(!(m_prefetch_list.at(i).empty())){
                  empty = false;
                  break;
            }
      }
      return empty;
}

void DramCntlrInterface::sendMiss(PrL1PrL2DramDirectoryMSI::ShmemMsg *shmem_msg){
      core_id_t requester = shmem_msg->getRequester(); // tile granularity requester 
      getMemoryManager()->sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_PREFETCH_REP,
               MemComponent::DRAM, MemComponent::TAG_DIR,
               requester /* requester */,
               requester /* receiver */,
               shmem_msg->getAddress(),
               NULL, 0,
               HitWhere::MISS, NULL, ShmemPerfModel::_SIM_THREAD,
               INVALID_CORE_ID, true, shmem_msg->getFakeAddress()); 
}

void DramCntlrInterface::doInitialCheck(PrL1PrL2DramDirectoryMSI::ShmemMsg *shmem_msg){          
      if(isAllEmpty()) sendMiss(shmem_msg); // we sent the prefetch rep that nothing to prefetch
      else{            
            assert(!priority_list.empty());            
            core_id_t prefetch_dest = priority_list.front()->m_core_id;
      
            std::deque<prefInfo*> &prefetch_list = m_prefetch_list.at(prefetch_dest);            
            
            if(prefetch_list.empty()){
                  cout << "Problem in core id " << prefetch_dest << endl;
                  cout << "Prefetch list front core " << priority_list.front()->m_core_id << endl;
                  cout << "Prefetch list front numPrefetches " << priority_list.front()->m_num_pref << endl;
            } 
            assert(!prefetch_list.empty());

            IntPtr prefetch_address = prefetch_list.front()->m_prefetch_address;

            getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_SIM_THREAD, prefetch_list.front()->m_prefetch_time); // start the checking now
            
            prefetch_list.pop_front();
            assert(getMemoryManager()->isPropertyCacheline(prefetch_address));
          
            // update priority list 
            priority_list.front()->m_num_pref--;
            if(priority_list.front()->m_num_pref == 0) priority_list.pop_front();         

            /*cout << "After doing InitialCheck: ";
            if(priority_list.empty()) cout << "Empty priority list" << endl;
            else cout << priority_list.front()->m_core_id << "  " << priority_list.front()->m_num_pref << endl;  */ 
           
            getMemoryManager()->sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::TAG_CHECK,
               MemComponent::DRAM, MemComponent::TAG_DIR,
               shmem_msg->getRequester() /* requester */,
               shmem_msg->getRequester() /* receiver */,
               prefetch_address,
               NULL, 0,
               HitWhere::MISS, shmem_msg->getPerf(), ShmemPerfModel::_SIM_THREAD,
               prefetch_dest, true, shmem_msg->getFakeAddress());              
      }
}

void DramCntlrInterface::doMemorySidePrefetch(PrL1PrL2DramDirectoryMSI::ShmemMsg *shmem_msg){      
      core_id_t prefetch_dest = shmem_msg->getIntraTileRequester();
      IntPtr prefetch_address = shmem_msg->getAddress();   

      Byte data_buf[getCacheBlockSize()];
      SubsecondTime dram_latency;
      HitWhere::where_t hit_where;     
      SubsecondTime msg_time = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD);

      boost::tie(dram_latency, hit_where) = getDataFromDram(prefetch_address, shmem_msg->getRequester(), data_buf, msg_time, NULL, true); // shmem_msg->getPerf() is made to be NULL 
      getShmemPerfModel()->incrElapsedTime(dram_latency, ShmemPerfModel::_SIM_THREAD);                
      
      //send a message to tag directory
      getMemoryManager()->sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_PREFETCH_REP,
               MemComponent::DRAM, MemComponent::TAG_DIR,
               shmem_msg->getRequester() /* requester */,
               shmem_msg->getRequester() /* receiver */,
               prefetch_address,
               data_buf, getCacheBlockSize(),
               hit_where, shmem_msg->getPerf(), ShmemPerfModel::_SIM_THREAD,
               prefetch_dest, true, shmem_msg->getFakeAddress());    
           
      ++m_sent_prefetches;           
}
//...........................................................................................................