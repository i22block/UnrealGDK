// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Interop/Connection/SpatialEventTracer.h"

#include "UObject/Class.h"
#include "GameFramework/Actor.h"

#include <WorkerSDK/improbable/c_trace.h>

DEFINE_LOG_CATEGORY(LogSpatialEventTracer);

using namespace SpatialGDK;
using namespace worker::c;

namespace
{
	static const char ActorKey[] = "Actor";
	const char* ActorKeyPtr = ActorKey;

	static const char ActorPositionKey[] = "Position";
	const char* ActorPositionKeyPtr = ActorPositionKey;

	static const char FunctionKey[] = "Function";
	const char* FunctionKeyPtr = FunctionKey;

	static const char NewRoleKey[] = "NewRole";
	const char* NewRoleKeyPtr = NewRoleKey;

	const char RoleNone[] = "ROLE_None";
	const char RoleAutonomousProxy[] = "ROLE_AutonomousProxy";
	const char RoleAuthority[] = "ROLE_Authority";
	const char RoleInvalid[] = "Invalid Role";

	static char SentStr[] = "Sent";
	static char ReceivedStr[] = "Received";
	const char* TypeToString(EventType Type)
	{
		switch (Type)
		{
		case SpatialGDK::EventType::Sent:
			return SentStr;
		case SpatialGDK::EventType::Received:
			return ReceivedStr;
		default:
			return nullptr;
		}

	}

	static char RPCStr[] = "RPC";
	static char AuthorityChangeStr[] = "AuthorityChange";
	const char* NameToString(EventName Name)
	{
		switch (Name)
		{
		case SpatialGDK::EventName::RPC:
			return RPCStr;
		case SpatialGDK::EventName::AuthorityChange:
			return AuthorityChangeStr;
		default:
			return nullptr;
		}

	}
}

void MyTraceCallback(void* UserData, const Trace_Item* Item)
{
	switch (Item->item_type)
	{
	case TRACE_ITEM_TYPE_EVENT:
	{
		const Trace_Event& Event = Item->item.event;

		if (Event.type == nullptr)
		{
			return;
		}

		// TODO: remove temporary filtering?
		if (Event.type == FString("network.receive_raw_message") ||
			Event.type == FString("network.receive_udp_datagram") ||
			Event.type == FString("network.send_raw_message") ||
			Event.type == FString("network.send_udp_datagram") ||
			Event.type == FString("worker.dequeue_op") ||
			Event.type == FString("worker.enqueue_op"))
		{
			return;
		}

		unsigned long int span1 = *reinterpret_cast<const unsigned long int*>(&Event.span_id.data[0]);
		unsigned long int span2 = *reinterpret_cast<const unsigned long int*>(&Event.span_id.data[8]);
		UE_LOG(LogSpatialEventTracer, Warning, TEXT("Span: %lu%lu, Type: %s, Message: %s, Timestamp: %lu"),
			span1, span2, *FString(Event.type), *FString(Event.message), Event.unix_timestamp_millis);

		if (Event.data != nullptr)
		{
			uint32_t DataFieldCount = Trace_EventData_GetFieldCount(Event.data);

			TArray<const char*> Keys;
			Keys.SetNumUninitialized(DataFieldCount);
			TArray<const char*> Values;
			Values.SetNumUninitialized(DataFieldCount);

			Trace_EventData_GetStringFields(Event.data, Keys.GetData(), Values.GetData());
			for (uint32_t i = 0; i < DataFieldCount; ++i)
			{
				UE_LOG(LogSpatialEventTracer, Warning, TEXT("%s : %s"), ANSI_TO_TCHAR(Keys[i]), ANSI_TO_TCHAR(Values[i]));
			}
		}

		break;
	}
	//case TRACE_ITEM_TYPE_SPAN:
	//{
	//	const Trace_Span& Span = Item->item.span;
	//	//UE_LOG(LogSpatialEventTracer, Warning, TEXT("Span: %s"), *FString(Span.id.data));
	//	unsigned long int span1 = *reinterpret_cast<const unsigned long int*>(&Span.id.data[0]);
	//	unsigned long int span2 = *reinterpret_cast<const unsigned long int*>(&Span.id.data[8]);
	//	UE_LOG(LogSpatialEventTracer, Warning, TEXT("Span: %ul%ul"), span1, span2);
	//	break;
	//}
	default:
	{
		break;
	}
	}
}

