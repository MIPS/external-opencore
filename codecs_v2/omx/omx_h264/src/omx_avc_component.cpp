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
#include "oscl_base.h"
#include "OMX_Types.h"
#include "pv_omxdefs.h"
#include "omx_avc_component.h"

#if PROXY_INTERFACE
#include "omx_proxy_interface.h"
#endif


// Use default DLL entry point
#ifndef OSCL_DLL_H_INCLUDED
#include "oscl_dll.h"
#endif

OSCL_DLL_ENTRY_POINT_DEFAULT()

// This function is called by OMX_GetHandle and it creates an instance of the avc component AO
OSCL_EXPORT_REF OMX_ERRORTYPE AvcOmxComponentFactory(OMX_OUT OMX_HANDLETYPE* pHandle, OMX_IN  OMX_PTR pAppData, OMX_IN OMX_PTR pProxy , OMX_STRING aOmxLibName, OMX_PTR &aOmxLib, OMX_PTR aOsclUuid, OMX_U32 &aRefCount)
{
    OSCL_UNUSED_ARG(aOmxLibName);
    OSCL_UNUSED_ARG(aOmxLib);
    OSCL_UNUSED_ARG(aOsclUuid);
    OSCL_UNUSED_ARG(aRefCount);

    OpenmaxAvcAO* pOpenmaxAOType;
    OMX_ERRORTYPE Status;

    // move InitAvcOmxComponentFields content to actual constructor

    pOpenmaxAOType = (OpenmaxAvcAO*) OSCL_NEW(OpenmaxAvcAO, ());

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorInsufficientResources;
    }

    //Call the construct component to initialize OMX types
    Status = pOpenmaxAOType->ConstructComponent(pAppData, pProxy);

    *pHandle = pOpenmaxAOType->GetOmxHandle();

    return Status;
    ///////////////////////////////////////////////////////////////////////////////////////
}

// This function is called by OMX_FreeHandle when component AO needs to be destroyed
OSCL_EXPORT_REF OMX_ERRORTYPE AvcOmxComponentDestructor(OMX_IN OMX_HANDLETYPE pHandle, OMX_PTR &aOmxLib, OMX_PTR aOsclUuid, OMX_U32 &aRefCount)
{
    OSCL_UNUSED_ARG(aOmxLib);
    OSCL_UNUSED_ARG(aOsclUuid);
    OSCL_UNUSED_ARG(aRefCount);

    // get pointer to component AO
    OpenmaxAvcAO* pOpenmaxAOType = (OpenmaxAvcAO*)((OMX_COMPONENTTYPE*)pHandle)->pComponentPrivate;

    // clean up decoder, OMX component stuff
    pOpenmaxAOType->DestroyComponent();

    // destroy the AO class
    OSCL_DELETE(pOpenmaxAOType);

    return OMX_ErrorNone;
}

#if DYNAMIC_LOAD_OMX_AVC_COMPONENT
class OsclSharedLibraryInterface;
class AvcOmxSharedLibraryInterface: public OsclSharedLibraryInterface,
        public OmxSharedLibraryInterface

{
    public:
        OsclAny *QueryOmxComponentInterface(const OsclUuid& aOmxTypeId, const OsclUuid& aInterfaceId)
        {
            if (PV_OMX_AVCDEC_UUID == aOmxTypeId)
            {
                if (PV_OMX_CREATE_INTERFACE == aInterfaceId)
                {
                    return ((OsclAny*)(&AvcOmxComponentFactory));
                }
                else if (PV_OMX_DESTROY_INTERFACE == aInterfaceId)
                {
                    return ((OsclAny*)(&AvcOmxComponentDestructor));
                }
            }
            return NULL;
        };
        OsclAny *SharedLibraryLookup(const OsclUuid& aInterfaceId)
        {
            if (aInterfaceId == PV_OMX_SHARED_INTERFACE)
            {
                return OSCL_STATIC_CAST(OmxSharedLibraryInterface*, this);
            }
            return NULL;
        };

        AvcOmxSharedLibraryInterface() {};
};

