/*
Copyright (c) 2009-2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// written by Roman Dementiev,
//            Thomas Willhalm,
//            Patrick Ungerer
//
// modified by Tiemo Bang, SAP

/*!     \file CacheMeter.hpp
\brief Example of using CPU counters: implements a simple performance counter monitoring utility
*/
#ifndef PCMMETER_CACHEMETER_HPP
#define PCMMETER_CACHEMETER_HPP
#include <iostream>
#ifdef _MSC_VER
#include <windows.h>
#include "PCM_Win/windriver.h"
#else
#include <unistd.h>
#include <signal.h>   // for atexit()
#include <sys/time.h> // for gettimeofday()
#endif
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <bitset>
#include "cpucounters.h"
#include "utils.h"
#include "types.h"
#include <numa.h>
#include <memory>
#include <functional>

#define SIZE (10000000)
#define PCM_DELAY_DEFAULT 1.0 // in seconds
#define PCM_DELAY_MIN 0.015 // 15 milliseconds is practical on most modern CPUs
#define PCM_CALIBRATION_INTERVAL 50 // calibrate clock only every 50th iteration
#define MAX_CORES 4096

using namespace std;


namespace PCMMeter
{

enum class MeasurementType {Default, Stalls, TMAM};

class CacheMeter
{

public:

    using CustomCoreEventPair = std::pair<std::vector<std::string>, std::vector<PCM::CustomCoreEventDescription>>;
    static inline const CustomCoreEventPair StallEvents = {
        {{"Stalls.Any", "Stalls.RS", "Stalls.SB", "Stalls.ROB"}}
        , {{{0xA2, 0x1}, {0xA2, 0x4}, {0xA2, 0x8}, {0xA2, 0x10}}}
    };

    
    //Events to calculate workload locators according to Intel's TMAM model
    // Total cycles:    CPU CLK UNHALTED.THREAD
    //
    // Retiring:        IDQ UOPS NOT DELIVERED.CORE
    // Bad Speculation: UOPS ISSUED.ANY, UOPS RETIRED.RETIRE SLOTS, INT MISC.RECOVERY CYCLES

    // (Memory Bound:    CYCLE ACTIVITY.STALLS LDM PENDING, RESOURCE STALLS.SB)

    static inline const CustomCoreEventPair TMAMLocatorEventNames = {
        {{"Cycles", "Retiring", "FE_Bound", "Bad_Speculation", "BE_BOUND", "UOPS_RETIRED.RETIRE_SLOTS", "IDQ_UOPS_NOT_DELIVERED.CORE", "UOPS_ISSUED.ANY", "INT_MISC.RECOVERY_CYCLES"}}
        , {{{0x0, 0x0}, {0x0, 0x0}, {0x0, 0x0}, {0x0, 0x0}}}
    };

    static PCM::ExtendedCustomCoreEventDescription* getTMAMLocatorEvents()
    {
        static PCM::ExtendedCustomCoreEventDescription eventDesc{};
        static EventSelectRegister events[4]{};
        eventDesc.nGPCounters = 4;
        eventDesc.gpCounterCfg = events;

        events[0].fields.event_select = 0xC2;
        events[0].fields.umask = 0x2;
        events[1].fields.event_select = 0x9C;
        events[1].fields.umask = 0x1;
        events[2].fields.event_select = 0x0E;
        events[2].fields.umask = 0x1;
        events[3].fields.event_select = 0x0D;
        events[3].fields.umask = 0x3;
        events[3].fields.cmask = 0x1;

        for(uint64_t i = 0; i < eventDesc.nGPCounters; ++i)
        {
            events[i].fields.usr = 1;
            events[i].fields.os = 1;
            events[i].fields.enable = 1;
        }

        std::cout << "Custom event 0: " << events[0].value << "\n";
        std::cout << "Custom event 1: " << events[1].value << "\n";
        std::cout << "Custom event 2: " << events[2].value << "\n";
        std::cout << "Custom event 3: " << events[3].value << "\n";

        return &eventDesc;
    }

    //! Retiring: UOPS RETIRED.RETIRE SLOTS / (4 * CPU CLK UNHALTED.THREAD)
    template <class CounterStateType>
    static double getRetiring(const CounterStateType &c1, const CounterStateType &c2)
    {
        return 1.0*getNumberOfCustomEvents(0, c1, c2) / 4.0 / getCycles(c1, c2);
    }

    //! FE: IDQ_UOPS_NOT_DELIVERED.CORE / (4 * CPU_CLK_UNHALTED.THREAD)
    template <class CounterStateType>
    static double getFEBound(const CounterStateType &c1, const CounterStateType &c2)
    {
        return 1.0*getNumberOfCustomEvents(1, c1, c2) / 4.0 / getCycles(c1, c2);
    }

    //! Bad Speculation: (UOPS ISSUED.ANY - UOPS RETIRED.RETIRE SLOTS + 4 * INT MISC.RECOVERY CYCLES / (4 * CPU CLK UNHALTED.THREAD)
    template <class CounterStateType>
    static double getBadSpeculation(const CounterStateType &c1, const CounterStateType &c2)
    {
        return (1.0*getNumberOfCustomEvents(2, c1, c2) - getNumberOfCustomEvents(0, c1, c2) + 4 * getNumberOfCustomEvents(3, c1, c2)) / 4.0 / getCycles(c1, c2);
    }

    //! BE 1 - Retiring - FE - Bad speculation
    template <class CounterStateType>
    static double getBEBound(const CounterStateType &c1, const CounterStateType &c2)
    {
        return 1.0 - getRetiring(c1, c2) - getFEBound(c1, c2) - getBadSpeculation(c1, c2);
    }


    const constexpr static char * longDiv = "---------------------------------------------------------------------------------------------------------------\n";

    template <class IntType>
    static double float_format(IntType n)
    {
        return double(n) / 1e6;
    }

    static std::string temp_format(int32 t)
    {
        char buffer[1024];
        if (t == PCM_INVALID_THERMAL_HEADROOM)
            return "N/A";

        snprintf(buffer, 1024, "%2d", t);
        return buffer;
    }

    static std::string l3cache_occ_format(uint64 o)
    {
        char buffer[1024];
        if (o == PCM_INVALID_QOS_MONITORING_DATA)
            return "N/A";

        snprintf(buffer, 1024, "%6d", (uint32) o);
        return buffer;
    }

    template <class CounterStateType>
    void print_custom_events(const CounterStateType &c1, const CounterStateType &c2, std::ostream& out)
    {
        std::string sep = " ";
        if(csv_output_)
        {
            sep = ";";
        }

        if(measurementType == MeasurementType::TMAM)
        {
            out << getCycles(c1, c2) << sep;
            out << getRetiring(c1, c2) << sep;
            out << getFEBound(c1, c2) << sep;
            out << getBadSpeculation(c1, c2) << sep;
            out << getBEBound(c1, c2) << sep;

            for (int32_t e = 0; e < measurementEvents.second.size(); ++e)
            {
                out << getNumberOfCustomEvents(e, c1, c2) << sep;
            }

        }
        else
        {
            out << getNumberOfCustomEvents(0, c1, c2) << sep;
            for (int32_t e = 1; e < measurementEvents.first.size(); ++e)
            {
                out << getNumberOfCustomEvents(e, c1, c2) << sep;
            }
        }
    }

    void print_custom_events_header(std::ostream& out)
    {
        std::string sep = " ";
        if(csv_output_)
        {
            sep = ";";
        }
         
        out << measurementEvents.first[0] << sep;
        for(int32_t e = 1; e < measurementEvents.first.size(); ++e)
        {
            out << measurementEvents.first[e] << sep;
        }
        
    }

    void print_custom_events_header_1(std::ostream& out, const std::string& title)
    {
        std::string sep = " ";
        if(csv_output_)
        {
            sep = ";";
        }
         
        out << title << sep;
        for(int32_t e = 1; e < measurementEvents.first.size(); ++e)
        {
            out << title << sep;
        }
        
    }