SpatialSpanId::SpatialSpanId(Trace_EventTracer* InEventTracer)
	: CurrentSpanId{}
	, EventTracer(InEventTracer)
{
	CurrentSpanId = Trace_EventTracer_AddSpan(EventTracer, nullptr, 0);
	Trace_EventTracer_SetActiveSpanId(EventTracer, CurrentSpanId);
}

SpatialSpanId::~SpatialSpanId()
{
	Trace_EventTracer_UnsetActiveSpanId(EventTracer);
}

SpatialEventTracer::SpatialEventTracer()
{
	Trace_EventTracer_Parameters parameters = {};
	parameters.callback = MyTraceCallback;
	EventTracer = Trace_EventTracer_Create(&parameters);
	Trace_EventTracer_Enable(EventTracer);
}

SpatialEventTracer::~SpatialEventTracer()
{
	Trace_EventTracer_Disable(EventTracer);
	Trace_EventTracer_Destroy(EventTracer);
}

SpatialSpanId SpatialEventTracer::CreateActiveSpan()
{
	return SpatialSpanId(EventTracer);
}

const Trace_EventTracer* SpatialEventTracer::GetWorkerEventTracer() const
{
	return EventTracer;
}

void SpatialEventTracer::TraceEvent(const SpatialGDKEvent& Event)
{
	Trace_SpanId CurrentSpanId = Trace_EventTracer_AddSpan(EventTracer, nullptr, 0);
	Trace_Event TraceEvent{ CurrentSpanId, 0, TCHAR_TO_ANSI(*Event.Message), TCHAR_TO_ANSI(*Event.Type), nullptr };

	if (Trace_EventTracer_ShouldSampleEvent(EventTracer, &TraceEvent))
	{
		Trace_EventData* EventData = Trace_EventData_Create();
		for (const auto& Elem : Event.Data)
		{
			const char* Key = TCHAR_TO_ANSI(*Elem.Key);
			const char* Value = TCHAR_TO_ANSI(*Elem.Value);
			Trace_EventData_AddStringFields(EventData, 1, &Key, &Value);
		}
		TraceEvent.data = EventData;
		Trace_EventTracer_AddEvent(EventTracer, &TraceEvent);
		Trace_EventData_Destroy(EventData);
	}
}

void SpatialGDK::SpatialEventTracer::TraceEvent(EventName Name, EventType Type, const AActor* Actor, const UFunction* Function)
{
	Trace_SpanId CurrentSpanId = Trace_EventTracer_AddSpan(EventTracer, nullptr, 0);
	Trace_Event TraceEvent{ CurrentSpanId, 0, TypeToString(Type), NameToString(Name), nullptr };
	if (Trace_EventTracer_ShouldSampleEvent(EventTracer, &TraceEvent))
	{
		Trace_EventData* EventData = Trace_EventData_Create();

		if (Actor != nullptr)
		{

			auto ActorName = StringCast<ANSICHAR>(*Actor->GetName());
			const char* ActorNameStr = ActorName.Get();
			Trace_EventData_AddStringFields(EventData, 1, &ActorKeyPtr, &ActorNameStr);

			auto ActorPosition = StringCast<ANSICHAR>(*Actor->GetActorTransform().GetTranslation().ToString());
			const char* ActorPositionStr = ActorPosition.Get();
			Trace_EventData_AddStringFields(EventData, 1, &ActorPositionKeyPtr, &ActorPositionStr);

		}

		if (Function != nullptr)
		{
			auto FunctionName = StringCast<ANSICHAR>(*Function->GetName());
			const char* FunctionNameStr = FunctionName.Get();
			Trace_EventData_AddStringFields(EventData, 1, &FunctionKeyPtr, &FunctionNameStr);
		}


		TraceEvent.data = EventData;
		Trace_EventTracer_AddEvent(EventTracer, &TraceEvent);
		Trace_EventData_Destroy(EventData);
	}
}