// function to obtain the interface object from the shared library
extern "C"
{
    OSCL_EXPORT_REF OsclAny* PVGetInterface()
    {
        return (OsclAny*) OSCL_NEW(AvcOmxSharedLibraryInterface, ());
    }

    OSCL_EXPORT_REF void PVReleaseInterface(OsclSharedLibraryInterface* aInstance)
    {
        AvcOmxSharedLibraryInterface* module = (AvcOmxSharedLibraryInterface*)aInstance;
        OSCL_DELETE(module);
    }
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE OpenmaxAvcAO::ConstructComponent(OMX_PTR pAppData, OMX_PTR pProxy)
{
    ComponentPortType* pInPort, *pOutPort;
    OMX_ERRORTYPE Status;

    iNumPorts = 2;
    iOmxComponent.nSize = sizeof(OMX_COMPONENTTYPE);
    iOmxComponent.pComponentPrivate = (OMX_PTR) this;  // pComponentPrivate points to THIS component AO class
    ipComponentProxy = pProxy;
    iOmxComponent.pApplicationPrivate = pAppData; // init the App data

    ipOutputBufferForRendering = NULL;
    iNumNALs = 0;
    iCurrNAL = 0;
    iNALOffset = 0;
    oscl_memset(iNALSizeArray, 0, MAX_NAL_PER_FRAME * sizeof(uint32));
    oscl_memset(iNALStartCodeSizeArray, 0, MAX_NAL_PER_FRAME * sizeof(uint32));


#if PROXY_INTERFACE
    iPVCapabilityFlags.iIsOMXComponentMultiThreaded = OMX_TRUE;

    iOmxComponent.SendCommand = OpenmaxAvcAO::BaseComponentProxySendCommand;
    iOmxComponent.GetParameter = OpenmaxAvcAO::BaseComponentProxyGetParameter;
    iOmxComponent.SetParameter = OpenmaxAvcAO::BaseComponentProxySetParameter;
    iOmxComponent.GetConfig = OpenmaxAvcAO::BaseComponentProxyGetConfig;
    iOmxComponent.SetConfig = OpenmaxAvcAO::BaseComponentProxySetConfig;
    iOmxComponent.GetExtensionIndex = OpenmaxAvcAO::BaseComponentProxyGetExtensionIndex;
    iOmxComponent.GetState = OpenmaxAvcAO::BaseComponentProxyGetState;
    iOmxComponent.UseBuffer = OpenmaxAvcAO::BaseComponentProxyUseBuffer;
    iOmxComponent.AllocateBuffer = OpenmaxAvcAO::BaseComponentProxyAllocateBuffer;
    iOmxComponent.FreeBuffer = OpenmaxAvcAO::BaseComponentProxyFreeBuffer;
    iOmxComponent.EmptyThisBuffer = OpenmaxAvcAO::BaseComponentProxyEmptyThisBuffer;
    iOmxComponent.FillThisBuffer = OpenmaxAvcAO::BaseComponentProxyFillThisBuffer;

#else
    iPVCapabilityFlags.iIsOMXComponentMultiThreaded = OMX_FALSE;

    iOmxComponent.SendCommand = OpenmaxAvcAO::BaseComponentSendCommand;
    iOmxComponent.GetParameter = OpenmaxAvcAO::BaseComponentGetParameter;
    iOmxComponent.SetParameter = OpenmaxAvcAO::BaseComponentSetParameter;
    iOmxComponent.GetConfig = OpenmaxAvcAO::BaseComponentGetConfig;
    iOmxComponent.SetConfig = OpenmaxAvcAO::BaseComponentSetConfig;
    iOmxComponent.GetExtensionIndex = OpenmaxAvcAO::BaseComponentGetExtensionIndex;
    iOmxComponent.GetState = OpenmaxAvcAO::BaseComponentGetState;
    iOmxComponent.UseBuffer = OpenmaxAvcAO::BaseComponentUseBuffer;
    iOmxComponent.AllocateBuffer = OpenmaxAvcAO::BaseComponentAllocateBuffer;
    iOmxComponent.FreeBuffer = OpenmaxAvcAO::BaseComponentFreeBuffer;
    iOmxComponent.EmptyThisBuffer = OpenmaxAvcAO::BaseComponentEmptyThisBuffer;
    iOmxComponent.FillThisBuffer = OpenmaxAvcAO::BaseComponentFillThisBuffer;
#endif

    iOmxComponent.SetCallbacks = OpenmaxAvcAO::BaseComponentSetCallbacks;
    iOmxComponent.nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
    iOmxComponent.nVersion.s.nVersionMinor = SPECVERSIONMINOR;
    iOmxComponent.nVersion.s.nRevision = SPECREVISION;
    iOmxComponent.nVersion.s.nStep = SPECSTEP;

    // PV capability
#if defined(TEST_FULL_AVC_FRAME_MODE)
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_FALSE; // since we need copying for frame assembly in streaming case
    iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesNALStartCodes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes = OMX_FALSE;
#elif defined(TEST_FULL_AVC_FRAME_MODE_SC)
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_FALSE; // since we need copying for frame assembly in streaming case
    iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesNALStartCodes = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes = OMX_FALSE;
#elif defined(TEST_FULL_AVC_FRAME_MODE_I2BNS)
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_FALSE; // since we need copying for frame assembly in streaming case
    iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesNALStartCodes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes = OMX_FALSE;
#elif defined(TEST_FULL_AVC_FRAME_MODE_I4BNS)
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_FALSE; // since we need copying for frame assembly in streaming case
    iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesNALStartCodes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes = OMX_TRUE;
#elif defined(TEST_AVC_NAL_MODE_SC)
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesNALStartCodes = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes = OMX_FALSE;
#else
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesNALStartCodes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes = OMX_FALSE;
    iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes = OMX_FALSE;
#endif

    if (iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes)
    {
        iSizeOfNALSize = sizeof(OMX_U16);
    }
    else if (iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes)
    {
        iSizeOfNALSize = sizeof(OMX_U32);
    }
    else
    {
        iSizeOfNALSize = 0;
    }


    if (ipAppPriv)
    {
        oscl_free(ipAppPriv);
        ipAppPriv = NULL;
    }

    ipAppPriv = (ComponentPrivateType*) oscl_malloc(sizeof(ComponentPrivateType));
    if (NULL == ipAppPriv)
    {
        return OMX_ErrorInsufficientResources;
    }

    //Construct base class now
    Status = ConstructBaseComponent(pAppData);

    if (OMX_ErrorNone != Status)
    {
        return Status;
    }

    /** Domain specific section for the ports. */
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nPortIndex = OMX_PORT_INPUTPORT_INDEX;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.eDomain = OMX_PortDomainVideo;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.cMIMEType = (OMX_STRING)"video/Avc";
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.pNativeRender = 0;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.bFlagErrorConcealment = OMX_FALSE;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.nFrameWidth = 176;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.nFrameHeight = 144;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.nBitrate = 64000;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.video.xFramerate = (15 << 16);
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.eDir = OMX_DirInput;
    //Set to a default value, will change later during setparameter call
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferCountActual = NUMBER_INPUT_BUFFER_AVC;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferCountMin = 1;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize = INPUT_BUFFER_SIZE_AVC;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.bEnabled = OMX_TRUE;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.bPopulated = OMX_FALSE;

    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nPortIndex = OMX_PORT_OUTPUTPORT_INDEX;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.eDomain = OMX_PortDomainVideo;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.cMIMEType = (OMX_STRING)"raw";
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.pNativeRender = 0;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.bFlagErrorConcealment = OMX_FALSE;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nFrameWidth = 176;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nStride = 176;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nFrameHeight = 144;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nSliceHeight = 144;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nBitrate = 64000;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.xFramerate = (15 << 16);
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.eDir = OMX_DirOutput;
    //Set to a default value, will change later during setparameter call
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nBufferCountActual = NUMBER_OUTPUT_BUFFER_AVC;

    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nBufferCountMin = NUMBER_OUTPUT_BUFFER_AVC;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nBufferSize = OUTPUT_BUFFER_SIZE_AVC; //just use QCIF (as default)
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.bEnabled = OMX_TRUE;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.bPopulated = OMX_FALSE;

    //Default values for Avc video param port
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->VideoAvc.nPortIndex = OMX_PORT_INPUTPORT_INDEX;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->VideoAvc.eProfile = OMX_VIDEO_AVCProfileBaseline;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->VideoAvc.eLevel = OMX_VIDEO_AVCLevel1;

    ipPorts[OMX_PORT_INPUTPORT_INDEX]->ProfileLevel.nPortIndex = OMX_PORT_INPUTPORT_INDEX;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->ProfileLevel.nProfileIndex = 0;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->ProfileLevel.eProfile = OMX_VIDEO_AVCProfileBaseline;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->ProfileLevel.eLevel = OMX_VIDEO_AVCLevel1;

    iPortTypesParam.nPorts = 2;
    iPortTypesParam.nStartPortNumber = 0;

    pInPort = (ComponentPortType*) ipPorts[OMX_PORT_INPUTPORT_INDEX];
    pOutPort = (ComponentPortType*) ipPorts[OMX_PORT_OUTPUTPORT_INDEX];

    pInPort->ActualNumPortFormatsSupported = 1;

    //OMX_VIDEO_PARAM_PORTFORMATTYPE INPUT PORT SETTINGS
    //On input port for index 0
    SetHeader(&pInPort->VideoParam[0], sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    pInPort->VideoParam[0].nPortIndex = 0;
    pInPort->VideoParam[0].nIndex = 0;
    pInPort->VideoParam[0].eCompressionFormat = OMX_VIDEO_CodingAVC;
    pInPort->VideoParam[0].eColorFormat = OMX_COLOR_FormatUnused;

    pOutPort->ActualNumPortFormatsSupported = 1;

    //OMX_VIDEO_PARAM_PORTFORMATTYPE OUTPUT PORT SETTINGS
    //On output port for index 0
    SetHeader(&pOutPort->VideoParam[0], sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    pOutPort->VideoParam[0].nPortIndex = 1;
    pOutPort->VideoParam[0].nIndex = 0;
    pOutPort->VideoParam[0].eCompressionFormat = OMX_VIDEO_CodingUnused;
    pOutPort->VideoParam[0].eColorFormat = OMX_COLOR_FormatYUV420Planar;

    oscl_strncpy((OMX_STRING)iComponentRole, (OMX_STRING)"video_decoder.avc", OMX_MAX_STRINGNAME_SIZE);

    iDecodeReturn = OMX_FALSE;
    iFlushOutputStatus = OMX_TRUE;

    if (ipAvcDec)
    {
        OSCL_DELETE(ipAvcDec);
        ipAvcDec = NULL;
    }

    ipAvcDec = OSCL_NEW(AvcDecoder_OMX, (this));

    if (ipAvcDec == NULL)
    {
        return OMX_ErrorInsufficientResources;
    }


#if PROXY_INTERFACE

    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentSendCommand = BaseComponentSendCommand;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentGetParameter = BaseComponentGetParameter;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentSetParameter = BaseComponentSetParameter;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentGetConfig = BaseComponentGetConfig;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentSetConfig = BaseComponentSetConfig;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentGetExtensionIndex = BaseComponentGetExtensionIndex;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentGetState = BaseComponentGetState;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentUseBuffer = BaseComponentUseBuffer;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentAllocateBuffer = BaseComponentAllocateBuffer;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentFreeBuffer = BaseComponentFreeBuffer;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentEmptyThisBuffer = BaseComponentEmptyThisBuffer;
    ((ProxyApplication_OMX*)ipComponentProxy)->ComponentFillThisBuffer = BaseComponentFillThisBuffer;

#endif

    return OMX_ErrorNone;
}


/** This function is called by the omx core when the component
    * is disposed by the IL client with a call to FreeHandle().
    * \param Component, the component to be disposed
    */

OMX_ERRORTYPE OpenmaxAvcAO::DestroyComponent()
{
    if (iIsInit != OMX_FALSE)
    {
        ComponentDeInit();
    }

    //Destroy the base class now
    DestroyBaseComponent();

    if (ipAvcDec)
    {
        OSCL_DELETE(ipAvcDec);
        ipAvcDec = NULL;
    }

    if (ipAppPriv)
    {
        ipAppPriv->CompHandle = NULL;

        oscl_free(ipAppPriv);
        ipAppPriv = NULL;
    }

    return OMX_ErrorNone;
}


/* This function will be called in case of buffer management without marker bit present
 * The purpose is to copy the current input buffer into a big temporary buffer, so that
 * an incomplete/partial frame is never passed to the decoder library for decode
 */
void OpenmaxAvcAO::ComponentBufferMgmtWithoutMarker()
{
    //This common routine has been written in the base class
    TempInputBufferMgmtWithoutMarker();
}

OMX_BOOL OpenmaxAvcAO::ParseFullAVCFramesIntoNALs(OMX_BUFFERHEADERTYPE* aInputBuffer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ParseFullAVCFramesIntoNALs IN"));

    ipInputBuffer = aInputBuffer;

    if (iNumInputBuffer == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OmxComponentBase : ParseFullAVCFramesIntoNALs ERROR"));
        return OMX_FALSE;
    }

    if (iPVCapabilityFlags.iOMXComponentUsesNALStartCodes && !(ipInputBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA))
    {
        OMX_U32 offset = ipInputBuffer->nOffset;
        OMX_U32 length = ipInputBuffer->nFilledLen;
        OMX_U8* pBuffer = ipInputBuffer->pBuffer + offset;
        OMX_U8* pNAL;
        int nalSize;

        iNumNALs = 0;

        while (length > 0)
        {
            if (AVCDEC_SUCCESS != ipAvcDec->GetNextFullNAL_OMX(&pNAL, &nalSize, pBuffer, &length))
            {
                break;
            }

            iNALSizeArray[iNumNALs] = nalSize;
            iNALStartCodeSizeArray[iNumNALs] = (uint32)(pNAL - pBuffer);

            // increment buffer to end of current NAL
            pBuffer = pNAL + nalSize;

            iNumNALs++;
        }

        if (iNumNALs > 0)
        {
            iCurrNAL = 0;
            iNALOffset = ipInputBuffer->nOffset + iNALStartCodeSizeArray[iCurrNAL];
            ipFrameDecodeBuffer = ipInputBuffer->pBuffer + iNALOffset;
            iInputCurrLength = iNALSizeArray[iCurrNAL];
            if (iNumNALs > 1)
            {
                iNALOffset += (iInputCurrLength + iNALStartCodeSizeArray[iCurrNAL + 1]); // offset for next NAL
            }

            //capture the timestamp to be send to the corresponding output buffer
            iFrameTimestamp = ipInputBuffer->nTimeStamp;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OmxComponentBase : ParseFullAVCFramesIntoNALs ERROR"));
            return OMX_FALSE;
        }
    }
    else if ((iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes || iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes)
             && !(ipInputBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA))
    {
        OMX_U32 offset = ipInputBuffer->nOffset;
        OMX_S32 length = ipInputBuffer->nFilledLen;
        OMX_U8* pBuffer = ipInputBuffer->pBuffer + offset;
        int32 nalSize = 0;

        iNumNALs = 0;

        while (length > 0)
        {
            if (iSizeOfNALSize == sizeof(OMX_U16))
            {
                nalSize = (OMX_U32)(*((OMX_U16*)pBuffer));
            }
            else if (iSizeOfNALSize == sizeof(OMX_U32))
            {
                nalSize = *((OMX_U32*)pBuffer);
            }

            uint32 jmp = nalSize + iSizeOfNALSize;
            length -= jmp;

            // make sure that nalSize fits in this buffer (since buffer should be full frames and therefore no partial NALs)
            if (length >= 0)
            {
                pBuffer += jmp;
                iNALSizeArray[iNumNALs] = nalSize;
                iNumNALs++;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OmxComponentBase : ParseFullAVCFramesIntoNALs ERROR"));
                return OMX_FALSE;
            }
        }

        if (iNumNALs > 0)
        {
            iCurrNAL = 0;
            iNALOffset = ipInputBuffer->nOffset + iSizeOfNALSize;
            ipFrameDecodeBuffer = ipInputBuffer->pBuffer + iNALOffset;
            iInputCurrLength = iNALSizeArray[iCurrNAL];
            iNALOffset += (iInputCurrLength + iSizeOfNALSize); // offset for next NAL
            //capture the timestamp to be send to the corresponding output buffer
            iFrameTimestamp = ipInputBuffer->nTimeStamp;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OmxComponentBase : ParseFullAVCFramesIntoNALs ERROR"));
            return OMX_FALSE;
        }
    }
    // may be a full frame, or may incomplete, therefore don't check for OMX_BUFFERFLAG_ENDOFFRAME
    else if (ipInputBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA)
    {
        // get extra data from end of buffer
        OMX_OTHER_EXTRADATATYPE *pExtra;
        OMX_U32 offset = ipInputBuffer->nOffset + ipInputBuffer->nFilledLen;
        OMX_U32 allocLen = ipInputBuffer->nAllocLen;
        OMX_U8* pTemp = ipInputBuffer->pBuffer + offset;

        // align
        pExtra = (OMX_OTHER_EXTRADATATYPE *)(((OMX_U32) pTemp + 3) & ~3);
        offset += (OMX_U32) pExtra - (OMX_U32) pTemp;

        while (pExtra->eType != OMX_ExtraDataNone)
        {
            if (pExtra->eType == OMX_ExtraDataNALSizeArray)
            {
                oscl_memcpy(iNALSizeArray, ((OMX_U8*)pExtra + 20), pExtra->nDataSize);
                iNumNALs = pExtra->nDataSize >> 2;
                iCurrNAL = 0;
                iNALOffset = ipInputBuffer->nOffset;
                break;
            }

            offset += pExtra->nSize;
            if (offset > (allocLen - 20))
            {
                // corrupt data
                break;
            }
            else
            {
                pExtra = (OMX_OTHER_EXTRADATATYPE *)((OMX_U8*)pExtra + pExtra->nSize);
            }
        }

        if (pExtra->eType != OMX_ExtraDataNALSizeArray)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OmxComponentBase : ParseFullAVCFramesIntoNALs ERROR"));
            return OMX_FALSE;
        }

        ipFrameDecodeBuffer = ipInputBuffer->pBuffer + iNALOffset;
        iInputCurrLength = iNALSizeArray[iCurrNAL];
        iNALOffset += iInputCurrLength; // offset for next NAL
        //capture the timestamp to be send to the corresponding output buffer
        iFrameTimestamp = ipInputBuffer->nTimeStamp;
    }
    else if (ipInputBuffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
    {
        iInputCurrLength = ipInputBuffer->nFilledLen;
        ipFrameDecodeBuffer = ipInputBuffer->pBuffer + ipInputBuffer->nOffset;
        //capture the timestamp to be send to the corresponding output buffer
        iFrameTimestamp = ipInputBuffer->nTimeStamp;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OmxComponentBase : ParseFullAVCFramesIntoNALs ERROR"));
        return OMX_FALSE;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ParseFullAVCFramesIntoNALs OUT"));
    return OMX_TRUE;
}


void OpenmaxAvcAO::ProcessData()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ProcessData IN"));
    if (!iEndOfFrameFlag)
    {
        DecodeWithoutMarker();
    }
    else
    {
        DecodeWithMarker();
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ProcessData OUT"));
}


/* Decoding function for input buffers without end of frame flag marked */
void OpenmaxAvcAO::DecodeWithoutMarker()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker IN"));

    QueueType*              pInputQueue  = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;
    QueueType*              pOutputQueue = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;
    ComponentPortType*  pOutPort =     ipPorts[OMX_PORT_OUTPUTPORT_INDEX];
    OMX_COMPONENTTYPE *     pHandle =      &iOmxComponent;

    OMX_U8*                 pTempInBuffer;
    OMX_U32                 TempInLength;
    OMX_BOOL                MarkerFlag = OMX_FALSE;
    OMX_BOOL                ResizeNeeded = OMX_FALSE;

    OMX_U32 TempInputBufferSize = (2 * sizeof(uint8) * (ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize));
    OMX_U32 CurrWidth =  ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nFrameWidth;
    OMX_U32 CurrHeight = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nFrameHeight;


    if ((!iIsInputBufferEnded) || ((iEndofStream) || (0 != iTempInputBufferLength)))
    {
        //Check whether prev output bufer has been released or not
        if (OMX_TRUE == iNewOutBufRequired)
        {
            //Check whether a new output buffer is available or not

            if (0 == iNumAvailableOutputBuffers)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker OUT output buffer unavailable"));
                //Store the mark data for output buffer, as it will be overwritten next time
                if (NULL != ipTargetComponent)
                {
                    ipTempTargetComponent = ipTargetComponent;
                    iTempTargetMarkData = iTargetMarkData;
                    iMarkPropagate = OMX_TRUE;
                }
                return;
            }

            // Dequeue until getting a valid (available output buffer)
            BufferCtrlStruct *pBCTRL = NULL;
            do
            {
                ipOutputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
                if (NULL == ipOutputBuffer)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker Error, output buffer dequeue returned NULL, OUT"));
                    return;
                }

                pBCTRL = (BufferCtrlStruct *)ipOutputBuffer->pOutputPortPrivate;
                if (pBCTRL->iRefCount == 0)
                {
                    //buffer is available
                    iNumAvailableOutputBuffers--;
                    pBCTRL->iIsBufferInComponentQueue = OMX_FALSE;
                    break; // found buffer, EXIT
                }
                else
                {
                    Queue(pOutputQueue, (void*)ipOutputBuffer);
                }
            }
            while (1);



            //Do not proceed if the output buffer can't fit the YUV data
            if ((ipOutputBuffer->nAllocLen < (OMX_U32)(((CurrWidth + 15)&(~15)) *((CurrHeight + 15)&(~15)) * 3 / 2)) && (OMX_TRUE == ipAvcDec->iAvcActiveFlag))
            {
                ipOutputBuffer->nFilledLen = 0;
                ReturnOutputBuffer(ipOutputBuffer, pOutPort);
                ipOutputBuffer = NULL;
                return;
            }

            ipOutputBuffer->nFilledLen = 0;
            iNewOutBufRequired = OMX_FALSE;
        }


        /* Code for the marking buffer. Takes care of the OMX_CommandMarkBuffer
         * command and hMarkTargetComponent as given by the specifications
         */
        if (NULL != ipMark)
        {
            ipOutputBuffer->hMarkTargetComponent = ipMark->hMarkTargetComponent;
            ipOutputBuffer->pMarkData = ipMark->pMarkData;
            ipMark = NULL;
        }

        if ((OMX_TRUE == iMarkPropagate) && (ipTempTargetComponent != ipTargetComponent))
        {
            ipOutputBuffer->hMarkTargetComponent = ipTempTargetComponent;
            ipOutputBuffer->pMarkData = iTempTargetMarkData;
            ipTempTargetComponent = NULL;
            iMarkPropagate = OMX_FALSE;
        }
        else if (ipTargetComponent != NULL)
        {
            ipOutputBuffer->hMarkTargetComponent = ipTargetComponent;
            ipOutputBuffer->pMarkData = iTargetMarkData;
            ipTargetComponent = NULL;
            iMarkPropagate = OMX_FALSE;
        }
        //Mark buffer code ends here


        pTempInBuffer = ipTempInputBuffer + iTempConsumedLength;
        TempInLength = iTempInputBufferLength;

        //Output buffer ipOutputBuffer is passed in - decoder may or may not use it as a reference
        // if the decoder decides to use ipOutputBuffer it will set ipOutputBuffer to NULL
        // Decoder may or may not produce a different output buffer in ipOutputBufferForRendering
        // if decoder produces output buffer for rendering - ipOutputBufferForRendering will != NULL
        ipOutputBufferForRendering = NULL;

        iDecodeReturn = ipAvcDec->AvcDecodeVideo_OMX(ipOutputBuffer, &ipOutputBufferForRendering,
                        &(pTempInBuffer),
                        &TempInLength,
                        &(ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam),
                        &iFrameCount,
                        MarkerFlag, &ResizeNeeded);

        if (ipOutputBufferForRendering)
        {
            //Set the timestamp equal to the input buffer timestamp
            ipOutputBufferForRendering->nTimeStamp = iFrameTimestamp;
        }

        iTempConsumedLength += (iTempInputBufferLength - TempInLength);
        iTempInputBufferLength = TempInLength;

        //If decoder returned error, report it to the client via a callback
        if (!iDecodeReturn && OMX_FALSE == ipAvcDec->iAvcActiveFlag)
        {
            // initialization error
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker ErrorBadParameter callback send"));

            (*(ipCallbacks->EventHandler))
            (pHandle,
             iCallbackData,
             OMX_EventError,
             OMX_ErrorBadParameter,
             0,
             NULL);
        }
        else if (!iDecodeReturn && OMX_FALSE == iEndofStream)
        {
            // decoding error
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker ErrorStreamCorrupt callback send"));

            (*(ipCallbacks->EventHandler))
            (pHandle,
             iCallbackData,
             OMX_EventError,
             OMX_ErrorStreamCorrupt,
             0,
             NULL);
        }


        //Do not decode if big buffer is less than half the size
        if (TempInLength < (TempInputBufferSize >> 1))
        {
            iIsInputBufferEnded = OMX_TRUE;
            iNewInBufferRequired = OMX_TRUE;
        }

        if (ResizeNeeded == OMX_TRUE)
        {
            OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*) ipAppPriv->CompHandle;

            // make sure to release buffers held by decoder
            ReleaseReferenceBuffers();

            iResizePending = OMX_TRUE;
            (*(ipCallbacks->EventHandler))
            (pHandle,
             iCallbackData,
             OMX_EventPortSettingsChanged, //The command was completed
             OMX_PORT_OUTPUTPORT_INDEX,
             0,
             NULL);
        }

        /* If EOS flag has come from the client & there are no more
         * input buffers to decode, send the callback to the client
         */
        if (OMX_TRUE == iEndofStream)
        {
            if ((0 == iTempInputBufferLength) && !iDecodeReturn)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker EOS callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventBufferFlag,
                 1,
                 OMX_BUFFERFLAG_EOS,
                 NULL);

                iNewInBufferRequired = OMX_TRUE;
                iEndofStream = OMX_FALSE;

                if (ipOutputBuffer)
                {
                    ipOutputBuffer->nFlags |= OMX_BUFFERFLAG_EOS;

                    ReturnOutputBuffer(ipOutputBuffer, pOutPort);
                    ipOutputBuffer = NULL;
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker OUT"));

                // method ReturnOutputBuffer automatically sets flag iNewOutBufRequired to true
                // it is possible that the decoder never used ipOutputBuffer for reference.
                // in this case - keep the ipOutputBuffer and reset the flag back


                if (OMX_TRUE == iNewOutBufRequired && ipOutputBuffer != NULL)
                {
                    iNewOutBufRequired = OMX_FALSE;
                }

                return;
            }
        }

        //Send the output buffer back when it has some data in it
        if (ipOutputBufferForRendering)
        {
            ReturnOutputBuffer(ipOutputBufferForRendering, pOutPort);
            ipOutputBufferForRendering = NULL;
        }

        // method ReturnOutputBuffer automatically sets flag iNewOutBufRequired to true
        // it is possible that the decoder never used ipOutputBuffer for reference.
        // in this case - keep the ipOutputBuffer and reset the flag back

        if (OMX_TRUE == iNewOutBufRequired && ipOutputBuffer != NULL)
        {
            iNewOutBufRequired = OMX_FALSE;
        }

        /* If there is some more processing left with current buffers, re-schedule the AO
         * Do not go for more than one round of processing at a time.
         * This may block the AO longer than required.
         */
        if ((ResizeNeeded == OMX_FALSE) && ((TempInLength != 0) || (GetQueueNumElem(pInputQueue) > 0))
                && ((iNumAvailableOutputBuffers > 0) || (OMX_FALSE == iNewOutBufRequired)))
        {
            RunIfNotReady();
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker OUT"));
    return;
}


/* Decoding function for input buffers with end of frame flag marked */
void OpenmaxAvcAO::DecodeWithMarker()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker IN"));

    OMX_COMPONENTTYPE  *pHandle = &iOmxComponent;
    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;
    QueueType* pOutputQueue = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;

    ComponentPortType*  pInPort = ipPorts[OMX_PORT_INPUTPORT_INDEX];
    ComponentPortType*  pOutPort = ipPorts[OMX_PORT_OUTPUTPORT_INDEX];

    OMX_BOOL                MarkerFlag = OMX_TRUE;
    OMX_BOOL                ResizeNeeded = OMX_FALSE;

    OMX_U32 CurrWidth =  ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nFrameWidth;
    OMX_U32 CurrHeight = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.video.nFrameHeight;

    if ((!iIsInputBufferEnded) || (iEndofStream))
    {
        //Check whether prev output bufer has been released or not
        if (OMX_TRUE == iNewOutBufRequired)
        {
            //Check whether a new output buffer is available or not
            if (iNumAvailableOutputBuffers > 0)
            {
                // Dequeue until getting a valid (available output buffer)
                BufferCtrlStruct *pBCTRL = NULL;
                do
                {
                    ipOutputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
                    if (NULL == ipOutputBuffer)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker Error, output buffer dequeue returned NULL, OUT"));
                        return;
                    }

                    pBCTRL = (BufferCtrlStruct *)ipOutputBuffer->pOutputPortPrivate;
                    if (pBCTRL->iRefCount == 0)
                    {
                        //buffer is available
                        iNumAvailableOutputBuffers--;
                        pBCTRL->iIsBufferInComponentQueue = OMX_FALSE;
                        break; // found buffer, EXIT
                    }
                    else
                    {
                        // queue the buffer back
                        Queue(pOutputQueue, (void*)ipOutputBuffer);
                    }
                }
                while (1);



                //Do not proceed if the output buffer can't fit the YUV data
                if ((ipOutputBuffer->nAllocLen < (OMX_U32)(((CurrWidth + 15)&(~15)) *((CurrHeight + 15)&(~15)) * 3 / 2)) && (OMX_TRUE == ipAvcDec->iAvcActiveFlag))
                {
                    ipOutputBuffer->nFilledLen = 0;
                    ReturnOutputBuffer(ipOutputBuffer, pOutPort);
                    ipOutputBuffer = NULL;
                    return;
                }
                ipOutputBuffer->nFilledLen = 0;
                iNewOutBufRequired = OMX_FALSE;
            }
            else if (OMX_TRUE == ipAvcDec->iAvcActiveFlag)
            {
                iNewInBufferRequired = OMX_FALSE;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker OUT output buffer unavailable"));
                return;
            }
        }

        if (NULL != ipOutputBuffer)
        {
            /* Code for the marking buffer. Takes care of the OMX_CommandMarkBuffer
             * command and hMarkTargetComponent as given by the specifications
             */
            if (NULL != ipMark)
            {
                ipOutputBuffer->hMarkTargetComponent = ipMark->hMarkTargetComponent;
                ipOutputBuffer->pMarkData = ipMark->pMarkData;
                ipMark = NULL;
            }

            if (NULL != ipTargetComponent)
            {
                ipOutputBuffer->hMarkTargetComponent = ipTargetComponent;
                ipOutputBuffer->pMarkData = iTargetMarkData;
                ipTargetComponent = NULL;

            }
            //Mark buffer code ends here
        }

        if (iInputCurrLength > 0)
        {
            //Store the input timestamp into a temp variable
            ipAvcDec->CurrInputTimestamp = iFrameTimestamp;

            //Output buffer ipOutputBuffer is passed in - decoder may or may not use it as a reference
            // if the decoder decides to use ipOutputBuffer it will set ipOutputBuffer to NULL
            // Decoder may or may not produce a different output buffer in ipOutputBufferForRendering
            // if there is output to be rendered - then ipOutputBufferForRendering will be != NULL
            ipOutputBufferForRendering = NULL;

            iDecodeReturn = ipAvcDec->AvcDecodeVideo_OMX(ipOutputBuffer, &ipOutputBufferForRendering,
                            &(ipFrameDecodeBuffer),
                            &(iInputCurrLength),
                            &(ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam),
                            &iFrameCount,
                            MarkerFlag,
                            &ResizeNeeded);


            if (ResizeNeeded == OMX_TRUE)
            {
                OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*) ipAppPriv->CompHandle;

                // set the flag. Do not process any more frames until
                // IL Client sends PortDisable event (thus starting the procedure for
                // dynamic port reconfiguration)
                iResizePending = OMX_TRUE;

                // make sure to release buffers held by decoder
                ReleaseReferenceBuffers();
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : Sending the PortSettings Changed Callback"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventPortSettingsChanged, //The command was completed
                 OMX_PORT_OUTPUTPORT_INDEX,
                 0,
                 NULL);
            }


            //If decoder returned error, report it to the client via a callback
            if (!iDecodeReturn && OMX_FALSE == ipAvcDec->iAvcActiveFlag)
            {
                // initialization error
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithoutMarker ErrorBadParameter callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventError,
                 OMX_ErrorBadParameter,
                 0,
                 NULL);
            }
            else if (!iDecodeReturn && OMX_FALSE == iEndofStream)
            {
                // decode error
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker ErrorStreamCorrupt callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventError,
                 OMX_ErrorStreamCorrupt,
                 0,
                 NULL);
            }


            if (0 == iInputCurrLength)
            {
                if (iPVCapabilityFlags.iOMXComponentUsesFullAVCFrames)
                {
                    iCurrNAL++;

                    if (iCurrNAL < iNumNALs)
                    {
                        ipFrameDecodeBuffer = ipInputBuffer->pBuffer + iNALOffset;
                        iInputCurrLength = iNALSizeArray[iCurrNAL];

                        if (iPVCapabilityFlags.iOMXComponentUsesNALStartCodes && ((iNumNALs - iCurrNAL) > 1)) // how many NALs are left? make sure at least 1
                        {
                            iNALOffset += (iInputCurrLength + iNALStartCodeSizeArray[iCurrNAL + 1]); // offset for next NAL
                        }
                        else if (iPVCapabilityFlags.iOMXComponentUsesInterleaved2BNALSizes || iPVCapabilityFlags.iOMXComponentUsesInterleaved4BNALSizes)
                        {
                            iNALOffset += (iInputCurrLength + iSizeOfNALSize); // offset for next NAL
                        }
                        else
                        {
                            iNALOffset += iInputCurrLength; // offset for next NAL
                        }

                        iNewInBufferRequired = OMX_FALSE;
                    }
                    else
                    {
                        ipInputBuffer->nFilledLen = 0;
                        ReturnInputBuffer(ipInputBuffer, pInPort);
                        iNewInBufferRequired = OMX_TRUE;
                        iIsInputBufferEnded = OMX_TRUE;
                        ipInputBuffer = NULL;
                    }
                }
                else
                {
                    ipInputBuffer->nFilledLen = 0;
                    ReturnInputBuffer(ipInputBuffer, pInPort);
                    iNewInBufferRequired = OMX_TRUE;
                    iIsInputBufferEnded = OMX_TRUE;
                    ipInputBuffer = NULL;
                }
            }
            else
            {
                iNewInBufferRequired = OMX_FALSE;
            }
        }
        else if (iEndofStream == OMX_FALSE)
        {
            // it's possible that after partial frame assembly, the input buffer still remains empty (due to
            // client erroneously sending such buffers). This may cause error in processing/returning buffers
            // This code adds robustness in the sense that it returns such buffer to the client

            ipInputBuffer->nFilledLen = 0;
            ReturnInputBuffer(ipInputBuffer, pInPort);
            ipInputBuffer = NULL;
            iNewInBufferRequired = OMX_TRUE;
            iIsInputBufferEnded = OMX_TRUE;
        }


        /* If EOS flag has come from the client & there are no more
         * input buffers to decode, send the callback to the client*/
        if (OMX_TRUE == iEndofStream)
        {
            if (!iFlushOutputStatus)
            {
                // this is the very last buffer to be sent out.
                // it is empty - and has EOS flag attached to it
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker EOS callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventBufferFlag,
                 1,
                 OMX_BUFFERFLAG_EOS,
                 NULL);

                iNewInBufferRequired = OMX_TRUE;
                iEndofStream = OMX_FALSE;
                iFlushOutputStatus = OMX_TRUE;

                // in this case - simply return the available ipOutputBuffer
                // there is no need for involving decoder and ipOutputBufferForRendering - since this
                // is an empty buffer
                if (ipOutputBuffer)
                {
                    ipOutputBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                    ReturnOutputBuffer(ipOutputBuffer, pOutPort);
                    ipOutputBuffer = NULL;
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker OUT"));

                return;
            }
            else if (iFlushOutputStatus)
            {
                // flush output buffer
                ipOutputBufferForRendering = NULL;

                iFlushOutputStatus = ipAvcDec->FlushOutput_OMX(&ipOutputBufferForRendering);

                if (OMX_FALSE != iFlushOutputStatus)
                {
                    // this is the case where Flush succeeded (i.e. there is one output
                    // buffer left)
                    if (ipOutputBufferForRendering)
                    {
                        ReturnOutputBuffer(ipOutputBufferForRendering, pOutPort);
                        ipOutputBufferForRendering = NULL;
                    }

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker OUT"));
                    RunIfNotReady();

                    // method ReturnOutputBuffer automatically sets flag iNewOutBufRequired to true
                    // it is possible that the decoder never used ipOutputBuffer for reference.
                    // in this case - keep the ipOutputBuffer and reset the flag back

                    if (OMX_TRUE == iNewOutBufRequired && ipOutputBuffer != NULL)
                    {
                        iNewOutBufRequired = OMX_FALSE;
                    }

                    return;
                }
                else
                {
                    // this is the case where no output buffers can be flushed.
                    // spin the AO once again to send the empty output buffer with EOS attached to it
                    RunIfNotReady();
                }
            }
        }

        //Send the output buffer back when it has some data in it
        if (ipOutputBufferForRendering)
        {
            ReturnOutputBuffer(ipOutputBufferForRendering, pOutPort);
            ipOutputBufferForRendering = NULL;
        }

        // method ReturnOutputBuffer automatically sets flag iNewOutBufRequired to true
        // it is possible that the decoder never used ipOutputBuffer for reference.
        // in this case - keep the ipOutputBuffer and reset the flag back

        if (OMX_TRUE == iNewOutBufRequired && ipOutputBuffer != NULL)
        {
            iNewOutBufRequired = OMX_FALSE;
        }


        /* If there is some more processing left with current buffers, re-schedule the AO
         * Do not go for more than one round of processing at a time.
         * This may block the AO longer than required.
         */
        if ((ResizeNeeded == OMX_FALSE) && ((iInputCurrLength != 0) || (GetQueueNumElem(pInputQueue) > 0))
                && ((iNumAvailableOutputBuffers > 0) || (OMX_FALSE == iNewOutBufRequired)))
        {
            RunIfNotReady();
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : DecodeWithMarker OUT"));
    return;
}





