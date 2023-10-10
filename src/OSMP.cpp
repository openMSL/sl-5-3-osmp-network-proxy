//
// Copyright 2016 -- 2018 PMSF IT Consulting Pierre R. Mai
// Copyright 2023 BMW AG
// SPDX-License-Identifier: MPL-2.0
//

#include "OSMP.h"

/*
 * Debug Breaks
 *
 * If you define DEBUG_BREAKS the FMU will automatically break
 * into an attached Debugger on all major computation functions.
 * Note that the FMU is likely to break all environments if no
 * Debugger is actually attached when the breaks are triggered.
 */
#if defined(DEBUG_BREAKS) && !defined(NDEBUG)
#if defined(__has_builtin) && !defined(__ibmxl__)
#if __has_builtin(__builtin_debugtrap)
#define DEBUGBREAK() __builtin_debugtrap()
#elif __has_builtin(__debugbreak)
#define DEBUGBREAK() __debugbreak()
#endif
#endif
#if !defined(DEBUGBREAK)
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <intrin.h>
#define DEBUGBREAK() __debugbreak()
#else
#include <signal.h>
#if defined(SIGTRAP)
#define DEBUGBREAK() raise(SIGTRAP)
#else
#define DEBUGBREAK() raise(SIGABRT)
#endif
#endif
#endif
#else
#define DEBUGBREAK()
#endif

#include <algorithm>
#include <cstdint>
#include <string>

using namespace std;

#ifdef PRIVATE_LOG_PATH
ofstream OSMP::private_log_file;
#endif

/*
 * ProtocolBuffer Accessors
 */

void* DecodeIntegerToPointer(fmi2Integer hi, fmi2Integer lo)
{
#if PTRDIFF_MAX == INT64_MAX
    union Addrconv
    {
        struct
        {
            int lo;
            int hi;
        } base;
        unsigned long long address;
    } myaddr{};
    myaddr.base.lo = lo;
    myaddr.base.hi = hi;
    return (void*)myaddr.address;
#elif PTRDIFF_MAX == INT32_MAX
    return reinterpret_cast<void*>(lo);
#else
#error "Cannot determine 32bit or 64bit environment!"
#endif
}

void EncodePointerToInteger(const void* ptr, fmi2Integer& hi, fmi2Integer& lo)
{
#if PTRDIFF_MAX == INT64_MAX
    union Addrconv
    {
        struct
        {
            int lo;
            int hi;
        } base;
        unsigned long long address;
    } myaddr{};
    myaddr.address = (unsigned long long)ptr;
    hi = myaddr.base.hi;
    lo = myaddr.base.lo;
#elif PTRDIFF_MAX == INT32_MAX
    hi = 0;
    lo = reinterpret_cast<int>(ptr);
#else
#error "Cannot determine 32bit or 64bit environment!"
#endif
}

/*
 * Actual Core Content
 */

fmi2Status OSMP::DoInit()
{

    /* Booleans */
    for (int& boolean_var : boolean_vars_)
    {
        boolean_var = fmi2False;
    }

    /* Integers */
    for (int& integer_var : integer_vars_)
    {
        integer_var = 0;
    }

    /* Reals */
    for (double& real_var : real_vars_)
    {
        real_var = 0.0;
    }

    /* Strings */
    for (auto& string_var : string_vars_)
    {
        string_var = "";
    }

    // Set default values
    SetFmiSender(1);
    SetFmiReceiver(0);
    SetFmiPushPull(1);
    SetFmiIp("127.0.0.1");
    SetFmiPort("3456");

    return fmi2OK;
}

fmi2Status OSMP::DoStart(fmi2Boolean tolerance_defined, fmi2Real tolerance, fmi2Real start_time, fmi2Boolean stop_time_defined, fmi2Real stop_time)
{
    return fmi2OK;
}

fmi2Status OSMP::DoEnterInitializationMode()
{
    return fmi2OK;
}

