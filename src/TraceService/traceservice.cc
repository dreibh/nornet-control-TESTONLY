// NorNet Trace Service
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>

#include <ctime>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
// #include <boost/date_time/local_time/local_time.hpp>

#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"

#include <boost/interprocess/streams/bufferstream.hpp> // ???????????

std::set<boost::asio::ip::address> sourceArray;
std::set<boost::asio::ip::address> destinationArray;


void addAddress(std::set<boost::asio::ip::address>& array,
                const std::string&                  addressString)
{
   boost::system::error_code errorCode;
   boost::asio::ip::address address = boost::asio::ip::address::from_string(addressString, errorCode);
   if(errorCode != boost::system::errc::success) {
      std::cerr << "ERROR: Bad address " << addressString << "!" << std::endl;
      ::exit(1);
   }
   array.insert(address);
}

unsigned long long ptimeToMircoTime(const boost::posix_time::ptime t)
{
   static const boost::posix_time::ptime myEpoch(boost::gregorian::date(1976,9,29));
   boost::posix_time::time_duration difference = t - myEpoch;
   return(difference.ticks());
}


enum HopStatus {
   Unknown                = 0,
   TimeExceeded           = 1,
   UnreachableScope       = 100,
   UnreachableNetwork     = 101,
   UnreachableHost        = 102,
   UnreachableProtocol    = 103,
   UnreachablePort        = 104,
   UnreachableProhibited  = 105,
   UnreachableUnknown     = 110,
   Timeout                = 200,
   Success                = 255
};

class ResultEntry {
   public:
   ResultEntry(const unsigned short           seqNumber,
               const unsigned int             hop,
               boost::asio::ip::address       address,
               const HopStatus                status,
               const boost::posix_time::ptime sendTime);

   inline unsigned int seqNumber()               const { return(SeqNumber);   }
   inline unsigned int hop()                     const { return(Hop);         }
   inline HopStatus    status()                  const { return(Status);      }
   inline boost::posix_time::ptime sendTime()    const { return(SendTime);    }
   inline boost::posix_time::ptime receiveTime() const { return(ReceiveTime); }
   inline boost::posix_time::time_duration rtt() const {
      return(ReceiveTime - SendTime);
   }

   inline void receiveTime(const boost::posix_time::ptime receiveTime) { ReceiveTime = receiveTime; }
   inline void status(const HopStatus status)                          { Status = status;           }
   inline void address(const boost::asio::ip::address address)         { Address = address;         }

   inline friend bool operator<(const ResultEntry& resultEntry1, const ResultEntry& resultEntry2) {
      return(resultEntry1.SeqNumber < resultEntry2.SeqNumber);
   }
   friend std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry);

   private:
   const unsigned short             SeqNumber;
   const unsigned int               Hop;
   boost::asio::ip::address         Address;
   HopStatus                        Status;
   const boost::posix_time::ptime   SendTime;
   boost::posix_time::ptime         ReceiveTime;
};


ResultEntry::ResultEntry(const unsigned short           seqNumber,
                         const unsigned int             hop,
                         boost::asio::ip::address       address,
                         const HopStatus                status,
                         const boost::posix_time::ptime sendTime)
   : SeqNumber(seqNumber),
     Hop(hop),
     Address(address),
     Status(status),
     SendTime(sendTime)
{
}

std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry)
{
   os << "#" << resultEntry.SeqNumber
      << "\t" << resultEntry.Hop
      << "\t" << resultEntry.rtt().total_microseconds() / 1000.0 << "ms"
      << "\t" << resultEntry.Status
      << "\t" << resultEntry.Address;
   return(os);
}



class Traceroute : public boost::noncopyable
{
   public:
   Traceroute(const boost::asio::ip::address& source,
              const boost::asio::ip::address& destination,
              const unsigned int              duration = 3000,
              const unsigned int              maxTTL   = 12);
   virtual ~Traceroute();
   void run();

   inline bool isIPv6() const {
      return(SourceEndpoint.address().is_v6());
   }