//Component object constructor
OpenmaxAvcAO::OpenmaxAvcAO()
{
    ipAvcDec = NULL;

    if (!IsAdded())
    {
        AddToScheduler();
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : constructed"));
}


//Component object destructor
OpenmaxAvcAO::~OpenmaxAvcAO()
{
    if (IsAdded())
    {
        RemoveFromScheduler();
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : destructed"));
}


/** The Initialization function
 */
OMX_ERRORTYPE OpenmaxAvcAO::ComponentInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ComponentInit IN"));

    OMX_ERRORTYPE Status = OMX_ErrorNone;

    if (OMX_TRUE == iIsInit)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ComponentInit error incorrect operation"));
        return OMX_ErrorIncorrectStateOperation;
    }
    iIsInit = OMX_TRUE;


    //avc lib init
    if (!iCodecReady)
    {
        Status = ipAvcDec->AvcDecInit_OMX();
        iCodecReady = OMX_TRUE;
    }

    iInputCurrLength = 0;
    iNumNALs = 0;
    iCurrNAL = 0;
    iNALOffset = 0;
    oscl_memset(iNALSizeArray, 0, MAX_NAL_PER_FRAME * sizeof(uint32));
    //Used in dynamic port reconfiguration
    iFrameCount = 0;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ComponentInit OUT"));

    return Status;

}

