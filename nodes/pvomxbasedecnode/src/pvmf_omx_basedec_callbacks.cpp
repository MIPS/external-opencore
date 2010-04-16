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
#include "pvmf_omx_basedec_callbacks.h"
#include "pvmf_omx_basedec_node.h"

////////////////////////////////////////////////////////////////////////////////////////////////
EventHandlerThreadSafeCallbackAO::EventHandlerThreadSafeCallbackAO(void* aObserver,
        uint32 aDepth,
        const char* aAOname,
        int32 aPriority)
        : ThreadSafeCallbackAO(aObserver, aDepth, aAOname, aPriority)
{

    // note: by using the semaphore before a chunk is allocated from the mempool and
    // after the chunk is returned to the mempool, the mempool can be the same size as the q depth
    iMemoryPool = ThreadSafeMemPoolFixedChunkAllocator::Create(aDepth);
    if (iMemoryPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                        PVLOGMSG_ERR, (0, "EventHandlerTSCAO::CreateMemPool() Memory pool failed to allocate"));
    }
    // MUST do a dummy ALLOC HERE TO Create mempool. Otherwise the mempool will be
    // created in the 2nd thread and will fail to deallocate properly.

    OsclAny *dummy = iMemoryPool->allocate(sizeof(EventHandlerSpecificData));
    iMemoryPool->deallocate(dummy);
}

EventHandlerThreadSafeCallbackAO::~EventHandlerThreadSafeCallbackAO()
{
    if (iMemoryPool)
    {

        iMemoryPool->removeRef();
        iMemoryPool = NULL;
    }
}

OsclReturnCode EventHandlerThreadSafeCallbackAO::ProcessEvent(OsclAny* EventData)
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    // In this case, ProcessEvent calls the method of the primary test AO to process the Event
    if (iObserver != NULL)
    {
        PVMFOMXBaseDecNode* ptr = (PVMFOMXBaseDecNode*) iObserver;

        if (ptr->IsAdded())
        {
            // call the node method that processes the callback
            EventHandlerSpecificData* ED = (EventHandlerSpecificData*) EventData;

            OMX_HANDLETYPE aComponent = ED->hComponent;
            OMX_PTR aAppData = ED->pAppData;
            OMX_EVENTTYPE aEvent = ED->eEvent;
            OMX_U32 aData1 = ED->nData1;
            OMX_U32 aData2 = ED->nData2;
            OMX_PTR aEventData = ED->pEventData;

            ptr->EventHandlerProcessing(aComponent, aAppData, aEvent, aData1, aData2, aEventData);

        }
    }

    // release the memory back to the mempool after processing the event
    iMemoryPool->deallocate(EventData);

    // Signal the semaphore that controls the remote thread.
    // The remote thread might be blocked and waiting for an event to be processed in case the event queue is full
    // Note: by signaling the semaphore AFTER releasing the memory back to the mempool, there is no
    // need to allocate more mempool items than the queue depth.
    OsclProcStatus::eOsclProcError sema_status;

    sema_status = RemoteThreadCtrlSema.Signal();
    if (sema_status != OsclProcStatus::SUCCESS_ERROR)
    {
        return OsclFailure;
    }


    return OsclSuccess;
}

// We override the Run to process multiple (i.e. all in the queue) events in one Run

void EventHandlerThreadSafeCallbackAO::Run()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EventHandlerThreadSafeCallbackAO::Run() In"));

    // Note: this method executes in the context of threadsafe AO (in the node thread)
    OsclAny *P; // parameter to dequeue
    OsclReturnCode status = OsclSuccess;


    do
    {


        P = DeQueue(status);


        if ((status == OsclSuccess) || (status == OsclPending))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EventHandlerThreadSafeCallbackAO_Audio::Run() - Calling Process Event"));
            ProcessEvent(P);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EventHandlerThreadSafeCallbackAO::Run() - could not dequeue event data"));
        }


        // it is possible that an event arrives between dequeueing the last event and this point.
        // If this is the case, we will be rescheduled and process the event
        // in the next Run


    }
    while (status == OsclSuccess);
    // if the status is "OsclPending" there were no more events in the queue
    // (if another event arrived in the meanwhile, AO will be rescheduled)



    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EventHandlerThreadSafeCallbackAO::Run() Out"));
}