   protected:
   virtual bool prepareSocket();
   virtual void prepareRun();
   virtual void scheduleTimeout();
   virtual void expectNextReply();
   virtual void sendRequests();
   virtual void handleTimeout(const boost::system::error_code& errorCode);
   virtual void handleMessage(std::size_t length);

   private:
   void recordResult(const boost::posix_time::ptime& receiveTime,
                     const ICMPHeader&               icmpHeader,
                     const unsigned short            seqNumber);

   const unsigned int                    Duration;
   const unsigned int                    MaxTTL;
   boost::asio::io_service               IO;
   boost::asio::ip::icmp::endpoint       SourceEndpoint;
   boost::asio::ip::icmp::endpoint       DestinationEndpoint;
   boost::asio::ip::icmp::socket         ICMPSocket;
   boost::asio::deadline_timer           TimeoutTimer;
   boost::asio::ip::icmp::endpoint       ReplyEndpoint;          // Store ICMP reply's source

   unsigned int                          Identifier;
   unsigned short                        SeqNumber;
   unsigned int                          MagicNumber;
   unsigned int                          LastHop;
   std::map<unsigned short, ResultEntry> ResultsMap;

   char                                  MessageBuffer[65536 + 40];
};


Traceroute::Traceroute(const boost::asio::ip::address& sourceAddress,
                       const boost::asio::ip::address& destinationAddress,
                       const unsigned int              duration,
                       const unsigned int              maxTTL)
   : Duration(duration),
     MaxTTL(maxTTL),
     IO(),
     SourceEndpoint(sourceAddress, 0),
     DestinationEndpoint(destinationAddress, 0),
     ICMPSocket(IO, (isIPv6() == true) ? boost::asio::ip::icmp::v6() : boost::asio::ip::icmp::v4()),
     TimeoutTimer(IO)
{
   Identifier  = 0;
   SeqNumber   = (unsigned short)(std::rand() & 0xffff);
   MagicNumber = ((std::rand() & 0xffff) << 16) | (std::rand() & 0xffff);
   LastHop     = 0xffffffff;
}


Traceroute::~Traceroute()
{
}


bool Traceroute::prepareSocket()
{
   // ====== Bind ICMP socket to given source address =======================
   boost::system::error_code errorCode;
   ICMPSocket.bind(SourceEndpoint, errorCode);
   if(errorCode !=  boost::system::errc::success) {
      std::cerr << "ERROR: Unable to bind to source address "
                << SourceEndpoint.address() << "!" << std::endl;
      return(false);
   }

   // ====== Set filter (not required, but much more efficient) =============
   if(isIPv6()) {
      struct icmp6_filter filter;
      ICMP6_FILTER_SETBLOCKALL(&filter);
      ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &filter);
      if(setsockopt(ICMPSocket.native(), IPPROTO_ICMPV6, ICMP6_FILTER,
                    &filter, sizeof(struct icmp6_filter)) < 0) {
         std::cerr << "WARNING: Unable to set ICMP6_FILTER!" << std::endl;
      }
   }
   return(true);
}


void Traceroute::prepareRun()
{
   ResultsMap.clear();
}


void Traceroute::scheduleTimeout()
{
   TimeoutTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(Duration));
   TimeoutTimer.async_wait(boost::bind(&Traceroute::handleTimeout, this, _1));
}


void Traceroute::expectNextReply()
{
   ICMPSocket.async_receive_from(boost::asio::buffer(MessageBuffer),
                                 ReplyEndpoint,
                                 boost::bind(&Traceroute::handleMessage, this, _2));
}


