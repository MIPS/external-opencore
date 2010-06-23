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
#include "pvmf_omx_videodec_node.h"

#define CONFIG_SIZE_AND_VERSION(param) \
        param.nSize=sizeof(param); \
        param.nVersion.s.nVersionMajor = SPECVERSIONMAJOR; \
        param.nVersion.s.nVersionMinor = SPECVERSIONMINOR; \
        param.nVersion.s.nRevision = SPECREVISION; \
        param.nVersion.s.nStep = SPECSTEP;


#define PVOMXVIDEODEC_EXTRA_YUVBUFFER_POOLNUM 3
#define PVOMXVIDEODEC_MEDIADATA_POOLNUM (PVOMXVIDEODECMAXNUMDPBFRAMESPLUS1 + PVOMXVIDEODEC_EXTRA_YUVBUFFER_POOLNUM)


// Node default settings
#define PVOMXVIDEODECNODE_CONFIG_POSTPROCENABLE_DEF false
#define PVOMXVIDEODECNODE_CONFIG_POSTPROCTYPE_DEF 0  // 0 (nopostproc),1(deblock),3(deblock&&dering)
#define PVOMXVIDEODECNODE_CONFIG_DROPFRAMEENABLE_DEF false
#define PVOMXVIDEODECNODE_CONFIG_KEYFRAMEONLYMODE_DEF false
#define PVOMXVIDEODECNODE_CONFIG_SKIPNUNTILKEYFRAME_DEF 0
// H263 default settings
#define PVOMXVIDEODECNODE_CONFIG_H263MAXBITSTREAMFRAMESIZE_DEF 40000
#define PVOMXVIDEODECNODE_CONFIG_H263MAXBITSTREAMFRAMESIZE_MIN 20000
#define PVOMXVIDEODECNODE_CONFIG_H263MAXBITSTREAMFRAMESIZE_MAX 120000
#define PVOMXVIDEODECNODE_CONFIG_H263MAXWIDTH_DEF 352
#define PVOMXVIDEODECNODE_CONFIG_H263MAXHEIGHT_DEF 288
#define PVOMXVIDEODECNODE_CONFIG_H263MAXDIMENSION_MIN 4
#define PVOMXVIDEODECNODE_CONFIG_H263MAXDIMENSION_MAX 352
// M4v default settings
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXBITSTREAMFRAMESIZE_DEF 40000
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXBITSTREAMFRAMESIZE_MIN 20000
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXBITSTREAMFRAMESIZE_MAX 120000
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXWIDTH_DEF 352
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXHEIGHT_DEF 288
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXDIMENSION_MIN 4
#define PVOMXVIDEODECNODE_CONFIG_M4VMAXDIMENSION_MAX 352
// AVC default settings
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXBITSTREAMFRAMESIZE_DEF 20000
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXBITSTREAMFRAMESIZE_MIN 20000
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXBITSTREAMFRAMESIZE_MAX 120000
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXWIDTH_DEF 352
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXHEIGHT_DEF 288
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXDIMENSION_MIN 4
#define PVOMXVIDEODECNODE_CONFIG_AVCMAXDIMENSION_MAX 352
/* WMV default settings */
#define PVOMXVIDEODECNODE_CONFIG_WMVMAXWIDTH_DEF 352
#define PVOMXVIDEODECNODE_CONFIG_WMVMAXHEIGHT_DEF 288


#define PVMF_OMXVIDEODEC_NUM_METADATA_VALUES 6

// Constant character strings for metadata keys
static const char PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_FORMAT_KEY[] = "codec-info/video/format";
static const char PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_WIDTH_KEY[] = "codec-info/video/width";
static const char PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_HEIGHT_KEY[] = "codec-info/video/height";
static const char PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_PROFILE_KEY[] = "codec-info/video/profile";
static const char PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_LEVEL_KEY[] = "codec-info/video/level";
static const char PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_AVGBITRATE_KEY[] = "codec-info/video/avgbitrate";//(bits per sec)

static const char PVOMXVIDEODECMETADATA_SEMICOLON[] = ";";



/////////////////////////////////////////////////////////////////////////////
// Class Destructor
/////////////////////////////////////////////////////////////////////////////
PVMFOMXVideoDecNode::~PVMFOMXVideoDecNode()
{
    ReleaseAllPorts();
}

/////////////////////////////////////////////////////////////////////////////
// Class Constructor
/////////////////////////////////////////////////////////////////////////////
PVMFOMXVideoDecNode::PVMFOMXVideoDecNode(int32 aPriority) :
        PVMFOMXBaseDecNode(aPriority, "PVMFOMXVideoDecNode"),
        iH263MaxBitstreamFrameSize(PVOMXVIDEODECNODE_CONFIG_H263MAXBITSTREAMFRAMESIZE_DEF),
        iH263MaxWidth(PVOMXVIDEODECNODE_CONFIG_H263MAXWIDTH_DEF),
        iH263MaxHeight(PVOMXVIDEODECNODE_CONFIG_H263MAXHEIGHT_DEF),
        iM4VMaxBitstreamFrameSize(PVOMXVIDEODECNODE_CONFIG_M4VMAXBITSTREAMFRAMESIZE_DEF),
        iM4VMaxWidth(PVOMXVIDEODECNODE_CONFIG_M4VMAXWIDTH_DEF),
        iM4VMaxHeight(PVOMXVIDEODECNODE_CONFIG_M4VMAXHEIGHT_DEF),
        iNewWidth(0),
        iNewHeight(0)
{
    iInterfaceState = EPVMFNodeCreated;

    iNodeConfig.iPostProcessingEnable = PVOMXVIDEODECNODE_CONFIG_POSTPROCENABLE_DEF;
    iNodeConfig.iPostProcessingMode = PVOMXVIDEODECNODE_CONFIG_POSTPROCTYPE_DEF;
    iNodeConfig.iDropFrame = PVOMXVIDEODECNODE_CONFIG_DROPFRAMEENABLE_DEF;
    iNodeConfig.iMimeType = PVMF_MIME_FORMAT_UNKNOWN;
    iNodeConfig.iKeyFrameOnlyMode = PVOMXVIDEODECNODE_CONFIG_KEYFRAMEONLYMODE_DEF;
    iNodeConfig.iSkipNUntilKeyFrame = PVOMXVIDEODECNODE_CONFIG_SKIPNUNTILKEYFRAME_DEF;

    // Get logger objects
    iRunlLogger = PVLogger::GetLoggerObject("Run.PVMFOMXVideoDecNode");
    iDataPathLogger = PVLogger::GetLoggerObject("datapath");
    iClockLogger = PVLogger::GetLoggerObject("clock");
    iDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.decnode.OMXVideoDecnode");

    int32 err;
    OSCL_TRY(err,

             //Create the input command queue.  Use a reserve to avoid lots of
             //dynamic memory allocation.
             iInputCommands.Construct(PVMF_OMXBASEDEC_NODE_COMMAND_ID_START, PVMF_OMXBASEDEC_NODE_COMMAND_VECTOR_RESERVE);

             //Set the node capability data.
             //This node can support an unlimited number of ports.
             iCapability.iCanSupportMultipleInputPorts = false;
             iCapability.iCanSupportMultipleOutputPorts = false;
             iCapability.iHasMaxNumberOfPorts = true;
             iCapability.iMaxNumberOfPorts = 2;
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO_MP4);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO_RAW);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_M4V);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_H2631998);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_H2632000);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_WMV);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_REAL_VIDEO);
             iCapability.iOutputFormatCapability.push_back(PVMF_MIME_YUV420);

             iAvailableMetadataKeys.reserve(PVMF_OMXVIDEODEC_NUM_METADATA_VALUES);
             iAvailableMetadataKeys.clear();
            );

    // need to init this allocator since verifyParameterSync (using the buffers) may be called through
    // port interface before anything else happens.
    OSCL_TRY(err, iFsiFragmentAlloc.size(PVOMXVIDEODEC_MEDIADATA_POOLNUM, sizeof(PVMFYuvFormatSpecificInfo0)));

    OSCL_TRY(err, iPrivateDataFsiFragmentAlloc.size(PVOMXVIDEODEC_MEDIADATA_POOLNUM, sizeof(OsclAny *)));

    iLastYUVWidth = 0;
    iLastYUVHeight = 0;
    iStride = 0;
    iSliceHeight = 0;
}

