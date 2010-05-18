/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef PV_2WAY_H324M_TYPES_H_INCLUDED
#define PV_2WAY_H324M_TYPES_H_INCLUDED

#ifndef PV_2WAY_BASIC_TYPES_H_INCLUDED
#include "pv_2way_basic_types.h"
#endif

#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif

#ifndef PV_INTERFACE_H
#include "pv_interface.h"
#endif

#ifndef PVT_COMMON_H_INCLUDED
#include "pvt_common.h"
#endif

// MACROS
/** The maximum number of supported formats for user input **/
#define PV_2WAY_MAX_USER_INPUT_FORMATS 4
/** The maximum skew that can be taken into account for both outgoing and incoming sides **/
#define PV_2WAY_MAX_SKEW_MS 1000

/**
 * TPVPostDisconnectOption Enum
 *
 * TPVPostDisconnectOption emumerates the mode the peer wants to transition to after the disconnect
 **/
typedef enum TPVPostDisconnectOption
{
    EDisconnectLine,
    EAnalogueTelephony
} PV2WayPostDisconnectOption;

/**
 * PV2Way324InitInfo Class
 *
 * PV2Way324InitInfo implements the PV2Way324InitInfo interface
 * and is used for 324M specific initialization.
 *
 **/

class PV2Way324InitInfo : public PV2WayInitInfo
{
    public:
        /**
         * Retrieves the class name
         *
         * @param aClassName
         *         A reference to an OSCL_wString, which is to hold the subclass
         *          name, this class will assign the string "CPV2Way324InitInfo"
         * @returns void
         **/
        virtual void GetInitInfoClassName(OSCL_wString &aClassName)
        {
            aClassName = _STRLIT_WCHAR("CPV2Way324InitInfo");
        }
        PV2Way324InitInfo() : PV2WayInitInfo(), iMultiplexingDelayMs(0) {};
        virtual ~PV2Way324InitInfo() {};

        /**
        *  The Multiplexing delay in Milliseconds
        **/
        uint16 iMultiplexingDelayMs;
};


/**
 * PV2Way324ConnectOptions Class
 *
 * PV2Way324ConnectOptions implements the PV2WayConnectOptions interface
 * and is used for 324M specific initialization.
 *
 **/
class PV2Way324ConnectOptions : public PV2WayConnectOptions
{
    public:
        /**
         * Constructor
         * @param disconnectTimeout
         *         The interval to wait after initiating a disconnect before stopping signalling
         *
         **/
        PV2Way324ConnectOptions(uint32 aDisconnectTimeoutInterval)
                : iDisconnectTimeoutInterval(aDisconnectTimeoutInterval) {}

        PV2Way324ConnectOptions() : PV2WayConnectOptions(), iDisconnectTimeoutInterval(0) {};
        virtual ~PV2Way324ConnectOptions() {};

        /**
         * Retrieves the class name
         *
         * @param aClassName
         *         A reference to an OSCL_wString, which is to hold the subclass
         *          name, this class will assign the string "CPV2Way324ConnectInfo"
         * @returns void
         **/
        virtual void GetConnectInfoClassName(OSCL_wString &aClassName)
        {
            aClassName = _STRLIT_WCHAR("PV2Way324ConnectOptions");
        }

        /**
        * The disconnect timeout interval in units of 100ms
        **/
        uint32 iDisconnectTimeoutInterval;
};

/**
 * PVH223AlConfig class
 *
 * This is the base class for H.223 Adaptation Layer configuration
 *
 **/
class PVH223AlConfig
{
    public:
        virtual ~PVH223AlConfig() {};
        enum PVH223AlIndex
        {
            PVH223_AL1 = 1,
            PVH223_AL2 = 2,
            PVH223_AL3 = 4
        };
        virtual PVH223AlIndex IsA() const = 0;
};


/**
 * PVH223Al1Config class
 *
 * This class defines configuration information for H.223 Adaptation Layer 1
 *
 **/
class PVH223Al1Config : public PVH223AlConfig
{
    public:
        PVH223AlIndex IsA()const
        {
            return PVH223_AL1;
        }

        bool iFramed;
};

/**
 * PVH223Al2Config class
 *
 * This class defines configuration information for H.223 Adaptation Layer 2
 *
 **/
class PVH223Al2Config : public PVH223AlConfig
{
    public:
        PVH223AlIndex IsA()const
        {
            return PVH223_AL2;
        }

        bool iUseSequenceNumbers;
};

/**
 * PVH223Al3Config class
 *
 * This class defines configuration information for H.223 Adaptation Layer 3
 *
 **/
class PVH223Al3Config : public PVH223AlConfig
{
    public:
        PVH223AlIndex IsA()const
        {
            return PVH223_AL3;
        }

        uint32 iControlFieldOctets;
        uint32 iSendBufferSize;
};

/**
 * CPVUserInputCapability class
 * User Input message capabilities
 **/