    void print_basic ()
    {
        if(show_partial_core_output_ && ycores_.none())
        {
            if(numa_available() < 0)
            {
                cerr << "Error numa not available, cpuset cannot be defined!" << endl;
                show_partial_core_output_ = false;
            }
            else
            {
                // Selecting all cpus the program is allowed to run on
                for(unsigned i = 0; i < m_->getNumCores(); ++i)
                {
                    if(numa_bitmask_isbitset(numa_all_cpus_ptr, i)) ycores_.set(i);
                }
            }
            
        }

        cout.precision(2);
        cout << std::fixed;
        cout << "Core metrics\n";
        if (cpu_model_ == PCM::ATOM)
            cout << " Core SKT   EXEC   IPC    FREQ   L2MISS   L2HIT   TEMP" << endl << endl;
        else if (cpu_model_ == PCM::KNL)
            cout << " Proc Tile Core Thread   EXEC   IPC   FREQ   AFREQ   L2MISS   L2HIT   TEMP" << endl << endl;
        else
        {
            cout << " Core SKT   EXEC   IPC    FREQ    AFREQ   L3MISS   L2MISS   L3HIT   L2HIT   L3MPI   L2MPI  ";

            if (m_->L3CacheOccupancyMetricAvailable())
                cout << "  L3OCC  ";
            if (m_->CoreLocalMemoryBWMetricAvailable())
                cout << "   LMB   ";
            if (m_->CoreRemoteMemoryBWMetricAvailable())
                cout << "   RMB   ";

            cout << " TEMP" << endl;
        }
        
        for ( uint32 i = 0; i < m_->getNumCores(); ++i ) {
            if ( m_->isCoreOnline ( i ) == false || ( show_partial_core_output_ && ycores_.test ( i ) == false ) )
                continue;

            if ( cpu_model_ == PCM::ATOM )
                cout << " " << setw ( 3 ) << i << "   " << setw ( 2 ) << m_->getSocketId ( i ) <<
                     "     " << getExecUsage ( cstates1_[i], cstates2_[i] ) <<
                     "   " << getIPC ( cstates1_[i], cstates2_[i] ) <<
                     "   " << getRelativeFrequency ( cstates1_[i], cstates2_[i] ) <<
                     "   " << getL2CacheMisses ( cstates1_[i], cstates2_[i] ) <<
                     "    " << getL2CacheHitRatio ( cstates1_[i], cstates2_[i] ) <<
                     "     " << temp_format ( cstates2_[i].getThermalHeadroom() ) <<
                     endl;
            else if ( cpu_model_ == PCM::KNL )
                cout << setfill ( ' ' ) << std::internal << setw ( 5 ) << i
                     << setw ( 5 ) << m_->getTileId ( i ) << setw ( 5 ) << m_->getCoreId ( i )
                     << setw ( 7 ) << m_->getThreadId ( i )
                     << setw ( 7 ) << getExecUsage ( cstates1_[i], cstates2_[i] )
                     << setw ( 6 ) << getIPC ( cstates1_[i], cstates2_[i] )
                     << setw ( 7 ) << getRelativeFrequency ( cstates1_[i], cstates2_[i] )
                     << setw ( 8 ) << getActiveRelativeFrequency ( cstates1_[i], cstates2_[i] )
                     << setw ( 9 ) << getL2CacheMisses ( cstates1_[i], cstates2_[i] )
                     << setw ( 8 ) << getL2CacheHitRatio ( cstates1_[i], cstates2_[i] )
                     << setw ( 7 ) << temp_format ( cstates2_[i].getThermalHeadroom() ) << endl;
            else { /* != ATOM && != KNL */
                cout << " " << setw ( 3 ) << i << "   " << setw ( 2 ) << m_->getSocketId ( i ) <<
                     "     " << getExecUsage ( cstates1_[i], cstates2_[i] ) <<
                     "   " << getIPC ( cstates1_[i], cstates2_[i] ) <<
                     "   " << getRelativeFrequency ( cstates1_[i], cstates2_[i] ) <<
                     "    " << getActiveRelativeFrequency ( cstates1_[i], cstates2_[i] ) <<
                     "    " << getL3CacheMisses ( cstates1_[i], cstates2_[i] ) <<
                     "   " << getL2CacheMisses ( cstates1_[i], cstates2_[i] ) <<
                     "    " << getL3CacheHitRatio ( cstates1_[i], cstates2_[i] ) <<
                     "    " << getL2CacheHitRatio ( cstates1_[i], cstates2_[i] ) <<
                     "    " << double ( getL3CacheMisses ( cstates1_[i], cstates2_[i] ) ) / getInstructionsRetired ( cstates1_[i], cstates2_[i] ) <<
                     "    " << double ( getL2CacheMisses ( cstates1_[i], cstates2_[i] ) ) / getInstructionsRetired ( cstates1_[i], cstates2_[i] ) ;
                if ( m_->L3CacheOccupancyMetricAvailable() )
                    cout << "   " << setw ( 6 ) << l3cache_occ_format ( getL3CacheOccupancy ( cstates2_[i] ) ) ;
                if ( m_->CoreLocalMemoryBWMetricAvailable() )
                    cout << "   " << setw ( 6 ) << getLocalMemoryBW ( cstates1_[i], cstates2_[i] );
                if ( m_->CoreRemoteMemoryBWMetricAvailable() )
                    cout << "   " << setw ( 6 ) << getRemoteMemoryBW ( cstates1_[i], cstates2_[i] ) ;
                cout << "     " << temp_format ( cstates2_[i].getThermalHeadroom() ) <<
                     "\n";
            }
        }
        cout << "\n";
    }
        