fmi2Status OSMP::DoExitInitializationMode()
{

    string address = "tcp://" + FmiIp() + ":" + FmiPort();
    std::cout << address << std::endl;
    const char* protocol = address.c_str();

    if (FmiSender() != 0)
    {
        if (FmiPushPull() != 0)  // Push/Pull configuration
        {
            socket_ = zmq::socket_t(context_, ZMQ_PUSH);
        }
        else  // Server/Client configuration
        {
            socket_ = zmq::socket_t(context_, ZMQ_REP);
        }
        const int wait_time_ms = 5000;
        zmq_setsockopt(socket_, ZMQ_SNDTIMEO, &wait_time_ms, sizeof(wait_time_ms));
        socket_.bind(protocol);
    }
    else
    {
        if (FmiPushPull() != 0)  // Push/Pull configuration
        {
            socket_ = zmq::socket_t(context_, ZMQ_PULL);
        }
        else  // Server/Client configuration
        {
            socket_ = zmq::socket_t(context_, ZMQ_REQ);
        }
        const int wait_time_ms = 5000;
        zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &wait_time_ms, sizeof(wait_time_ms));
        socket_.connect(protocol);
    }

    return fmi2OK;
}

void OSMP::ProcessMessage(zmq::message_t& message)
{
    EncodePointerToInteger(message.data(), integer_vars_[FMI_INTEGER_OSI_OUT_BASEHI_IDX], integer_vars_[FMI_INTEGER_OSI_OUT_BASELO_IDX]);
    integer_vars_[FMI_INTEGER_OSI_OUT_SIZE_IDX] = (fmi2Integer)message.size();
    NormalLog("OSMP", "Providing %08X %08X, writing from %p ...", integer_vars_[FMI_INTEGER_OSI_OUT_BASEHI_IDX], integer_vars_[FMI_INTEGER_OSI_OUT_BASELO_IDX], message.data());
    swap(message, last_message_);
    SetFmiValid(1);
}

fmi2Status OSMP::DoCalc(fmi2Real current_communication_point, fmi2Real communication_step_size, fmi2Boolean no_set_fmu_state_prior_to_current_pointfmi_2_component)
{
    fmi2Status output_status = fmi2OK;

    void* buffer = DecodeIntegerToPointer(integer_vars_[FMI_INTEGER_OSI_IN_BASEHI_IDX], integer_vars_[FMI_INTEGER_OSI_IN_BASELO_IDX]);
    int buffer_size = integer_vars_[FMI_INTEGER_OSI_IN_SIZE_IDX];

    if (FmiSender() != 0)
    {
        if (FmiPushPull() == 0)  // Server/Client configuration
        {
            zmq::message_t request_message;
            socket_.recv(request_message, zmq::recv_flags::none);
        }
        zmq::message_t send_message(buffer, buffer_size, nullptr);
        auto success = socket_.send(send_message, zmq::send_flags::none);
        if (success.has_value())
        {
            ProcessMessage(send_message);
        }
        else
        {
            NormalLog("OSMP", "Sender time out: No receiver with given IP and port found.");
            output_status = fmi2Error;
        }
    }
    else if (FmiReceiver() != 0)
    {
        if (FmiPushPull() == 0)  // Server/Client configuration
        {
            std::stringstream s;
            s << "ready";
            zmq::message_t send_message(s.str().data(), s.str().size());
            socket_.send(send_message, zmq::send_flags::none);
        }
        zmq::message_t rec_message;
        auto success = socket_.recv(rec_message, zmq::recv_flags::none);
        if (success.has_value())
        {
            ProcessMessage(rec_message);
        }
        else
        {
            NormalLog("OSMP", "Receiver time out: No sender with given IP and port found.");
            output_status = fmi2Error;
        }
    }
    else
    {
        NormalLog("OSMP", "Either sender or receiver has to be set to true.");
        output_status = fmi2Error;
    }

    return output_status;
}

fmi2Status OSMP::DoTerm()
{
    return fmi2OK;
}

void OSMP::DoFree() {}