class CPVUserInputCapability: public HeapBase, public PVInterface
{
    public:
        /**
         * Constructor of CPVUserInputCapability class.
         *
         **/
        OSCL_IMPORT_REF CPVUserInputCapability(): iRefCounter(1) {};

        /**
         * Virtual destructor
         **/
        virtual ~CPVUserInputCapability() {};

        /**
         * Function to return basic string capability
         *
         * @return true if basic string is supported
         **/
        bool HasBasicString()
        {
            return iBasicString;
        }

        /**
         * Function to return IA5 string capability
         *
         * @return true if A5 string is supported
         **/
        bool HasIa5String()
        {
            return iIa5String;
        }

        /**
         * Function to return general string capability
         *
         * @return true if general string is supported
         **/
        bool HasGeneralString()
        {
            return iGeneralString;
        }

        /**
         * Function to return DTMF capability
         *
         * @return true if DTMF is supported
         **/
        bool HasDtmf()
        {
            return iDtmf;
        }

        /**
         * Function to return any user input capability
         *
         * @return true if any user input capability is supported
         **/
        bool HasUserInputCapability()
        {
            if (iBasicString || iIa5String || iGeneralString || iDtmf)
            {
                return true;
            }
            return false;
        }


        /**
         * Function to set basic string capability
         *
         * @param aBasicString basic string capability
         **/
        void SetBasicString(bool aBasicString)
        {
            iBasicString = aBasicString;
        }

        /**
         * Function to return IA5 string capability
         *
         * @param aIa5String IA5 string capability
         **/
        void SetIa5String(bool aIa5String)
        {
            iIa5String = aIa5String;
        }

        /**
         * Function to set general string capability
         *
         * @param aGeneralString general string capability
         **/
        void SetGeneralString(bool aGeneralString)
        {
            iGeneralString = aGeneralString;
        }

        /**
         * Function to set DTMF capability
         *
         * @param aDtmf DTMF capability
         **/
        void SetDtmf(bool aDtmf)
        {
            iDtmf = aDtmf;
        }

        /**
         * Function to add capability
         *
         * @param aFormatType format type capability
         **/
        void AddCapability(PVMFFormatType aFormatType)
        {
            if (aFormatType == PVMF_MIME_USERINPUT_BASIC_STRING)
            {
                iBasicString = true;
            }
            else if (aFormatType == PVMF_MIME_USERINPUT_IA5_STRING)
            {
                iIa5String = true;
            }
            else if (aFormatType == PVMF_MIME_USERINPUT_GENERAL_STRING)
            {
                iGeneralString = true;
            }
            else if (aFormatType == PVMF_MIME_USERINPUT_DTMF)
            {
                iDtmf = true;
            }
        }

        // from PVInterface
        void addRef()
        {
            iRefCounter++;
        }

        void removeRef()
        {
            --iRefCounter;
            if (iRefCounter == 0)
                OSCL_DELETE(this);
        }

    private:

        virtual bool queryInterface(const PVUuid& arUuid, PVInterface*& aprInterface)
        {
            OSCL_UNUSED_ARG(arUuid);
            OSCL_UNUSED_ARG(aprInterface);
            return false;
        }

        uint32 iRefCounter;

        bool iBasicString;
        bool iIa5String;
        bool iGeneralString;
        bool iDtmf;
};

/**
 * UserInputType enum
 * Enumeration of user input types
 **/
typedef enum UserInputType
{
    PV_ALPHANUMERIC = 0,
    PV_DTMF
} PV2WayUserInputType;


/**
 * CPVUserInput class
 * Base class for User Input mesages
 **/
class CPVUserInput: public HeapBase, public PVInterface
{
    public:
        /**
         * Constructor of CPVUserInputDtmf class.
         *
         **/
        OSCL_IMPORT_REF CPVUserInput();

        /**
         * Virtual destructor
         **/
        virtual ~CPVUserInput() {};
        /**
         * Virtual function to return the user input type
         **/
        virtual PV2WayUserInputType GetType() = 0;

        // from PVInterface
        void addRef()
        {
            iRefCounter++;
        }

        void removeRef()
        {
            --iRefCounter;
            if (iRefCounter == 0)
                OSCL_DELETE(this);
        }

    private:

        virtual bool queryInterface(const PVUuid& arUuid, PVInterface*& aprInterface)
        {
            OSCL_UNUSED_ARG(arUuid);
            OSCL_UNUSED_ARG(aprInterface);
            return false;
        }

        uint32 iRefCounter;

};

/**
 * CPVUserInputDtmf Class
 *
 * CPVUserInputDtmf class contains DTMF signal information from
 * an H.245 UserInputIndication message.
 **/