/** This function is called upon a transition to the idle or invalid state.
 *  Also it is called by the AvcComponentDestructor() function
 */
OMX_ERRORTYPE OpenmaxAvcAO::ComponentDeInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ComponentDeInit IN"));

    OMX_ERRORTYPE Status = OMX_ErrorNone;

    iIsInit = OMX_FALSE;

    if (iCodecReady)
    {
        Status = ipAvcDec->AvcDecDeinit_OMX();
        iCodecReady = OMX_FALSE;
    }

#if PROFILING_ON
    if (0 != iFrameCount)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iDiagnosticsLogger, PVLOGMSG_INFO, (0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));

        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iDiagnosticsLogger, PVLOGMSG_INFO, (0, "OpenmaxAvcAO - Total Decoding Time (ms) = %d", OsclTickCount::TicksToMsec(ipAvcDec->iTotalTicks)));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iDiagnosticsLogger, PVLOGMSG_INFO, (0, "OpenmaxAvcAO - Total Number of Decoded Frames = %d", iFrameCount));
    }
#endif

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAvcAO : ComponentDeInit OUT"));

    return Status;

}


OMX_ERRORTYPE OpenmaxAvcAO::GetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OSCL_UNUSED_ARG(hComponent);
    OSCL_UNUSED_ARG(nIndex);
    OSCL_UNUSED_ARG(pComponentConfigStructure);
    return OMX_ErrorNotImplemented;
}

