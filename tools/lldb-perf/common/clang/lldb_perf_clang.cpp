//===-- lldb_perf_clang.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb-perf/lib/Timer.h"
#include "lldb-perf/lib/Metric.h"
#include "lldb-perf/lib/Measurement.h"
#include "lldb-perf/lib/Results.h"
#include "lldb-perf/lib/TestCase.h"
#include "lldb-perf/lib/Xcode.h"
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <getopt.h>

using namespace lldb_perf;

class ClangTest : public TestCase
{
public:
    ClangTest () :
        TestCase(),
        m_time_create_target ([this] () -> void
                              {
                                  m_memory_change_create_target.Start();
                                  m_target = m_debugger.CreateTarget(m_exe_path.c_str());
                                  m_memory_change_create_target.Stop();
                              }, "time-create-target", "The time it takes to create a target."),
        m_time_set_bp_main([this] () -> void
                              {
                                  m_memory_change_break_main.Start();
                                  m_target.BreakpointCreateByName("main");
                                  m_memory_change_break_main.Stop();
                              }, "time-set-break-main", "Elapsed time it takes to set a breakpoint at 'main' by name."),
        m_memory_change_create_target(),
        m_memory_change_break_main(),
        m_memory_total(),
        m_exe_path(),
        m_out_path(),
        m_launch_info (NULL),
        m_use_dsym (false)
    {
    }

    virtual
    ~ClangTest ()
    {
    }
    
    virtual bool
	Setup (int argc, const char** argv)
    {
        if (m_exe_path.empty())
            return false;
        m_launch_info.SetArguments(argv, false);
        return true;
    }
    
    void
    DoTest ()
    {
    }
    
	virtual void
	TestStep (int counter, ActionWanted &next_action)
    {
        switch (counter)
        {
            case 0:
                {
                    m_memory_total.Start();
                    m_time_total.Start();
                    
                    // Time creating the target
                    m_time_create_target();
                    
                    m_time_set_bp_main();

                    m_time_launch_stop_main.Start();
                    const char *clang_argv[] = { "clang --version", NULL };
                    SBLaunchInfo launch_info(clang_argv);
                    Launch (launch_info);
                }
                break;
            case 1:
                puts("stop");
                m_time_launch_stop_main.Stop();
                m_time_total.Stop();
//                next_action.StepOver(m_thread);
//                break;
//            case 2:
//                next_action.StepOver(m_thread);
//                break;
//            case 3:
//                next_action.StepOver(m_thread);
//                break;
            default:
                m_memory_total.Stop();
                next_action.Kill();
                break;
        }
    }
    
    void
    WriteResults (Results &results)
    {
        Results::Dictionary& results_dict = results.GetDictionary();
        
        m_time_set_bp_main.WriteAverageValue(results);
        results_dict.Add ("memory-change-create-target",
                          "Memory increase that occurs due to creating the target.",
                          m_memory_change_create_target.GetDeltaValue().GetResult(NULL, NULL));
        
        results_dict.Add ("memory-change-break-main",
                          "Memory increase that occurs due to setting a breakpoint at main by name.",
                          m_memory_change_break_main.GetDeltaValue().GetResult(NULL, NULL));

        m_time_create_target.WriteAverageValue(results);
        results_dict.Add ("memory-total-break-main",
                          "The total memory that the current process is using after setting the first breakpoint.",
                          m_memory_total.GetStopValue().GetResult(NULL, NULL));
        
        results_dict.AddDouble("time-launch-stop-main",
                               "The time it takes to launch the process and stop at main.",
                               m_time_launch_stop_main.GetDeltaValue());

        results_dict.AddDouble("time-total",
                               "The time it takes to create the target, set breakpoint at main, launch clang and hit the breakpoint at main.",
                               m_time_total.GetDeltaValue());
        results.Write(GetResultFilePath());
    }
    
    
    
    const char *
    GetExecutablePath () const
    {
        if (m_exe_path.empty())
            return NULL;
        return m_exe_path.c_str();
    }

    const char *
    GetResultFilePath () const
    {
        if (m_out_path.empty())
            return NULL;
        return m_out_path.c_str();
    }

    void
    SetExecutablePath (const char *path)
    {
        if (path && path[0])
            m_exe_path = path;
        else
            m_exe_path.clear();
    }
    