void Traceroute::sendRequests()
{
   // ====== Send Echo Requests =============================================
   for(unsigned int ttl = MaxTTL; ttl > 0; ttl--) {
      // ------ Set TTL ----------------------------------------
      const boost::asio::ip::unicast::hops option(ttl);
      ICMPSocket.set_option(option);

      // ------ Create an ICMP header for an echo request ------
      SeqNumber++;
      ICMPHeader echoRequest;
      echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
      echoRequest.code(0);
      echoRequest.identifier(Identifier);
      echoRequest.seqNumber(SeqNumber);
      const boost::posix_time::ptime sendTime = boost::posix_time::microsec_clock::universal_time();
      TraceServiceHeader tsHeader;
      tsHeader.magicNumber(MagicNumber);
      tsHeader.sendTTL(ttl);
      tsHeader.sendTimeStamp(ptimeToMircoTime(sendTime));
      const std::vector<unsigned char> tsHeaderContents = tsHeader.contents();
      computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());

      // ------ Encode the request packet ----------------------
      boost::asio::streambuf request_buffer;
      std::ostream os(&request_buffer);
      os << echoRequest << tsHeader;

      // ------ Send the request -------------------------------

      size_t s = ICMPSocket.send_to(request_buffer.data(), DestinationEndpoint);
      if(s < 1) {  // FIXME!
         printf("s=%d\n",(int)s);
      }

      // ------ Record the request -----------------------------
      ResultEntry resultEntry(SeqNumber, ttl, DestinationEndpoint.address(), HopStatus::Unknown, sendTime);
      ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));
   }

   scheduleTimeout();
   expectNextReply();
}


void Traceroute::run()
{
   // ====== Prepare run ====================================================
   Identifier = ::getpid();
   if(!prepareSocket()) {
      return;
   }

   prepareRun();
   sendRequests();

   IO.run();
}


void Traceroute::handleTimeout(const boost::system::error_code& errorCode)
{
   std::cout << "Timeout" << std::endl;

   std::vector<ResultEntry*> resultsVector;
   for(auto iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(&iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), [](ResultEntry* a, ResultEntry* b) {
        return(a->hop() < b->hop());
   });

   for(auto iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;
      std::cout << *resultEntry << std::endl;
      if(resultEntry->status() == HopStatus::Success) {
         std::cout << "BREAK" << std::endl;
         break;
      }
   }

   prepareRun();
   sendRequests();
}


