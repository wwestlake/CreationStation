#include "DjehutiRouterDriver.h"

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    const DJEHUTI_ROUTER_CABLE_SPEC cableSpec = DjehutiRouterMakeCableSpec();

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, DriverEvtDeviceAdd);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDFDRIVER driver = NULL;
    const NTSTATUS status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, &driver);

    if (!NT_SUCCESS(status))
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DjehutiRouter: WdfDriverCreate failed with status 0x%08X\n", status));
        return status;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DjehutiRouter: DriverEntry completed for %u Hz, %u channel cable (%ls / %ls)\n",
        cableSpec.sampleRate,
        cableSpec.channelCount,
        cableSpec.renderEndpoint.name,
        cableSpec.captureEndpoint.name));

    return STATUS_SUCCESS;
}
