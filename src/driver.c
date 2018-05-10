#include <ntifs.h>
#include <ntstrsafe.h>
#include "driver_codes.h"
#include "driver_config.h"

// Copies virtual memory from one process to another.
NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
	IN PEPROCESS FromProcess,
	IN PVOID FromAddress,
	IN PEPROCESS ToProcess,
	OUT PVOID ToAddress,
	IN SIZE_T BufferSize,
	IN KPROCESSOR_MODE PreviousMode,
	OUT PSIZE_T NumberOfBytesCopied
);

// Forward declaration for suppressing code analysis warnings.
DRIVER_INITIALIZE DriverEntry;

// Dispatch function.
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH DriverDispatch;

// Performs a memory copy request.
NTSTATUS DriverCopy(IN PDRIVER_COPY_MEMORY copy) {
	NTSTATUS status = STATUS_SUCCESS;
	PEPROCESS process;

	status = PsLookupProcessByProcessId((HANDLE)copy->ProcessId, &process);

	if (NT_SUCCESS(status)) {
		PEPROCESS sourceProcess, targetProcess;
		PVOID sourcePtr, targetPtr;

		if (copy->Write == FALSE) {
			sourceProcess = process;
			targetProcess = PsGetCurrentProcess();
			sourcePtr = (PVOID)copy->Target;
			targetPtr = (PVOID)copy->Source;
		} else {
			sourceProcess = PsGetCurrentProcess();
			targetProcess = process;
			sourcePtr = (PVOID)copy->Source;
			targetPtr = (PVOID)copy->Target;
		}

		SIZE_T bytes;
		status = MmCopyVirtualMemory(sourceProcess, sourcePtr, targetProcess, targetPtr, copy->Size, KernelMode, &bytes);

		ObDereferenceObject(process);
	}

	return status;
}

// Handles a IRP request.
NTSTATUS DriverDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
	PVOID ioBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

	if (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
		ULONG ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

		if (ioControlCode == IOCTL_DRIVER_COPY_MEMORY) {
			if (ioBuffer && inputLength >= sizeof(DRIVER_COPY_MEMORY)) {
				Irp->IoStatus.Status = DriverCopy((PDRIVER_COPY_MEMORY)ioBuffer);
				Irp->IoStatus.Information = sizeof(DRIVER_COPY_MEMORY);
			} else {
				Irp->IoStatus.Status = STATUS_INFO_LENGTH_MISMATCH;
			}
		} else {
			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}
	}

	NTSTATUS status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

// Unloads the driver.
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING dosDeviceName;
	RtlUnicodeStringInit(&dosDeviceName, DRIVER_DOS_DEVICE_NAME);

	IoDeleteSymbolicLink(&dosDeviceName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

// Entry point for the driver.
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING deviceName;
	RtlUnicodeStringInit(&deviceName, DRIVER_DEVICE_NAME);

	PDEVICE_OBJECT deviceObject = NULL;
	status = IoCreateDevice(DriverObject, 0, &deviceName, DRIVER_DEVICE_TYPE, 0, FALSE, &deviceObject);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDispatch;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatch;
	DriverObject->DriverUnload = DriverUnload;

	UNICODE_STRING dosDeviceName;
	RtlUnicodeStringInit(&dosDeviceName, DRIVER_DOS_DEVICE_NAME);

	status = IoCreateSymbolicLink(&dosDeviceName, &deviceName);

	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(deviceObject);
	}

	return status;
}
