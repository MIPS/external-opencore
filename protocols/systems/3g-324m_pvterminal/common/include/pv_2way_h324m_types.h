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
        OSCL_IMPORT_REF CPVUserInput(): iRefCounter(1) {};

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
        OSCL_IMPORT_REF CPVUserInputDtmf(uint8 aInput, bool aUpdate, uint16 aDuration = 0) :
                iInput(aInput),
                iIsUpdate(aUpdate),
                iDuration(aDuration)
        {};

        /**
         * Destructor.
         **/
        OSCL_IMPORT_REF ~CPVUserInputDtmf() {};

        // from CPVUserInput
        OSCL_IMPORT_REF PV2WayUserInputType GetType()
        {
            return PV_DTMF;
        }

        /**
         * Return the user input DTMF tone
         *
         * @returns Returns the input DTMF tone.
         **/
        OSCL_IMPORT_REF uint8 GetInput()
        {
            return iInput;
        }

        /**
         * Return if the DTMF tone is an update
         *
         * @returns Returns if the input DTMF tone is an update.
         **/
        OSCL_IMPORT_REF bool IsUpdate()
        {
            return iIsUpdate;
        }

        /**
         * Return the duration of the update.
         *
         * @returns Returns the duration of the update.
         **/
        OSCL_IMPORT_REF uint16 GetDuration()
        {
            return iDuration;
        }

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
        OSCL_IMPORT_REF CPVUserInputAlphanumeric(uint8* apInput, uint16 aLen)
        {
            if (aLen)
            {
                int err;
                ipInput = OSCL_STATIC_CAST(uint8*, OSCL_MALLOC(aLen));
                OSCL_TRY(err, oscl_memcpy(ipInput, apInput, aLen));
                OSCL_FIRST_CATCH_ANY(err,
                                     OSCL_DELETE(ipInput);
                                     OSCL_LEAVE(OsclErrGeneral);
                                    );
                iLength = aLen;
            }
        }

        /**
         * Destructor.
         **/
        OSCL_IMPORT_REF ~CPVUserInputAlphanumeric()
        {
            if (ipInput)
            {
                OSCL_DELETE(ipInput);
            }
        }

        // from CPVUserInput
        OSCL_IMPORT_REF PV2WayUserInputType GetType()
        {
            return PV_ALPHANUMERIC;
        }

        /**
         * Return the user input alphanumeric user input
         *
         * @returns Returns pointer to alphanumeric user input.
         **/
        OSCL_IMPORT_REF uint8* GetInput()
        {
            return ipInput;
        }

        /**
         * Return the size of alphanumeric user input
         *
         * @returns Returns size of alphanumeric user input.
         **/
        OSCL_IMPORT_REF uint16 GetLength()
        {
            return iLength;
        }

    protected:
        /**
         * The input alphanumeric string.
         **/
        uint8* ipInput;  /* We own the memory*/
        uint16 iLength; /* length of the string */
};

#endif