/////////////////////
// Private Section //
/////////////////////
/////////////////////////////////////////////////////////////////////////////
// This routine will handle the PortReEnable state
/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXVideoDecNode::HandlePortReEnable()
{
    OMX_ERRORTYPE Err = OMX_ErrorNone;

    // set the port index so that we get parameters for the proper port
    iParamPort.nPortIndex = iPortIndexForDynamicReconfig;

    CONFIG_SIZE_AND_VERSION(iParamPort);

    // get new parameters of the port
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (OMX_ErrorNone != Err)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Problem getting parameters in port %d ", iPortIndexForDynamicReconfig));
        SetState(EPVMFNodeError);
        ReportErrorEvent(PVMFErrNoMemory);
        return false;
    }

    // do not send port reenable command until all the parameters have been set

    // is this output port?
    if (iPortIndexForDynamicReconfig == iOutputPortIndex)
    {
        uint32 iBeforeConfigNumOutputBuffers = iNumOutputBuffers;

        // read the alignment
        iOutputBufferAlignment = iParamPort.nBufferAlignment;

        /* Get the display dimensions from mp4ffparser outport */
        /* This is needed when buffer dimensions are not default QCIF */
        ((PVMFOMXDecPort*)iInPort)->getDisplayDimension(&iYUVWidth, &iYUVHeight);

        /*
        ** Check whether display dimensions are valid dimensions.
        ** If not,then assign the buffer dimensions as display dimensions.
        */
        if ((iYUVWidth <= 0) || (iYUVWidth > (int32)iParamPort.format.video.nFrameWidth))
        {
            iYUVWidth = iParamPort.format.video.nFrameWidth;
        }
        if ((iYUVHeight <= 0) || (iYUVHeight > (int32)iParamPort.format.video.nFrameHeight))
        {
            iYUVHeight = iParamPort.format.video.nFrameHeight;
        }

        iOMXComponentOutputBufferSize = iParamPort.nBufferSize;

        iNumOutputBuffers = iParamPort.nBufferCountActual;

        if (iNumOutputBuffers > NUMBER_OUTPUT_BUFFER)
            iNumOutputBuffers = NUMBER_OUTPUT_BUFFER; // make sure number of output buffers is not larger than port queue size

        // do we need to increase the number of buffers?
        if (iNumOutputBuffers < iParamPort.nBufferCountMin)
            iNumOutputBuffers = iParamPort.nBufferCountMin;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() new output buffers %d, size %d", iNumOutputBuffers, iOMXComponentOutputBufferSize));

        iStride = OSCL_ABS(iParamPort.format.video.nStride);
        iSliceHeight = iParamPort.format.video.nSliceHeight;

        // This should not happen. If it does, it is a bug in the OMX component.
        OSCL_ASSERT((iStride >= iParamPort.format.video.nFrameWidth) && (iSliceHeight >= iParamPort.format.video.nFrameHeight));
        if (iStride < iParamPort.format.video.nFrameWidth)
        {
            iStride = iParamPort.format.video.nFrameWidth;
        }

        if (iSliceHeight < iParamPort.format.video.nFrameHeight)
        {
            iSliceHeight = iParamPort.format.video.nFrameHeight;
        }

        // make sure to set the actual number of buffers (in case the number has changed)
        iParamPort.nBufferCountActual = iNumOutputBuffers;
        CONFIG_SIZE_AND_VERSION(iParamPort);

        Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Problem setting parameters in output port %d ", iOutputPortIndex));
            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return false;
        }

        iLastYUVWidth = iYUVWidth ;
        iLastYUVHeight = iYUVHeight;

        // Before allocating new set of output buffers, re-send Video FSI to
        // media output node in case of dynamic port reconfiguration


        sendFsi = true;
        iCompactFSISettingSucceeded = false;

        //Try querying the buffer allocator KVP for output buffer allocation outside of the node
        PvmiKvp* pkvp = NULL;
        int NumKvp = 0;
        PvmiKeyType aKvpIdentifier = (PvmiKeyType)PVMF_SUPPORT_FOR_BUFFER_ALLOCATOR_IN_MIO_KEY;
        // Check if Fsi configuration need to be sent
        // Send it only if the MIO supports the buffer allocator
        if (((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aKvpIdentifier, pkvp, NumKvp) == PVMFSuccess)
        {
            //Send the FSI information to media output node here, before setting output
            //port parameters to the omx component
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable - Re-sending YUV FSI after Dynamic port reconfiguration"));

            int fsiErrorCode = 0;
            OsclRefCounterMemFrag yuvFsiMemfrag;

            OSCL_TRY(fsiErrorCode, yuvFsiMemfrag = iFsiFragmentAlloc.get(););

            OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                 (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Failed to allocate memory for FSI")));

            if (fsiErrorCode == 0)
            {
                PVMFYuvFormatSpecificInfo0* fsiInfo = OSCL_PLACEMENT_NEW(yuvFsiMemfrag.getMemFragPtr(), PVMFYuvFormatSpecificInfo0());
                if (fsiInfo != NULL)
                {
                    fsiInfo->uid = PVMFYuvFormatSpecificInfo0_UID;
                    fsiInfo->video_format = iYUVFormat;
                    fsiInfo->viewable_width = iYUVWidth;
                    fsiInfo->viewable_height = iYUVHeight;
                    fsiInfo->num_buffers = iNumOutputBuffers;
                    fsiInfo->buffer_size = iOMXComponentOutputBufferSize;
                    fsiInfo->buffer_width = iStride;
                    fsiInfo->buffer_height = iSliceHeight;

                    OsclMemAllocator alloc;
                    int32 KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) + 1;
                    PvmiKeyType KvpKey = (PvmiKeyType)alloc.ALLOCATE(KeyLength);

                    if (NULL == KvpKey)
                    {
                        return false;
                    }

                    oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV, KeyLength);
                    int32 err;
                    bool success = false;

                    OSCL_TRY(err, success = ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(yuvFsiMemfrag, KvpKey););
                    if (err != OsclErrNone || !success)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable - Problem to set FSI"));

                    }
                    else
                    {
                        sendFsi = false;
                        iCompactFSISettingSucceeded = true;
                    }

                    alloc.deallocate((OsclAny*)(KvpKey));
                    fsiInfo->video_format.~PVMFFormatType();
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXVideoDecNode::HandlePortReEnable - Problem allocating Output FSI"));
                    SetState(EPVMFNodeError);
                    ReportErrorEvent(PVMFErrNoMemory);
                    return false; // this is going to make everything go out of scope
                }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXVideoDecNode::HandlePortReEnable - Problem allocating Output FSI"));
                return false; // this is going to make everything go out of scope
            }


        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable Warning- It is OK for this KVP to fail"
                             "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"));

            ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
        }

        //Buffer allocation has to be done again in case we landed to port reconfiguration
        PvmiKvp* kvp = NULL;
        int numKvp = 0;
        PvmiKeyType aIdentifier = (PvmiKeyType)PVMF_BUFFER_ALLOCATOR_KEY;
        int32 err, err1;
        if (ipExternalOutputBufferAllocatorInterface)
        {
            ipExternalOutputBufferAllocatorInterface->removeRef();
            ipExternalOutputBufferAllocatorInterface = NULL;
        }

        bool success = false;
        OSCL_TRY(err, success = ((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aIdentifier, kvp, numKvp););

        if ((err == OsclErrNone) && (NULL != kvp) && success)
        {
            ipExternalOutputBufferAllocatorInterface = (PVInterface*) kvp->value.key_specific_value;

            if (ipExternalOutputBufferAllocatorInterface)
            {
                PVInterface* pTempPVInterfacePtr = NULL;

                OSCL_TRY(err, ipExternalOutputBufferAllocatorInterface->queryInterface(PVMFFixedSizeBufferAllocUUID, pTempPVInterfacePtr););

                OSCL_TRY(err1, ((PVMFOMXDecPort*)iOutPort)->releaseParametersSync(kvp, numKvp););

                if (err1 != OsclErrNone)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::HandlePortReEnable - Unable to Release Parameters"));
                }

                if ((err == OsclErrNone) && (NULL != pTempPVInterfacePtr))
                {
                    ipFixedSizeBufferAlloc = OSCL_STATIC_CAST(PVMFFixedSizeBufferAlloc*, pTempPVInterfacePtr);

                    uint32 iNumBuffers, iBufferSize;

                    iNumBuffers = ipFixedSizeBufferAlloc->getNumBuffers();
                    iBufferSize = ipFixedSizeBufferAlloc->getBufferSize();

                    if ((iNumBuffers < iParamPort.nBufferCountMin) || (iBufferSize < iOMXComponentOutputBufferSize))
                    {
                        ipExternalOutputBufferAllocatorInterface->removeRef();
                        ipExternalOutputBufferAllocatorInterface = NULL;
                    }
                    else
                    {
                        iNumOutputBuffers = iNumBuffers;
                        iOMXComponentOutputBufferSize = iBufferSize;
                        ReportInfoEvent(PVMFPvmiBufferAlloctorAcquired);
                    }
                }
                else
                {
                    ipExternalOutputBufferAllocatorInterface->removeRef();
                    ipExternalOutputBufferAllocatorInterface = NULL;
                }
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable Warning- It is OK for this KVP to fail"
                             "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                             "the MIO BUFFER ALLOCATOR -see documentation"));
            ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);

        }


        if (iNumOutputBuffers > iBeforeConfigNumOutputBuffers)
        {
            //Reallocate FillBufferDone THREADSAFE CALLBACK AOs in case of port reconfiguration
            if (iThreadSafeHandlerFillBufferDone)
            {
                OSCL_DELETE(iThreadSafeHandlerFillBufferDone);
                iThreadSafeHandlerFillBufferDone = NULL;
            }
            // use the new queue depth of iNumOutputBuffers to prevent deadlock
            iThreadSafeHandlerFillBufferDone = OSCL_NEW(FillBufferDoneThreadSafeCallbackAO, (this, iNumOutputBuffers, "FillBufferDoneAO", Priority() + 1));

            if (NULL == iThreadSafeHandlerFillBufferDone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Can't reallocate FillBufferDone threadsafe callback queue!"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false;
            }
        }

        // it is now safe to send command for port reenable
        // send command for port re-enabling (for this to happen, we must first recreate the buffers)
        Err = OMX_SendCommand(iOMXDecoder, OMX_CommandPortEnable, iPortIndexForDynamicReconfig, NULL);
        if (OMX_ErrorNone != Err)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Problem sending port enable command to port %d ", iPortIndexForDynamicReconfig));
            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return false;
        }


        /* Allocate output buffers */
        if (!CreateOutMemPool(iNumOutputBuffers))
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot allocate output buffers "));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;
        }


        if (!ProvideBuffersToComponent(iOutBufMemoryPool, // allocator
                                       iOutputAllocSize,     // size to allocate from pool (hdr only or hdr+ buffer)
                                       iNumOutputBuffers, // number of buffers
                                       iOMXComponentOutputBufferSize, // actual buffer size
                                       iOutputPortIndex, // port idx
                                       iOMXComponentSupportsExternalOutputBufferAlloc, // can component use OMX_UseBuffer
                                       false // this is not input
                                      ))
        {


            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot provide output buffers to component"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;

        }

        // do not drop output any more, i.e. enable output to be sent downstream
        iDoNotSendOutputBuffersDownstreamFlag = false;


    }
    else
    {
        uint32 iBeforeConfigNumInputBuffers = iNumInputBuffers;

        // read the alignment
        iInputBufferAlignment = iParamPort.nBufferAlignment;

        // this is input port
        iNumInputBuffers = iParamPort.nBufferCountActual;
        if (iNumInputBuffers > NUMBER_INPUT_BUFFER)
            iNumInputBuffers = NUMBER_INPUT_BUFFER; // make sure number of output buffers is not larger than port queue size

        iOMXComponentInputBufferSize = iParamPort.nBufferSize;
        // do we need to increase the number of buffers?
        if (iNumInputBuffers < iParamPort.nBufferCountMin)
            iNumInputBuffers = iParamPort.nBufferCountMin;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() new buffers %d, size %d", iNumInputBuffers, iOMXComponentInputBufferSize));

        // make sure to set the actual number of buffers (in case the number has changed)
        iParamPort.nBufferCountActual = iNumInputBuffers;
        CONFIG_SIZE_AND_VERSION(iParamPort);

        Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Problem setting parameters in input port %d ", iInputPortIndex));
            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return false;
        }

        if (iNumInputBuffers > iBeforeConfigNumInputBuffers)
        {
            //Reallocate EmptyBufferDone THREADSAFE CALLBACK AOs in case of port reconfiguration
            if (iThreadSafeHandlerEmptyBufferDone)
            {
                OSCL_DELETE(iThreadSafeHandlerEmptyBufferDone);
                iThreadSafeHandlerEmptyBufferDone = NULL;
            }
            // use the new queue depth of iNumInputBuffers to prevent deadlock
            iThreadSafeHandlerEmptyBufferDone = OSCL_NEW(EmptyBufferDoneThreadSafeCallbackAO, (this, iNumInputBuffers, "EmptyBufferDoneAO", Priority() + 1));

            if (NULL == iThreadSafeHandlerEmptyBufferDone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Can't reallocate EmptyBufferDone threadsafe callback queue!"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false;
            }
        }

        // it is now safe to send command for port reenable
        // send command for port re-enabling (for this to happen, we must first recreate the buffers)
        Err = OMX_SendCommand(iOMXDecoder, OMX_CommandPortEnable, iPortIndexForDynamicReconfig, NULL);
        if (OMX_ErrorNone != Err)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Problem sending port enable command to port %d ", iPortIndexForDynamicReconfig));
            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return false;
        }

        /* Allocate input buffers */
        if (!CreateInputMemPool(iNumInputBuffers))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot allocate new input buffers to component"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;
        }



        if (!ProvideBuffersToComponent(iInBufMemoryPool, // allocator
                                       iInputAllocSize,  // size to allocate from pool (hdr only or hdr+ buffer)
                                       iNumInputBuffers, // number of buffers
                                       iOMXComponentInputBufferSize, // actual buffer size
                                       iInputPortIndex, // port idx
                                       iOMXComponentSupportsExternalInputBufferAlloc, // can component use OMX_UseBuffer
                                       true // this is input
                                      ))
        {


            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot provide new input buffers to component"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;

        }
    }

    // if the callback that the port was re-enabled has not arrived yet, wait for it
    // if it has arrived, it will set the state to either PortReconfig or to ReadyToDecode or InitDecoder (if more init buffers need to be sent)
    if (iProcessingState != EPVMFOMXBaseDecNodeProcessingState_PortReconfig &&
            iProcessingState != EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode &&
            iProcessingState != EPVMFOMXBaseDecNodeProcessingState_InitDecoder)
        iProcessingState = EPVMFOMXBaseDecNodeProcessingState_WaitForPortEnable;

    return PVMFSuccess; // allow rescheduling of the node
}
////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXVideoDecNode::NegotiateComponentParameters(OMX_PTR aOutputParameters)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() In"));

    OMX_ERRORTYPE Err;
    // first get the number of ports and port indices
    OMX_PORT_PARAM_TYPE VideoPortParameters;
    uint32 NumPorts;
    uint32 ii;

    //Initialize the timestamp delta to 0 for all video formats
    iTimestampDeltaForMemFragment = 0;

    //set version and size;
    CONFIG_SIZE_AND_VERSION(VideoPortParameters);
    // get starting number
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamVideoInit, &VideoPortParameters);
    NumPorts = VideoPortParameters.nPorts; // must be at least 2 of them (in&out)

    if (Err != OMX_ErrorNone || NumPorts < 2)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() There is insuffucient (%d) ports", NumPorts));
        return false;
    }


    // loop through video ports starting from the starting index to find index of the first input port
    for (ii = VideoPortParameters.nStartPortNumber ; ii < VideoPortParameters.nStartPortNumber + NumPorts; ii++)
    {
        // get port parameters, and determine if it is input or output
        // if there are more than 2 ports, the first one we encounter that has input direction is picked

        CONFIG_SIZE_AND_VERSION(iParamPort);

        //port
        iParamPort.nPortIndex = ii; // iInputPortIndex; //OMF_MC_H264D_PORT_INDEX_OF_STREAM;
        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);

        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem negotiating with port %d ", ii));

            return false;
        }

        if (iParamPort.eDir == OMX_DirInput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Found Input port index %d ", ii));

            iInputPortIndex = ii;
            break;
        }
    }
    if (ii == VideoPortParameters.nStartPortNumber + NumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Cannot find any input port "));
        return false;
    }


    // loop through video ports starting from the starting index to find index of the first output port
    for (ii = VideoPortParameters.nStartPortNumber ; ii < VideoPortParameters.nStartPortNumber + NumPorts; ii++)
    {
        // get port parameters, and determine if it is input or output
        // if there are more than 2 ports, the first one we encounter that has output direction is picked

        CONFIG_SIZE_AND_VERSION(iParamPort);

        //port
        iParamPort.nPortIndex = ii;
        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);

        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem negotiating with port %d ", ii));

            return false;
        }

        if (iParamPort.eDir == OMX_DirOutput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Found Output port index %d ", ii));

            iOutputPortIndex = ii;
            break;
        }
    }
    if (ii == VideoPortParameters.nStartPortNumber + NumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Cannot find any output port "));
        return false;
    }



    // now get input parameters
    CONFIG_SIZE_AND_VERSION(iParamPort);

    //Input port
    iParamPort.nPortIndex = iInputPortIndex;
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem negotiating with input port %d ", iInputPortIndex));
        return false;
    }

    // read the alignment
    iInputBufferAlignment = iParamPort.nBufferAlignment;

    //iNumInputBuffers = NUMBER_INPUT_BUFFER;
    iNumInputBuffers = iParamPort.nBufferCountActual;  // use the value provided by component

    // do we need to increase the number of buffers?
    if (iNumInputBuffers < iParamPort.nBufferCountMin)
        iNumInputBuffers = iParamPort.nBufferCountMin;
    iOMXComponentInputBufferSize = iParamPort.nBufferSize;

    iParamPort.nBufferCountActual = iNumInputBuffers;

    // set the number of actual input buffers
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Inport buffers %d,size %d", iNumInputBuffers, iOMXComponentInputBufferSize));


    VideoOMXConfigParserOutputs *pOutputParameters;

    pOutputParameters = (VideoOMXConfigParserOutputs *)aOutputParameters;


    // set the width/height on INPUT port parameters (this may change during port reconfig)
    if ((pOutputParameters->width != 0) && (pOutputParameters->height != 0))
    {
        iParamPort.format.video.nFrameWidth = pOutputParameters->width;
        iParamPort.format.video.nFrameHeight = pOutputParameters->height;
    }

    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting parameters in input port %d ", iInputPortIndex));
        return false;
    }


    //After setting the parameters, do GetParameter once again to confirm whether the parameters have been set properly or not
    iParamPort.nPortIndex = iInputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem negotiating with input port %d ", iInputPortIndex));
        return false;
    }

    //If the component has not taken the updated value, we can go back to the number of buffers that the component suggested
    if (iNumInputBuffers != iParamPort.nBufferCountActual)
    {
        iNumInputBuffers = iParamPort.nBufferCountActual;
    }

    //Port 1 for output port
    iParamPort.nPortIndex = iOutputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem negotiating with output port %d ", iOutputPortIndex));
        return false;
    }

    // check if params are OK. In case of H263, width/height cannot be obtained until
    // 1st frame is decoded, so read them from the output port.
    // otherwise, used Width/Height from the config parser utility
    // set the width/height based on port parameters (this may change during port reconfig)


    /* Get the display dimensions from mp4ffparser outport */
    ((PVMFOMXDecPort*)iInPort)->getDisplayDimension(&iYUVWidth, &iYUVHeight);

    if ((pOutputParameters->width != 0) && (pOutputParameters->height != 0) && iInPort && (((PVMFOMXDecPort*)iInPort)->iFormat != PVMF_MIME_H2631998 || ((PVMFOMXDecPort*)iInPort)->iFormat != PVMF_MIME_H2632000))
    {
        // set width and height obtained from config parser in the output port as well
        iParamPort.format.video.nFrameWidth = pOutputParameters->width;
        iParamPort.format.video.nFrameHeight = pOutputParameters->height;

        if (iYUVWidth == 0 || iYUVHeight == 0)  // this info is not available in fileformat.
        {
            iYUVWidth  = pOutputParameters->width;  // take it from config parser
            iYUVHeight = pOutputParameters->height;
        }
    }
    else
    {
        if (iYUVWidth == 0 || iYUVHeight == 0)  // this info is not available in fileformat.
        {
            iYUVWidth =  iParamPort.format.video.nFrameWidth;  // take the default value from OMX
            iYUVHeight = iParamPort.format.video.nFrameHeight;
        }
    }

    // Send the parameters right away to allow the OMX component to re-calculate the buffer size
    // based on the new width and height that was just provided
    CONFIG_SIZE_AND_VERSION(iParamPort);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Outport buffers %d,size %d", iNumOutputBuffers, iOMXComponentOutputBufferSize));

    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting parameters in output port %d ", iOutputPortIndex));
        return false;
    }

    // Now - read the same parameters back again with potentially new buffer sizes
    iParamPort.nPortIndex = iOutputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem negotiating with output port %d ", iOutputPortIndex));
        return false;
    }


    // read the alignment
    iOutputBufferAlignment = iParamPort.nBufferAlignment;

    //iNumOutputBuffers = NUMBER_OUTPUT_BUFFER;
    iNumOutputBuffers = iParamPort.nBufferCountActual;
    if (iNumOutputBuffers > NUMBER_OUTPUT_BUFFER)
        iNumOutputBuffers = NUMBER_OUTPUT_BUFFER; // make sure number of output buffers is not larger than port queue size
    iOMXComponentOutputBufferSize = iParamPort.nBufferSize;
    if (iNumOutputBuffers < iParamPort.nBufferCountMin)
        iNumOutputBuffers = iParamPort.nBufferCountMin;

    {
        // Get video color format
        OMX_VIDEO_PARAM_PORTFORMATTYPE VideoPortFormat;
        // init to unknown
        iOMXVideoColorFormat = OMX_COLOR_FormatUnused;
        CONFIG_SIZE_AND_VERSION(VideoPortFormat);
        VideoPortFormat.nPortIndex = iOutputPortIndex;

        VideoPortFormat.nIndex = 0; // read the preferred format - first

        // doing this in a while loop while incrementing nIndex will get all supported formats
        // until component says OMX_ErrorNoMore
        // For now, we just use the preferred one (with nIndex=0) assuming it is supported at MIO

        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamVideoPortFormat, &VideoPortFormat);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem getting video port format"));
            return false;
        }
        // check if color format is valid and set DeBlocking
        if (VideoPortFormat.eCompressionFormat == OMX_VIDEO_CodingUnused)
        {
            // color format is valid, so read it
            iOMXVideoColorFormat = VideoPortFormat.eColorFormat;

            // Now set the format to confirm parameters
            CONFIG_SIZE_AND_VERSION(VideoPortFormat);

            Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamVideoPortFormat, &VideoPortFormat);
            if (Err != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting video port format"));
                return false;
            }
        }

        // now that we have the color format, interpret it
        if (iOMXVideoColorFormat == OMX_COLOR_Format8bitRGB332)
        {
            iYUVFormat = PVMF_MIME_RGB8;
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_Format12bitRGB444)
        {
            iYUVFormat = PVMF_MIME_RGB12;
        }
        else if (iOMXVideoColorFormat >= OMX_COLOR_Format16bitARGB4444 && iOMXVideoColorFormat <= OMX_COLOR_Format16bitBGR565)
        {
            iYUVFormat = PVMF_MIME_RGB16;
        }
        else if (iOMXVideoColorFormat >= OMX_COLOR_Format24bitRGB888 && iOMXVideoColorFormat <= OMX_COLOR_Format24bitARGB1887)
        {
            iYUVFormat = PVMF_MIME_RGB24;
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV420Planar)
        {
            iYUVFormat = PVMF_MIME_YUV420_PLANAR; // Y, U, V are separate - entire planes
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV420PackedPlanar)
        {
            iYUVFormat = PVMF_MIME_YUV420_PACKEDPLANAR; // each slice contains Y,U,V separate
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV420SemiPlanar)
        {
            iYUVFormat = PVMF_MIME_YUV420_SEMIPLANAR; // Y and UV interleaved - entire planes
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV420PackedSemiPlanar)
        {
            iYUVFormat = PVMF_MIME_YUV420_PACKEDSEMIPLANAR; // Y and UV interleaved - sliced
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV422Planar)
        {
            iYUVFormat = PVMF_MIME_YUV422_PLANAR; // Y, U, V are separate - entire planes
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV422PackedPlanar)
        {
            iYUVFormat = PVMF_MIME_YUV422_PACKEDPLANAR; // each slice contains Y,U,V separate
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV422SemiPlanar)
        {
            iYUVFormat = PVMF_MIME_YUV422_SEMIPLANAR; // Y and UV interleaved - entire planes
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatYUV422PackedSemiPlanar)
        {
            iYUVFormat = PVMF_MIME_YUV422_PACKEDSEMIPLANAR; // Y and UV interleaved - sliced
        }
        else if (iOMXVideoColorFormat == OMX_COLOR_FormatCbYCrY)
        {
            iYUVFormat = PVMF_MIME_YUV422_INTERLEAVED_UYVY; // Y, U, V interleaved
        }
        else if (iOMXVideoColorFormat == 0x7FA30C00) // SPECIAL VALUE
        {
            iYUVFormat = PVMF_MIME_YUV420_SEMIPLANAR_YVU; // semiplanar with Y and VU interleaved
        }
        else
        {
            iYUVFormat = PVMF_MIME_FORMAT_UNKNOWN;
            return false;
        }
    }

    iStride = OSCL_ABS(iParamPort.format.video.nStride);
    iSliceHeight = iParamPort.format.video.nSliceHeight;

    // This should not happen. If it does, it is a bug in the OMX component.
    OSCL_ASSERT((iStride >= iParamPort.format.video.nFrameWidth) || (iSliceHeight >= iParamPort.format.video.nFrameHeight));
    if (iStride < iParamPort.format.video.nFrameWidth)
    {
        iStride = iParamPort.format.video.nFrameWidth;
    }

    if (iSliceHeight < iParamPort.format.video.nFrameHeight)
    {
        iSliceHeight = iParamPort.format.video.nFrameHeight;
    }

    /* This should not happen for default QCIF. If it does, it is a bug in the FF parser width/height.
    ** Below checks are only to take care for QCIF default size.
    ** Higher dimensions will always enter checks and PortReEnable state will be called.
    */
    if (iYUVWidth > (int32)iStride)
    {
        iYUVWidth = (int32)iStride;
    }
    if (iYUVHeight > (int32)iSliceHeight)
    {
        iYUVHeight = (int32)iSliceHeight;
    }

    //Send the FSI information to media output node here

    if (iLastYUVWidth != iYUVWidth || iYUVHeight != iLastYUVHeight)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters - Sending YUV FSI"));

        //store new values for reference
        iLastYUVWidth = iYUVWidth ;
        iLastYUVHeight = iYUVHeight;

        // assume that this call will NOT succeed - set a flag to send Fsi configuration
        sendFsi = true;
        iCompactFSISettingSucceeded = false;

        //Try querying the buffer allocator KVP for output buffer allocation outside of the node
        PvmiKvp* pkvp = NULL;
        int NumKvp = 0;
        PvmiKeyType aKvpIdentifier = (PvmiKeyType)PVMF_SUPPORT_FOR_BUFFER_ALLOCATOR_IN_MIO_KEY;
        // Check if Fsi configuration need to be sent
        // Send it only if the MIO supports the buffer allocator
        if (((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aKvpIdentifier, pkvp, NumKvp) == PVMFSuccess)
        {
            //Send the FSI information to media output node here, before setting output
            //port parameters to the omx component
            int fsiErrorCode = 0;
            OsclRefCounterMemFrag yuvFsiMemfrag;

            OSCL_TRY(fsiErrorCode, yuvFsiMemfrag = iFsiFragmentAlloc.get(););

            OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                 (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Failed to allocate memory for FSI")));

            if (fsiErrorCode == 0)
            {
                PVMFYuvFormatSpecificInfo0* fsiInfo = OSCL_PLACEMENT_NEW(yuvFsiMemfrag.getMemFragPtr(), PVMFYuvFormatSpecificInfo0());
                if (fsiInfo != NULL)
                {
                    fsiInfo->uid = PVMFYuvFormatSpecificInfo0_UID;
                    fsiInfo->video_format = iYUVFormat;
                    fsiInfo->viewable_width = iYUVWidth;
                    fsiInfo->viewable_height = iYUVHeight;
                    fsiInfo->num_buffers = iNumOutputBuffers;
                    fsiInfo->buffer_size = iOMXComponentOutputBufferSize;
                    fsiInfo->buffer_width = iStride;
                    fsiInfo->buffer_height = iSliceHeight;

                    OsclMemAllocator alloc;
                    int32 KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) + 1;
                    PvmiKeyType KvpKey = (PvmiKeyType)alloc.ALLOCATE(KeyLength);

                    if (NULL == KvpKey)
                    {
                        return false;
                    }

                    oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV, KeyLength);
                    int32 err;
                    bool success = false;

                    OSCL_TRY(err, success = ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(yuvFsiMemfrag, KvpKey););

                    if (err != OsclErrNone)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters - Problem to set FSI"));
                    }
                    else if (success)
                    {
                        // the call succeeded - no need to send extra fsi
                        iCompactFSISettingSucceeded = true;
                        sendFsi = false;

                    }

                    alloc.deallocate((OsclAny*)(KvpKey));
                    fsiInfo->video_format.~PVMFFormatType();
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters - Problem allocating Output FSI"));
                    SetState(EPVMFNodeError);
                    ReportErrorEvent(PVMFErrNoMemory);
                    return false; // this is going to make everything go out of scope
                }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters - Problem allocating Output FSI"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false; // this is going to make everything go out of scope
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters Warning- It is OK for this KVP to fail"
                             "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                             "the MIO BUFFER ALLOCATOR -see documentation"));
            ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
        }

    }

    //Try querying the buffer allocator KVP for output buffer allocation outside the node
    PvmiKvp* kvp = NULL;
    int numKvp = 0;
    PvmiKeyType aIdentifier = (PvmiKeyType)PVMF_BUFFER_ALLOCATOR_KEY;
    int32 err, err1;

    if (ipExternalOutputBufferAllocatorInterface)
    {
        ipExternalOutputBufferAllocatorInterface->removeRef();
        ipExternalOutputBufferAllocatorInterface = NULL;
    }

    OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aIdentifier, kvp, numKvp););

    if ((err == OsclErrNone) && (NULL != kvp))
    {
        ipExternalOutputBufferAllocatorInterface = (PVInterface*) kvp->value.key_specific_value;

        if (ipExternalOutputBufferAllocatorInterface)
        {
            PVInterface* pTempPVInterfacePtr = NULL;

            OSCL_TRY(err, ipExternalOutputBufferAllocatorInterface->queryInterface(PVMFFixedSizeBufferAllocUUID, pTempPVInterfacePtr););

            OSCL_TRY(err1, ((PVMFOMXDecPort*)iOutPort)->releaseParametersSync(kvp, numKvp););

            if (err1 != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters - Unable to Release Parameters"));
            }


            if ((err == OsclErrNone) && (NULL != pTempPVInterfacePtr))
            {
                ipFixedSizeBufferAlloc = OSCL_STATIC_CAST(PVMFFixedSizeBufferAlloc*, pTempPVInterfacePtr);

                uint32 iNumBuffers, iBufferSize;

                iNumBuffers = ipFixedSizeBufferAlloc->getNumBuffers();
                iBufferSize = ipFixedSizeBufferAlloc->getBufferSize();

                if ((iNumBuffers < iParamPort.nBufferCountMin) || (iBufferSize < iOMXComponentOutputBufferSize))
                {
                    ipExternalOutputBufferAllocatorInterface->removeRef();
                    ipExternalOutputBufferAllocatorInterface = NULL;
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                                    (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters Warning - problem setting number of buffers/buffer size."));
                }
                else
                {
                    iNumOutputBuffers = iNumBuffers;
                    iOMXComponentOutputBufferSize = iBufferSize;
                    ReportInfoEvent(PVMFPvmiBufferAlloctorAcquired);
                }
            }
            else
            {
                ipExternalOutputBufferAllocatorInterface->removeRef();
                ipExternalOutputBufferAllocatorInterface = NULL;
            }
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters Warning- It is OK for this KVP to fail"
                         "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                         "the MIO BUFFER ALLOCATOR -see documentation"));

        ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
    }


    iParamPort.nBufferCountActual = iNumOutputBuffers;
    CONFIG_SIZE_AND_VERSION(iParamPort);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Outport buffers %d,size %d", iNumOutputBuffers, iOMXComponentOutputBufferSize));

    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting parameters in output port %d ", iOutputPortIndex));
        return false;
    }


    //After setting the parameters, do GetParameter once again to confirm whether the parameters have been set properly or not
    iParamPort.nPortIndex = iOutputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Output Port parameters not set properly in the component"));
        return false;
    }

    //If the component has not taken the updated value,
    //we can go back to the number of buffers that the component suggested only if External Output Buffer Allocator is not supported
    if (iNumOutputBuffers != iParamPort.nBufferCountActual)
    {
        if ((NULL != ipExternalOutputBufferAllocatorInterface) || (iNumOutputBuffers > NUMBER_OUTPUT_BUFFER))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Output Port parameters not set properly in the component"));
            return false;
        }
        else
        {
            iNumOutputBuffers = iParamPort.nBufferCountActual;
        }
    }


    //Set input video format
    //This is need it since a single component could handle differents roles

    // Init to desire format
    PVMFFormatType Format = PVMF_MIME_FORMAT_UNKNOWN;
    if (iInPort != NULL)
    {
        Format = ((PVMFOMXDecPort*)iInPort)->iFormat;
    }
    if (Format == PVMF_MIME_H264_VIDEO ||
            Format == PVMF_MIME_H264_VIDEO_MP4 ||
            Format == PVMF_MIME_H264_VIDEO_RAW)
    {
        iOMXVideoCompressionFormat = OMX_VIDEO_CodingAVC;
    }
    else if (Format == PVMF_MIME_M4V)
    {
        iOMXVideoCompressionFormat = OMX_VIDEO_CodingMPEG4;
    }
    else if (Format == PVMF_MIME_H2631998 ||
             Format == PVMF_MIME_H2632000)
    {
        iOMXVideoCompressionFormat = OMX_VIDEO_CodingH263;
    }
    else if (Format == PVMF_MIME_WMV)
    {
        iOMXVideoCompressionFormat = OMX_VIDEO_CodingWMV;
    }
    else if (Format == PVMF_MIME_REAL_VIDEO)
    {
        iOMXVideoCompressionFormat = OMX_VIDEO_CodingRV;
    }
    else
    {
        // Illegal codec specified.
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting video compression format"));
        return false;
    }

    if ((iOMXVideoCompressionFormat == OMX_VIDEO_CodingMPEG4) ||
            (iOMXVideoCompressionFormat == OMX_VIDEO_CodingH263))
    {
        if (iNodeConfig.iPostProcessingEnable)
        {
            // Disable/Enable deblocking for mpeg4/h263 video types
            OMX_PARAM_DEBLOCKINGTYPE DeBlock;
            CONFIG_SIZE_AND_VERSION(DeBlock);

            DeBlock.nPortIndex = iOutputPortIndex;
            DeBlock.bDeblocking = OMX_FALSE;

            if (1 == iNodeConfig.iPostProcessingMode)   //1 (deblock)
            {
                DeBlock.bDeblocking = OMX_TRUE;
            }


            Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamCommonDeblocking, &DeBlock);
            if (Err != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting deblocking flag"));
                // Dont return false in this case.  If enabling DeBlocking fails, just continue.
            }
        }
    }

    OMX_VIDEO_PARAM_PORTFORMATTYPE VideoPortFormat;
    CONFIG_SIZE_AND_VERSION(VideoPortFormat);
    VideoPortFormat.nPortIndex = iInputPortIndex;

    // Search the proper format index and set it.
    // Since we already know that the component has the role we need, search until finding the proper nIndex
    // if component does not find the format will return OMX_ErrorNoMore

    for (ii = 0; ii < PVOMXVIDEO_MAX_SUPPORTED_FORMAT; ii++)
    {
        VideoPortFormat.nIndex = ii;
        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamVideoPortFormat, &VideoPortFormat);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting video compression format"));
            return false;
        }
        if (iOMXVideoCompressionFormat == VideoPortFormat.eCompressionFormat)
        {
            break;
        }
    }

    if (ii == PVOMXVIDEO_MAX_SUPPORTED_FORMAT)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() No Video compression format found"));
        return false;
    }

    // Now set the format to confirm parameters
    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamVideoPortFormat, &VideoPortFormat);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXVideoDecNode::NegotiateComponentParameters() Problem setting video compression format"));
        return false;
    }


    return true;
}