void SpatialGDK::SpatialEventTracer::TraceEvent(EventName Name, EventType Type, const AActor* Actor, ENetRole Role)
{
	Trace_SpanId CurrentSpanId = Trace_EventTracer_AddSpan(EventTracer, nullptr, 0);
	Trace_Event TraceEvent{ CurrentSpanId, 0, TypeToString(Type), NameToString(Name), nullptr };
	if (Trace_EventTracer_ShouldSampleEvent(EventTracer, &TraceEvent))
	{
		Trace_EventData* EventData = Trace_EventData_Create();

		if (Actor != nullptr)
		{
			auto ActorName = StringCast<ANSICHAR>(*Actor->GetName());
			const char* ActorNameStr = ActorName.Get();
			Trace_EventData_AddStringFields(EventData, 1, &ActorKeyPtr, &ActorNameStr);

			auto ActorPosition = StringCast<ANSICHAR>(*Actor->GetActorTransform().GetTranslation().ToString());
			const char* ActorPositionStr = ActorPosition.Get();
			Trace_EventData_AddStringFields(EventData, 1, &ActorPositionKeyPtr, &ActorPositionStr);
		}

		// Adding Role info
		{
			const char* NewRoleValue = nullptr;

			switch (Role)
			{
			case ROLE_SimulatedProxy:
				NewRoleValue = RoleNone;
				break;
			case ROLE_AutonomousProxy:
				NewRoleValue = RoleAutonomousProxy;
				break;
			case ROLE_Authority:
				NewRoleValue = RoleAuthority;
				break;
			default:
				NewRoleValue = RoleInvalid;
				break;
			}

			Trace_EventData_AddStringFields(EventData, 1, &NewRoleKeyPtr, &NewRoleValue);
		}

		TraceEvent.data = EventData;
		Trace_EventTracer_AddEvent(EventTracer, &TraceEvent);
		Trace_EventData_Destroy(EventData);
	}
}

void SpatialEventTracer::Enable()
{
	Trace_EventTracer_Enable(EventTracer);
}

void SpatialEventTracer::Disable()
{
	Trace_EventTracer_Disable(EventTracer);
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(Worker_RequestId RequestID, bool bSuccess)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "CommandResponse";
	Event.Data.Add("RequestID", FString::Printf(TEXT("%li"), RequestID));
	Event.Data.Add("Success", bSuccess ? TEXT("true") : TEXT("false"));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, const UObject* TargetObject, const UFunction* Function, Worker_CommandResponseOp ResponseOp)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "ComponentUpdate";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	if (TargetObject != nullptr)
	{
		Event.Data.Add("TargetObject", TargetObject->GetName());
	}
	Event.Data.Add("Function", Function->GetName());
	//Event.Data.Add("ResponseOp", ResponseOp);
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, const FString& Message, Worker_CreateEntityResponseOp ResponseOp)
{
	SpatialGDKEvent Event;
	Event.Message = Message;
	Event.Type = "CreateEntityOp";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	//Event.Data.Add("ResponseOp", FString::Printf(TEXT("%lu"), ResponseOp));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, const FString& Type, Worker_CommandResponseOp ResponseOp)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = Type;
	//Event.Data.Add("ResponseOp", ResponseOp);
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, const UObject* TargetObject, const UFunction* Function, TraceKey TraceId, Worker_RequestId RequestID)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "ComponentUpdate";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	if (TargetObject != nullptr)
	{
		Event.Data.Add("TargetObject", TargetObject->GetName());
	}
	Event.Data.Add("Function", Function->GetName());
	Event.Data.Add("TraceId", FString::Printf(TEXT("%i"), TraceId));
	Event.Data.Add("RequestID", FString::Printf(TEXT("%li"), RequestID));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, const FString& Type, Worker_RequestId RequestID)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = Type;
	Event.Data.Add("RequestID", FString::Printf(TEXT("%li"), RequestID));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, const UObject* TargetObject, Worker_ComponentId ComponentId)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "ComponentUpdate";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	if (TargetObject != nullptr)
	{
		Event.Data.Add("TargetObject", TargetObject->GetName());
	}
	Event.Data.Add("ComponentId", FString::Printf(TEXT("%u"), ComponentId));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, Worker_EntityId EntityId, Worker_RequestId RequestID)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "RetireEntity";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	Event.Data.Add("CreateEntityRequestId", FString::Printf(TEXT("%lu"), RequestID));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, VirtualWorkerId NewAuthoritativeWorkerId)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "AuthorityIntentUpdate";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	Event.Data.Add("NewAuthoritativeWorkerId", FString::Printf(TEXT("%u"), NewAuthoritativeWorkerId));
	return Event;
}

SpatialGDK::SpatialGDKEvent SpatialGDK::ConstructEvent(const AActor* Actor, Worker_RequestId CreateEntityRequestId)
{
	SpatialGDKEvent Event;
	Event.Message = "";
	Event.Type = "CreateEntity";
	if (Actor != nullptr)
	{
		Event.Data.Add("Actor", Actor->GetName());
		Event.Data.Add("Position", Actor->GetActorTransform().GetTranslation().ToString());
	}
	Event.Data.Add("CreateEntityRequestId", FString::Printf(TEXT("%li"), CreateEntityRequestId));
	return Event;
}