/* This routine will reset the decoder library and some of the associated flags*/
void OpenmaxAvcAO::ResetComponent()
{
    // reset decoder
    if (ipAvcDec)
    {
        ipAvcDec->ResetDecoder();
    }

}

void OpenmaxAvcAO::ReleaseReferenceBuffers()
{
    if (ipAvcDec)
    {
        ipAvcDec->ReleaseReferenceBuffers();
    }
}

/* This routine finds the size of the start code and points to the beginning of the NAL.
 * The start code should be at the beginning of the buffer for this routine to function properly.
 * If the buffer does not have a valid start code at the beginning of the buffer, the function
 * will return false.
 */
OMX_BOOL OpenmaxAvcAO::DetectStartCodeLength(OMX_U8* aBitstream, OMX_U8** aNalUnit, OMX_U32 aBufSize, OMX_U32* aSCSize)
{
    uint32 i = 0;

    while (aBitstream[i] == 0 && i < aBufSize)
    {
        i++;
    }
    if (i >= aBufSize)
    {
        // buffer is entirely 0s - cannot find any start_code_prefix.
        *aNalUnit = aBitstream;
        *aSCSize = 0;
        return OMX_FALSE;
    }
    else if (aBitstream[i] != 0x1)
    {
        // start_code_prefix is not at the beginning
        *aNalUnit = aBitstream;
        *aSCSize = 0;
        return OMX_FALSE;
    }
    else
    {
        // start_code found
        i++;
        *aNalUnit = aBitstream + i; /* point to the beginning of the NAL unit */
        *aSCSize = i;

        if (i != 3 && i != 4)
        {
            // AVC start codes are either 3 or 4 bytes.  if it finds otherwise, then this frame is corrupt
            return OMX_FALSE;
        }
        else
        {
            return OMX_TRUE;
        }
    }
}

void OpenmaxAvcAO::CalculateBufferParameters(OMX_U32 PortIndex)
{
    ipAvcDec->CalculateBufferParameters(&ipPorts[PortIndex]->PortParam);

    return ;
}