// same as base-class DeQueue method, except no RunIfNotReady/PendForExec is called (since all events are processed in a loop)
// (i.e. PendForExec control is done in the loop in Run)
OsclAny* EventHandlerThreadSafeCallbackAO::DeQueue(OsclReturnCode &stat)
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    OsclAny *pData;

    stat = OsclSuccess;

    // Protect the queue while accessing it:
    Mutex.Lock();

    if (Q->NumElem == 0)
    {
        // nothing to de-queue
        stat = OsclFailure;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EventHandlerThreadSafeCallbackAO::DeQueue() - No events in the queue - return ()"));
        Mutex.Unlock();

        return NULL;
    }

    pData = (Q->pFirst[Q->index_out]).pData;

    Q->index_out++;
    // roll-over the index
    if (Q->index_out == Q->MaxNumElements)
        Q->index_out = 0;

    Q->NumElem--;
    // check if there is need to call PendForExec
    if ((Q->NumElem) == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EventHandlerThreadSafeCallbackAO::Run() - No more events, call PendForExec()"));
        PendForExec();
        stat = OsclPending; // let the Run know that the last event was pulled out of the queue
        // so that it can get out of the loop
    }


    //release queue access
    Mutex.Unlock();

    return pData;
}

OsclReturnCode EventHandlerThreadSafeCallbackAO::ReceiveEvent(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_EVENTTYPE aEvent,
        OMX_OUT OMX_U32 aData1,
        OMX_OUT OMX_U32 aData2,
        OMX_OUT OMX_PTR aEventData)
{
    // Note: this method executes in the context of remote thread trying to make the callback
    OsclReturnCode status;
    OsclProcStatus::eOsclProcError sema_status;

    // Wait on the remote thread control semaphore. If the queue is full, must block and wait
    // for the AO to dequeue some previous event. If the queue is not full, proceed
    sema_status = RemoteThreadCtrlSema.Wait();
    if (sema_status != OsclProcStatus::SUCCESS_ERROR)
        return OsclFailure;

    // NOTE: the semaphore will prevent the mempool allocate to be used if the queue is full

    // allocate the memory for the callback event specific data
    EventHandlerSpecificData* ED = (EventHandlerSpecificData*) iMemoryPool->allocate(sizeof(EventHandlerSpecificData));

    // pack the relevant data into the structure
    ED->hComponent = aComponent;
    ED->pAppData = aAppData;
    ED->eEvent = aEvent;
    ED->nData1 = aData1;
    ED->nData2 = aData2;
    ED->pEventData = aEventData;

    // convert the pointer into OsclAny ptr
    OsclAny* P = (OsclAny*) ED;

    // now , queue the event pointer
    status = Queue(P);

    return status;
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF EmptyBufferDoneThreadSafeCallbackAO::EmptyBufferDoneThreadSafeCallbackAO(void* aObserver,
        uint32 aDepth,
        const char* aAOname,
        int32 aPriority)
        : ThreadSafeCallbackAO(aObserver, aDepth, aAOname, aPriority)
{

    // note: by using the semaphore before a chunk is allocated from the mempool and
    // after the chunk is returned to the mempool, the mempool can be the same size as the q depth
    iMemoryPool = ThreadSafeMemPoolFixedChunkAllocator::Create(aDepth);
    if (iMemoryPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                        PVLOGMSG_ERR, (0, "EventHandlerTSCAO::CreateMemPool() Memory pool failed to allocate"));

    }
    // MUST do a dummy ALLOC HERE TO Create mempool. Otherwise the mempool will be
    // created in the 2nd thread and will fail to deallocate properly.

    OsclAny *dummy = iMemoryPool->allocate(sizeof(EmptyBufferDoneSpecificData));
    iMemoryPool->deallocate(dummy);
}