/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXVideoDecNode::InitDecoder(PVMFSharedMediaDataPtr& DataIn)
{
    OSCL_UNUSED_ARG(DataIn);

    uint32 nal_length = 0, size = 0;
    uint8 *tmp_ptr;
    PVMFFormatType Format = PVMF_MIME_FORMAT_UNKNOWN;
    PVMFStatus status = PVMFSuccess;
    uint8* initbuffer = ((PVMFOMXDecPort*)iInPort)->getTrackConfig();
    int32 initbufsize = (int32)((PVMFOMXDecPort*)iInPort)->getTrackConfigSize();



    // NOTE: the component may not start decoding without providing the Output buffer to it,
    //      here, we're sending input/config buffers. Output buffers are sent as well outside this method.

    if (iInPort != NULL)
    {
        Format = ((PVMFOMXDecPort*)iInPort)->iFormat;
    }
    if (Format == PVMF_MIME_H264_VIDEO ||
            Format == PVMF_MIME_H264_VIDEO_MP4)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::InitDecoder() for H264 Decoder. Initialization data Size %d.", initbufsize));

        if ((initbufsize > 0) && (iConfigDataBytesProcessed < (uint8)initbufsize))
        {


            // this flag may be reset right away, but we may end-up running out of buffers
            iIsThereMoreConfigDataToBeSent = true;

            // there may be more than 1 NAL in config info in format specific data memfragment (SPS, PPS)
            tmp_ptr = initbuffer + iConfigDataBytesProcessed;
            do
            {
                // config data for H264 through OMX dec node port is in format - 2 byte interleaved nal length
                nal_length = (uint32)(((uint16)tmp_ptr[1] << 8) | tmp_ptr[0]);
                tmp_ptr += 2; // skip 2 byte NAL length

                // just to make sure we're not out of bounds
                if ((nal_length + iConfigDataBytesProcessed + 2) > (uint8)initbufsize)
                    break;

                status = SendConfigBufferToOMXComponent(tmp_ptr, nal_length);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::InitDecoder() Cannot process config buffers - possibly out of input buffers due to large number of SPS or PPS NALs"));
                    return status;

                }

                // after at least one buffer is sent, set the flag requiring config data processing by the component
                iIsConfigDataProcessingCompletionNeeded = true;

                iConfigDataBuffersOutstanding++;
                iConfigDataBytesProcessed += (nal_length + 2); // account for 2 byte NAL size
                tmp_ptr += nal_length;

            }
            while (iConfigDataBytesProcessed < (uint8)initbufsize);
        }
        // at this point, we've sent all there is to be sent
        iIsThereMoreConfigDataToBeSent = false;


    }
    else if (Format == PVMF_MIME_H264_VIDEO_RAW)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::InitDecoder() for H264 Decoder. Initialization data Size %d.", initbufsize));

        if ((initbufsize > 0)  && (iConfigDataBytesProcessed < (uint8)initbufsize))
        {


            // this flag may be reset right away, but we may end-up running out of buffers and come back here
            iIsThereMoreConfigDataToBeSent = true;

            uint32 sc_size = 0;

            // there may be more than 1 NAL in config info in format specific data memfragment (SPS, PPS)
            tmp_ptr = initbuffer + iConfigDataBytesProcessed;
            size = initbufsize - iConfigDataBytesProcessed;
            do
            {
                sc_size = (uint32)tmp_ptr;
                nal_length = GetNAL_OMXNode(&tmp_ptr, &size);
                sc_size = (uint32)tmp_ptr - sc_size;

                // send ptr to beginning of start code rather than beginning of NAL
                status = SendConfigBufferToOMXComponent(tmp_ptr - sc_size, nal_length + sc_size);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::InitDecoder() Problem in processing config buffer - possibly out of input buffers for SPS PPS NALs"));
                    return status;

                }

                // set the flag requiring config data processing by the component
                iIsConfigDataProcessingCompletionNeeded = true;

                iConfigDataBuffersOutstanding++;
                tmp_ptr += nal_length;
                iConfigDataBytesProcessed += (nal_length + sc_size);
            }
            while (size);
        }
        // at this point, we've sent all there is to be sent
        iIsThereMoreConfigDataToBeSent = false;


    }
    else if (Format == PVMF_MIME_M4V ||
             Format == PVMF_MIME_H2631998 ||
             Format == PVMF_MIME_H2632000 ||
             Format == PVMF_MIME_REAL_VIDEO)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::InitDecoder() for MPEG4 Decoder. Initialization data Size %d.", initbufsize));

        // for H263, the initbufsize is 0, and initbuf= NULL. Config is done after 1st frame of data
        if (initbufsize > 0)
        {


            status = SendConfigBufferToOMXComponent(initbuffer, initbufsize);
            if (status != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::InitDecoder() Error in processing config buffer"));
                iIsThereMoreConfigDataToBeSent = true;
                return status;
            }

            iConfigDataBuffersOutstanding++;
            // set the flag requiring config data processing by the component
            iIsConfigDataProcessingCompletionNeeded = true;
            // at this point, we've sent all there is to be sent
            iIsThereMoreConfigDataToBeSent = false;

        }
        else
        {
            iIsThereMoreConfigDataToBeSent = false;
        }
    }
    else if (Format == PVMF_MIME_WMV)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::InitDecoder() for WMV Decoder. Initialization data Size %d.", initbufsize));

        if ((initbufsize > 0)  && (iConfigDataBytesProcessed < (uint8)initbufsize))
        {
            if (iIsVC1)
            {
                // VC1 component, need to convert from ASF CodecSpecificInfo to VC1 AnnexL2
                if (!ConvertASFConfigInfoToVC1AndSend(initbuffer, initbufsize))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::InitDecoder() Error in processing seq header for VC1"));
                    return PVMFFailure;
                }
                // iConfigDataBuffersOutstanding was incremented inside the above method
            }
            else
            {
                status = SendConfigBufferToOMXComponent(initbuffer, initbufsize);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::InitDecoder() Error in processing config buffer for WMV"));

                    iIsThereMoreConfigDataToBeSent = true;
                    return status;
                }
                iConfigDataBuffersOutstanding++;
            }

            // set the flag requiring config data processing by the component
            iIsConfigDataProcessingCompletionNeeded = true;
        }

        // at this point, we've sent all there is to be sent
        iIsThereMoreConfigDataToBeSent = false;

    }
    else
    {
        // Unknown codec type
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXVideoDecNode::InitDecoder() Unknown codec type"));
        return PVMFFailure;
    }

    //Varibles initialization
    //sendFsi = true;

    return PVMFSuccess;
}