/*
 * Generic C++ Wrapper Code
 */

OSMP::OSMP(fmi2String theinstance_name,
           fmi2Type thefmu_type,
           fmi2String thefmu_guid,
           fmi2String thefmu_resource_location,
           const fmi2CallbackFunctions* thefunctions,
           fmi2Boolean thevisible,
           fmi2Boolean thelogging_on)
    : instance_name_(theinstance_name),
      fmu_type_(thefmu_type),
      fmu_guid_(thefmu_guid),
      fmu_resource_location_(thefmu_resource_location),
      functions_(*thefunctions),
      visible_(thevisible != 0),
      logging_on_(thelogging_on != 0)
{
    logging_categories_.clear();
    logging_categories_.insert("FMI");
    logging_categories_.insert("OSMP");
    logging_categories_.insert("OSI");
}

fmi2Status OSMP::SetDebugLogging(fmi2Boolean thelogging_on, size_t n_categories, const fmi2String categories[])
{
    FmiVerboseLog("fmi2SetDebugLogging(%s)", thelogging_on != 0 ? "true" : "false");
    logging_on_ = thelogging_on != 0;
    if ((categories != nullptr) && (n_categories > 0))
    {
        logging_categories_.clear();
        for (size_t i = 0; i < n_categories; i++)
        {
            if (0 == strcmp(categories[i], "FMI"))
            {
                logging_categories_.insert("FMI");
            }
            else if (0 == strcmp(categories[i], "OSMP"))
            {
                logging_categories_.insert("OSMP");
            }
            else if (0 == strcmp(categories[i], "OSI"))
            {
                logging_categories_.insert("OSI");
            }
        }
    }
    else
    {
        logging_categories_.clear();
        logging_categories_.insert("FMI");
        logging_categories_.insert("OSMP");
        logging_categories_.insert("OSI");
    }
    return fmi2OK;
}

fmi2Component OSMP::Instantiate(fmi2String instance_name,
                                fmi2Type fmu_type,
                                fmi2String fmu_guid,
                                fmi2String fmu_resource_location,
                                const fmi2CallbackFunctions* functions,
                                fmi2Boolean visible,
                                fmi2Boolean logging_on)
{
    auto* myc = new OSMP(instance_name, fmu_type, fmu_guid, fmu_resource_location, functions, visible, logging_on);

    if (myc == nullptr)
    {
        FmiVerboseLogGlobal(R"(fmi2Instantiate("%s",%d,"%s","%s","%s",%d,%d) = NULL (alloc failure))",
                            instance_name,
                            fmu_type,
                            fmu_guid,
                            (fmu_resource_location != nullptr) ? fmu_resource_location : "<NULL>",
                            "FUNCTIONS",
                            visible,
                            logging_on);
        return nullptr;
    }

    if (myc->DoInit() != fmi2OK)
    {
        FmiVerboseLogGlobal(R"(fmi2Instantiate("%s",%d,"%s","%s","%s",%d,%d) = NULL (DoInit failure))",
                            instance_name,
                            fmu_type,
                            fmu_guid,
                            (fmu_resource_location != nullptr) ? fmu_resource_location : "<NULL>",
                            "FUNCTIONS",
                            visible,
                            logging_on);
        delete myc;
        return nullptr;
    }
    FmiVerboseLogGlobal(R"(fmi2Instantiate("%s",%d,"%s","%s","%s",%d,%d) = %p)",
                        instance_name,
                        fmu_type,
                        fmu_guid,
                        (fmu_resource_location != nullptr) ? fmu_resource_location : "<NULL>",
                        "FUNCTIONS",
                        visible,
                        logging_on,
                        myc);
    return (fmi2Component)myc;
}

fmi2Status OSMP::SetupExperiment(fmi2Boolean tolerance_defined, fmi2Real tolerance, fmi2Real start_time, fmi2Boolean stop_time_defined, fmi2Real stop_time)
{
    FmiVerboseLog("fmi2SetupExperiment(%d,%g,%g,%d,%g)", tolerance_defined, tolerance, start_time, stop_time_defined, stop_time);
    return DoStart(tolerance_defined, tolerance, start_time, stop_time_defined, stop_time);
}