OSCL_EXPORT_REF EmptyBufferDoneThreadSafeCallbackAO::~EmptyBufferDoneThreadSafeCallbackAO()
{
    if (iMemoryPool)
    {

        iMemoryPool->removeRef();
        iMemoryPool = NULL;
    }

}
OsclReturnCode EmptyBufferDoneThreadSafeCallbackAO::ProcessEvent(OsclAny* EventData)
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    // In this case, ProcessEvent calls the method of the primary test AO to process the Event
    if (iObserver != NULL)
    {
        PVMFOMXBaseDecNode* ptr = (PVMFOMXBaseDecNode *) iObserver;
        if (ptr->IsAdded())
        {
            // re-cast the pointer
            EmptyBufferDoneSpecificData* ED = (EmptyBufferDoneSpecificData*) EventData;

            OMX_HANDLETYPE aComponent = ED->hComponent;
            OMX_PTR aAppData = ED->pAppData;
            OMX_BUFFERHEADERTYPE* aBuffer = ED->pBuffer;

            // call the node method to process the callback
            ptr->EmptyBufferDoneProcessing(aComponent, aAppData, aBuffer);

        }
    }

    // release the memory back to the mempool after processing the event
    iMemoryPool->deallocate(EventData);

    // Signal the semaphore that controls the remote thread.
    // The remote thread might be blocked and waiting for an event to be processed in case the event queue is full
    // Note: by signaling the semaphore AFTER releasing the memory back to the mempool, there is no
    // need to allocate more mempool items than the queue depth.
    OsclProcStatus::eOsclProcError sema_status;

    sema_status = RemoteThreadCtrlSema.Signal();
    if (sema_status != OsclProcStatus::SUCCESS_ERROR)
    {
        return OsclFailure;
    }


    return OsclSuccess;
}

// We override the Run to process multiple (i.e. all in the queue) events in one Run

void EmptyBufferDoneThreadSafeCallbackAO::Run()
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EmptyBufferDoneThreadSafeCallbackAO::Run() In"));

    OsclAny *P; // parameter to dequeue
    OsclReturnCode status = OsclSuccess;

    do
    {


        P = DeQueue(status);


        if ((status == OsclSuccess) || (status == OsclPending))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EmptyBufferDoneThreadSafeCallbackAO::Run() - Calling Process Event"));
            ProcessEvent(P);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EmptyBufferDoneThreadSafeCallbackAO::Run() - could not dequeue event data"));
        }


        // it is possible that an event arrives between dequeueing the last event and this point.
        // If this is the case, we will be rescheduled and process the event
        // in the next Run


    }
    while (status == OsclSuccess);
    // if the status is "OsclPending" there were no more events in the queue
    // (if another event arrived in the meanwhile, AO will be rescheduled)


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EmptyBufferDoneThreadSafeCallbackAO::Run() Out"));
}

// same as base-class DeQueue method, except no RunIfNotReady/PendForExec is called (since all events are processed in a loop)
// (i.e. PendForExec control is done in the loop in Run)
OsclAny* EmptyBufferDoneThreadSafeCallbackAO::DeQueue(OsclReturnCode &stat)
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    OsclAny *pData;
    stat = OsclSuccess;

    // Protect the queue while accessing it:
    Mutex.Lock();

    if (Q->NumElem == 0)
    {
        // nothing to de-queue
        stat = OsclFailure;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EmptyBufferDoneThreadSafeCallbackAO::DeQueue() - No events in the queue - return ()"));
        Mutex.Unlock();

        return NULL;
    }

    pData = (Q->pFirst[Q->index_out]).pData;

    Q->index_out++;
    // roll-over the index
    if (Q->index_out == Q->MaxNumElements)
        Q->index_out = 0;

    Q->NumElem--;
    // check if there is need to call PendForExec
    if ((Q->NumElem) == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "EmptyBufferDoneThreadSafeCallbackAO::Run() - No more events, call PendForExec()"));
        PendForExec();
        stat = OsclPending; // let the Run know that the last event was pulled out of the queue
        // so that it can get out of the loop
    }

    //release queue access
    Mutex.Unlock();


    return pData;
}

