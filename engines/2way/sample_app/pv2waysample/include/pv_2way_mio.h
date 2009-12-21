/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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
#ifndef PV_2WAY_MIO_H_INCLUDED
#define PV_2WAY_MIO_H_INCLUDED
////////////////////////////////////////////////////////////////////////////
// This file includes the class definition for the base class for an MIO
//
// This class includes pure virtual functions to create and delete the MIO
// that need to be overriden in derived classes (such as the AudioSink, AudioSource,
// VideoSink, VideoSource classes).
//
// This class also defines common MIO behavior: Adding/Removing an MIO, handling
// events with respect to the MIO, etc.
//
// With this design the only code that needs to be in the derived classes is how
// to create and delete the specific MIO.
//
////////////////////////////////////////////////////////////////////////////

#include "pv_engine_observer.h"
#include "pv_2way_interface.h"

#include "pvmi_media_io_fileoutput.h"

#include "pv_2way_mio_node_factory.h"
#include "pv_2way_codecspecifier.h"

#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif

#ifndef OSCL_MAP_H_INCLUDED
#include "oscl_map.h"
#endif


class DummyMIOSettings;

#define TITLE_LENGTH   300
#define FILENAME_LEN   255
#define OUTPUT_TXT_LEN   20
#define EXTN_LEN       10
#define PORT_LEN 8
#define TRANS_LEN 64
#define LOOP_LEN 64

#define OUTPUT_MIO_SIZE 300

typedef enum
{
    EPVFile,
    EPVDevice,
    EPVNone,
    EPVEndSourceSinkType
} PV2WaySourceSinkType;


TPVChannelId GetIdFromLocalBuffer(PVAsyncInformationalEvent &aEvent);


TPVDirection GetDir(int32 dir);
PV2WayMediaType GetMediaType(int32 mediaType);

typedef PV2WayRegFormatTypeCompare<OsclMemAllocator> pvmf_format_type_key_compare_class;

class PV2WayMIOObserver
{
    public:
        virtual ~PV2WayMIOObserver() {};
        virtual PVMFNodeInterface* CreateMIONode(CodecSpecifier* aformat, TPVDirection adir) = 0;
        virtual void DeleteMIONode(CodecSpecifier* aformat,
                                   TPVDirection adir,
                                   PVMFNodeInterface** aMioNode) = 0;

};

class PV2WayMIO
{
    public:
        OSCL_IMPORT_REF PV2WayMIO(Oscl_Vector<PVMFFormatType, OsclMemAllocator>* aFormats,
                                  PV2WayMIOObserver* aObserver);
        virtual OSCL_IMPORT_REF ~PV2WayMIO();

        OSCL_IMPORT_REF PVCommandId Add();
        OSCL_IMPORT_REF PVCommandId Remove();
        OSCL_IMPORT_REF void PrintFormatTypes();


        OSCL_IMPORT_REF void Init();
        OSCL_IMPORT_REF void SetTerminal(CPV2WayInterface *aTerminal)
        {
            iTerminal = aTerminal;
        }

        OSCL_IMPORT_REF PVCommandId HandleEvent(const PVAsyncInformationalEvent& aEvent);
        OSCL_IMPORT_REF void HandleClosingTrack();
        OSCL_IMPORT_REF void HandleCloseTrack();

        OSCL_IMPORT_REF void ResetCompleted();

        OSCL_IMPORT_REF bool EstablishedMIO()
        {
            if (iChannelId != 0)
                return true;
            return false;

        }

        OSCL_IMPORT_REF void SetPreviewHandle(uint32 aPreviewHandle)
        {
            iPreviewHandle = aPreviewHandle;
        }
        //virtual OSCL_IMPORT_REF int Create();
        virtual OSCL_IMPORT_REF void Delete();


        OSCL_IMPORT_REF void AddCompleted(const PVCmdResponse& aResponse);
        OSCL_IMPORT_REF void RemoveCompleted(const PVCmdResponse& aResponse);


        bool IsAddId(PVCommandId aAddId)
        {
            return (iAddId == aAddId);
        }
        bool IsRemoveId(PVCommandId aRemoveId)
        {
            return (iRemoveId == aRemoveId);
        }
        bool IsPauseId(PVCommandId aPauseId)
        {
            return (iPauseId == aPauseId);
        }
        bool IsResumeId(PVCommandId aResumeId)
        {
            return (iResumeId == aResumeId);
        }

        bool IsChannelId(TPVChannelId aChannelId)
        {
            return (iChannelId == aChannelId);
        }

        bool IsNextChannelId(TPVChannelId aNextChannelId)
        {
            return (iNextChannelId == aNextChannelId);
        }


        bool IsRemoved()
        {
            return (iMioNode == NULL);
        }
        bool IsAdded()
        {
            return (iAdded);
        }

        OSCL_IMPORT_REF void Closed();
        virtual OSCL_IMPORT_REF int AddCodec(PVMFFormatType aFormat);
        virtual OSCL_IMPORT_REF int AddCodec(PvmiMIOFileInputSettings& aFileSettings) = 0;
        virtual OSCL_IMPORT_REF int AddCodec(PVMFFileInputSettings& aFileSettings) = 0;
        virtual OSCL_IMPORT_REF int AddCodec(DummyMIOSettings& aSettings) = 0;

        OSCL_IMPORT_REF void ClearCodecs();
        OSCL_IMPORT_REF int AddFormat(PVMFFormatType aFormat);
        OSCL_IMPORT_REF int AddFormat(PvmiMIOFileInputSettings& format);
        OSCL_IMPORT_REF int AddFormat(PVMFFileInputSettings& format);
        OSCL_IMPORT_REF int AddFormat(DummyMIOSettings& format);
        PVMFFormatType& GetSelectedFormat()
        {
            return iMySelectedFormat;
        }
    protected:
        CodecSpecifier* FormatInList(PVMFFormatType& aType);


        CodecSpecifier* FormatMatchesCapabilities(const PVAsyncInformationalEvent& aEvent);

        void ParseResponse(const PVAsyncInformationalEvent& aEvent,
                           PVMFFormatType& aMimeString,
                           int& aMedia_type);

        void OutputInfo(PVLogger::log_level_type aLogLevel, const char * str, ...)
        {
            va_list args;
            va_start(args, str);
            // output to screen everything in formatted string
            vprintf(str, args);

            // output to logger
            char buffer[256];
            vsprintf(buffer, str, args);
            if (!iLogger)
            {
                iLogger = PVLogger::GetLoggerObject("PV2WaySourceAndSinks.PV2WayMIO");
            }
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, aLogLevel,
                            (0, "---PV2WayMIO:: %s", buffer));
            va_end(args);
        }


        bool iAdded;
        bool iRemoving;
        bool iClosing;


        Oscl_Map < PVMFFormatType, CodecSpecifier*,
        OsclMemAllocator, pvmf_format_type_key_compare_class > iFormatsMap;
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>* iFormats;


        PVMFNodeInterface*      iMioNode;
        PV2WayMIONodeFactory*   iMioNodeFactory;

        PVCommandId iAddId;
        PVCommandId iRemoveId;
        PVCommandId iPauseId;
        PVCommandId iResumeId;

        TPVChannelId iChannelId;
        TPVChannelId iNextChannelId;

        PV2WayMediaType iMyType;
        TPVDirection iMyDir;
        PVMFFormatType iMySelectedFormat;


        uint32              iPreviewHandle;
        CPV2WayInterface*   iTerminal;
        PVLogger*           iLogger;
        PV2WayMIOObserver*  iObserver;
        CodecSpecifier*     iSelectedCodec;
};



#endif
