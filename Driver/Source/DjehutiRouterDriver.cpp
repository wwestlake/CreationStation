#include "DjehutiRouterDriver.h"

NTSTATUS DriverEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    UNREFERENCED_PARAMETER(Driver);

    const DJEHUTI_ROUTER_CABLE_SPEC cableSpec = DjehutiRouterMakeCableSpec();

    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&deviceAttributes);

    WDFDEVICE device = NULL;
    const NTSTATUS status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (!NT_SUCCESS(status))
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DjehutiRouter: WdfDeviceCreate failed with status 0x%08X\n", status));
        return status;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DjehutiRouter: device add completed for %ls -> %ls at %u Hz / %u channels\n",
        cableSpec.renderEndpoint.name,
        cableSpec.captureEndpoint.name,
        cableSpec.sampleRate,
        cableSpec.channelCount));

    return STATUS_SUCCESS;
}