#define GetUnalignedDword( pb, dw ) \
            (dw) = ((uint32) *(pb + 3) << 24) + \
                   ((uint32) *(pb + 2) << 16) + \
                   ((uint16) *(pb + 1) << 8) + *pb;

#define GetUnalignedDwordEx( pb, dw )   GetUnalignedDword( pb, dw ); (pb) += sizeof(uint32);
#define LoadDWORD( dw, p )  GetUnalignedDwordEx( p, dw )
//For WMVA
#define SC_SEQ          0x0F
#define SC_ENTRY        0x0E

#ifndef MAKEFOURCC_WMC
#define MAKEFOURCC_WMC(ch0, ch1, ch2, ch3) \
        ((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |   \
        ((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define mmioFOURCC_WMC(ch0, ch1, ch2, ch3)  MAKEFOURCC_WMC(ch0, ch1, ch2, ch3)
#endif

#define FOURCC_WMV3     mmioFOURCC_WMC('W','M','V','3')
#define FOURCC_WMVA     mmioFOURCC_WMC('W','M','V','A')
#define FOURCC_WVC1     mmioFOURCC_WMC('W','V','C','1')


bool PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1AndSend(uint8* initbuffer, int32 initbufsize)
{
    PVMFStatus status = PVMFSuccess;
    uint8 *pData;
    uint32 dwdat;
    uint32 temp;
    uint32 Compression;
    uint32 hrd_buffer, hrd_rate, cbr, level;
    int32 Width, Height;
    uint32 vc1_buf[9]; // 36 bytes according to Table 265 of VC1 spec.

    temp = 0xC5FFFFFF;/* 0xC5 |   NUM_FRAMES  | */

    vc1_buf[0] = temp; /* write unsigned int in little-endian */

    temp = 0x4;
    vc1_buf[1] = temp; /* write unsigned int in little-endian */

    pData = initbuffer + 15; // position ptr to Width & Height
    LoadDWORD(dwdat, pData);
    Width = dwdat;
    LoadDWORD(dwdat, pData);
    Height = dwdat;

    pData += 4; //position ptr to Compression type

    LoadDWORD(dwdat, pData);
    Compression = dwdat;

    switch (Compression)
    {
        case FOURCC_WMV3:
        {
            iIsVC1AdvancedProfile = false;
            pData = initbuffer + 11 + 40; //sizeof(BITMAPINFOHEADER); // position to sequence header

            LoadDWORD(dwdat, pData);
            vc1_buf[2] = dwdat;  /* write STRUCT_C */

            level = (dwdat & 0x38) >> 3;
            hrd_rate = 128000; /* default value for peak bitrate */
            hrd_buffer = 5000; /* default value in millisecond */

            vc1_buf[3] = Height; /* write STRUCT_A VERT_SIZE */
            vc1_buf[4] = Width; /* write STRUCT_A HORIZ_SIZE*/
            vc1_buf[5] = 0xC;

            cbr = 0; /* default to VBR */
            temp = (level << 29) | (cbr << 28) | (hrd_buffer);
            vc1_buf[6] = temp;   /* write  STRUCT_B (Level:3|CBR:1|RES1:4|HRD_BUFFER:24) */
            vc1_buf[7] = hrd_rate; /* write STRUCT_B (HRD_RATE) */
            vc1_buf[8] = 0xFFFFFFFF; /* write STRUCT_B (FRAME_RATE)  unknown*/

            status = SendConfigBufferToOMXComponent((uint8*)(&vc1_buf[0]), 36);
            if (status != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1AndSend() Error in processing config buffer"));
                iIsThereMoreConfigDataToBeSent = true;
                return false;
            }
            iConfigDataBuffersOutstanding++;
            iIsThereMoreConfigDataToBeSent = false;

        }
        break;
        case FOURCC_WMVA:
        case FOURCC_WVC1:
        {
            int size = 0;

            iIsVC1AdvancedProfile = true;
            pData = initbuffer + 52; // position to sequence header
            // search for entry point SC
            while (size < initbufsize - 52)
            {
                if (!pData[0] && !pData[1] && (pData[2] == 1) && (pData[3] == 0xE))
                {
                    break;
                }
                pData++;
                size++;
            }

            if (size)
            {
                /* send sequence start code */
                status = SendConfigBufferToOMXComponent(initbuffer + 52, size);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1AndSend() Error in processing seq header"));
                    return false;
                }
                iConfigDataBuffersOutstanding++;

            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1AndSend() no sequence start code"));
                return false;
            }

            if (initbufsize - 52 - size) // size of entry point SC
            {
                /* send entry point start code */
                status = SendConfigBufferToOMXComponent(initbuffer + 52 + size, initbufsize - 52 - size);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1AndSend() Error in processing seq header"));
                    return false;
                }
                iConfigDataBuffersOutstanding++;
            }
            else  // if not present, shall we generate it ?
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1AndSend() no entry point start code"));
                return false;
            }

        }
        break;
        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "PVMFOMXVideoDecNode::ConvertASFConfigInfoToVC1() unknown FOURCC."));
            return false;
    }


    return true ;

}

////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Put output buffer in outgoing queue //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXVideoDecNode::QueueOutputBuffer(OsclSharedPtr<PVMFMediaDataImpl> &mediadataimplout, uint32 aDataLen)
{

    bool status = true;
    PVMFSharedMediaDataPtr mediaDataOut;
    int32 leavecode = OsclErrNone;

    // NOTE: ASSUMPTION IS THAT OUTGOING QUEUE IS BIG ENOUGH TO QUEUE ALL THE OUTPUT BUFFERS
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::QueueOutputBuffer: In"));

    if (!iOutPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFOMXVideoDecNode::QueueOutputBuffer() Output Port doesn't exist!!!!!!!!!!"));
        return false;
    }
    // First check if we can put outgoing msg. into the queue
    if (iOutPort->IsOutgoingQueueBusy())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFOMXVideoDecNode::QueueOutputBuffer() OutgoingQueue is busy"));
        return false;
    }

    OSCL_TRY(leavecode,
             mediaDataOut = PVMFMediaData::createMediaData(mediadataimplout, iOutputMediaDataMemPool););
    if (OsclErrNone == leavecode)
    {

        // Update the filled length of the fragment
        mediaDataOut->setMediaFragFilledLen(0, aDataLen);

        // Set timestamp
        mediaDataOut->setTimestamp(iOutTimeStamp);

        // If Duration is available, set the Duration Bit flag in MarkerInfo and set the duration
        if ((iSampleDurationVec.size() > 0) && (iSampleDurationVec.back() > 0))
        {
            // continue this until iOutTimestamp matches the timestamp in the queue.
            while (!(iTimestampVec.empty()) && (iOutTimeStamp > iTimestampVec.back()))
            {
                // continue discarding samples
                iTimestampVec.pop_back();
                iSampleDurationVec.pop_back();
            }

            // Go further only if the sample timestamp matches with the one in TimeStamp vector queue
            if (!(iTimestampVec.empty()) && (iOutTimeStamp == iTimestampVec.back()))
            {
                uint32 markerInfo = 0;
                markerInfo |= mediaDataOut->getMarkerInfo();

                //set the duration bit
                markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_DURATION_AVAILABLE_BIT;
                // set the markerInfo
                mediaDataOut->setMarkerInfo(markerInfo);

                // set the duration and then erase from SampleDurationVector
                mediaDataOut->setDuration(iSampleDurationVec.back());
                iSampleDurationVec.pop_back();

                // also pop out the Timestamp from thw queue
                iTimestampVec.pop_back();
            }
        }

        // Set Streamid
        mediaDataOut->setStreamID(iStreamID);

        // Set sequence number
        mediaDataOut->setSeqNum(iSeqNum++);

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iDataPathLogger, PVLOGMSG_INFO, (0, ":PVMFOMXVideoDecNode::QueueOutputBuffer(): - SeqNum=%d, TS=%d", iSeqNum, iOutTimeStamp));

        int fsiErrorCode = 0;

        // Check if Fsi configuration need to be sent
        if (sendFsi && !iCompactFSISettingSucceeded)
        {
            OsclRefCounterMemFrag yuvFsiMemfrag;

            OSCL_TRY(fsiErrorCode, yuvFsiMemfrag = iFsiFragmentAlloc.get(););

            OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                 (0, "PVMFOMXVideoDecNode::QueueOutputBuffer() Failed to allocate memory for  FSI")));

            if (fsiErrorCode == 0)
            {
                PVMFYuvFormatSpecificInfo0* fsiInfo = OSCL_PLACEMENT_NEW(yuvFsiMemfrag.getMemFragPtr(), PVMFYuvFormatSpecificInfo0());
                if (fsiInfo != NULL)
                {
                    fsiInfo->uid = PVMFYuvFormatSpecificInfo0_UID;
                    fsiInfo->video_format = iYUVFormat;
                    fsiInfo->viewable_width = iYUVWidth;
                    fsiInfo->viewable_height = iYUVHeight;
                    fsiInfo->num_buffers = 0;
                    fsiInfo->buffer_size = 0;
                    fsiInfo->buffer_width = iStride;
                    fsiInfo->buffer_height = iSliceHeight;
                    fsiInfo->buffer_size = iOMXComponentOutputBufferSize;

                    // NOTE: we first try to send the FSI_YUV key. If the MIO can take it - we're ok.
                    // if MIO can't take it - for bw compatibility - we'll send the ordinary FSI key
                    OsclMemAllocator alloc;
                    int32 KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) + 1;
                    PvmiKeyType KvpKey = NULL;

                    AllocatePvmiKey(&KvpKey, &alloc, KeyLength);

                    if (NULL == KvpKey)
                    {
                        SetState(EPVMFNodeError);
                        ReportErrorEvent(PVMFErrNoMemory);
                        return false;
                    }

                    oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV, KeyLength);
                    int32 err;
                    bool success = false;

                    OSCL_TRY(err, success = ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(yuvFsiMemfrag, KvpKey););
                    if (err != OsclErrNone || !success)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "PVMFOMXVideoDecNode::QueueOutputBuffer - Problem to set FSI_YUV key, will try to send ordinary FSI"));

                        alloc.deallocate((OsclAny*)(KvpKey));

                        // try to send the ordinary FSI
                        KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY) + 1;
                        KvpKey = (PvmiKeyType)alloc.ALLOCATE(KeyLength);
                        if (NULL == KvpKey)
                        {
                            SetState(EPVMFNodeError);
                            ReportErrorEvent(PVMFErrNoMemory);
                            fsiInfo->video_format.~PVMFFormatType();
                            return false;
                        }

                        oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY, KeyLength);


                        OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(yuvFsiMemfrag, KvpKey););
                        if (err != OsclErrNone)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "PVMFOMXVideoDecNode::QueueOutputBuffer - Problem to set ordinary FSI as well"));

                            alloc.deallocate((OsclAny*)(KvpKey));

                            fsiInfo->video_format.~PVMFFormatType();

                            SetState(EPVMFNodeError);
                            ReportErrorEvent(PVMFErrNoMemory);
                            return false; // this is going to make everything go out of scope
                        }
                    }

                    alloc.deallocate((OsclAny*)(KvpKey));
                    fsiInfo->video_format.~PVMFFormatType();

                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXVideoDecNode::QueueOutputBuffer - Problem allocating Output FSI"));
                    SetState(EPVMFNodeError);
                    ReportErrorEvent(PVMFErrNoMemory);
                    return false; // this is going to make everything go out of scope
                }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXVideoDecNode::QueueOutputBuffer - Problem allocating Output FSI"));
                return false; // this is going to make everything go out of scope
            }


            // Reset the flag
            sendFsi = false;
        }



        // in case of special YVU format, attach fsi to every outgoing message containing ptr to private data
        if (iYUVFormat == PVMF_MIME_YUV420_SEMIPLANAR_YVU)
        {
            OsclRefCounterMemFrag privatedataFsiMemFrag;

            OSCL_TRY(fsiErrorCode, privatedataFsiMemFrag = iPrivateDataFsiFragmentAlloc.get(););

            OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                 (0, "PVMFOMXVideoDecNode::QueueOutputBuffer() Failed to allocate memory for  FSI for private data")));


            if (fsiErrorCode == 0)
            {
                uint8 *fsiptr = (uint8*) privatedataFsiMemFrag.getMemFragPtr();
                privatedataFsiMemFrag.getMemFrag().len = sizeof(OsclAny*);
                oscl_memcpy(fsiptr, &ipPrivateData, sizeof(OsclAny *)); // store ptr data into fsi
                mediaDataOut->setFormatSpecificInfo(privatedataFsiMemFrag);
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXVideoDecNode::QueueOutputBuffer - Problem allocating Output FSI for private data"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false; // this is going to make everything go out of scope
            }
        }


        if (fsiErrorCode == 0)
        {
            // Send frame to downstream node
            PVMFSharedMediaMsgPtr mediaMsgOut;
            convertToPVMFMediaMsg(mediaMsgOut, mediaDataOut);

            if (iOutPort && (iOutPort->QueueOutgoingMsg(mediaMsgOut) == PVMFSuccess))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "PVMFOMXVideoDecNode::QueueOutputBuffer(): Queued frame OK "));

            }
            else
            {
                // we should not get here because we always check for whether queue is busy or not
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::QueueOutputBuffer(): Send frame failed"));
                return false;
            }

        }


    }//end of if (leavecode==0)
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXVideoDecNode::QueueOutputBuffer() call PVMFMediaData::createMediaData is failed"));
        return false;
    }

    return status;

}