OsclReturnCode EmptyBufferDoneThreadSafeCallbackAO::ReceiveEvent(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{

    // Note: this method executes in the context of remote thread trying to make the callback
    OsclReturnCode status;
    OsclProcStatus::eOsclProcError sema_status;

    // Wait on the remote thread control semaphore. If the queue is full, must block and wait
    // for the AO to dequeue some previous event. If the queue is not full, proceed
    sema_status = RemoteThreadCtrlSema.Wait();
    if (sema_status != OsclProcStatus::SUCCESS_ERROR)
        return OsclFailure;

    // NOTE: the semaphore will prevent the mempool allocate to be used if the queue is full


    // allocate the memory for the callback event specific data
    EmptyBufferDoneSpecificData* ED = (EmptyBufferDoneSpecificData*) iMemoryPool->allocate(sizeof(EmptyBufferDoneSpecificData));

    // pack the relevant data into the structure
    ED->hComponent = aComponent;
    ED->pAppData = aAppData;
    ED->pBuffer = aBuffer;

    // convert the pointer into OsclAny ptr
    OsclAny* P = (OsclAny*) ED;

    // now , queue the event pointer
    status = Queue(P);

    return status;
}

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF FillBufferDoneThreadSafeCallbackAO::FillBufferDoneThreadSafeCallbackAO(void* aObserver,
        uint32 aDepth,
        const char* aAOname,
        int32 aPriority)
        : ThreadSafeCallbackAO(aObserver, aDepth, aAOname, aPriority)
{

    // note: by using the semaphore before a chunk is allocated from the mempool and
    // after the chunk is returned to the mempool, the mempool can be the same size as the q depth
    iMemoryPool = ThreadSafeMemPoolFixedChunkAllocator::Create(aDepth);
    if (iMemoryPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                        PVLOGMSG_ERR, (0, "EventHandlerTSCAO::CreateMemPool() Memory pool failed to allocate"));
    }
    // MUST do a dummy ALLOC HERE TO Create mempool. Otherwise the mempool will be
    // created in the 2nd thread and will fail to deallocate properly.

    OsclAny *dummy = iMemoryPool->allocate(sizeof(FillBufferDoneSpecificData));
    iMemoryPool->deallocate(dummy);
}

OSCL_EXPORT_REF FillBufferDoneThreadSafeCallbackAO::~FillBufferDoneThreadSafeCallbackAO()
{
    if (iMemoryPool)
    {

        iMemoryPool->removeRef();
        iMemoryPool = NULL;
    }
}

OsclReturnCode FillBufferDoneThreadSafeCallbackAO::ProcessEvent(OsclAny* EventData)
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    // In this case, ProcessEvent calls the method of the primary test AO to process the Event
    if (iObserver != NULL)
    {
        PVMFOMXBaseDecNode* ptr = (PVMFOMXBaseDecNode*) iObserver;
        if (ptr->IsAdded())
        {
            // re-cast the pointer
            FillBufferDoneSpecificData* ED = (FillBufferDoneSpecificData*) EventData;

            OMX_HANDLETYPE aComponent = ED->hComponent;
            OMX_PTR aAppData = ED->pAppData;
            OMX_BUFFERHEADERTYPE* aBuffer = ED->pBuffer;


            ptr->FillBufferDoneProcessing(aComponent, aAppData, aBuffer);

        }
    }


    // release the memory back to the mempool after processing the event
    iMemoryPool->deallocate(EventData);

    // Signal the semaphore that controls the remote thread.
    // The remote thread might be blocked and waiting for an event to be processed in case the event queue is full
    // Note: by signaling the semaphore AFTER releasing the memory back to the mempool, there is no
    // need to allocate more mempool items than the queue depth.
    OsclProcStatus::eOsclProcError sema_status;

    sema_status = RemoteThreadCtrlSema.Signal();
    if (sema_status != OsclProcStatus::SUCCESS_ERROR)
    {
        return OsclFailure;
    }

    return OsclSuccess;
}