    void print_output()
    {
        cout << "\n";
        cout << " EXEC  : instructions per nominal CPU cycle" << "\n";
        cout << " IPC   : instructions per CPU cycle" << "\n";
        cout << " FREQ  : relation to nominal CPU frequency='unhalted clock ticks'/'invariant timer ticks' (includes Intel Turbo Boost)" << "\n";
        if (cpu_model_ != PCM::ATOM)
            cout << " AFREQ : relation to nominal CPU frequency while in active state (not in power-saving C state)='unhalted clock ticks'/'invariant timer ticks while in C0-state'  (includes Intel Turbo Boost)" << "\n";
        if (cpu_model_ != PCM::ATOM && cpu_model_ != PCM::KNL)
            cout << " L3MISS: L3 cache misses " << "\n";
        if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
            cout << " L2MISS: L2 cache misses " << "\n";
        else
            cout << " L2MISS: L2 cache misses (including other core's L2 cache *hits*) " << "\n";
        if (cpu_model_ != PCM::ATOM && cpu_model_ != PCM::KNL)
            cout << " L3HIT : L3 cache hit ratio (0.00-1.00)" << "\n";
        cout << " L2HIT : L2 cache hit ratio (0.00-1.00)" << "\n";
        if (cpu_model_ != PCM::ATOM && cpu_model_ != PCM::KNL)
            cout << " L3MPI : number of L3 cache misses per instruction\n";
        if (cpu_model_ != PCM::ATOM && cpu_model_ != PCM::KNL)
            cout << " L2MPI : number of L2 cache misses per instruction\n";
        if (cpu_model_ != PCM::ATOM) cout << " READ  : bytes read from main memory controller (in GBytes)" << "\n";
        if (cpu_model_ != PCM::ATOM) cout << " WRITE : bytes written to main memory controller (in GBytes)" << "\n";
        if (m_->MCDRAMmemoryTrafficMetricsAvailable()) cout << " MCDRAM READ  : bytes read from MCDRAM controller (in GBytes)" << "\n";
        if (m_->MCDRAMmemoryTrafficMetricsAvailable()) cout << " MCDRAM WRITE : bytes written to MCDRAM controller (in GBytes)" << "\n";
        if (m_->memoryIOTrafficMetricAvailable()) cout << " IO    : bytes read/written due to IO requests to memory controller (in GBytes); this may be an over estimate due to same-cache-line partial requests" << "\n";
        if (m_->L3CacheOccupancyMetricAvailable()) cout << " L3OCC : L3 occupancy (in KBytes)" << "\n";
        if (m_->CoreLocalMemoryBWMetricAvailable()) cout << " LMB   : L3 cache external bandwidth satisfied by local memory (in MBytes)" << "\n";
        if (m_->CoreRemoteMemoryBWMetricAvailable()) cout << " RMB   : L3 cache external bandwidth satisfied by remote memory (in MBytes)" << "\n";
        cout << " TEMP  : Temperature reading in 1 degree Celsius relative to the TjMax temperature (thermal headroom): 0 corresponds to the max temperature" << "\n";
        cout << " energy: Energy in Joules" << "\n";
        cout << "\n";
        cout << "\n";
        const char * longDiv = "---------------------------------------------------------------------------------------------------------------\n";
        cout.precision(2);
        cout << std::fixed;
        
        if ( show_socket_output_ ) {
            if ( cpu_model_ == PCM::ATOM )
                cout << " Core SKT | EXEC | IPC  | FREQ | L2MISS | L2HIT | TEMP" << endl << endl;
            else if ( cpu_model_ == PCM::KNL )
                cout << " Proc Tile Core Thread | EXEC | IPC | FREQ | AFREQ | L2MISS | L2HIT | TEMP" << endl << endl;
            else {
                cout << " Core SKT | EXEC | IPC  | FREQ  | AFREQ | L3MISS | L2MISS | L3HIT | L2HIT | L3MPI | L2MPI |";

                if ( m_->L3CacheOccupancyMetricAvailable() )
                    cout << "  L3OCC |";
                if ( m_->CoreLocalMemoryBWMetricAvailable() )
                    cout << "   LMB  |";
                if ( m_->CoreRemoteMemoryBWMetricAvailable() )
                    cout << "   RMB  |";

                cout << " TEMP" << endl << endl;
            }
            if ( ! ( m_->getNumSockets() == 1 && ( cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL ) ) ) {
                cout << longDiv;
                for ( uint32 i = 0; i < m_->getNumSockets(); ++i ) {
                    cout << " SKT   " << setw ( 2 ) << i <<
                         "     " << getExecUsage ( sktstate1_[i], sktstate2_[i] ) <<
                         "   " << getIPC ( sktstate1_[i], sktstate2_[i] ) <<
                         "   " << getRelativeFrequency ( sktstate1_[i], sktstate2_[i] ) <<
                         "    " << getActiveRelativeFrequency ( sktstate1_[i], sktstate2_[i] ) <<
                         "    " << getL3CacheMisses ( sktstate1_[i], sktstate2_[i] ) <<
                         "   " << getL2CacheMisses ( sktstate1_[i], sktstate2_[i] ) <<
                         "    " << getL3CacheHitRatio ( sktstate1_[i], sktstate2_[i] ) <<
                         "    " << getL2CacheHitRatio ( sktstate1_[i], sktstate2_[i] ) <<
                         "    " << double ( getL3CacheMisses ( sktstate1_[i], sktstate2_[i] ) ) / getInstructionsRetired ( sktstate1_[i], sktstate2_[i] ) <<
                         "    " << double ( getL2CacheMisses ( sktstate1_[i], sktstate2_[i] ) ) / getInstructionsRetired ( sktstate1_[i], sktstate2_[i] );
                    if ( m_->L3CacheOccupancyMetricAvailable() )
                        cout << "   " << setw ( 6 ) << l3cache_occ_format ( getL3CacheOccupancy ( sktstate2_[i] ) ) ;
                    if ( m_->CoreLocalMemoryBWMetricAvailable() )
                        cout << "   " << setw ( 6 ) << getLocalMemoryBW ( sktstate1_[i], sktstate2_[i] );
                    if ( m_->CoreRemoteMemoryBWMetricAvailable() )
                        cout << "   " << setw ( 6 ) << getRemoteMemoryBW ( sktstate1_[i], sktstate2_[i] );

                    cout << "     " << temp_format ( sktstate2_[i].getThermalHeadroom() ) << "\n";
                }
            }
        }
        cout << longDiv;
        
        if (show_core_output_)
            print_basic();

        if (show_system_output_)
        {
            if (cpu_model_ == PCM::ATOM)
                cout << " TOTAL  *     " << getExecUsage(sstate1_, sstate2_) <<
                     "   " << getIPC(sstate1_, sstate2_) <<
                     "   " << getRelativeFrequency(sstate1_, sstate2_) <<
                     "   " << getL2CacheMisses(sstate1_, sstate2_) <<
                     "    " << getL2CacheHitRatio(sstate1_, sstate2_) <<
                     "     N/A\n";
            else if (cpu_model_ == PCM::KNL)
                cout << setw(22) << left << " TOTAL" << std::internal
                     << setw(7)  << getExecUsage(sstate1_, sstate2_)
                     << setw(6)  << getIPC(sstate1_, sstate2_)
                     << setw(7)  << getRelativeFrequency(sstate1_, sstate2_)
                     << setw(8)  << getActiveRelativeFrequency(sstate1_, sstate2_)
                     << setw(9)  << getL2CacheMisses(sstate1_, sstate2_)
                     << setw(8)  << getL2CacheHitRatio(sstate1_, sstate2_) << setw(7) << "N/A" << endl;
            else
            {
                cout << " TOTAL  *     " << getExecUsage(sstate1_, sstate2_) <<
                     "   " << getIPC(sstate1_, sstate2_) <<
                     "   " << getRelativeFrequency(sstate1_, sstate2_) <<
                     "    " << getActiveRelativeFrequency(sstate1_, sstate2_) <<
                     "    " << getL3CacheMisses(sstate1_, sstate2_) <<
                     "   " << getL2CacheMisses(sstate1_, sstate2_) <<
                     "    " << getL3CacheHitRatio(sstate1_, sstate2_) <<
                     "    " << getL2CacheHitRatio(sstate1_, sstate2_) <<
                     "    " << double(getL3CacheMisses(sstate1_, sstate2_)) / getInstructionsRetired(sstate1_, sstate2_) <<
                     "    " << double(getL2CacheMisses(sstate1_, sstate2_)) / getInstructionsRetired(sstate1_, sstate2_);
                if (m_->L3CacheOccupancyMetricAvailable())
                    cout << "    " << " N/A ";
                if (m_->CoreLocalMemoryBWMetricAvailable())
                    cout << "   " << " N/A ";
                if (m_->CoreRemoteMemoryBWMetricAvailable())
                    cout << "   " << " N/A ";

                cout << "     N/A\n";
            }
        }

        if (show_system_output_)
        {
            cout << "\n" << " Instructions retired: " << unit_format(getInstructionsRetired(sstate1_, sstate2_)) << " ; Active cycles: " << unit_format(getCycles(sstate1_, sstate2_)) << " ; Time (TSC): " << unit_format(getInvariantTSC(cstates1_[0], cstates2_[0])) << "ticks ; C0 (active,non-halted) core residency: " << (getCoreCStateResidency(0, sstate1_, sstate2_)*100.) << " %\n";
            cout << "\n";
            for (int s = 1; s <= PCM::MAX_C_STATE; ++s)
                if (m_->isCoreCStateResidencySupported(s))
                    std::cout << " C" << s << " core residency: " << (getCoreCStateResidency(s, sstate1_, sstate2_)*100.) << " %;";
            cout << "\n";
            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                if (m_->isPackageCStateResidencySupported(s))
                    std::cout << " C" << s << " package residency: " << (getPackageCStateResidency(s, sstate1_, sstate2_)*100.) << " %;";
            cout << "\n";
            if (m_->getNumCores() == m_->getNumOnlineCores())
            {
                cout << "\n" << " PHYSICAL CORE IPC                 : " << getCoreIPC(sstate1_, sstate2_) << " => corresponds to " << 100. * (getCoreIPC(sstate1_, sstate2_) / double(m_->getMaxIPC())) << " % utilization for cores in active state";
                cout << "\n" << " Instructions per nominal CPU cycle: " << getTotalExecUsage(sstate1_, sstate2_) << " => corresponds to " << 100. * (getTotalExecUsage(sstate1_, sstate2_) / double(m_->getMaxIPC())) << " % core utilization over time interval" << "\n";
            }
            cout <<" SMI count: "<< getSMICount(sstate1_, sstate2_) <<"\n";
        }

        if (show_socket_output_)
        {
            if (m_->getNumSockets() > 1 && m_->incomingQPITrafficMetricsAvailable()) // QPI info only for multi socket systems
            {
                cout << "\n" << "Intel(r) "<< m_->xPI() <<" data traffic estimation in bytes (data traffic coming to CPU/socket through "<< m_->xPI() <<" links):" << "\n" << "\n";
                const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                cout << "              ";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << " "<< m_->xPI() << i << "    ";

                if (m_->qpiUtilizationMetricsAvailable())
                {
                    cout << "| ";
                    for (uint32 i = 0; i < qpiLinks; ++i)
                        cout << " "<< m_->xPI() << i << "  ";
                }

                cout << "\n" << longDiv;


                for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                {
                    cout << " SKT   " << setw(2) << i << "     ";
                    for (uint32 l = 0; l < qpiLinks; ++l)
                        cout << getIncomingQPILinkBytes(i, l, sstate1_, sstate2_) << "   ";

                    if (m_->qpiUtilizationMetricsAvailable())
                    {
                        cout << "|  ";
                        for (uint32 l = 0; l < qpiLinks; ++l)
                            cout << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1_, sstate2_)) << "%   ";
                    }

                    cout << "\n";
                }
            }
        }

