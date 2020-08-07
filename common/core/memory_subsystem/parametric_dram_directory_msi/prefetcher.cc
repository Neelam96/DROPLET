#include "prefetcher.h"
#include "simulator.h"
#include "config.hpp"
#include "log.h"
#include "simple_prefetcher.h"
#include "ghb_prefetcher.h"
#include "stream_prefetcher.h"
#include "graph_stream_prefetcher.h"
#include "baseline_stream_prefetcher.h"
#include "address_prefetcher.h"
#include "dropletL1.h"
#include "vldp.h"

Prefetcher* Prefetcher::createPrefetcher(String type, String configName, core_id_t core_id, UInt32 shared_cores)
{
   if (type == "none")
      return NULL;
   else if (type == "simple")
      return new SimplePrefetcher(configName, core_id, shared_cores);
   else if (type == "ghb")
      return new GhbPrefetcher(configName, core_id);
   else if (type == "stream")
      return new StreamPrefetcher(configName, core_id, shared_cores);
   else if (type == "graph_stream")
      return new GraphStreamPrefetcher(configName, core_id, shared_cores);
   else if (type == "baseline_stream")
      return new BaselineStreamPrefetcher(configName, core_id, shared_cores);
   else if (type == "address_prefetch")
      return new AddressPrefetcher(configName, core_id, shared_cores);
   else if (type == "vldp")
      return new VLDP(configName, core_id);

   LOG_PRINT_ERROR("Invalid prefetcher type %s", type.c_str());
}