/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXVideoDecNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::DoRequestPort() In"));
    //This node supports port request from any state

    //retrieve port tag.
    int32 tag;
    OSCL_String* portconfig;

    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, portconfig);

    int32 leavecode = OsclErrNone;
    //validate the tag...
    switch (tag)
    {
        case PVMF_OMX_DEC_NODE_PORT_TYPE_INPUT:
            if (iInPort)
            {
                return PVMFFailure;
                break;
            }
            OSCL_TRY(leavecode, iInPort = OSCL_NEW(PVMFOMXDecPort, ((int32)tag, this, PVMF_OMX_VIDEO_DEC_INPUT_PORT_NAME)););
            if (leavecode || iInPort == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::DoRequestPort: Error - Input port instantiation failed"));
                return PVMFErrArgument;
            }
            aPort = iInPort;
            break;

        case PVMF_OMX_DEC_NODE_PORT_TYPE_OUTPUT:
            if (iOutPort)
            {
                return PVMFFailure;
                break;
            }
            OSCL_TRY(leavecode, iOutPort = OSCL_NEW(PVMFOMXDecPort, ((int32)tag, this, PVMF_OMX_VIDEO_DEC_OUTPUT_PORT_NAME)));
            if (leavecode || iOutPort == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXVideoDecNode::DoRequestPort: Error - Output port instantiation failed"));
                return PVMFErrArgument;
            }
            aPort = iOutPort;
            break;

        default:
            //bad port tag
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXVideoDecNode::DoRequestPort: Error - Invalid port tag"));
            return PVMFErrArgument;
    }
    return PVMFSuccess;
}


/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXVideoDecNode::DoGetNodeMetadataValue()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoGetNodeMetadataValue() In"));

    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    iCurrentCommand.PVMFNodeCommand::Parse(keylistptr, valuelistptr, starting_index, max_entries);

    // Check the parameters
    if (keylistptr == NULL || valuelistptr == NULL)
    {
        return PVMFErrArgument;
    }

    uint32 numkeys = keylistptr->size();

    if (starting_index > (numkeys - 1) || numkeys <= 0 || max_entries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    int32 numentriesadded = 0;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        int32 leavecode = OsclErrNone;
        int32 leavecode1 = OsclErrNone;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;
        uint32 KeyLen = 0;

        if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_WIDTH_KEY) == 0) &&
                iYUVWidth > 0)
        {
            // Video width
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_WIDTH_KEY) + 1; // for "codec-info/video/width;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                // Allocate memory for the string
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);
                if (OsclErrNone == leavecode)
                {
                    // Copy the key string
                    oscl_strncpy(KeyVal.key, PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_WIDTH_KEY, oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_WIDTH_KEY) + 1);
                    oscl_strncat(KeyVal.key, PVOMXVIDEODECMETADATA_SEMICOLON, oscl_strlen(PVOMXVIDEODECMETADATA_SEMICOLON));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                    KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                    // Copy the value
                    KeyVal.value.uint32_value = iYUVWidth;
                    // Set the length and capacity
                    KeyVal.length = 1;
                    KeyVal.capacity = 1;
                }
                else
                {
                    // Memory allocation failed
                    KeyVal.key = NULL;
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_HEIGHT_KEY) == 0) &&
                 iYUVHeight > 0)
        {
            // Video height
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_HEIGHT_KEY) + 1; // for "codec-info/video/height;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                // Allocate memory for the string
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);
                if (OsclErrNone == leavecode)
                {
                    // Copy the key string
                    oscl_strncpy(KeyVal.key, PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_HEIGHT_KEY, oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_HEIGHT_KEY) + 1);
                    oscl_strncat(KeyVal.key, PVOMXVIDEODECMETADATA_SEMICOLON, oscl_strlen(PVOMXVIDEODECMETADATA_SEMICOLON));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                    KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                    // Copy the value
                    KeyVal.value.uint32_value = iYUVHeight;
                    // Set the length and capacity
                    KeyVal.length = 1;
                    KeyVal.capacity = 1;
                }
                else
                {
                    // Memory allocation failed
                    KeyVal.key = NULL;
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_PROFILE_KEY) == 0))
        {
            // Video profile
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_PROFILE_KEY) + 1; // for "codec-info/video/profile;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                // Allocate memory for the string
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);

                if (OsclErrNone == leavecode)
                {
                    PVMF_MPEGVideoProfileType aProfile;
                    PVMF_MPEGVideoLevelType aLevel;
                    if (GetProfileAndLevel(aProfile, aLevel) == PVMFSuccess)
                    {
                        // Copy the key string
                        oscl_strncpy(KeyVal.key, PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_PROFILE_KEY, oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_PROFILE_KEY) + 1);
                        oscl_strncat(KeyVal.key, PVOMXVIDEODECMETADATA_SEMICOLON, oscl_strlen(PVOMXVIDEODECMETADATA_SEMICOLON));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                        KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                        // Copy the value
                        KeyVal.value.uint32_value = (uint32)aProfile; // This is to be decided, who will interpret these value
                        // Set the length and capacity
                        KeyVal.length = 1;
                        KeyVal.capacity = 1;
                    }
                    else
                    {
                        // Memory allocation failed
                        KeyVal.key = NULL;
                        break;
                    }
                }
                else
                {
                    // Memory allocation failed
                    KeyVal.key = NULL;
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_LEVEL_KEY) == 0))
        {
            // Video level
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_LEVEL_KEY) + 1; // for "codec-info/video/level;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                // Allocate memory for the string
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);

                if (OsclErrNone == leavecode)
                {
                    PVMF_MPEGVideoProfileType aProfile;
                    PVMF_MPEGVideoLevelType aLevel;
                    if (GetProfileAndLevel(aProfile, aLevel) == PVMFSuccess)
                    {
                        // Copy the key string
                        oscl_strncpy(KeyVal.key, PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_LEVEL_KEY, oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_LEVEL_KEY) + 1);
                        oscl_strncat(KeyVal.key, PVOMXVIDEODECMETADATA_SEMICOLON, oscl_strlen(PVOMXVIDEODECMETADATA_SEMICOLON));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                        KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                        // Copy the value
                        KeyVal.value.uint32_value = (uint32)aLevel; // This is to be decided, who will interpret these value
                        // Set the length and capacity
                        KeyVal.length = 1;
                        KeyVal.capacity = 1;
                    }
                    else
                    {
                        // Memory allocation failed
                        KeyVal.key = NULL;
                        break;
                    }
                }
                else
                {
                    // Memory allocation failed
                    KeyVal.key = NULL;
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_AVGBITRATE_KEY) == 0) &&
                 (iAvgBitrateValue > 0))
        {
            // Video average bitrate
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_AVGBITRATE_KEY) + 1; // for "codec-info/video/avgbitrate;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                // Allocate memory for the string
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);

                if (OsclErrNone == leavecode)
                {
                    // Copy the key string
                    oscl_strncpy(KeyVal.key, PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_AVGBITRATE_KEY, oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_AVGBITRATE_KEY) + 1);
                    oscl_strncat(KeyVal.key, PVOMXVIDEODECMETADATA_SEMICOLON, oscl_strlen(PVOMXVIDEODECMETADATA_SEMICOLON));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                    KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                    // Copy the value
                    KeyVal.value.uint32_value = iAvgBitrateValue;
                    // Set the length and capacity
                    KeyVal.length = 1;
                    KeyVal.capacity = 1;

                }
                else
                {
                    // Memory allocation failed
                    KeyVal.key = NULL;
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_FORMAT_KEY) == 0) &&
                 (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2631998 || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2632000 ||
                  ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_M4V ||
                  ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4 ||
                  ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW  || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMV || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_VIDEO))
        {
            // Format
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_FORMAT_KEY) + 1; // for "codec-info/video/format;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING_CONSTCHAR) + 1; // for "char*" and NULL terminator

                uint32 valuelen = 0;

                if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_H264_VIDEO)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_H264_VIDEO_MP4)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_H264_VIDEO_RAW)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_M4V)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_M4V)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2631998)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_H2631998)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2632000)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_H2632000)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMV)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_WMV)) + 1; // Value string plus one for NULL terminator
                }
                else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_VIDEO)
                {
                    valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_REAL_VIDEO)) + 1; // Value string plus one for NULL terminator
                }
                else
                {
                    // Should not enter here
                    OSCL_ASSERT(false);
                    valuelen = 1;
                }

                // Allocate memory for the strings
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);
                if (OsclErrNone == leavecode)
                {
                    KeyVal.value.pChar_value = (char*) AllocateKVPKeyArray(leavecode1, PVMI_KVPVALTYPE_CHARPTR, valuelen);
                }

                if (OsclErrNone == leavecode && OsclErrNone == leavecode1)
                {
                    // Copy the key string
                    oscl_strncpy(KeyVal.key, PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_FORMAT_KEY, oscl_strlen(PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_FORMAT_KEY) + 1);
                    oscl_strncat(KeyVal.key, PVOMXVIDEODECMETADATA_SEMICOLON, oscl_strlen(PVOMXVIDEODECMETADATA_SEMICOLON));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_CHARPTR_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING_CONSTCHAR));
                    KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                    // Copy the value
                    if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_H264_VIDEO), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_H264_VIDEO_MP4), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_H264_VIDEO_RAW), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_M4V)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_M4V), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2631998)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_H2631998), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2632000)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_H2632000), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMV)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_WMV), valuelen);
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_VIDEO)
                    {
                        oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_REAL_VIDEO), valuelen);
                    }
                    else
                    {
                        // Should not enter here
                        OSCL_ASSERT(false);
                    }
                    KeyVal.value.pChar_value[valuelen-1] = NULL_TERM_CHAR;
                    // Set the length and capacity
                    KeyVal.length = valuelen;
                    KeyVal.capacity = valuelen;
                }
                else
                {
                    // Memory allocation failed so clean up
                    if (KeyVal.key)
                    {
                        OSCL_ARRAY_DELETE(KeyVal.key);
                        KeyVal.key = NULL;
                    }
                    if (KeyVal.value.pChar_value)
                    {
                        OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                    }
                    break;
                }
            }
        }

        if (KeyVal.key != NULL)
        {
            leavecode = OsclErrNone;
            leavecode = PushKVP(KeyVal, *valuelistptr);
            if (OsclErrNone != leavecode)
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                OSCL_ARRAY_DELETE(KeyVal.key);
                KeyVal.key = NULL;
            }
            else
            {
                // Increment the counter for number of value entries added to the list
                ++numentriesadded;
            }

            // Check if the max number of value entries were added
            if (max_entries > 0 && numentriesadded >= max_entries)
            {
                break;
            }
        }
    }

    return PVMFSuccess;
}

/////////////////////////////////////////////////////////////////////////////
bool PVMFOMXVideoDecNode::ReleaseAllPorts()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::ReleaseAllPorts() In"));

    if (iInPort)
    {
        iInPort->ClearMsgQueues();
        iInPort->Disconnect();
        OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
        iInPort = NULL;
    }

    if (iOutPort)
    {
        iOutPort->ClearMsgQueues();
        iOutPort->Disconnect();
        OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
        iOutPort = NULL;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
uint32 PVMFOMXVideoDecNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::GetNumMetadataValues() called"));

    uint32 numkeys = aKeyList.size();

    if (numkeys <= 0)
    {
        // Don't do anything
        return 0;
    }

    // Count the number of value entries for the provided key list
    uint32 numvalentries = 0;
    PVMF_MPEGVideoProfileType aProfile;
    PVMF_MPEGVideoLevelType aLevel;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_WIDTH_KEY) == 0) &&
                iYUVWidth > 0)
        {
            // Video width
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_HEIGHT_KEY) == 0) &&
                 iYUVHeight > 0)
        {
            // Video height
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_PROFILE_KEY) == 0) &&
                 (GetProfileAndLevel(aProfile, aLevel) == PVMFSuccess))

        {
            // Video profile
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_LEVEL_KEY) == 0) &&
                 (GetProfileAndLevel(aProfile, aLevel) == PVMFSuccess))
        {
            // Video level
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_AVGBITRATE_KEY) == 0) &&
                 (iAvgBitrateValue > 0))

        {
            // Video average bitrate
            if (iAvgBitrateValue > 0)
                ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXVIDEODECMETADATA_CODECINFO_VIDEO_FORMAT_KEY) == 0) &&
                 (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMV || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_M4V || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2631998 || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2632000 || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4 || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_VIDEO || ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW))
        {
            // Format
            ++numvalentries;
        }
    }

    return numvalentries;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// CAPABILITY CONFIG PRIVATE