    void
    SetResultFilePath (const char *path)
    {
        if (path && path[0])
            m_out_path = path;
        else
            m_out_path.clear();
    }

    void
    SetUseDSYM (bool b)
    {
        m_use_dsym = b;
    }


    
private:
    // C++ formatters
    TimeMeasurement<std::function<void()>> m_time_create_target;
    TimeMeasurement<std::function<void()>> m_time_set_bp_main;
    MemoryGauge m_memory_change_create_target;
    MemoryGauge m_memory_change_break_main;
    MemoryGauge m_memory_total;
    TimeGauge m_time_launch_stop_main;
    TimeGauge m_time_total;
    std::string m_exe_path;
    std::string m_out_path;
    SBLaunchInfo m_launch_info;
    bool m_use_dsym;

};


struct Options
{
    std::string clang_path;
    std::string out_file;
    bool verbose;
    bool use_dsym;
    bool error;
    bool print_help;
    
    Options() :
        verbose (false),
        error (false),
        print_help (false)
    {
    }
};

static struct option g_long_options[] = {
    { "verbose",    no_argument,            NULL, 'v' },
    { "clang",      required_argument,      NULL, 'c' },
    { "out-file",   required_argument,      NULL, 'o' },
    { "dsym",       no_argument,            NULL, 'd' },
    { NULL,         0,                      NULL,  0  }
};


std::string
GetShortOptionString (struct option *long_options)
{
    std::string option_string;
    for (int i = 0; long_options[i].name != NULL; ++i)
    {
        if (long_options[i].flag == NULL)
        {
            option_string.push_back ((char) long_options[i].val);
            switch (long_options[i].has_arg)
            {
                default:
                case no_argument:
                    break;
                case required_argument:
                    option_string.push_back (':');
                    break;
                case optional_argument:
                    option_string.append (2, ':');
                    break;
            }
        }
    }
    return option_string;
}

int main(int argc, const char * argv[])
{

    // Prepare for & make calls to getopt_long.
    
    std::string short_option_string (GetShortOptionString(g_long_options));
    
    ClangTest test;

    Options option_data;
    bool done = false;

#if __GLIBC__
    optind = 0;
#else
    optreset = 1;
    optind = 1;
#endif
    while (!done)
    {
        int long_options_index = -1;
        const int short_option = ::getopt_long_only (argc,
                                                     const_cast<char **>(argv),
                                                     short_option_string.c_str(),
                                                     g_long_options,
                                                     &long_options_index);
        
        switch (short_option)
        {
            case 0:
                // Already handled
                break;

            case -1:
                done = true;
                break;

            case '?':
                option_data.print_help = true;
                break;

            case 'h':
                option_data.print_help = true;
                break;
                
            case 'v':
                option_data.verbose = true;
                break;
                
            case 'c':
                {
                    SBFileSpec file(optarg);
                    if (file.Exists())
                        test.SetExecutablePath(optarg);
                    else
                        fprintf(stderr, "error: file specified in --clang (-c) option doesn't exist: '%s'\n", optarg);
                }
                break;
                
            case 'o':
                test.SetResultFilePath(optarg);
                break;
                
            case 'd':
                test.SetUseDSYM(true);
                break;
                
            default:
                option_data.error = true;
                option_data.print_help = true;
                fprintf (stderr, "error: unrecognized option %c\n", short_option);
                break;
        }
    }


    if (test.GetExecutablePath() == NULL)
    {
        // --clang is mandatory
        option_data.print_help = true;
        option_data.error = true;
        fprintf (stderr, "error: the '--clang=PATH' option is mandatory\n");
    }

    if (option_data.print_help)
    {
        puts(R"(
NAME
    lldb_perf_clang -- a tool that measures LLDB peformance while debugging clang.

SYNOPSIS
    lldb_perf_clang --clang=PATH [--out-file=PATH --verbose --dsym] -- [clang options]
             
DESCRIPTION
    Runs a set of static timing and memory tasks against clang and outputs results
    to a plist file.
)");
    }
    if (option_data.error)
    {
        exit(1);
    }

    // Update argc and argv after parsing options
    argc -= optind;
    argv += optind;

    test.SetVerbose(true);
    TestCase::Run(test, argc, argv);
    return 0;
}