class CPVUserInputDtmf : public CPVUserInput
{
    public:
        /**
         * Constructor of CPVUserInputDtmf class.
         *
         * @param aInput
         *         The input DTMF tone.
         * @param aUpdate
         *         Indicates if this is an update to a continuing DTMF tone.
         * @param aDuration
         *         The duration of the update in milli-seconds.
         * @leave   This method can leave with one of the following error codes
         *          KErrNoMemory if the SDK failed to allocate memory during this operation.
         * @returns void
         **/
        OSCL_IMPORT_REF CPVUserInputDtmf(uint8 aInput, bool aUpdate, uint16 aDuration = 0);

        /**
         * Destructor.
         **/
        //OSCL_IMPORT_REF ~CPVUserInputDtmf() {};
        OSCL_IMPORT_REF ~CPVUserInputDtmf();

        // from CPVUserInput
        OSCL_IMPORT_REF PV2WayUserInputType GetType();

        /**
         * Return the user input DTMF tone
         *
         * @returns Returns the input DTMF tone.
         **/
        OSCL_IMPORT_REF uint8 GetInput();

        /**
         * Return if the DTMF tone is an update
         *
         * @returns Returns if the input DTMF tone is an update.
         **/
        OSCL_IMPORT_REF bool IsUpdate();

        /**
         * Return the duration of the update.
         *
         * @returns Returns the duration of the update.
         **/
        OSCL_IMPORT_REF uint16 GetDuration();

    private:
        /**
         * Constructor.
         *
         * @param aInput
         *         The input DTMF tone.
         * @param aUpdate
         *         Indicates if this is an update to a continuing DTMF tone.
         * @param aDuration
         *         The duration of the update in milli-seconds.
         **/
        uint8 iInput;
        bool iIsUpdate;
        uint16 iDuration;
};

/**
 * CPVUserInputAlphanumeric Class
 *
 * CPVUserInputAlphanumeric class contains an alphanumeric string from
 * an H.245 UserInputIndication message.
 **/
class CPVUserInputAlphanumeric : public CPVUserInput
{
    public:
        /**
         * Constructor of CPVUserInputAlphanumeric class.
         *
         * @param apInput The input alphanumeric string (T.50 encoded).
         * @param aLen The lenght of alphanumeric string in bytes
         * @returns none
         * @leave  This method can leave with one of the following error codes
         *         OsclErrGeneral memory copy failed
         **/
        OSCL_IMPORT_REF CPVUserInputAlphanumeric(uint8* apInput, uint16 aLen);

        /**
         * Destructor.
         **/
        OSCL_IMPORT_REF ~CPVUserInputAlphanumeric();

        // from CPVUserInput
        OSCL_IMPORT_REF PV2WayUserInputType GetType();

        /**
         * Return the user input alphanumeric user input
         *
         * @returns Returns pointer to alphanumeric user input.
         **/
        OSCL_IMPORT_REF uint8* GetInput();

        /**
         * Return the size of alphanumeric user input
         *
         * @returns Returns size of alphanumeric user input.
         **/
        OSCL_IMPORT_REF uint16 GetLength();

    protected:
        /**
         * The input alphanumeric string.
         **/
        uint8* ipInput;  /* We own the memory*/
        uint16 iLength; /* length of the string */
};

class CPVLogicalChannelIndication : public HeapBase, public PVInterface
{
    public:
        OSCL_IMPORT_REF CPVLogicalChannelIndication(TPVChannelId channelId);
        OSCL_IMPORT_REF ~CPVLogicalChannelIndication();
        OSCL_IMPORT_REF TPVChannelId GetChannelId();

        // from PVInterface
        OSCL_IMPORT_REF void addRef();

        OSCL_IMPORT_REF void removeRef();

    private:

        virtual bool queryInterface(const PVUuid& arUuid, PVInterface*& aprInterface)
        {
            OSCL_UNUSED_ARG(arUuid);
            OSCL_UNUSED_ARG(aprInterface);
            return false;
        }

        uint32 iRefCounter;
        TPVChannelId iChannelId;
};

class CPVVideoSpatialTemporalTradeoff : public HeapBase, public PVInterface
{
    public:
        OSCL_IMPORT_REF CPVVideoSpatialTemporalTradeoff(TPVChannelId aChannelId, uint8 aTradeoff);
        OSCL_IMPORT_REF ~CPVVideoSpatialTemporalTradeoff();
        OSCL_IMPORT_REF TPVChannelId GetChannelId();
        OSCL_IMPORT_REF uint8 GetTradeoff();

        // from PVInterface
        OSCL_IMPORT_REF void addRef();

        OSCL_IMPORT_REF void removeRef();

    private:

        virtual bool queryInterface(const PVUuid& arUuid, PVInterface*& aprInterface)
        {
            OSCL_UNUSED_ARG(arUuid);
            OSCL_UNUSED_ARG(aprInterface);
            return false;
        }

        uint32 iRefCounter;
        TPVChannelId iChannelId;
        uint8 iTradeoff;
};

#endif
