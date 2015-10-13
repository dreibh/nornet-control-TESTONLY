// =================================================================
//          #     #                 #     #
//          ##    #   ####   #####  ##    #  ######   #####
//          # #   #  #    #  #    # # #   #  #          #
//          #  #  #  #    #  #    # #  #  #  #####      #
//          #   # #  #    #  #####  #   # #  #          #
//          #    ##  #    #  #   #  #    ##  #          #
//          #     #   ####   #    # #     #  ######     #
//
//       ---   The NorNet Testbed for Multi-Homed Systems  ---
//                       https://www.nntb.no
// =================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015 by Thomas Dreibholz
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@simula.no

#include "ping.h"

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


// ###### Constructor #######################################################
Ping::Ping(SQLWriter*                               sqlWriter,
           const bool                               verboseMode,
           const boost::asio::ip::address&          sourceAddress,
           const std::set<boost::asio::ip::address> destinationAddressArray,
           const unsigned long long                 interval,
           const unsigned int                       expiration,
           const unsigned int                       ttl)
   : Traceroute(sqlWriter, verboseMode,
                sourceAddress, destinationAddressArray,
                interval, expiration, ttl, ttl, ttl)
{
}


// ###### Destructor ########################################################
Ping::~Ping()
{
}


// ###### All requests have received a response #############################
void Ping::noMoreOutstandingRequests()
{
   // Nothing to do for Ping!
}


// ###### Prepare a new run #################################################
void Ping::prepareRun()
{
   // Nothing to do for Ping!
}


// ###### The destination has not been reached with the current TTL #########
bool Ping::notReachedWithCurrentTTL()
{
   // Nothing to do for Ping!
   return(false);
}


// ###### Schedule timer ####################################################
void Ping::scheduleTimeout()
{
   const unsigned long long deviation = std::max(10ULL, Interval / 5ULL);   // 20% deviation
   const unsigned long long duration  = Interval + (std::rand() % deviation);
   TimeoutTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(duration));
   TimeoutTimer.async_wait(boost::bind(&Ping::handleTimeout, this, _1));
}


// ###### Comparison function for results output ############################
int Ping::comparePingResults(const ResultEntry* a, const ResultEntry* b)
{
   return(a->address() < b->address());
}


// ###### Process results ###################################################
void Ping::processResults()
{
   std::vector<ResultEntry*> resultsVector;
   for(std::map<unsigned short, ResultEntry>::iterator iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(&iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), &comparePingResults);

   const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
   for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          ((now - resultEntry->sendTime()).total_milliseconds() >= Expiration) ) {
         resultEntry->status(Timeout);
         resultEntry->receiveTime(resultEntry->sendTime() + boost::posix_time::milliseconds(Expiration));
      }

      // ====== Print completed elements ====================================
      if(resultEntry->status() != Unknown) {
      std::cout << *resultEntry << std::endl;
         if(SQLOutput) {
            SQLOutput->insert(
               str(boost::format("'%s','%s','%s',%d,%d")
                  % boost::posix_time::to_iso_extended_string(resultEntry->sendTime())
                  % SourceAddress.to_string()
                  % resultEntry->address().to_string()
                  % resultEntry->status()
                  % (resultEntry->receiveTime() - resultEntry->sendTime()).total_milliseconds()
            ));
         }
      }

      // ====== Remove completed entries ====================================
      if(resultEntry->status() != Unknown) {
         assert(ResultsMap.erase(resultEntry->seqNumber()) == 1);
         if(resultEntry->status() == Unknown) {
            if(OutstandingRequests > 0) {
               OutstandingRequests--;
            }
         }
      }
   }
}


// ###### Send requests to all destinations #################################
void Ping::sendRequests()
{
   for(std::set<boost::asio::ip::address>::const_iterator destinationIterator = DestinationAddressArray.begin();
       destinationIterator != DestinationAddressArray.end(); destinationIterator++) {
      const boost::asio::ip::address& destinationAddress = *destinationIterator;
      sendICMPRequest(destinationAddress, FinalMaxTTL);
   }

   scheduleTimeout();
}