PVMFStatus PVMFOMXVideoDecNode::DoCapConfigGetParametersSync(PvmiKeyType aIdentifier, PvmiKvp*& aParameters, int& aNumParamElements, PvmiCapabilityContext aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() In"));
    OSCL_UNUSED_ARG(aContext);

    // Initialize the output parameters
    aNumParamElements = 0;
    aParameters = NULL;

    // Count the number of components and parameters in the key
    int compcount = pv_mime_string_compcnt(aIdentifier);
    // Retrieve the first component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(0, aIdentifier, compstr);

    if (pv_mime_strcmp(aIdentifier, _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) == 0)
    {
        // get the list of omx components that support the track.
        // Track config info and mime type was previously set through capconfig - setparametersync
        Oscl_Vector<OMX_STRING, OsclMemAllocator> roles;

        pvVideoConfigParserInputs aInputs;
        OMXConfigParserInputs aInputParameters;
        VideoOMXConfigParserOutputs aOutputParameters;

        aInputs.inPtr = (uint8*) iTrackUnderVerificationConfig;
        aInputs.inBytes = (int32) iTrackUnderVerificationConfigSize;
        aInputs.iMimeType = iNodeConfig.iMimeType;

        aInputParameters.inBytes = aInputs.inBytes;
        aInputParameters.inPtr = aInputs.inPtr;

        if (aInputs.inBytes == 0 || aInputs.inPtr == NULL)
        {
            // in case of following formats - config codec data is expected to
            // be present in the query. If not, config parser cannot be called

            if (aInputs.iMimeType == PVMF_MIME_H264_VIDEO ||
                    aInputs.iMimeType == PVMF_MIME_H264_VIDEO_MP4 ||
                    aInputs.iMimeType == PVMF_MIME_H264_VIDEO_RAW ||
                    aInputs.iMimeType == PVMF_MIME_M4V ||
                    aInputs.iMimeType == PVMF_MIME_WMV)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParameters() Codec Config data is not present"));
                return PVMFErrNotSupported;
            }
        }

        if (aInputs.iMimeType ==  PVMF_MIME_H264_VIDEO ||
                aInputs.iMimeType == PVMF_MIME_H264_VIDEO_MP4 ||
                aInputs.iMimeType == PVMF_MIME_H264_VIDEO_RAW)
        {
            roles.push_back((OMX_STRING) "video_decoder.avc");

        }
        else if (aInputs.iMimeType ==  PVMF_MIME_M4V)
        {
            roles.push_back((OMX_STRING)"video_decoder.mpeg4");
        }
        else if (aInputs.iMimeType ==  PVMF_MIME_H2631998 ||
                 aInputs.iMimeType == PVMF_MIME_H2632000)
        {
            roles.push_back((OMX_STRING)"video_decoder.h263");
        }
        else if (aInputs.iMimeType ==  PVMF_MIME_WMV)
        {
            roles.push_back((OMX_STRING)"video_decoder.vc1");
            roles.push_back((OMX_STRING)"video_decoder.wmv");
        }
        else
        {
            // Illegal codec specified.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Input port format other then codec type", iName.Str()));
            return PVMFErrNotSupported;
        }



        OMX_BOOL status = OMX_FALSE;
        OMX_U32 total_num_comps = 0;
        OMX_STRING *CompOfRole;
        OMX_U32 ii;
        OMX_U32 *num_comps_for_role = NULL;
        OMX_U32 num_roles = roles.size(); // this is typically 1, but can be more than 1
        OMX_ERRORTYPE Err = OMX_ErrorNone;

        num_comps_for_role = (OMX_U32*) oscl_malloc(num_roles * sizeof(OMX_U32));
        if (num_comps_for_role == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for num array failed"));
            return PVMFErrNoMemory;
        }

        Oscl_Vector<OMX_STRING, OsclMemAllocator>::iterator role;

        // get the total number of components first that supports N roles
        for (ii = 0, role = roles.begin(); role != roles.end(); role++, ii++)
        {
            aInputParameters.cComponentRole = *role;

            // call once to find out the number of components that can fit the given role and save
            Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps_for_role[ii], NULL);
            if (OMX_ErrorNone == Err)
            {
                total_num_comps += num_comps_for_role[ii];
            }
        }

        // do memory and parameter allocation
        if (total_num_comps > 0)
        {
            // allocate num_comps kvps and keys all in one block

            aParameters = (PvmiKvp*)oscl_malloc(total_num_comps * sizeof(PvmiKvp));
            if (aParameters == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for KVP failed"));
                oscl_free(num_comps_for_role);
                return PVMFErrNoMemory;
            }

            oscl_memset(aParameters, 0, total_num_comps*sizeof(PvmiKvp));

            // Allocate memory for the key strings in each KVP
            PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(total_num_comps * (sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) + 1) * sizeof(char));
            if (memblock == NULL)
            {
                oscl_free(aParameters);
                oscl_free(num_comps_for_role);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for key string failed"));
                return PVMFErrNoMemory;
            }
            oscl_strset(memblock, 0, total_num_comps *(sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) + 1) * sizeof(char));


            // allocate a block of memory for component names
            OMX_U8 *memblockomx = (OMX_U8 *) oscl_malloc(total_num_comps * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            if (memblockomx == NULL)
            {
                oscl_free(aParameters);
                oscl_free(memblock);
                oscl_free(num_comps_for_role);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for omx component strings failed"));
                return PVMFErrNoMemory;
            }

            oscl_memset(memblockomx, 0, total_num_comps * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));

            // allocate a placeholder
            CompOfRole = (OMX_STRING *)oscl_malloc(total_num_comps * sizeof(OMX_STRING));
            if (CompOfRole == NULL)
            {
                oscl_free(memblockomx);
                oscl_free(aParameters);
                oscl_free(memblock);
                oscl_free(num_comps_for_role);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for omx component strings failed"));
                return PVMFErrNoMemory;
            }

            for (ii = 0; ii < total_num_comps; ii++)
            {
                CompOfRole[ii] = (OMX_STRING)(memblockomx + (ii * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8)));
            }
            //////////// so far, we allocated & initialized all memory. Now, go through roles, and add components that support the role



            OMX_U32 num = 0;
            OMX_U32 jj = 0;
            OMX_U32 cumulative_comps = 0;
            for (jj = 0, role = roles.begin(); role != roles.end(); role++, jj++)
            {
                aInputParameters.cComponentRole = *role;

                // adjust the value of indices
                // call 2nd time to get the component names
                Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num, (OMX_U8 **) & CompOfRole[cumulative_comps]);

                if (OMX_ErrorNone == Err)
                {
                    for (ii = cumulative_comps; ii < cumulative_comps + num_comps_for_role[jj]; ii++)
                    {
                        aInputParameters.cComponentName = CompOfRole[ii];
                        status = OMX_MasterConfigParser(&aInputParameters, &aOutputParameters);
                        if (status == OMX_TRUE)
                        {
                            // component passes the test - write the component name into kvp list
                            // write the key
                            aParameters[ii].key = memblock + (aNumParamElements * (sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) + 1) * sizeof(char)) ;
                            oscl_strncat(aParameters[ii].key, _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY), sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)));
                            aParameters[ii].key[sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY))] = 0; // null terminate

                            // write the length
                            aParameters[ii].length = PV_OMX_MAX_COMPONENT_NAME_LENGTH;

                            aParameters[ii].value.pChar_value = CompOfRole[ii];
                            aNumParamElements++;

                            // also record width and height for future reference (query by the player engine)
                            iNewWidth = aOutputParameters.width;
                            iNewHeight = aOutputParameters.height;

                        }
                    }
                    cumulative_comps += num_comps_for_role[jj];
                }
            }

            // free memory for CompOfRole placeholder.
            // The other blocks of memory will be freed during release parameter sync
            oscl_free(CompOfRole);
            oscl_free(num_comps_for_role);
        }
        else
        {
            // if no component supports the role, nothing else to do
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::PVMFOMXVideoDecNode::DoCapConfigGetParameters() No OMX components support the role", iName.Str()));
            oscl_free(num_comps_for_role);
            return PVMFErrNotSupported;
        }


        return PVMFSuccess;


    }
    if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/decoder")) < 0) || compcount < 3)
    {
        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/render")) < 0) || compcount != 3)
        {
            // First 3 component should be "x-pvmf/video/decoder" and there must
            // be at least three components
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() Unsupported key"));
            return PVMFErrArgument;
        }
    }

    if (pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/render")) >= 0)
    {
        aParameters = (PvmiKvp*)oscl_malloc(PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS * sizeof(PvmiKvp));
        if (aParameters == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for KVP failed"));
            return PVMFErrNoMemory;
        }
        oscl_memset(aParameters, 0, PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS*sizeof(PvmiKvp));
        // Allocate memory for the key strings in each KVP
        PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS * PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE * sizeof(char));
        if (memblock == NULL)
        {
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for key string failed"));
            return PVMFErrNoMemory;
        }
        oscl_strset(memblock, 0, PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS*PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE*sizeof(char));
        // Assign the key string buffer to each KVP
        int32 j;
        for (j = 0; j < PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS; ++j)
        {
            aParameters[j].key = memblock + (j * PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE);
        }
        // Copy the requested info
        for (j = 0; j < PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS; ++j)
        {
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR("x-pvmf/video/render/"), 20);
            oscl_strncat(aParameters[j].key, PVOMXVideoDecNodeConfigRenderKeys[j].iString, oscl_strlen(PVOMXVideoDecNodeConfigRenderKeys[j].iString));
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";valtype=uint32_value"), 21);
            aParameters[j].key[PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE-1] = 0;

            // Copy the requested info
            switch (j)
            {
                case 0: // "width"
                    // Return current value
                    aParameters[j].value.uint32_value = iNewWidth;
                    break;
                case 1: // "height"
                    aParameters[j].value.uint32_value = iNewHeight;
                    break;
                default:
                    break;
            }
        }

        aNumParamElements = PVOMXVIDEODECNODECONFIG_RENDER_NUMKEYS;
    }
    else if (compcount == 3)
    {
        // Since key is "x-pvmf/video/decoder" return all
        // nodes available at this level. Ignore attribute
        // since capability is only allowed

        // Allocate memory for the KVP list
        aParameters = (PvmiKvp*)oscl_malloc(PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS * sizeof(PvmiKvp));
        if (aParameters == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for KVP failed"));
            return PVMFErrNoMemory;
        }
        oscl_memset(aParameters, 0, PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS*sizeof(PvmiKvp));
        // Allocate memory for the key strings in each KVP
        PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS * PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE * sizeof(char));
        if (memblock == NULL)
        {
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for key string failed"));
            return PVMFErrNoMemory;
        }
        oscl_strset(memblock, 0, PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS*PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE*sizeof(char));
        // Assign the key string buffer to each KVP
        int32 j;
        for (j = 0; j < PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS; ++j)
        {
            aParameters[j].key = memblock + (j * PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE);
        }
        // Copy the requested info
        for (j = 0; j < PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS; ++j)
        {
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR("x-pvmf/video/decoder/"), 21);
            oscl_strncat(aParameters[j].key, PVOMXVideoDecNodeConfigBaseKeys[j].iString, oscl_strlen(PVOMXVideoDecNodeConfigBaseKeys[j].iString));
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";type="), 6);
            switch (PVOMXVideoDecNodeConfigBaseKeys[j].iType)
            {
                case PVMI_KVPTYPE_AGGREGATE:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_AGGREGATE_STRING), oscl_strlen(PVMI_KVPTYPE_AGGREGATE_STRING));
                    break;

                case PVMI_KVPTYPE_POINTER:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_POINTER_STRING), oscl_strlen(PVMI_KVPTYPE_POINTER_STRING));
                    break;

                case PVMI_KVPTYPE_VALUE:
                default:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_VALUE_STRING), oscl_strlen(PVMI_KVPTYPE_VALUE_STRING));
                    // Now append the valtype param
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";valtype="), 9);
                    switch (PVOMXVideoDecNodeConfigBaseKeys[j].iValueType)
                    {
                        case PVMI_KVPVALTYPE_BITARRAY32:
                            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_BITARRAY32_STRING), oscl_strlen(PVMI_KVPVALTYPE_BITARRAY32_STRING));
                            break;

                        case PVMI_KVPVALTYPE_UINT32:
                            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_UINT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING));
                            break;

                        case PVMI_KVPVALTYPE_BOOL:
                        default:
                            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_BOOL_STRING), oscl_strlen(PVMI_KVPVALTYPE_BOOL_STRING));
                            break;
                    }
                    break;
            }

            aParameters[j].key[PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE-1] = 0;
        }

        aNumParamElements = PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS;
    }
    else
    {
        // Retrieve the fourth component from the key string
        pv_mime_string_extract_type(3, aIdentifier, compstr);

        for (int32 vdeccomp4ind = 0; vdeccomp4ind < PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS; ++vdeccomp4ind)
        {
            // Go through each video dec component string at 4th level
            if (pv_mime_strcmp(compstr, (char*)(PVOMXVideoDecNodeConfigBaseKeys[vdeccomp4ind].iString)) >= 0)
            {
                if (vdeccomp4ind == 3)
                {
                    // Right now video dec node doesn't support more than 5 components
                    // for the key sub-string so error out
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Unsupported key"));
                    return PVMFErrArgument;
                }
                else if (vdeccomp4ind == 4)
                {
                    // Right now video dec node doesn't support more than 5 components
                    // for the key sub-string so error out
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Unsupported key"));
                    return PVMFErrArgument;
                }
                else if ((vdeccomp4ind == 0) || // "postproc_enable",
                         (vdeccomp4ind == 1) || // "postproc_type"
                         (vdeccomp4ind == 2) || // "dropframe_enable"
                         (vdeccomp4ind == 5) || // "format_type"
                         (vdeccomp4ind == 6) || // "key_frame_only_mode"
                         (vdeccomp4ind == 7)    // "skip_n_until_key_frame"
                        )
                {
                    if (compcount == 4)
                    {
                        // Determine what is requested
                        PvmiKvpAttr reqattr = GetAttrTypeFromKeyString(aIdentifier);
                        if (reqattr == PVMI_KVPATTR_UNKNOWN)
                        {
                            reqattr = PVMI_KVPATTR_CUR;
                        }

                        // Return the requested info
                        PVMFStatus retval = DoGetVideoDecNodeParameter(aParameters, aNumParamElements, vdeccomp4ind, reqattr);
                        if (retval != PVMFSuccess)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Retrieving video dec node parameter failed"));
                            return retval;
                        }
                    }
                    else
                    {
                        // Right now videodec node doesn't support more than 4 components
                        // for this sub-key string so error out
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Unsupported key"));
                        return PVMFErrArgument;
                    }
                }

                // Breakout of the for(vdeccomp4ind) loop
                break;
            }
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Out"));
    if (aNumParamElements == 0)
    {
        // If no one could get the parameter, return error
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Unsupported key"));
        return PVMFFailure;
    }
    else
    {
        return PVMFSuccess;
    }
}


PVMFStatus PVMFOMXVideoDecNode::DoCapConfigReleaseParameters(PvmiKvp* aParameters, int aNumElements)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() In"));

    if (aParameters == NULL || aNumElements < 1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() KVP list is NULL or number of elements is 0"));
        return PVMFErrArgument;
    }


    if (pv_mime_strcmp(aParameters[0].key, _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) == 0)
    {
        // everything was allocated in blocks, so it's enough to release the beginning
        oscl_free(aParameters[0].key);
        oscl_free(aParameters[0].value.pChar_value);
        // Free memory for the parameter list
        oscl_free(aParameters);
        aParameters = NULL;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() Out"));
        return PVMFSuccess;
    }

    // Count the number of components and parameters in the key
    int compcount = pv_mime_string_compcnt(aParameters[0].key);
    // Retrieve the first component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(0, aParameters[0].key, compstr);

    if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/decoder")) < 0) || compcount < 3)
    {
        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/render")) < 0) || compcount != 4)
        {
            // First 3 component should be "x-pvmf/video/decoder" and there must
            // be at least three components
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() Unsupported key"));
            return PVMFErrArgument;
        }
    }

    if (pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/decoder")) >= 0)
    {
        // Retrieve the third component from the key string
        pv_mime_string_extract_type(2, aParameters[0].key, compstr);

        // Go through each KVP and release memory for value if allocated from heap
        for (int32 i = 0; i < aNumElements; ++i)
        {
            // Next check if it is a value type that allocated memory
            PvmiKvpType kvptype = GetTypeFromKeyString(aParameters[i].key);
            if (kvptype == PVMI_KVPTYPE_VALUE || kvptype == PVMI_KVPTYPE_UNKNOWN)
            {
                PvmiKvpValueType keyvaltype = GetValTypeFromKeyString(aParameters[i].key);
                if (keyvaltype == PVMI_KVPVALTYPE_UNKNOWN)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() Valtype not specified in key string"));
                    return PVMFErrArgument;
                }

                if (keyvaltype == PVMI_KVPVALTYPE_CHARPTR && aParameters[i].value.pChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pChar_value);
                    aParameters[i].value.pChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_KSV && aParameters[i].value.key_specific_value != NULL)
                {
                    oscl_free(aParameters[i].value.key_specific_value);
                    aParameters[i].value.key_specific_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_RANGE_UINT32 && aParameters[i].value.key_specific_value != NULL)
                {
                    range_uint32* rui32 = (range_uint32*)aParameters[i].value.key_specific_value;
                    aParameters[i].value.key_specific_value = NULL;
                    oscl_free(rui32);
                }

            }
        }
    }
    // Video dec node allocated its key strings in one chunk so just free the first key string ptr
    oscl_free(aParameters[0].key);

    // Free memory for the parameter list
    oscl_free(aParameters);
    aParameters = NULL;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigReleaseParameters() Out"));
    return PVMFSuccess;
}