// We override the Run to process multiple (i.e. all in the queue) events in one Run

void FillBufferDoneThreadSafeCallbackAO::Run()
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "FillBufferDoneThreadSafeCallbackAO::Run() In"));

    OsclAny *P; // parameter to dequeue
    OsclReturnCode status = OsclSuccess;

    do
    {


        P = DeQueue(status);


        if ((status == OsclSuccess) || (status == OsclPending))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "FillBufferDoneThreadSafeCallbackAO::Run() - Calling Process Event"));
            ProcessEvent(P);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "FillBufferDoneThreadSafeCallbackAO::Run() - could not dequeue event data"));
        }


        // it is possible that an event arrives between dequeueing the last event and this point.
        // If this is the case, we will be rescheduled and process the event
        // in the next Run


    }
    while (status == OsclSuccess);
    // if the status is "OsclPending" there were no more events in the queue
    // (if another event arrived in the meanwhile, AO will be rescheduled)





    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "FillBufferDoneThreadSafeCallbackAO::Run() Out"));
}

// same as base-class DeQueue method, except no RunIfNotReady/PendForExec is called (since all events are processed in a loop)
// (i.e. PendForExec control is done in the loop in Run)
OsclAny* FillBufferDoneThreadSafeCallbackAO::DeQueue(OsclReturnCode &stat)
{
    // Note: this method executes in the context of threadsafe AO (in the node thread)
    OsclAny *pData;
    stat = OsclSuccess;

    // Protect the queue while accessing it:
    Mutex.Lock();

    if (Q->NumElem == 0)
    {
        // nothing to de-queue
        stat = OsclFailure;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "FillBufferDoneThreadSafeCallbackAO::DeQueue() - No events in the queue - return ()"));
        Mutex.Unlock();

        return NULL;
    }

    pData = (Q->pFirst[Q->index_out]).pData;

    Q->index_out++;
    // roll-over the index
    if (Q->index_out == Q->MaxNumElements)
        Q->index_out = 0;

    Q->NumElem--;
    // check if there is need to call PendForExec
    if ((Q->NumElem) == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "FillBufferDoneThreadSafeCallbackAO::Run() - No more events, call PendForExec()"));
        PendForExec();
        stat = OsclPending; // let the Run know that the last event was pulled out of the queue
        // so that it can get out of the loop
    }

    //release queue access
    Mutex.Unlock();


    return pData;
}

OsclReturnCode FillBufferDoneThreadSafeCallbackAO::ReceiveEvent(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{

    // Note: this method executes in the context of remote thread trying to make the callback
    OsclReturnCode status;
    OsclProcStatus::eOsclProcError sema_status;

    // Wait on the remote thread control semaphore. If the queue is full, must block and wait
    // for the AO to dequeue some previous event. If the queue is not full, proceed
    sema_status = RemoteThreadCtrlSema.Wait();
    if (sema_status != OsclProcStatus::SUCCESS_ERROR)
        return OsclFailure;

    // NOTE: the semaphore will prevent the mempool allocate to be used if the queue is full

    // allocate the memory for the callback event specific data
    FillBufferDoneSpecificData* ED = (FillBufferDoneSpecificData*) iMemoryPool->allocate(sizeof(FillBufferDoneSpecificData));

    // pack the relevant data into the structure
    ED->hComponent = aComponent;
    ED->pAppData = aAppData;
    ED->pBuffer = aBuffer;

    // convert the pointer into OsclAny ptr
    OsclAny* P = (OsclAny*) ED;
    // now , queue the event pointer
    status = Queue(P);

    return status;
}