fmi2Status OSMP::EnterInitializationMode()
{
    FmiVerboseLog("fmi2EnterInitializationMode()");
    return DoEnterInitializationMode();
}

fmi2Status OSMP::ExitInitializationMode()
{
    FmiVerboseLog("fmi2ExitInitializationMode()");
    return DoExitInitializationMode();
}

fmi2Status OSMP::DoStep(fmi2Real current_communication_point, fmi2Real communication_step_size, fmi2Boolean no_set_fmu_state_prior_to_current_pointfmi_2_component)
{
    FmiVerboseLog("fmi2DoStep(%g,%g,%d)", current_communication_point, communication_step_size, no_set_fmu_state_prior_to_current_pointfmi_2_component);
    return DoCalc(current_communication_point, communication_step_size, no_set_fmu_state_prior_to_current_pointfmi_2_component);
}

fmi2Status OSMP::Terminate()
{
    FmiVerboseLog("fmi2Terminate()");
    return DoTerm();
}

fmi2Status OSMP::Reset()
{
    FmiVerboseLog("fmi2Reset()");

    DoFree();
    return DoInit();
}

void OSMP::FreeInstance()
{
    FmiVerboseLog("fmi2FreeInstance()");
    DoFree();
}

fmi2Status OSMP::GetReal(const fmi2ValueReference vr[], size_t nvr, fmi2Real value[])
{
    FmiVerboseLog("fmi2GetReal(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_REAL_VARS)
        {
            value[i] = real_vars_[vr[i]];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::GetInteger(const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[])
{
    FmiVerboseLog("fmi2GetInteger(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_INTEGER_VARS)
        {
            value[i] = integer_vars_[vr[i]];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::GetBoolean(const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[])
{
    FmiVerboseLog("fmi2GetBoolean(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_BOOLEAN_VARS)
        {
            value[i] = boolean_vars_[vr[i]];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::GetString(const fmi2ValueReference vr[], size_t nvr, fmi2String value[])
{
    FmiVerboseLog("fmi2GetString(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_STRING_VARS)
        {
            value[i] = string_vars_[vr[i]].c_str();
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::SetReal(const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[])
{
    FmiVerboseLog("fmi2SetReal(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_REAL_VARS)
        {
            real_vars_[vr[i]] = value[i];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::SetInteger(const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[])
{
    FmiVerboseLog("fmi2SetInteger(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_INTEGER_VARS)
        {
            integer_vars_[vr[i]] = value[i];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::SetBoolean(const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[])
{
    FmiVerboseLog("fmi2SetBoolean(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_BOOLEAN_VARS)
        {
            boolean_vars_[vr[i]] = value[i];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

fmi2Status OSMP::SetString(const fmi2ValueReference vr[], size_t nvr, const fmi2String value[])
{
    FmiVerboseLog("fmi2SetString(...)");
    for (size_t i = 0; i < nvr; i++)
    {
        if (vr[i] < FMI_STRING_VARS)
        {
            string_vars_[vr[i]] = value[i];
        }
        else
        {
            return fmi2Error;
        }
    }
    return fmi2OK;
}

/*
 * FMI 2.0 Co-Simulation Interface API
 */

extern "C" {

FMI2_Export const char* fmi2GetTypesPlatform()
{
    return fmi2TypesPlatform;
}

FMI2_Export const char* fmi2GetVersion()
{
    return fmi2Version;
}

FMI2_Export fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean logging_on, size_t n_categories, const fmi2String categories[])
{
    auto* myc = (OSMP*)c;
    return myc->SetDebugLogging(logging_on, n_categories, categories);
}

/*
 * Functions for Co-Simulation
 */
FMI2_Export fmi2Component fmi2Instantiate(fmi2String instance_name,
                                          fmi2Type fmu_type,
                                          fmi2String fmu_guid,
                                          fmi2String fmu_resource_location,
                                          const fmi2CallbackFunctions* functions,
                                          fmi2Boolean visible,
                                          fmi2Boolean logging_on)
{
    return OSMP::Instantiate(instance_name, fmu_type, fmu_guid, fmu_resource_location, functions, visible, logging_on);
}

FMI2_Export fmi2Status
fmi2SetupExperiment(fmi2Component c, fmi2Boolean tolerance_defined, fmi2Real tolerance, fmi2Real start_time, fmi2Boolean stop_time_defined, fmi2Real stop_time)
{
    auto* myc = (OSMP*)c;
    return myc->SetupExperiment(tolerance_defined, tolerance, start_time, stop_time_defined, stop_time);
}

FMI2_Export fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    auto* myc = (OSMP*)c;
    return myc->EnterInitializationMode();
}

FMI2_Export fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    auto* myc = (OSMP*)c;
    return myc->ExitInitializationMode();
}

FMI2_Export fmi2Status fmi2DoStep(fmi2Component c,
                                  fmi2Real current_communication_point,
                                  fmi2Real communication_step_size,
                                  fmi2Boolean no_set_fmu_state_prior_to_current_pointfmi2_component)
{
    auto* myc = (OSMP*)c;
    return myc->DoStep(current_communication_point, communication_step_size, no_set_fmu_state_prior_to_current_pointfmi2_component);
}

FMI2_Export fmi2Status fmi2Terminate(fmi2Component c)
{
    auto* myc = (OSMP*)c;
    return myc->Terminate();
}

FMI2_Export fmi2Status fmi2Reset(fmi2Component c)
{
    auto* myc = (OSMP*)c;
    return myc->Reset();
}

FMI2_Export void fmi2FreeInstance(fmi2Component c)
{
    auto* myc = (OSMP*)c;
    myc->FreeInstance();
    delete myc;
}

/*
 * Data Exchange Functions
 */
FMI2_Export fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[])
{
    auto* myc = (OSMP*)c;
    return myc->GetReal(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[])
{
    auto* myc = (OSMP*)c;
    return myc->GetInteger(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[])
{
    auto* myc = (OSMP*)c;
    return myc->GetBoolean(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[])
{
    auto* myc = (OSMP*)c;
    return myc->GetString(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[])
{
    auto* myc = (OSMP*)c;
    return myc->SetReal(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[])
{
    auto* myc = (OSMP*)c;
    return myc->SetInteger(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[])
{
    auto* myc = (OSMP*)c;
    return myc->SetBoolean(vr, nvr, value);
}

FMI2_Export fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[])
{
    auto* myc = (OSMP*)c;
    return myc->SetString(vr, nvr, value);
}

/*
 * Unsupported Features (FMUState, Derivatives, Async DoStep, Status Enquiries)
 */
FMI2_Export fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* fmu_state)
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate fmu_state)
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* fmu_state)
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate fmu_state, size_t* size)
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate fmu_state, fmi2Byte serialized_state[], size_t size)
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serialized_state[], size_t size, fmi2FMUstate* fmu_state)
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetDirectionalDerivative(fmi2Component c,
                                                    const fmi2ValueReference v_unknown_ref[],
                                                    size_t n_unknown,
                                                    const fmi2ValueReference v_known_ref[],
                                                    size_t n_known,
                                                    const fmi2Real dv_known[],
                                                    fmi2Real dv_unknown[])
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetRealInputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[], const fmi2Real value[])
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetRealOutputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[], fmi2Real value[])
{
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2CancelStep(fmi2Component c)
{
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status* value)
{
    return fmi2Discard;
}

FMI2_Export fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real* value)
{
    return fmi2Discard;
}

FMI2_Export fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value)
{
    return fmi2Discard;
}

FMI2_Export fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value)
{
    return fmi2Discard;
}

FMI2_Export fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String* value)
{
    return fmi2Discard;
}
}