void PVMFOMXVideoDecNode::DoCapConfigSetParameters(PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() In"));

    if (aParameters == NULL || aNumElements < 1)
    {
        if (aParameters)
        {
            aRetKVP = aParameters;
        }
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() Passed in parameter invalid"));
        return;
    }

    // Go through each parameter
    for (int32 paramind = 0; paramind < aNumElements; ++paramind)
    {
        // Count the number of components and parameters in the key
        int compcount = pv_mime_string_compcnt(aParameters[paramind].key);
        // Retrieve the first component from the key string
        char* compstr = NULL;
        pv_mime_string_extract_type(0, aParameters[paramind].key, compstr);

        // find out if the format specific key is used for the query
        if (pv_mime_strcmp(aParameters[paramind].key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {
            // set the mime type if audio format is being used
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() set video format specific info\n"));

            if (iTrackUnderVerificationConfig)
            {
                oscl_free(iTrackUnderVerificationConfig);
                iTrackUnderVerificationConfig = NULL;
                iTrackUnderVerificationConfigSize = 0;
            }

            iTrackUnderVerificationConfigSize = aParameters[paramind].capacity;
            if (iTrackUnderVerificationConfigSize > 0)
            {
                iTrackUnderVerificationConfig = (uint8*)(oscl_malloc(sizeof(uint8) * iTrackUnderVerificationConfigSize));
                oscl_memcpy(iTrackUnderVerificationConfig, aParameters[paramind].value.key_specific_value, iTrackUnderVerificationConfigSize);
            }

            // skip to the next parameter
            continue;
        }

        if (pv_mime_strcmp(aParameters[paramind].key,  _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() set preferred omx component list\n"));

            // each kvp paramater corresponds to one omx component - a new item in the vector is created
            iOMXPreferredComponentOrderVec.push_back((OMX_STRING)(aParameters[paramind].value.pChar_value));
            continue;

        }

        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/decoder")) < 0) || compcount < 4)
        {
            // First 3 components should be "x-pvmf/video/decoder" and there must
            // be at least four components
            aRetKVP = &aParameters[paramind];
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() Unsupported key"));
            return;
        }

        if (compcount == 4)
        {
            // Verify and set the passed-in video dec node setting
            PVMFStatus retval = DoVerifyAndSetVideoDecNodeParameter(aParameters[paramind], true);
            if (retval != PVMFSuccess)
            {
                aRetKVP = &aParameters[paramind];
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() Setting parameter %d failed", paramind));
                return;
            }
        }
        else
        {
            // Do not support more than 5 components right now
            aRetKVP = &aParameters[paramind];
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() Unsupported key"));
            return;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigSetParameters() Out"));
}

/* This function finds a nal from the SC's, moves the bitstream pointer to the beginning of the NAL unit, returns the
    size of the NAL, and at the same time, updates the remaining size in the bitstream buffer that is passed in */
int32 PVMFOMXVideoDecNode::GetNAL_OMXNode(uint8** bitstream, uint32* size)
{
    int32 i = 0;
    int32 j = 0;
    uint8* nal_unit = *bitstream;
    int32 count = 0;

    /* find SC at the beginning of the NAL */
    while (nal_unit[i++] == 0 && i < (int32)*size)
    {
    }

    if (nal_unit[i-1] == 1)
    {
        *bitstream = nal_unit + i;
    }
    else
    {
        j = *size;
        *size = 0;
        return j;  // no SC at the beginning, not supposed to happen
    }

    j = i;

    /* found the SC at the beginning of the NAL, now find the SC at the beginning of the next NAL */
    while (i < (int32)*size)
    {
        if (count == 2 && nal_unit[i] == 0x01)
        {
            i -= 2;
            break;
        }
        else if (count == 3 && nal_unit[i] == 0x01)
        {
            i -= 3;
            break;
        }

        if (nal_unit[i])
            count = 0;
        else
            count++;
        i++;
    }

    *size -= i;
    return (i - j);
}


PVMFStatus PVMFOMXVideoDecNode::DoCapConfigVerifyParameters(PvmiKvp* aParameters, int aNumElements)
{
    Oscl_Vector<OMX_STRING, OsclMemAllocator> roles;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() In"));

    if (aParameters == NULL || aNumElements < 1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Passed in parameter invalid"));
        return PVMFErrArgument;
    }

    // Go through each parameter
    for (int32 paramind = 0; paramind < aNumElements; ++paramind)
    {
        // Count the number of components and parameters in the key
        int compcount = pv_mime_string_compcnt(aParameters[paramind].key);
        // Retrieve the first component from the key string
        char* compstr = NULL;
        pv_mime_string_extract_type(0, aParameters[paramind].key, compstr);

        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/video/decoder")) < 0) || compcount < 4)
        {
            if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/media/format_specific_info")) < 0) || compcount < 3)
            {
                // First 3 components should be "x-pvmf/media/format_specific_info" and there must
                // be at least three components
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Unsupported key"));
                return PVMFErrArgument;
            }
            else
            {
                pvVideoConfigParserInputs aInputs;
                OMXConfigParserInputs aInputParameters;
                VideoOMXConfigParserOutputs aOutputParameters;

                aInputs.inPtr = (uint8*)(aParameters->value.key_specific_value);
                aInputs.inBytes = (int32)aParameters->capacity;
                aInputs.iMimeType = iNodeConfig.iMimeType;
                aInputParameters.inBytes = aInputs.inBytes;
                aInputParameters.inPtr = aInputs.inPtr;

                if (aInputs.inBytes == 0 || aInputs.inPtr == NULL)
                {
                    // in case of following formats - config codec data is expected to
                    // be present in the query. If not, config parser cannot be called

                    if (aInputs.iMimeType == PVMF_MIME_H264_VIDEO ||
                            aInputs.iMimeType == PVMF_MIME_H264_VIDEO_MP4 ||
                            aInputs.iMimeType == PVMF_MIME_M4V ||
                            aInputs.iMimeType == PVMF_MIME_WMV ||
                            aInputs.iMimeType == PVMF_MIME_REAL_VIDEO)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Codec Config data is not present"));
                        return PVMFErrNotSupported;
                    }
                }

                if (aInputs.iMimeType ==  PVMF_MIME_H264_VIDEO ||
                        aInputs.iMimeType == PVMF_MIME_H264_VIDEO_MP4 ||
                        aInputs.iMimeType == PVMF_MIME_H264_VIDEO_RAW)
                {
                    roles.push_back((OMX_STRING)"video_decoder.avc");
                }
                else if (aInputs.iMimeType ==  PVMF_MIME_M4V)
                {
                    roles.push_back((OMX_STRING)"video_decoder.mpeg4");
                }
                else if (aInputs.iMimeType ==  PVMF_MIME_H2631998 ||
                         aInputs.iMimeType == PVMF_MIME_H2632000)
                {
                    roles.push_back((OMX_STRING)"video_decoder.h263");
                }
                else if (aInputs.iMimeType ==  PVMF_MIME_WMV)
                {
                    roles.push_back((OMX_STRING)"video_decoder.vc1");
                    roles.push_back((OMX_STRING)"video_decoder.wmv");
                }
                else if (aInputs.iMimeType ==  PVMF_MIME_REAL_VIDEO)
                {
                    roles.push_back((OMX_STRING)"video_decoder.rv");
                }
                else
                {
                    // Illegal codec specified.
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Input port format other then codec type", iName.Str()));

                }


                OMX_BOOL status = OMX_FALSE;
                OMX_U32 num_comps = 0;
                OMX_STRING *CompOfRole;
                OMX_U32 ii;
                Oscl_Vector<OMX_STRING, OsclMemAllocator>::iterator role;
                OMX_ERRORTYPE Err = OMX_ErrorNone;

                for (role = roles.begin(); role != roles.end(); role++)
                {
                    // call once to find out the number of components that can fit the role
                    Err = OMX_MasterGetComponentsOfRole(*role, &num_comps, NULL);

                    if ((num_comps > 0) && (OMX_ErrorNone == Err))
                    {
                        CompOfRole = (OMX_STRING *)oscl_malloc(num_comps * sizeof(OMX_STRING));
                        for (ii = 0; ii < num_comps; ii++)
                            CompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));

                        // call 2nd time to get the component names
                        Err = OMX_MasterGetComponentsOfRole(*role, &num_comps, (OMX_U8 **)CompOfRole);

                        if (OMX_ErrorNone == Err)
                        {
                            for (ii = 0; ii < num_comps; ii++)
                            {
                                aInputParameters.cComponentRole = *role;
                                aInputParameters.cComponentName = CompOfRole[ii];
                                status = OMX_MasterConfigParser(&aInputParameters, &aOutputParameters);
                                if (status == OMX_TRUE)
                                {
                                    break;
                                }
                                else
                                {
                                    status = OMX_FALSE;
                                }
                            }
                        }

                        // whether successful or not, need to free CompOfRoles
                        for (ii = 0; ii < num_comps; ii++)
                        {
                            oscl_free(CompOfRole[ii]);
                            CompOfRole[ii] = NULL;
                        }
                        oscl_free(CompOfRole);
                    }
                    if (status == OMX_TRUE) break; //component for role found.
                }

                if (status == OMX_FALSE)
                {
                    return PVMFErrNotSupported;
                }

                iNewWidth = aOutputParameters.width;
                iNewHeight = aOutputParameters.height;

                return PVMFSuccess;
            }
        }
        else
        {
            if (compcount == 4)
            {
                // Verify and set the passed-in video dec node setting
                PVMFStatus retval = DoVerifyAndSetVideoDecNodeParameter(aParameters[paramind], false);
                if (retval != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Setting parameter %d failed", paramind));
                    return retval;
                }
            }
            else
            {
                // Do not support more than 5 components right now
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Unsupported key"));
                return PVMFErrArgument;
            }
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigVerifyParameters() Out"));
    return PVMFSuccess;
}



PVMFStatus PVMFOMXVideoDecNode::DoGetVideoDecNodeParameter(PvmiKvp*& aParameters, int& aNumParamElements, int32 aIndex, PvmiKvpAttr reqattr)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoGetVideoDecNodeParameter() In"));

    aNumParamElements = 0;

    // Allocate memory for the KVP
    aParameters = (PvmiKvp*)oscl_malloc(sizeof(PvmiKvp));
    if (aParameters == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoGetVideoDecNodeParameter() Memory allocation for KVP failed"));
        return PVMFErrNoMemory;
    }
    oscl_memset(aParameters, 0, sizeof(PvmiKvp));
    // Allocate memory for the key string in KVP
    PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE * sizeof(char));
    if (memblock == NULL)
    {
        oscl_free(aParameters);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoGetVideoDecNodeParameter() Memory allocation for key string failed"));
        return PVMFErrNoMemory;
    }
    oscl_strset(memblock, 0, PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE*sizeof(char));
    // Assign the key string buffer to KVP
    aParameters[0].key = memblock;

    // Copy the key string
    oscl_strncat(aParameters[0].key, _STRLIT_CHAR("x-pvmf/video/decoder/"), 21);
    oscl_strncat(aParameters[0].key, PVOMXVideoDecNodeConfigBaseKeys[aIndex].iString, oscl_strlen(PVOMXVideoDecNodeConfigBaseKeys[aIndex].iString));
    oscl_strncat(aParameters[0].key, _STRLIT_CHAR(";type=value;valtype="), 20);
    switch (PVOMXVideoDecNodeConfigBaseKeys[aIndex].iValueType)
    {
        case PVMI_KVPVALTYPE_BITARRAY32:
            oscl_strncat(aParameters[0].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_BITARRAY32_STRING), oscl_strlen(PVMI_KVPVALTYPE_BITARRAY32_STRING));
            break;

        case PVMI_KVPVALTYPE_KSV:
            oscl_strncat(aParameters[0].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_KSV_STRING), oscl_strlen(PVMI_KVPVALTYPE_KSV_STRING));
            break;

        case PVMI_KVPVALTYPE_BOOL:
            oscl_strncat(aParameters[0].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_BOOL_STRING), oscl_strlen(PVMI_KVPVALTYPE_BOOL_STRING));
            break;

        case PVMI_KVPVALTYPE_UINT32:
        default:
            if (reqattr == PVMI_KVPATTR_CAP)
            {
                oscl_strncat(aParameters[0].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_RANGE_UINT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_RANGE_UINT32_STRING));
            }
            else
            {
                oscl_strncat(aParameters[0].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_UINT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING));
            }
            break;
    }
    aParameters[0].key[PVOMXVIDEODECNODECONFIG_KEYSTRING_SIZE-1] = 0;

    // Copy the requested info
    switch (aIndex)
    {
        case 0: // "postproc_enable"
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                aParameters[0].value.bool_value = iNodeConfig.iPostProcessingEnable;
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.bool_value = PVOMXVIDEODECNODE_CONFIG_POSTPROCENABLE_DEF;
            }

            break;

        case 1: // "postproc_type"
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                aParameters[0].value.uint32_value = iNodeConfig.iPostProcessingMode;
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.uint32_value = PVOMXVIDEODECNODE_CONFIG_POSTPROCTYPE_DEF;
            }

            break;

        case 2: // "dropframe_enable"
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                aParameters[0].value.bool_value = iNodeConfig.iDropFrame;
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.bool_value = PVOMXVIDEODECNODE_CONFIG_DROPFRAMEENABLE_DEF;
            }

            break;

        case 5: //"format-type"
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                aParameters[0].value.pChar_value = (char*)iNodeConfig.iMimeType.getMIMEStrPtr();
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.pChar_value = (char*)PVMF_MIME_FORMAT_UNKNOWN;
            }

            break;

        case 6: // "key_frame_only_mode"
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                aParameters[0].value.bool_value = iNodeConfig.iKeyFrameOnlyMode;
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.bool_value = PVOMXVIDEODECNODE_CONFIG_KEYFRAMEONLYMODE_DEF;
            }

            break;

        case 7: // "skip_n_until_key_frame_mode"
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                aParameters[0].value.uint32_value = iNodeConfig.iSkipNUntilKeyFrame;
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.bool_value = PVOMXVIDEODECNODE_CONFIG_SKIPNUNTILKEYFRAME_DEF;
            }

            break;

        default:
            // Invalid index
            oscl_free(aParameters[0].key);
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoGetVideoDecNodeParameter() Invalid index to video dec node parameter"));
            return PVMFErrArgument;
    }

    aNumParamElements = 1;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoGetVideoDecNodeParameter() Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter(PvmiKvp& aParameter, bool aSetParam)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() In"));

    // Determine the valtype
    PvmiKvpValueType keyvaltype = GetValTypeFromKeyString(aParameter.key);
    if (keyvaltype == PVMI_KVPVALTYPE_UNKNOWN)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Valtype in key string unknown"));
        return PVMFErrArgument;
    }
    // Retrieve the fourth component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(3, aParameter.key, compstr);

    int32 vdeccomp4ind = 0;
    for (vdeccomp4ind = 0; vdeccomp4ind < PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS; ++vdeccomp4ind)
    {
        // Go through each component string at 4th level
        if (pv_mime_strcmp(compstr, (char*)(PVOMXVideoDecNodeConfigBaseKeys[vdeccomp4ind].iString)) >= 0)
        {
            // Break out of the for loop
            break;
        }
    }

    if (vdeccomp4ind == PVOMXVIDEODECNODECONFIG_BASE_NUMKEYS || vdeccomp4ind == 3 || vdeccomp4ind == 4)
    {
        // Match couldn't be found or non-leaf node specified
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Unsupported key or non-leaf node"));
        return PVMFErrArgument;
    }

    // Verify the valtype
    if (keyvaltype != PVOMXVideoDecNodeConfigBaseKeys[vdeccomp4ind].iValueType)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Valtype does not match for key"));
        return PVMFErrArgument;
    }

    switch (vdeccomp4ind)
    {
        case 0: // "postproc_enable"
            // Nothing to validate since it is boolean
            // Change the config if to set
            if (aSetParam)
            {
                iNodeConfig.iPostProcessingEnable = aParameter.value.bool_value;
            }
            break;

        case 1: // "postproc_type"
            // Nothing to validate since it is bitarray32
            // Change the config if to set
            if (aSetParam)
            {
                iNodeConfig.iPostProcessingMode = aParameter.value.uint32_value;
                // Don't do anything yet: DeBlocking Type of Post-processing will be communicated
                // to decoder later at the time of NegotiateComponentParameters()
                // Note: Setting of this Kvp through OMX can only happen before the beginning of decoding.
                // If the kvp is set after decoding has already begun, then it won't be passed to the decoder.
            }
            break;

        case 2: // "dropframe_enable"
            // Nothing to validate since it is boolean
            // Change the config if to set
            if (aSetParam)
            {
                if (iInterfaceState == EPVMFNodeStarted || iInterfaceState == EPVMFNodePaused)
                {
                    // This setting cannot be changed when decoder has been initialized
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Setting cannot be changed while started or paused"));
                    return PVMFErrInvalidState;
                }

                iNodeConfig.iDropFrame = aParameter.value.bool_value;
            }
            break;

        case 5: // "format-type"
            // Nothing to validate since it is boolean
            // Change the config if to set
            if (aSetParam)
            {
                if (iInterfaceState == EPVMFNodeStarted || iInterfaceState == EPVMFNodePaused)
                {
                    // This setting cannot be changed when decoder has been initialized
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Setting cannot be changed while started or paused"));
                    return PVMFErrInvalidState;
                }

                iNodeConfig.iMimeType = aParameter.value.pChar_value;
            }
            break;

        case 6: // "key_frame_only_mode"
            // Change the config if to set
            if (aSetParam)
            {
                SetKeyFrameOnlyModeFlag(aParameter.value.bool_value);
            }
            break;

        case 7: // "skip_n_until_key_frame"
            // Change the config if to set
            if (aSetParam)
            {
                SetSkipNUntilKeyFrame(aParameter.value.uint32_value);
            }
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Invalid index for video dec node parameter"));
            return PVMFErrArgument;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoVerifyAndSetVideoDecNodeParameter() Out"));
    return PVMFSuccess;
}


