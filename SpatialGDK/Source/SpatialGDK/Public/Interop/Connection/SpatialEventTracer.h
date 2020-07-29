// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "SpatialCommonTypes.h"

// TODO Remove maybe?
#include <WorkerSDK/improbable/c_worker.h>

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialEventTracer, Log, All);

//namespace worker {
//namespace c {
//	struct Trace_EventTracer;
//	struct Trace_SpanId;
//}
//}

// TODO: Hook up to build system
#define GDK_SPATIAL_EVENT_TRACING_ENABLED 1

class UFunction;
class AActor;

namespace SpatialGDK
{

struct SpatialSpanId
{
	SpatialSpanId(worker::c::Trace_EventTracer* InEventTracer);
	~SpatialSpanId();

private:
	Trace_SpanId CurrentSpanId;
	worker::c::Trace_EventTracer* EventTracer;
};

struct SpatialGDKEvent
{
	//SpatialSpanId SpanId;
	FString Message;
	FString Type;
	TMap<FString, FString> Data;
};

// TODO: discuss overhead from constructing SpatialGDKEvents
// TODO: Rename
SpatialGDKEvent ConstructEvent(const AActor* Actor, const UFunction* Function);
SpatialGDKEvent ConstructEvent(const AActor* Actor, ENetRole Role);
SpatialGDKEvent ConstructEvent(const AActor* Actor, const UObject* TargetObject, Worker_ComponentId ComponentId);
SpatialGDKEvent ConstructEvent(const AActor* Actor, Worker_RequestId CreateEntityRequestId);
SpatialGDKEvent ConstructEvent(const AActor* Actor, VirtualWorkerId NewAuthoritativeWorkerId);
SpatialGDKEvent ConstructEvent(const AActor* Actor, Worker_EntityId EntityId, Worker_RequestId RequestID);
SpatialGDKEvent ConstructEvent(const AActor* Actor, const FString& Type, Worker_RequestId RequestID);
SpatialGDKEvent ConstructEvent(const AActor* Actor, const FString& Type, Worker_CommandResponseOp ResponseOp);
SpatialGDKEvent ConstructEvent(const AActor* Actor, const UObject* TargetObject, const UFunction* Function, TraceKey TraceId, Worker_RequestId RequestID);
SpatialGDKEvent ConstructEvent(const AActor* Actor, const FString& Message, Worker_CreateEntityResponseOp ResponseOp);
SpatialGDKEvent ConstructEvent(const AActor* Actor, const UObject* TargetObject, const UFunction* Function, Worker_CommandResponseOp ResponseOp);
SpatialGDKEvent ConstructEvent(Worker_RequestId RequestID, bool bSuccess);

struct SpatialEventTracer
{
	SpatialEventTracer();
	~SpatialEventTracer();
	SpatialSpanId CreateActiveSpan();
	TOptional<Trace_SpanId> TraceEvent(const SpatialGDKEvent& Event);

	void Enable();
	void Disable();
	bool IsEnabled() { return bEnalbed; }
	worker::c::Trace_EventTracer* GetWorkerEventTracer() { return EventTracer; }

private:
	bool bEnalbed;
	worker::c::Trace_EventTracer* EventTracer;
};

}

// TODO

/*
Sending create entity request

Sending authority intent update

Sending delete entity request

Sending RPC

Sending RPC retry

Sending command response

Receiving add entity

Receiving remove entity

Receiving authority change

Receiving component update

Receiving command request

Receiving command response

Receiving create entity response

Individual RPC Calls (distinguishing between GDK and USER)

Custom events can be added
*/

/*
Actor name, Position,
Add/Remove Entity (can we also distinguish Remove Entity when moving to another worker vs Delete entity),
Authority/Authority intent changes,
RPC calls (when they were sent/received/processed),
Component Updates
*/