void Traceroute::handleMessage(std::size_t length)
{
   const boost::posix_time::ptime receiveTime = boost::posix_time::microsec_clock::universal_time();
   boost::interprocess::bufferstream is(MessageBuffer, length);

   printf("r=%d\n",(int)length);

   ICMPHeader icmpHeader;
   if(isIPv6()) {
      is >> icmpHeader;
      if(is) {
         if(icmpHeader.type() == ICMPHeader::IPv6EchoReply) {
            if(icmpHeader.identifier() == Identifier) {
               TraceServiceHeader tsHeader;
               is >> tsHeader;
               if(is) {
                  if(tsHeader.magicNumber() == MagicNumber) {
                     recordResult(receiveTime, icmpHeader, icmpHeader.seqNumber());
                  }
               }
            }
         }
         else if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
                  (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ) {
            IPv6Header innerIPv6Header;
            ICMPHeader innerICMPHeader;
            TraceServiceHeader tsHeader;
            is >> innerIPv6Header >> innerICMPHeader >> tsHeader;
            if(is) {
               if(tsHeader.magicNumber() == MagicNumber) {
                  recordResult(receiveTime, icmpHeader, innerICMPHeader.seqNumber());
               }
            }
         }
      }
   }
   else {
      IPv4Header ipv4Header;
      is >> ipv4Header;
      if(is) {
         is >> icmpHeader;
         if(is) {
            if(icmpHeader.type() == ICMPHeader::IPv4EchoReply) {
               if(icmpHeader.identifier() == Identifier) {
                  TraceServiceHeader tsHeader;
                  is >> tsHeader;
                  if(is) {
                     if(tsHeader.magicNumber() == MagicNumber) {
                        recordResult(receiveTime, icmpHeader, icmpHeader.seqNumber());
                     }
                  }
               }
            }
            else if(icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) {
               IPv4Header innerIPv4Header;
               ICMPHeader innerICMPHeader;
               is >> innerIPv4Header >> innerICMPHeader;
               if(is) {
                  if( (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ||
                      (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
                     if(innerICMPHeader.identifier() == Identifier) {
                        // Unfortunately, ICMPv4 does not return the full TraceServiceHeader here!
                        std::cout << "@@@@@@@ Good ICMP of router: from " << ReplyEndpoint.address().to_string()
                                 << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
                        recordResult(receiveTime, icmpHeader, innerICMPHeader.seqNumber());
                     }
                  }
               }
            }

         } else puts("--- ICMP is bad!");
      } else puts("--- IP is bad!");

   }

   expectNextReply();
}




void Traceroute::recordResult(const boost::posix_time::ptime& receiveTime,
                              const ICMPHeader&               icmpHeader,
                              const unsigned short            seqNumber)
{
   // ====== Find corresponding request =====================================
   std::map<unsigned short, ResultEntry>::iterator found = ResultsMap.find(seqNumber);
   if(found == ResultsMap.end()) {
      printf("F=%d\n",seqNumber);
      puts("NOT FOUND!");
      return;
   }
   ResultEntry& resultEntry = found->second;

   // ====== Get status =====================================================
   if(resultEntry.status() == HopStatus::Unknown) {
      resultEntry.receiveTime(receiveTime);
      resultEntry.address(ReplyEndpoint.address());

      HopStatus status = HopStatus::Unknown;
      if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
          (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ) {
         status = HopStatus::TimeExceeded;
      }
      else if( (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ||
               (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
         if(isIPv6()) {
            switch(icmpHeader.code()) {
               case ICMP6_DST_UNREACH_ADMIN:
                  status = HopStatus::UnreachableProhibited;
               break;
               case ICMP6_DST_UNREACH_BEYONDSCOPE:
                  status = HopStatus::UnreachableScope;
               break;
               case ICMP6_DST_UNREACH_NOROUTE:
                  status = HopStatus::UnreachableNetwork;
               break;
               case ICMP6_DST_UNREACH_ADDR:
                  status = HopStatus::UnreachableHost;
               break;
               case ICMP6_DST_UNREACH_NOPORT:
                  status = HopStatus::UnreachablePort;
               break;
               default:
                  status = HopStatus::UnreachableUnknown;
               break;
            }
         }
         else {
            switch(icmpHeader.code()) {
               case ICMP_PKT_FILTERED:
                  status = HopStatus::UnreachableProhibited;
               break;
               case ICMP_NET_UNREACH:
               case ICMP_NET_UNKNOWN:
                  status = HopStatus::UnreachableNetwork;
               break;
               case ICMP_HOST_UNREACH:
               case ICMP_HOST_UNKNOWN:
                  status = HopStatus::UnreachableHost;
               break;
               case ICMP_PORT_UNREACH:
                  status = HopStatus::UnreachablePort;
               break;
               default:
                  status = HopStatus::UnreachableUnknown;
               break;
            }
         }
      }
      else if( (icmpHeader.type() == ICMPHeader::IPv6EchoReply) ||
               (icmpHeader.type() == ICMPHeader::IPv4EchoReply) ) {
         status  = HopStatus::Success;
         LastHop = std::max(LastHop, resultEntry.hop());
      }
      resultEntry.status(status);
   printf("STATUS: %d   body=%d\n",(int)status,0);
   }

   // ====== Get hop ========================================================
}


int main(int argc, char** argv)
{
   std::srand(std::time(0));

   for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "-source=", 8) == 0) {
         addAddress(sourceArray, (const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-destination=", 13) == 0) {
         addAddress(destinationArray, (const char*)&argv[i][13]);
      }
      else {
         std::cerr << "Usage: " << argv[0] << " -source=source ... -destination=destination ..." << std::endl;
      }
   }
   if( (sourceArray.size() < 1) ||
       (destinationArray.size() < 1) ) {
      std::cerr << "ERROR: At least one source and destination are needed!" << std::endl;
      ::exit(1);
   }

   for(std::set<boost::asio::ip::address>::iterator sourceIterator = sourceArray.begin(); sourceIterator != sourceArray.end(); sourceIterator++) {
      for(std::set<boost::asio::ip::address>::iterator destinationIterator = destinationArray.begin(); destinationIterator != destinationArray.end(); destinationIterator++) {
         Traceroute t(*sourceIterator, *destinationIterator);
         t.run();
      }
   }
}