PVMFStatus PVMFOMXVideoDecNode::GetProfileAndLevel(PVMF_MPEGVideoProfileType& aProfile, PVMF_MPEGVideoLevelType& aLevel)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::GetProfileAndLevel() In"));

    if (NULL == iOMXDecoder)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::GetProfileAndLevel() iVideoDecoder is Null"));
        aProfile = PV_MPEG_VIDEO_RESERVED_PROFILE;
        aLevel  = PV_MPEG_VIDEO_LEVEL_UNKNOWN;
        return PVMFFailure;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::GetProfileAndLevel() iVideoDecoder is Null"));
    aProfile = PV_MPEG_VIDEO_RESERVED_PROFILE;
    aLevel  = PV_MPEG_VIDEO_LEVEL_UNKNOWN;
    // FOR NOW, JUST RETURN FAILURE, WE DON'T SUPPORT THIS FEATURE YET
    return PVMFFailure;


}


bool PVMFOMXVideoDecNode::VerifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    // unused parameters
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(num_elements);

    // call this in case of WMV format
    if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMV)
    {
        //verify bitrate
        if (pv_mime_strcmp(aParameters->key, PVMF_BITRATE_VALUE_KEY) == 0)
        {
            if (((PVMFOMXDecPort*)iOutPort)->verifyConnectedPortParametersSync(PVMF_BITRATE_VALUE_KEY, &(aParameters->value.uint32_value)) != PVMFSuccess)
            {
                return false;
            }
            return true;
        }
        else if (pv_mime_strcmp(aParameters->key, PVMF_FRAMERATE_VALUE_KEY) == 0)
        {
            if (((PVMFOMXDecPort*)iOutPort)->verifyConnectedPortParametersSync(PVMF_FRAMERATE_VALUE_KEY, &(aParameters->value.uint32_value)) != PVMFSuccess)
            {
                return false;
            }
            return true;
        }
        else if (pv_mime_strcmp(aParameters->key, PVMF_FORMAT_SPECIFIC_INFO_KEY) < 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::VerifyParametersSync() - Unsupported Key"));
            return true;
        }

        pvVideoConfigParserInputs aInput;
        pvVideoConfigParserOutputs aOutput;

        aInput.inPtr = (uint8*)(aParameters->value.key_specific_value);
        aInput.inBytes = (int32)aParameters->capacity;
        aInput.iMimeType = PVMF_MIME_WMV;
        if (aInput.inBytes == 0 || aInput.inPtr == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::VerifyParametersSync() - null config info"));
            return false;
        }

        if (pv_video_config_parser(&aInput, &aOutput)) // return 0 for success
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::VerifyParametersSync() - invalid config info"));
            return false;
        }


        iNewWidth = aOutput.width;
        iNewHeight = aOutput.height;

        if (((iNewWidth != (uint32)iYUVWidth) || (iNewHeight != (uint32)iYUVHeight)) && iOutPort != NULL)
        {
            // see if downstream node can handle the re-sizing
            int32 errcode = OsclErrNone;
            OsclRefCounterMemFrag yuvFsiMemfrag;
            OSCL_TRY(errcode, yuvFsiMemfrag = iFsiFragmentAlloc.get());


            OSCL_FIRST_CATCH_ANY(errcode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::VerifyParametersSync() Failed to allocate memory for verifyParametersSync FSI")));
            if (OsclErrNone == errcode)
            {
                PVMFYuvFormatSpecificInfo0* fsiInfo = OSCL_PLACEMENT_NEW(yuvFsiMemfrag.getMemFragPtr(), PVMFYuvFormatSpecificInfo0());
                if (fsiInfo != NULL)
                {
                    fsiInfo->video_format = PVMF_MIME_YUV420;
                    fsiInfo->uid = PVMFYuvFormatSpecificInfo0_UID;
                    fsiInfo->viewable_width = iNewWidth;
                    fsiInfo->viewable_height = iNewHeight;
                    fsiInfo->buffer_width = (iNewWidth + 3) & -4;
                    fsiInfo->buffer_height = iNewHeight;

                    if (((PVMFOMXDecPort*)iOutPort)->verifyConnectedPortParametersSync(PVMF_FORMAT_SPECIFIC_INFO_KEY, &yuvFsiMemfrag) != PVMFSuccess)
                    {
                        fsiInfo->video_format.~PVMFFormatType();
                        return false;
                    }
                    fsiInfo->video_format.~PVMFFormatType();
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    } // end of if(format == PVMF_MIME_WMV)

    return true;
}

bool PVMFOMXVideoDecNode::ParseAndReWrapH264RAW(PVMFSharedMediaDataPtr& aMediaDataPtr)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::ParseAndReWrapH264RAW() In"));

    // in RAW, each media fragment is an AVC frame, and the NALs are separated by start codes
    // we need to parse the NALs into separate media fragments before continuing, since most of the logic in this
    // function regarding the formatting of input buffers works on a media fragment basis

    uint32 aligned_cleanup_size = oscl_mem_aligned_size(sizeof(PVOMXDecInputBufferSharedPtrWrapperCombinedCleanupDA));
    uint32 aligned_refcnt_size = oscl_mem_aligned_size(sizeof(OsclRefCounterDA));
    uint8 *wrap_ptr = (uint8*) oscl_malloc(aligned_cleanup_size + aligned_refcnt_size);

    if (wrap_ptr == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::ParseAndReWrapH264RAW() Error allocating NAL media wrapper", iName.Str()));

        aMediaDataPtr.Unbind();

        return false;
    }

    OsclSharedPtr<PVMFMediaDataImpl> media_data_impl;
    aMediaDataPtr->getMediaDataImpl(media_data_impl);

    // create a deallocator and pass the buffer_allocator to it as well as pointer to data that needs to be returned to the mempool
    // keep track of previous media_data_impl ref counter, since that tracks the data buffer
    PVOMXDecInputBufferSharedPtrWrapperCombinedCleanupDA *cleanup_ptr =
        OSCL_PLACEMENT_NEW(wrap_ptr + aligned_refcnt_size, PVOMXDecInputBufferSharedPtrWrapperCombinedCleanupDA((OsclRefCounterDA *)media_data_impl.GetRefCounter()));

    // create the ref counter after the cleanup object (refcount is set to 1 at creation)
    OsclRefCounterDA *refcnt;
    PVMFMediaDataImpl* media_data_ptr;

    media_data_ptr = OSCL_NEW(PVMFMediaFragGroup<OsclMemAllocator>, (MAX_NAL_PER_FRAME / 10)); // start capacity at 10, it can grow from there if necessary
    refcnt = OSCL_PLACEMENT_NEW(wrap_ptr, OsclRefCounterDA(media_data_ptr, cleanup_ptr));

    OsclRefCounterMemFrag full_frame_mem_frag;
    aMediaDataPtr->getMediaFragment(0, full_frame_mem_frag);
    uint8* frame_buff = (uint8*)full_frame_mem_frag.getMemFragPtr();
    uint32 frame_size = full_frame_mem_frag.getMemFragSize();

    // loop through and assign a create a media fragment for each NAL
    while (frame_size > 0)
    {
        uint32 nal_len;
        uint8* temp_ptr = frame_buff;

        nal_len = GetNAL_OMXNode(&frame_buff, &frame_size);

        OsclMemoryFragment memFrag;

        if (iOMXComponentUsesNALStartCodes)
        {
            uint32 sc_size = (uint32) frame_buff - (uint32) temp_ptr;

            if (sc_size != 3 && sc_size != 4)
            {
                // start code size should be 3 or 4 bytes
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::ParseAndReWrapH264RAW() Corrupt NAL data", iName.Str()));

                aMediaDataPtr.Unbind();

                return false;
            }
            memFrag.ptr = temp_ptr;
            memFrag.len = nal_len + sc_size;
        }
        else
        {
            memFrag.ptr = frame_buff;
            memFrag.len = nal_len;
        }

        frame_buff += nal_len;

        OsclRefCounterMemFrag refCountMemFragOut(memFrag, refcnt, memFrag.len);
        media_data_ptr->appendMediaFragment(refCountMemFragOut);
    }

    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut(media_data_ptr, refcnt);

    media_data_ptr->setMarkerInfo(aMediaDataPtr->getMarkerInfo());

    // by binding this, we are unbinding the previous data as well
    aMediaDataPtr.Bind(PVMFMediaData::createMediaData(mediaDataImplOut, aMediaDataPtr->getMessageHeader(), iInputMediaDataMemPool));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::ParseAndReWrapH264RAW() Out"));

    return true;
}


PVMFStatus PVMFOMXVideoDecNode::SkipNonKeyFrames()
{
    PVMFStatus status = PVMFSuccess;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXVideoDecNode::SkipNonKeyFrames()"));

    // iKeyFrameOnlyModeFlag and iSkipNUntilKeyFrameFlag shouldn't be set if it's not these formats
    OSCL_ASSERT(((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_H264_VIDEO ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4 ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW ||
                ((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_M4V ||
                ((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_H2631998 ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H2632000 ||
                ((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_WMV);

    if ((iDataIn->getSeqNum() - iInPacketSeqNum) > 1)
    {

        // if there was packet loss, we don't know if we're on a frame (or NAL) boundary
        // and the frame detection would be unreliable.  skip frames until a reliable check point is available
        iCheckForKeyFrame = false;
        iSkippingNonKeyFrames = true;
    }

    if (iCheckForKeyFrame)
    {
        pvVideoGetFrameTypeParserOutputs output;

        // if there are multipe fragments, get the 1st one
        // get a memfrag from the message

        OsclRefCounterMemFrag frag;
        iDataIn->getMediaFragment(0, frag);
        iPVVideoFrameParserInput.inBuf = (uint8 *)frag.getMemFragPtr();
        iPVVideoFrameParserInput.inBufSize = frag.getMemFragSize();
        iPVVideoFrameParserInput.iMimeType = ((PVMFOMXDecPort*)iInPort)->iFormat;

        if (iPVVideoFrameParserInput.iMimeType ==  PVMF_MIME_WMV)
        {
            iPVVideoFrameParserInput.SeqHeaderInfo = &iWmvSeqHeaderInfo;
            status = pv_detect_keyframe(&iPVVideoFrameParserInput, &output);
        }
        else
        {
            status = pv_detect_keyframe(&iPVVideoFrameParserInput, &output);
        }

        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::SkipNonKeyFrames() - Error in detecting frame type"));

            // remember the sequence number & timestamp so that we have reference
            iInPacketSeqNum = iDataIn->getSeqNum();
            iInTimestamp = iDataIn->getTimestamp();

            // drop this message
            iDataIn.Unbind();

            return status;
        }

        if ((!output.isKeyFrame) && iNodeConfig.iKeyFrameOnlyMode)
        {
            iSkippingNonKeyFrames = true;
        }
        else if (iNodeConfig.iSkipNUntilKeyFrame)
        {
            if ((!output.isKeyFrame) && (iSkipFrameCount < iNodeConfig.iSkipNUntilKeyFrame))
            {
                iSkipFrameCount++;
                iSkippingNonKeyFrames = true;
            }
            else
            {
                iSkipFrameCount = iNodeConfig.iSkipNUntilKeyFrame; // we found a keyframe, so stop checking
                iSkippingNonKeyFrames = false;
            }
        }
        else
        {
            iSkippingNonKeyFrames = false;
        }
    }

    // determine if we should check for key frame on the next message
    if (iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT)
    {
        // check for keyframes on frame boundaries
        iCheckForKeyFrame = true;
    }
    else if ((iDataIn->getSeqNum() - iInPacketSeqNum) > 1)
    {
        // if there is packet loss, we should be skipping frames right now and are waiting for the next boundary
        // to be able to check for key frames.  In H264, NAL boundaries are appropriate places to do so as well.
        if (((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_H264_VIDEO ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4 ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW)
        {
            if (iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT)
            {
                iCheckForKeyFrame = true;
            }
            else
            {
                iCheckForKeyFrame = false;
            }
        }

    }
    else
    {
        iCheckForKeyFrame = false;
    }

    if (iSkippingNonKeyFrames)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::SkipNonKeyFrames() - skipped non key frame"));

        // remember the sequence number & timestamp so that we have reference
        iInPacketSeqNum = iDataIn->getSeqNum();
        iInTimestamp = iDataIn->getTimestamp();

        // drop this message
        iDataIn.Unbind();
    }

    return status;
}