        if (show_system_output_)
        {
            cout << longDiv;

            if (m_->getNumSockets() > 1 && m_->incomingQPITrafficMetricsAvailable()) // QPI info only for multi socket systems
                cout << "Total "<< m_->xPI() <<" incoming data traffic: " << getAllIncomingQPILinkBytes(sstate1_, sstate2_) << "     "<< m_->xPI() <<" data traffic/Memory controller traffic: " << getQPItoMCTrafficRatio(sstate1_, sstate2_) << "\n";
        }

        if (show_socket_output_)
        {
            if (m_->getNumSockets() > 1 && (m_->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
            {
                cout << "\n" << "Intel(r) "<< m_->xPI() <<" traffic estimation in bytes (data and non-data traffic outgoing from CPU/socket through "<< m_->xPI() <<" links):" << "\n" << "\n";

                const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                cout << "              ";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << " " << m_->xPI() << i << "    ";


                cout << "| ";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << " "<< m_->xPI() << i << "  ";

                cout << "\n" << longDiv;

                for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                {
                    cout << " SKT   " << setw(2) << i << "     ";
                    for (uint32 l = 0; l < qpiLinks; ++l)
                        cout << getOutgoingQPILinkBytes(i, l, sstate1_, sstate2_) << "   ";

                    cout << "|  ";
                    for (uint32 l = 0; l < qpiLinks; ++l)
                        cout << setw(3) << std::dec << int(100. * getOutgoingQPILinkUtilization(i, l, sstate1_, sstate2_)) << "%   ";

                    cout << "\n";
                }

                cout << longDiv;
                cout << "Total "<< m_->xPI() <<" outgoing data and non-data traffic: " << getAllOutgoingQPILinkBytes(sstate1_, sstate2_) << "\n";
            }
        }
        if (show_socket_output_ || show_core_output_)
        {
            cout << longDiv;
            cout << "Memory metrics\n";
            cout << " SKT  " ;
            if (m_->memoryTrafficMetricsAvailable())
                cout << "  READ    WRITE  ";
            if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                cout << " MCDRAM READ   MCDRAM WRITE  ";
            if (m_->memoryIOTrafficMetricAvailable())
                cout << "   IO    ";
            if (m_->packageEnergyMetricsAvailable())
                cout << " CPU energy  ";
            if (m_->dramEnergyMetricsAvailable())
                cout << " DIMM energy";
            cout << "\n";
            for (uint32 i = 0; i < m_->getNumSockets(); ++i)
            {
                cout << setw(2) << i;
                if (m_->memoryTrafficMetricsAvailable())
                    cout << "    " << setw(5) << getBytesReadFromMC(sktstate1_[i], sktstate2_[i]) / double(1e9) <<
                         "    " << setw(5) << getBytesWrittenToMC(sktstate1_[i], sktstate2_[i]) / double(1e9);
                if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                    cout << "   " << setw(11) << getBytesReadFromEDC(sktstate1_[i], sktstate2_[i]) / double(1e9) <<
                         "    " << setw(11) << getBytesWrittenToEDC(sktstate1_[i], sktstate2_[i]) / double(1e9);
                if (m_->memoryIOTrafficMetricAvailable())
                    cout << "    " << setw(5) << getIORequestBytesFromMC(sktstate1_[i], sktstate2_[i]) / double(1e9);
                cout << "     ";
                if(m_->packageEnergyMetricsAvailable()) {
                    cout << setw(6) << getConsumedJoules(sktstate1_[i], sktstate2_[i]);
                }
                cout << "     ";
                if(m_->dramEnergyMetricsAvailable()) {
                    cout << setw(6) << getDRAMConsumedJoules(sktstate1_[i], sktstate2_[i]);
                }
                cout << "\n";
            }
            cout << "\n";
            if (m_->getNumSockets() > 1) {
                cout << " *";
                if (m_->memoryTrafficMetricsAvailable())
                    cout << "    " << setw(5) << getBytesReadFromMC(sstate1_, sstate2_) / double(1e9) <<
                         "    " << setw(5) << getBytesWrittenToMC(sstate1_, sstate2_) / double(1e9);
                if (m_->memoryIOTrafficMetricAvailable())
                    cout << "    " << setw(5) << getIORequestBytesFromMC(sstate1_, sstate2_) / double(1e9);
                cout << "     ";
                if (m_->packageEnergyMetricsAvailable()) {
                    cout << setw(6) << getConsumedJoules(sstate1_, sstate2_);
                }
                cout << "     ";
                if (m_->dramEnergyMetricsAvailable()) {
                    cout << setw(6) << getDRAMConsumedJoules(sstate1_, sstate2_);
                }
                cout << "\n";
            }
        }

    }

    void print_csv_header(std::ostream& out = std::cout)
    {
        // print first header line
        //out << "System;;";
        if (show_system_output_)
        {
            if (measurementType != MeasurementType::Default)
            {
                print_custom_events_header_1(out, "System");
            }
            else
            {
                if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
                    out << ";;;;;";
                else
                    out << "System;System;System;System;System;System;System;System;System;System;";

                if (m_->memoryTrafficMetricsAvailable())
                    out << "System;System;";

                if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                    out << "System;System;";

                out << "System;System;System;System;System;System;System;";
                if (m_->getNumSockets() > 1) { // QPI info only for multi socket systems
                    if (m_->incomingQPITrafficMetricsAvailable())
                    {
                        out << "System;System;"; // Total Incoming

                        // Incoming per Link
                        const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                        for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                        {
                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out << "System;";

                            if (m_->qpiUtilizationMetricsAvailable())
                            {
                                for (uint32 i = 0; i < qpiLinks; ++i)
                                    out << "System;";
                            }
                        }
                    }
                    if (m_->outgoingQPITrafficMetricsAvailable())
                    {
                        out << "System;"; // TotalOutGoing

                        // Outgoing per link
                        const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                        for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                        {
                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out << "System;";
                            
                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out << "System;";
                        }
                    }
                }

                /*
                out << "System Core C-States";
                for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                    if (m_->isCoreCStateResidencySupported(s))
                        out << ";";
                out << "System Pack C-States";
                for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                    if (m_->isPackageCStateResidencySupported(s))
                        out << ";";
                */
                if (m_->packageEnergyMetricsAvailable())
                    out << "System;";
                if (m_->dramEnergyMetricsAvailable())
                    out << "System;";
            }
        }

        if (show_socket_output_)
        {
            for (uint32 i = 0; i < m_->getNumSockets(); ++i)
            {
                if (measurementType != MeasurementType::Default)
                {
                    print_custom_events_header_1(out, "Socket" + std::to_string(i));
                }
                else
                {
                    if (cpu_model_ == PCM::ATOM)
                    {
                        out << "Socket" << i << ";;;;;;";
                    }
                    else if (cpu_model_ == PCM::KNL)
                    {
                        out << "Socket" << i << ";;;;;;";
                        if (m_->memoryTrafficMetricsAvailable())
                            out << ";;";
                        if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                            out << ";;";
                    }
                    else
                    {
                        out << "Socket" <<  i << ";;;;;;;;;;;";
                        if (m_->L3CacheOccupancyMetricAvailable())
                            out << ";";
                        if (m_->CoreLocalMemoryBWMetricAvailable())
                            out << ";";
                        if (m_->CoreRemoteMemoryBWMetricAvailable())
                            out << ";";
                        if (m_->memoryTrafficMetricsAvailable())
                            out << ";;";
                    }
                }
            }

            if (measurementType == MeasurementType::Default)
            {
                if (m_->getNumSockets() > 1 && (m_->incomingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
                {
                    const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                    for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                    {
                        out << "SKT" << s << "dataIn";
                        for (uint32 i = 0; i < qpiLinks; ++i)
                            out << ";";
                        if (m_->qpiUtilizationMetricsAvailable())
                        {
                            out << "SKT" << s << "dataIn (percent)";
                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out << ";";
                        }
                    }
                }

                if (m_->getNumSockets() > 1 && (m_->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
                {
                    const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                    for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                    {
                        out << "SKT" << s << "trafficOut";
                        for (uint32 i = 0; i < qpiLinks; ++i)
                            out << ";";
                        out << "SKT" << s << "trafficOut (percent)";
                        for (uint32 i = 0; i < qpiLinks; ++i)
                            out << ";";
                    }
                }


                for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                {
                    out << "SKT" << i << " Core C-State";
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isCoreCStateResidencySupported(s))
                            out << ";";
                    out << "SKT" << i << " Package C-State";
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isPackageCStateResidencySupported(s))
                            out << ";";
                }

                if (m_->packageEnergyMetricsAvailable())
                {
                    out << "Proc Energy (Joules)";
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        out << ";";
                }
                if (m_->dramEnergyMetricsAvailable())
                {
                    out << "DRAM Energy (Joules)";
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        out << ";";
                }
            }
        }

        if (show_core_output_)
        {
            for (uint32 i = 0; i < m_->getNumCores(); ++i)
            {
                if (measurementType != MeasurementType::Default)
                {
                    print_custom_events_header_1(out, "Core" + std::to_string(i));
                }
                else
                {
                    if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
                        out << "Core" << i << " (Socket" << setw(2) << m_->getSocketId(i) << ");;;;;";
                    else {
                        //out << "Core" << i << " (Socket" << setw(2) << m_->getSocketId(i) << ");;;;;;;;;;";
                        for(uint64_t j = 0; j < 10; ++j)
                            out << "Core" << i << ";";
                        if (m_->L3CacheOccupancyMetricAvailable())
                            out << "Core" << i << ";";
                        if (m_->CoreLocalMemoryBWMetricAvailable())
                            out << "Core" << i << ";";
                        if (m_->CoreRemoteMemoryBWMetricAvailable())
                            out << "Core" << i << ";";
                    }
                    /*
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isCoreCStateResidencySupported(s))
                            out << ";";
                    */
                    out << "Core" << i << ";"; // TEMP
                }
            }
        }

        // print second header line
        //out << "\nDate;Time;";
        out << "\n";
        if (show_system_output_)
        {
            if (measurementType != MeasurementType::Default)
            {
                print_custom_events_header(out);
            }
            else
            {
                if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
                    out << "EXEC;IPC;FREQ;L2MISS;L2HIT;";
                else
                    out << "EXEC;IPC;FREQ;AFREQ;L3MISS;L2MISS;L3HIT;L2HIT;L3MPI;L2MPI;";

                if (m_->memoryTrafficMetricsAvailable())
                    out << "READ;WRITE;";

                if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                    out << "MCDRAM_READ;MCDRAM_WRITE;";

                out << "INST;ACYC;TIME(ticks);PhysIPC;PhysIPC%;INSTnom;INSTnom%;";
                if (m_->getNumSockets() > 1) { // QPI info only for multi socket systems
                    if (m_->incomingQPITrafficMetricsAvailable())
                        {
                            out << "Total"<<m_->xPI()<<"in;"<<m_->xPI()<<"toMC;";

                            const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                            for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                            {
                                for (uint32 i = 0; i < qpiLinks; ++i)
                                    out <<m_->xPI() << "dataIn" << (s*qpiLinks+i) << ";";
                                if (m_->qpiUtilizationMetricsAvailable())
                                {
                                    for (uint32 i = 0; i < qpiLinks; ++i)
                                        out << m_->xPI() << "dataIn" << (s*qpiLinks+i) << " (percent);";
                                }
                            }
                        }
                    if (m_->outgoingQPITrafficMetricsAvailable())
                    {
                        out << "Total"<<m_->xPI()<<"out;";

                        const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                        for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                        {
                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out <<m_->xPI() << "trafficOut" << (s*qpiLinks+i) << ";";

                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out  << m_->xPI() << "trafficOut" << (s*qpiLinks+i) << " (percent);";
                        }
                    }
                }

                /*
                for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                    if (m_->isCoreCStateResidencySupported(s))
                        out << "C" << s << "res%;";

                for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                    if (m_->isPackageCStateResidencySupported(s))
                        out << "C" << s << "res%;";
                */

                if (m_->packageEnergyMetricsAvailable())
                    out << "Proc Energy (Joules);";
                if (m_->dramEnergyMetricsAvailable())
                    out << "DRAM Energy (Joules);";
            }
        }


        if (show_socket_output_)
        {
            for (uint32 i = 0; i < m_->getNumSockets(); ++i)
            {
                if (measurementType != MeasurementType::Default)
                {
                    print_custom_events_header(out);
                }
                else
                {
                    if (cpu_model_ == PCM::ATOM)
                    {
                        out << "EXEC;IPC;FREQ;L2MISS;L2HIT;TEMP;";
                    }
                    else if (cpu_model_ == PCM::KNL)
                    {
                        out << "EXEC;IPC;FREQ;L2MISS;L2HIT;";
                        if (m_->memoryTrafficMetricsAvailable())
                            out << "READ;WRITE;";
                        if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                            out << "MCDRAM_READ;MCDRAM_WRITE;";
                        out << "TEMP;";
                    }
                    else
                    {
                        out << "EXEC;IPC;FREQ;AFREQ;L3MISS;L2MISS;L3HIT;L2HIT;L3MPI;L2MPI;";
                        if (m_->L3CacheOccupancyMetricAvailable())
                            out << "L3OCC;";
                        if (m_->CoreLocalMemoryBWMetricAvailable())
                            out << "LMB;";
                        if (m_->CoreRemoteMemoryBWMetricAvailable())
                            out << "RMB;";
                        if (m_->memoryTrafficMetricsAvailable())
                            out << "READ;WRITE;";
                        out << "TEMP;";
                    }
                }
            }

            if (measurementType == MeasurementType::Default)
            {
                if (m_->getNumSockets() > 1 && (m_->incomingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
                {
                    const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();

                    for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                    {
                        for (uint32 i = 0; i < qpiLinks; ++i)
                            out << m_->xPI() << i << ";";

                        if (m_->qpiUtilizationMetricsAvailable())
                            for (uint32 i = 0; i < qpiLinks; ++i)
                                out << m_->xPI() << i << ";";
                    }

                }

                if (m_->getNumSockets() > 1 && (m_->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
                {
                    const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();
                    for (uint32 s = 0; s < m_->getNumSockets(); ++s)
                    {
                        for (uint32 i = 0; i < qpiLinks; ++i)
                            out << m_->xPI() << i << ";";
                        for (uint32 i = 0; i < qpiLinks; ++i)
                            out << m_->xPI() << i << ";";
                    }

                }

                /*
                for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                {
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isCoreCStateResidencySupported(s))
                            out << "C" << s << "res%;";

                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isPackageCStateResidencySupported(s))
                            out << "C" << s << "res%;";
                }
                */

                if (m_->packageEnergyMetricsAvailable())
                {
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        out << "SKT" << i << ";";
                }
                if (m_->dramEnergyMetricsAvailable())
                {
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        out << "SKT" << i << ";";
                }
            }
        }

        if (show_core_output_)
        {
            for (uint32 i = 0; i < m_->getNumCores(); ++i)
            {
                if (measurementType != MeasurementType::Default)
                {
                    print_custom_events_header(out);
                }
                else
                {
                    if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
                        out << "EXEC;IPC;FREQ;L2MISS;L2HIT;";
                    else
                    {
                        out << "EXEC;IPC;FREQ;AFREQ;L3MISS;L2MISS;L3HIT;L2HIT;L3MPI;L2MPI;";
                        if (m_->L3CacheOccupancyMetricAvailable())
                            out << "L3OCC;";
                        if (m_->CoreLocalMemoryBWMetricAvailable())
                            out << "LMB;";
                        if (m_->CoreRemoteMemoryBWMetricAvailable())
                            out << "RMB;";
                    }

                    /*
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isCoreCStateResidencySupported(s))
                            out << "C" << s << "res%;";
                    */

                    out << "TEMP;";
                }
            }
        }
    }

    void print_csv(std::ostream& out = std::cout)
    {
        
#ifndef _MSC_VER
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
#endif
        tm tt = pcm_localtime();
        char old_fill = out.fill('0');
        out.precision(3);
        out << endl;
        /*
        out << endl << setw(4) << 1900 + tt.tm_year << '-' << setw(2) << 1 + tt.tm_mon << '-'
             << setw(2) << tt.tm_mday << ';' << setw(2) << tt.tm_hour << ':'
             << setw(2) << tt.tm_min << ':' << setw(2) << tt.tm_sec
#ifdef _MSC_VER
             << ';';
#else
             << "." << setw(3) << ceil(timestamp.tv_usec / 1000) << ';';
#endif
        */
        out.fill(old_fill);

        if (show_system_output_)
        {
            if ( measurementType == MeasurementType::Default)
            {
                if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
                    out << getExecUsage(sstate1_, sstate2_) <<
                        ';' << getIPC(sstate1_, sstate2_) <<
                        ';' << getRelativeFrequency(sstate1_, sstate2_) <<
                        ';' << float_format(getL2CacheMisses(sstate1_, sstate2_)) <<
                        ';' << getL2CacheHitRatio(sstate1_, sstate2_) <<
                        ';';
                else
                    out << getExecUsage(sstate1_, sstate2_) <<
                        ';' << getIPC(sstate1_, sstate2_) <<
                        ';' << getRelativeFrequency(sstate1_, sstate2_) <<
                        ';' << getActiveRelativeFrequency(sstate1_, sstate2_) <<
                        ';' << float_format(getL3CacheMisses(sstate1_, sstate2_)) <<
                        ';' << float_format(getL2CacheMisses(sstate1_, sstate2_)) <<
                        ';' << getL3CacheHitRatio(sstate1_, sstate2_) <<
                        ';' << getL2CacheHitRatio(sstate1_, sstate2_) <<
                        ';' << double(getL3CacheMisses(sstate1_, sstate2_)) / getInstructionsRetired(sstate1_, sstate2_) <<
                        ';' << double(getL2CacheMisses(sstate1_, sstate2_)) / getInstructionsRetired(sstate1_, sstate2_) << ";";

                if (m_->memoryTrafficMetricsAvailable())
                    out << getBytesReadFromMC(sstate1_, sstate2_) / double(1e9) <<
                        ';' << getBytesWrittenToMC(sstate1_, sstate2_) / double(1e9) << ';';

                if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                    out << getBytesReadFromEDC(sstate1_, sstate2_) / double(1e9) <<
                        ';' << getBytesWrittenToEDC(sstate1_, sstate2_) / double(1e9) << ';';

                out << float_format(getInstructionsRetired(sstate1_, sstate2_)) << ";"
                    << float_format(getCycles(sstate1_, sstate2_)) << ";"
                    << float_format(getInvariantTSC(cstates1_[0], cstates2_[0])) << ";"
                    << getCoreIPC(sstate1_, sstate2_) << ";"
                    << 100. * (getCoreIPC(sstate1_, sstate2_) / double(m_->getMaxIPC())) << ";"
                    << getTotalExecUsage(sstate1_, sstate2_) << ";"
                    << 100. * (getTotalExecUsage(sstate1_, sstate2_) / double(m_->getMaxIPC())) << ";";

                if (m_->getNumSockets() > 1) { // QPI info only for multi socket systems
                    if (m_->incomingQPITrafficMetricsAvailable())
                    {
                        out << float_format(getAllIncomingQPILinkBytes(sstate1_, sstate2_)) << ";"
                            << getQPItoMCTrafficRatio(sstate1_, sstate2_) << ";";

                        const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();
                        for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        {
                            for (uint32 l = 0; l < qpiLinks; ++l)
                                out << float_format(getIncomingQPILinkBytes(i, l, sstate1_, sstate2_)) << ";";

                            if (m_->qpiUtilizationMetricsAvailable())
                            {
                                for (uint32 l = 0; l < qpiLinks; ++l)
                                    out << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1_, sstate2_)) << ";";
                            }
                        }
                    }
                    if (m_->outgoingQPITrafficMetricsAvailable())
                    {
                        out << float_format(getAllOutgoingQPILinkBytes(sstate1_, sstate2_)) << ";";

                        const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();
                        for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        {
                            for (uint32 l = 0; l < qpiLinks; ++l)
                                out << float_format(getOutgoingQPILinkBytes(i, l, sstate1_, sstate2_)) << ";";

                            for (uint32 l = 0; l < qpiLinks; ++l)
                                out << setw(3) << std::dec << int(100. * getOutgoingQPILinkUtilization(i, l, sstate1_, sstate2_)) << ";";
                        }
                    }
                }
                
                /*
                for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                    if (m_->isCoreCStateResidencySupported(s))
                        out << getCoreCStateResidency(s, sstate1_, sstate2_) * 100 << ";";

                for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                    if (m_->isPackageCStateResidencySupported(s))
                        out << getPackageCStateResidency(s, sstate1_, sstate2_) * 100 << ";";
                */

                if (m_->packageEnergyMetricsAvailable())
                    out << getConsumedJoules(sstate1_, sstate2_) << ";";
                if (m_->dramEnergyMetricsAvailable())
                    out << getDRAMConsumedJoules(sstate1_, sstate2_) << ";";
            }
            else
            {
                print_custom_events(sstate1_, sstate2_, out);
            }
        }

        if (show_socket_output_)
        {
            for (uint32 i = 0; i < m_->getNumSockets(); ++i)
            {
                if ( measurementType == MeasurementType::Default)
                {
                    if (cpu_model_ == PCM::ATOM)
                    {
                        out << getExecUsage(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getIPC(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getRelativeFrequency(sktstate1_[i], sktstate2_[i]) <<
                            ';' << float_format(getL2CacheMisses(sktstate1_[i], sktstate2_[i])) <<
                            ';' << getL2CacheHitRatio(sktstate1_[i], sktstate2_[i]);
                    }
                    else if (cpu_model_ == PCM::KNL)
                    {
                        out << getExecUsage(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getIPC(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getRelativeFrequency(sktstate1_[i], sktstate2_[i]) <<
                            ';' << float_format(getL2CacheMisses(sktstate1_[i], sktstate2_[i])) <<
                            ';' << getL2CacheHitRatio(sktstate1_[i], sktstate2_[i]);
                        if (m_->memoryTrafficMetricsAvailable())
                            out << ';' << getBytesReadFromMC(sktstate1_[i], sktstate2_[i]) / double(1e9) <<
                                ';' << getBytesWrittenToMC(sktstate1_[i], sktstate2_[i]) / double(1e9);
                        if (m_->MCDRAMmemoryTrafficMetricsAvailable())
                            out << ';' << getBytesReadFromEDC(sktstate1_[i], sktstate2_[i]) / double(1e9) <<
                                ';' << getBytesWrittenToEDC(sktstate1_[i], sktstate2_[i]) / double(1e9);
                        out << ';' << temp_format(sktstate2_[i].getThermalHeadroom()) << ';';
                    }
                    else
                    {
                        out << getExecUsage(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getIPC(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getRelativeFrequency(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getActiveRelativeFrequency(sktstate1_[i], sktstate2_[i]) <<
                            ';' << float_format(getL3CacheMisses(sktstate1_[i], sktstate2_[i])) <<
                            ';' << float_format(getL2CacheMisses(sktstate1_[i], sktstate2_[i])) <<
                            ';' << getL3CacheHitRatio(sktstate1_[i], sktstate2_[i]) <<
                            ';' << getL2CacheHitRatio(sktstate1_[i], sktstate2_[i]) <<
                            ';' << double(getL3CacheMisses(sktstate1_[i], sktstate2_[i])) / getInstructionsRetired(sktstate1_[i], sktstate2_[i]) <<
                            ';' << double(getL2CacheMisses(sktstate1_[i], sktstate2_[i])) / getInstructionsRetired(sktstate1_[i], sktstate2_[i]) ;
                        if (m_->L3CacheOccupancyMetricAvailable())
                            out << ';' << l3cache_occ_format(getL3CacheOccupancy(sktstate2_[i]));
                        if (m_->CoreLocalMemoryBWMetricAvailable())
                            out << ';' << getLocalMemoryBW(sktstate1_[i], sktstate2_[i]);
                        if (m_->CoreRemoteMemoryBWMetricAvailable())
                            out << ';' << getRemoteMemoryBW(sktstate1_[i], sktstate2_[i]) ;
                        if (m_->memoryTrafficMetricsAvailable())
                            out << ';' << getBytesReadFromMC(sktstate1_[i], sktstate2_[i]) / double(1e9) <<
                                ';' << getBytesWrittenToMC(sktstate1_[i], sktstate2_[i]) / double(1e9);
                        out << ';' << temp_format(sktstate2_[i].getThermalHeadroom()) << ';';
                    }
                }
                else
                {
                    print_custom_events(sktstate1_[i], sktstate2_[i], out);
                }
            }

            if ( measurementType == MeasurementType::Default)
            {
                if (m_->getNumSockets() > 1 && (m_->incomingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
                {
                    const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                    {
                        for (uint32 l = 0; l < qpiLinks; ++l)
                            out << float_format(getIncomingQPILinkBytes(i, l, sstate1_, sstate2_)) << ";";

                        if (m_->qpiUtilizationMetricsAvailable())
                        {
                            for (uint32 l = 0; l < qpiLinks; ++l)
                                out << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1_, sstate2_)) << ";";
                        }
                    }
                }

                if (m_->getNumSockets() > 1 && (m_->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
                {
                    const uint32 qpiLinks = (uint32)m_->getQPILinksPerSocket();
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                    {
                        for (uint32 l = 0; l < qpiLinks; ++l)
                            out << float_format(getOutgoingQPILinkBytes(i, l, sstate1_, sstate2_)) << ";";

                        for (uint32 l = 0; l < qpiLinks; ++l)
                            out << setw(3) << std::dec << int(100. * getOutgoingQPILinkUtilization(i, l, sstate1_, sstate2_)) << ";";
                    }
                }

                /*
                for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                {
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isCoreCStateResidencySupported(s))
                            out << getCoreCStateResidency(s, sktstate1_[i], sktstate2_[i]) * 100 << ";";

                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isPackageCStateResidencySupported(s))
                            out << getPackageCStateResidency(s, sktstate1_[i], sktstate2_[i]) * 100 << ";";
                }
                */

                if (m_->packageEnergyMetricsAvailable())
                {
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        out << getConsumedJoules(sktstate1_[i], sktstate2_[i]) << ";";
                }
                if (m_->dramEnergyMetricsAvailable())
                {
                    for (uint32 i = 0; i < m_->getNumSockets(); ++i)
                        out << getDRAMConsumedJoules(sktstate1_[i], sktstate2_[i]) << " ;";
                }
            }
        }

        if (show_core_output_)
        {
            for (uint32 i = 0; i < m_->getNumCores(); ++i)
            {
                if ( measurementType == MeasurementType::Default)
                {
                    if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL)
                        out << getExecUsage(cstates1_[i], cstates2_[i]) <<
                            ';' << getIPC(cstates1_[i], cstates2_[i]) <<
                            ';' << getRelativeFrequency(cstates1_[i], cstates2_[i]) <<
                            ';' << float_format(getL2CacheMisses(cstates1_[i], cstates2_[i])) <<
                            ';' << getL2CacheHitRatio(cstates1_[i], cstates2_[i]) <<
                            ';';
                    else
                    {
                        out << getExecUsage(cstates1_[i], cstates2_[i]) <<
                            ';' << getIPC(cstates1_[i], cstates2_[i]) <<
                            ';' << getRelativeFrequency(cstates1_[i], cstates2_[i]) <<
                            ';' << getActiveRelativeFrequency(cstates1_[i], cstates2_[i]) <<
                            ';' << float_format(getL3CacheMisses(cstates1_[i], cstates2_[i])) <<
                            ';' << float_format(getL2CacheMisses(cstates1_[i], cstates2_[i])) <<
                            ';' << getL3CacheHitRatio(cstates1_[i], cstates2_[i]) <<
                            ';' << getL2CacheHitRatio(cstates1_[i], cstates2_[i]) <<
                            ';' << double(getL3CacheMisses(cstates1_[i], cstates2_[i])) / getInstructionsRetired(cstates1_[i], cstates2_[i]) <<
                            ';' << double(getL2CacheMisses(cstates1_[i], cstates2_[i])) / getInstructionsRetired(cstates1_[i], cstates2_[i]);
                        if (m_->L3CacheOccupancyMetricAvailable())
                            out << ';' << l3cache_occ_format(getL3CacheOccupancy(cstates2_[i]));
                        if (m_->CoreLocalMemoryBWMetricAvailable())
                            out << ';' << getLocalMemoryBW(cstates1_[i], cstates2_[i]);
                        if (m_->CoreRemoteMemoryBWMetricAvailable())
                            out << ';' << getRemoteMemoryBW(cstates1_[i], cstates2_[i]);
                        out << ';';
                    }
                    /*
                    for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
                        if (m_->isCoreCStateResidencySupported(s))
                            out << getCoreCStateResidency(s, cstates1_[i], cstates2_[i]) * 100 << ";";
                    */

                    out << temp_format(cstates2_[i].getThermalHeadroom()) << ';';
                }
                else
                {
                    print_custom_events(cstates1_[i], cstates2_[i], out);
                }
            }
        }
    }

    void filterCores(const std::bitset<MAX_CORES> & ycores)
    {
        ycores_ = ycores;
    }

private:
    std::bitset<MAX_CORES> ycores_;
    bool show_partial_core_output_ = false;
    double delay = 1;
    PCM * m_;

    std::vector<CoreCounterState> cstates1_, cstates2_;
    std::vector<SocketCounterState> sktstate1_, sktstate2_;
    SystemCounterState sstate1_, sstate2_;
    int cpu_model_;
    long diff_usec = 0; // deviation of clock is useconds between measurements
    int calibrated = PCM_CALIBRATION_INTERVAL - 2; // keeps track is the clock calibration needed
    MeasurementType measurementType = MeasurementType::Default;
    CustomCoreEventPair measurementEvents;
        
    void init()
    {
        cout << "\n\nReset: " << reset_pmu_ << endl;
      m_ = PCM::getInstance();

        if(m_->getNumCores() > MAX_CORES)
        {
            cerr << "Error: --yescores option is enabled, but #define MAX_CORES " << MAX_CORES << " is less than  m_->getNumCores() = " << m_->getNumCores() << endl;
            cerr << "There is a potential to crash the system. Please increase MAX_CORES to at least " << m_->getNumCores() << " and re-enable this option." << endl;
            exit(EXIT_FAILURE);
        }

        if (disable_JKT_workaround_) m_->disableJKTWorkaround();

        if (reset_pmu_)
        {
            cerr << "\n Resetting PMU configuration" << endl;
            m_->resetPMU();
        }

        if (allow_multiple_instances_)
        {
            m_->allowMultipleInstances();
        }
        
        PCM::ErrorCode status;
        
        // program() creates common semaphore for the singleton, so ideally to be called before any other references to PCM
        switch(measurementType)
        {
            case MeasurementType::Stalls:
                measurementEvents = StallEvents;

                status = m_->program(PCM::CUSTOM_CORE_EVENTS, &measurementEvents.second);
                break;
            case MeasurementType::TMAM:
                measurementEvents = TMAMLocatorEventNames;
                status = m_->program(PCM::EXT_CUSTOM_CORE_EVENTS, getTMAMLocatorEvents());
                //measurementEvents = TMAMLocatorEvent2;
                //status = m_->program(PCM::CUSTOM_CORE_EVENTS, TMAMLocatorEvent2.second.data());
                break;
            default: status = m_->program();
        }
            

        switch (status)
        {
        case PCM::Success:
            break;
        case PCM::MSRAccessDenied:
            cerr << "Access to Processor Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
            exit(EXIT_FAILURE);
        case PCM::PMUBusy:
            cerr << "Access to Processor Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
            cerr << "Alternatively you can try running PCM with option -r to reset PMU configuration at your own risk." << endl;
            exit(EXIT_FAILURE);
        default:
            cerr << "Access to Processor Counter Monitor has denied (Unknown error)." << endl;
            exit(EXIT_FAILURE);
        }

        cerr << "\nDetected " << " \"Intel(r) microarchitecture codename " <<
             m_->getUArchCodename() << "\" stepping "<< m_->getCPUStepping() << endl;
        //cerr << "\nDetected " << m_->getCPUBrandString() << " \"Intel(r) microarchitecture codename " <<
    //    m_->getUArchCodename() << "\" stepping "<< m_->getCPUStepping() << endl;
        cpu_model_ = m_->getCPUModel();
        
        m_->setBlocked(false);

        // cerr << "DEBUG: Delay: " << delay << " seconds. Blocked: " << m_->isBlocked() << endl;

        ycores_.set();
    };

public:
    
    bool show_core_output_ = true;
    bool show_socket_output_ = false;
    bool show_system_output_ = true;
    bool csv_output_ = true;
    bool reset_pmu_ = true;
    bool allow_multiple_instances_ = false;
    bool disable_JKT_workaround_ = false; // as per http://software.intel.com/en-us/articles/performance-impact-when-sampling-certain-llc-events-on-snb-ep-with-vtune

    bool redirect_to_file_ = false;
    std::string output_file_name_ = "";
    bool should_print_header_ = true;
    /*
    CacheMeter(std::string csv_output): csv_output_(true)
    {
        m_->setOutput(filename);
    }
    CacheMeter(std::bitset<MAX_CORES> ycores_): ycores_(ycores_), show_partial_core_output_(true)
    {

    }
    */
    CacheMeter(bool show_core_output, bool show_partial_core_output,
               bool show_socket_output ,bool show_system_output,
               bool csv_output, bool reset_pmu,
               bool allow_multiple_instances, bool disable_JKT_workaround,
               unsigned num_custom_qpi_events = 0,
               std::string output_file_name = "")
        : show_core_output_(show_core_output), show_partial_core_output_(show_partial_core_output),
          show_socket_output_(show_socket_output), show_system_output_(show_system_output),
          csv_output_(csv_output), reset_pmu_(reset_pmu),
          allow_multiple_instances_(allow_multiple_instances), disable_JKT_workaround_(disable_JKT_workaround),
          redirect_to_file_(!output_file_name.empty()), output_file_name_(output_file_name)
    {
        init();
    };
    
    CacheMeter(std::string output_file_name = "")
        : redirect_to_file_(!output_file_name.empty()), output_file_name_(output_file_name)
    {
        init();
    };
    
    CacheMeter(MeasurementType mt, std::string output_file_name = "")
        : redirect_to_file_(!output_file_name.empty()), output_file_name_(output_file_name)
        , measurementType(mt)
    {
        init();
    };
    
    ~CacheMeter()
    {
        m_->cleanup();
    }
    
    void print()
    {
        if (csv_output_)
        {
            if(redirect_to_file_)
            {
                std::ofstream out = std::ofstream(output_file_name_, std::ofstream::out|std::ofstream::app);

                print_csv(out);

                out.close();
            }
            else
            {
                print_csv();
            }
        }
        else
        {
            print_output();
        }
    };
    
    void Start()
    {
        m_->getAllCounterStates(sstate1_, sktstate1_, cstates1_, ycores_);
    };
    
    void Stop(bool do_print = false, unsigned int numberOfIterations = 1)
    {
        if (csv_output_ && do_print && should_print_header_) {
            should_print_header_ = false;

            // Redirect cout to file
            //std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
            std::ofstream out;
            if(redirect_to_file_)
            {
                out = std::ofstream(output_file_name_, std::ofstream::out|std::ofstream::trunc);
                //std::cout.rdbuf(out.rdbuf()); //redirect std::cout to out.txt!

                print_csv_header(out);

                out.close();

            }
            else
            {
                print_csv_header();
            }
            
        }

        unsigned int i = 1;

        while ((i <= numberOfIterations) || (numberOfIterations == 0))
        {
            if (!csv_output_) cout << std::flush;
            int delay_ms = int(delay * 1000);
            int calibrated_delay_ms = delay_ms;

            // compensation of delay on Linux/UNIX
            // to make the sampling interval as monotone as possible
            struct timeval start_ts, end_ts;
            if (calibrated == 0) {
                gettimeofday(&end_ts, NULL);
                diff_usec = (end_ts.tv_sec - start_ts.tv_sec)*1000000.0 + (end_ts.tv_usec - start_ts.tv_usec);
                calibrated_delay_ms = delay_ms - diff_usec / 1000.0;
            }

            if (numberOfIterations != 0 || m_->isBlocked() == false)
            {
                MySleepMs(calibrated_delay_ms);
            }

            m_->getAllCounterStates(sstate2_, sktstate2_, cstates2_, ycores_);

            if( do_print )
            {
                print();
            }

            
            // sanity checks
            if( measurementType == MeasurementType::Default)
                if (cpu_model_ == PCM::ATOM || cpu_model_ == PCM::KNL )
                {
                    assert(getNumberOfCustomEvents(0, sstate1_, sstate2_) == getL2CacheMisses(sstate1_, sstate2_));
                    assert(getNumberOfCustomEvents(1, sstate1_, sstate2_) == getL2CacheMisses(sstate1_, sstate2_) + getL2CacheHits(sstate1_, sstate2_));
                }
                else
                {
                    assert(getNumberOfCustomEvents(0, sstate1_, sstate2_) == getL3CacheMisses(sstate1_, sstate2_));
                    if (m_->useSkylakeEvents()) {
                        assert(getNumberOfCustomEvents(1, sstate1_, sstate2_) == getL3CacheHits(sstate1_, sstate2_));
                        assert(getNumberOfCustomEvents(2, sstate1_, sstate2_) == getL2CacheMisses(sstate1_, sstate2_));
                    }
                    else {
                        assert(getNumberOfCustomEvents(1, sstate1_, sstate2_) == getL3CacheHitsNoSnoop(sstate1_, sstate2_));
                        assert(getNumberOfCustomEvents(2, sstate1_, sstate2_) == getL3CacheHitsSnoop(sstate1_, sstate2_));
                    }
                    assert(getNumberOfCustomEvents(3, sstate1_, sstate2_) == getL2CacheHits(sstate1_, sstate2_));
                }

            std::swap(sstate1_, sstate2_);
            std::swap(sktstate1_, sktstate2_);
            std::swap(cstates1_, cstates2_);

            ++i;
        }
    }
};

}
#endif
