// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "Containers/Queue.h"
#include "SpatialCommonTypes.h"
#include "SpatialConstants.h"

#include <WorkerSDK/improbable/c_worker.h>
#include <WorkerSDK/improbable/c_schema.h>

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialVirtualWorkerTranslator, Log, All)

class UAbstractLBStrategy;
class USpatialStaticComponentView;
class USpatialReceiver;
class USpatialWorkerConnection;

class SPATIALGDK_API SpatialVirtualWorkerTranslator
{
public:
	SpatialVirtualWorkerTranslator();

	void Init(UAbstractLBStrategy* InLoadBalanceStrategy,
		USpatialStaticComponentView* InStaticComponentView,
		USpatialReceiver* InReceiver,
		USpatialWorkerConnection* InConnection,
		PhysicalWorkerName InPhysicalWorkerName);

	// Returns true if the Translator has received the information needed to map virtual workers to physical workers.
	// Currently that is only the number of virtual workers desired.
	bool IsReady() const { return bIsReady; }

	void AddVirtualWorkerIds(const TSet<VirtualWorkerId>& InVirtualWorkerIds);
	VirtualWorkerId GetLocalVirtualWorkerId() const { return LocalVirtualWorkerId; }
	PhysicalWorkerName GetLocalPhysicalWorkerName() const { return LocalPhysicalWorkerName; }

	// Returns the name of the worker currently assigned to VirtualWorkerId id or nullptr if there is
	// no worker assigned.
	// TODO(harkness): Do we want to copy this data? Otherwise it's only guaranteed to be valid until
	// the next mapping update.
	const PhysicalWorkerName* GetPhysicalWorkerForVirtualWorker(VirtualWorkerId Id) const;

	// On receiving a version of the translation state, apply that to the internal mapping.
	void ApplyVirtualWorkerManagerData(Schema_Object* ComponentObject);

	// Authority may change on one of two components we care about:
	// 1) The translation component, in which case this worker is now authoritative on the virtual to physical worker translation.
	// 2) The ACL component for some entity, in which case this worker is now authoritative for the entity and will be
	// responsible for updating the ACL in the future if this worker loses authority.
	void AuthorityChanged(const Worker_AuthorityChangeOp& AuthChangeOp);

private:
	TWeakObjectPtr<UAbstractLBStrategy> LoadBalanceStrategy;
	TWeakObjectPtr<USpatialStaticComponentView> StaticComponentView;
	TWeakObjectPtr<USpatialReceiver> Receiver;
	TWeakObjectPtr<USpatialWorkerConnection> Connection;

	TMap<VirtualWorkerId, PhysicalWorkerName> VirtualToPhysicalWorkerMapping;
	TQueue<VirtualWorkerId> UnassignedVirtualWorkers;

	bool bWorkerEntityQueryInFlight;

	bool bIsReady;

	// The WorkerId of this worker, for logging purposes.
	PhysicalWorkerName LocalPhysicalWorkerName;
	VirtualWorkerId LocalVirtualWorkerId;

	// Serialization and deserialization of the mapping.
	void ApplyMappingFromSchema(Schema_Object* Object);
	void WriteMappingToSchema(Schema_Object* Object) const;
	bool IsValidMapping(Schema_Object* Object) const;

	// The following methods are used to query the Runtime for all worker entities and update the mapping
	// based on the response.
	void QueryForWorkerEntities();
	void WorkerEntityQueryDelegate(const Worker_EntityQueryResponseOp& Op);
	void ConstructVirtualWorkerMappingFromQueryResponse(const Worker_EntityQueryResponseOp& Op);
	void SendVirtualWorkerMappingUpdate();

	void AssignWorker(const PhysicalWorkerName& WorkerId);

	void UpdateMapping(VirtualWorkerId Id, PhysicalWorkerName Name);
};