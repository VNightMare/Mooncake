#include "cuda_alike.h"
#include <sys/types.h>

#include <iostream>

enum class MemoryBackendType { use_aclmalloc, use_aclmallocphysical, unknown };

extern "C" {

// 通过调用接口申请物理内存判断是否支持Fabric Mem
MemoryBackendType mc_probe_ub_fabric_support(int device_id) {
    // aclDevice dev;
    // CUresult res = cuDeviceGet(&dev, device_id);
    // if (res != ACL_ERROR_NONE) {
    //     return MemoryBackendType::unknown;
    // }

    // Check device attribute first
    // int fabric_attr = 0;
    // res = cuDeviceGetAttribute(
    //     &fabric_attr, CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_FABRIC_SUPPORTED, dev);
    // if (res != ACL_ERROR_NONE || !fabric_attr) {
    //     return MemoryBackendType::use_aclmalloc;
    // }

    aclError res = aclrtSetDevice(device_id);
    if (res != ACL_ERROR_NONE) {
        std::cerr << "Set device failed: " << device_id;
        return MemoryBackendType::unknown;
    }
    aclrtPhysicalMemProp prop = {};
    prop.handleType = ACL_MEM_HANDLE_TYPE_NONE;
    prop.allocationType = ACL_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = ACL_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = device_id;
    prop.memAttr = ACL_HBM_MEM_HUGE;
    // prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_FABRIC;  // require fabric

    aclrtDrvMemHandle handle;
    size_t size = 2097152;

    res = aclrtMallocPhysical(&handle, size, &prop, 0);

    if (res == ACL_ERROR_NONE) {
        aclrtFreePhysical(handle);  // success → clean up
        return MemoryBackendType::use_aclmallocphysical;
    } else {
        return MemoryBackendType::use_aclmalloc;
    }
}

// 是否需要入参指定prop.memAttr?
void *mc_ub_fabric_malloc(ssize_t size, int device) {
    size_t granularity = 0;
    aclrtPhysicalMemProp prop = {};
    aclrtDrvMemHandle handle;
    void *ptr = nullptr;

    prop.handleType = ACL_MEM_HANDLE_TYPE_NONE;
    prop.allocationType = ACL_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = ACL_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = device;
    prop.memAttr = ACL_HBM_MEM_HUGE;
    prop.reserve = 0;

    aclError result = aclrtMemGetAllocationGranularity(
        &prop, ACL_RT_MEM_ALLOC_GRANULARITY_MINIMUM, &granularity);
    if (result != ACL_ERROR_NONE) {
        std::cerr << "aclrtMemGetAllocationGranularity failed: " << result;
        return nullptr;
    }
    // fix size
    size = (size + granularity - 1) & ~(granularity - 1);
    if (size == 0) {
        size = granularity;
    }
    result = aclrtMallocPhysical(&handle, size, &prop, 0);
    if (result != ACL_ERROR_NONE) {
        std::cerr << "aclrtMallocPhysical failed: " << result;
        return nullptr;
    }
    uint64_t page_type = 1;
    result =
        aclrtReserveMemAddress(&ptr, size, granularity, nullptr, page_type);
    if (result != ACL_ERROR_NONE) {
        std::cerr << "aclrtReserveMemAddress failed: " << result;
        (void)aclrtFreePhysical(handle);
        return nullptr;
    }
    result = aclrtMapMem(ptr, size, 0, handle, 0);
    if (result != ACL_ERROR_NONE) {
        std::cerr << "aclrtMapMem failed: " << result;
        (void)aclrtReleaseMemAddress(ptr, size);
        (void)aclrtFreePhysical(handle);
        return nullptr;
    }

    // 是否需要set access？
    // int device_count;
    // cudaGetDeviceCount(&device_count);
    // CUmemAccessDesc accessDesc[device_count];
    // for (int idx = 0; idx < device_count; ++idx) {
    //     accessDesc[idx].location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    //     accessDesc[idx].location.id = idx;
    //     accessDesc[idx].flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
    // }
    // result = cuMemSetAccess(ptr, size, accessDesc, device_count);
    // if (result != ACL_ERROR_NONE) {
    //     std::cerr << "cuMemSetAccess failed: " << result;
    //     aclrtUnmapMem(ptr, size);
    //     aclrtReleaseMemAddress(ptr, size);
    //     aclrtFreePhysical(handle);
    //     return nullptr;
    // }
    return ptr;
}

void mc_ub_fabric_free(void *ptr, int device) {
    aclrtDrvMemHandle handle;
    if (!ptr) {
        return;
    }
    auto result = aclrtMemRetainAllocationHandle(&handle, ptr);
    if (result != ACL_ERROR_NONE) {
        std::cerr << "aclrtMemRetainAllocationHandle failed: " << result
                  << "\n";
        return;
    } else {
        (void)aclrtUnmapMem(ptr);
        (void)aclrtReleaseMemAddress(ptr);
    }
    (void)aclrtFreePhysical(handle);
}
}
