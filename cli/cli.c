
#define VC_EXTRALEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "../cdi/cdi.h"
#include "disklib.h"
#include "cJSON.h"

#pragma comment(lib, "cdi.lib")
#pragma comment(lib, "setupapi.lib")


// ----------------------------------------------------------------------------------
// ----------------------------------X-String start----------------------------------
// ----------------------------------------------------------------------------------

// 默认初始容量
#define XS_DEFAULT_CAPACITY 256

// 动态字符串结构体（对外暴露，方便用户操作）
typedef struct XString {
    char *data; // 字符串数据
    size_t length; // 有效字符长度（不含'\0'）
    size_t capacity; // 总容量（含'\0'）
} XString;

/**
 * @brief 创建新的动态字符串
 * @param value 初始字符串（NULL 则创建空字符串）
 * @return 初始化后的 XString 实例
 */
XString xs_new(const char *value) {
    // 初始化为空
    XString sb = {NULL, 0, 0};
    if (value == NULL) {
        sb.length = 0;
        sb.capacity = XS_DEFAULT_CAPACITY;
        sb.data = malloc(sb.capacity * sizeof(char));
        if (!sb.data) {
            sb.length = 0;
            sb.capacity = 0;
            return sb;
        }
        sb.data[0] = '\0';
        return sb;
    }

    sb.length = strlen(value);
    sb.capacity = (sb.length + 1 > XS_DEFAULT_CAPACITY) ? sb.length + 1 : XS_DEFAULT_CAPACITY;
    sb.data = malloc(sb.capacity * sizeof(char));
    if (!sb.data) {
        sb.length = 0;
        sb.capacity = 0;
        return sb;
    }

    // strcpy(sb.data, value);
    strcpy_s(sb.data, sizeof(sb.data), value);
    return sb;
}

/**
 * @brief 确保动态字符串有足够的容量
 * @param x_string 动态字符串指针
 * @param new_capacity 所需最小容量
 * @return 0 成功，-1 失败（参数错误/内存分配失败）
 */
int xs_ensure_capacity(XString *x_string, const size_t new_capacity) {
    if (!x_string || !x_string->data) {
        return -1;
    }
    if (new_capacity > x_string->capacity) {
        const size_t new_cap = new_capacity + (new_capacity / 2);
        char *new_data = realloc(x_string->data, new_cap);
        if (!new_data) {
            return -1;
        }
        x_string->data = new_data;
        x_string->capacity = new_cap;
    }
    return 0;
}

/**
 * @brief 追加格式化字符串到动态字符串
 * @param x_string 动态字符串指针
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 0 成功，-1 失败
 */
int xs_append_format(XString *x_string, const char *format, ...) {
    if (!x_string || !format) {
        return -1;
    }

    va_list args;
    va_start(args, format);

    int needed_size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed_size < 0) {
        return -1;
    }

    size_t new_len = x_string->length + needed_size + 1;
    if (new_len > x_string->capacity) {
        if (xs_ensure_capacity(x_string, new_len) != 0) {
            return -1;
        }
    }

    va_start(args, format);
    int result = vsnprintf(x_string->data + x_string->length, needed_size + 1, format, args);
    va_end(args);

    if (result < 0) {
        return -1;
    }

    x_string->length += needed_size;

    return 0;
}

/**
 * @brief 释放动态字符串内存
 * @param x_string 动态字符串指针
 */
void xs_free(XString *x_string) {
    if (x_string && x_string->data) {
        free(x_string->data);
        x_string->data = NULL;
        x_string->length = 0;
        x_string->capacity = XS_DEFAULT_CAPACITY;
    }
}
// --------------------------------------------------------------------------------
// ----------------------------------X-String end----------------------------------
// --------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------
// ----------------------------------convert start----------------------------------
// ---------------------------------------------------------------------------------
// 封装成通用函数：输入UINT64值，输出字符串缓冲区
void uint64_to_string(uint64_t num, char *result, size_t buffer_size) {
	snprintf(result, buffer_size, "%" PRIu64, num);
}
// -------------------------------------------------------------------------------
// ----------------------------------convert end----------------------------------
// -------------------------------------------------------------------------------

static INT
GetSmartIndex(CDI_SMART* cdiSmart, DWORD dwId)
{
	INT nCount = cdi_get_disk_count(cdiSmart);
	for (INT i = 0; i < nCount; i++)
	{
		if (cdi_get_int(cdiSmart, i, CDI_INT_DISK_ID) == (INT)dwId)
			return i;
	}
	return -1;
}

static VOID
PrintSmartInfo(CDI_SMART* cdiSmart, PHY_DRIVE_INFO* pdInfo, INT nIndex)
{
	INT d;
	DWORD n;
	WCHAR* str;
	BOOL ssd;
	BYTE id;

	if (nIndex < 0)
	{
		printf("            \"SSD\": \"%s\",\n", pdInfo->Ssd ? "\"Yes\"" : "\"No\"");
		printf("            \"Serial\": \"%s\",\n", pdInfo->SerialNumber);
		return;
	}

	cdi_update_smart(cdiSmart, nIndex);

	ssd = cdi_get_bool(cdiSmart, nIndex, CDI_BOOL_SSD);
	printf("            \"SSD\": \"%s\",\n", ssd ? "\"Yes\"" : "\"No\"");

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_SN);
	printf("            \"Serial\": \"%s\",\n", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_FIRMWARE);
	printf("            \"Firmware\": \"%s\",\n", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_INTERFACE);
	printf("            \"Interface\": \"%s\",\n", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_TRANSFER_MODE_CUR);
	printf("            \"CurrentTransferMode\": \"%s\",\n", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_TRANSFER_MODE_MAX);
	printf("            \"MaxTransferMode\": \"%s\",\n", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_FORM_FACTOR);
	printf("            \"FormFactor\": \"%s\",\n", Ucs2ToUtf8(str));
	cdi_free_string(str);

	d = cdi_get_int(cdiSmart, nIndex, CDI_INT_LIFE);
	if (d >= 0)
		printf("            \"HealthStatus\": \"%s (%d%%)\",\n", cdi_get_health_status(cdi_get_int(cdiSmart, nIndex, CDI_INT_DISK_STATUS)), d);
	else
		printf("            \"HealthStatus\": \"%s\",\n", cdi_get_health_status(cdi_get_int(cdiSmart, nIndex, CDI_INT_DISK_STATUS)));

	printf("            \"Temperature:\" \"%d (C)\",\n", cdi_get_int(cdiSmart, nIndex, CDI_INT_TEMPERATURE));

	str = cdi_get_smart_format(cdiSmart, nIndex);
	Ucs2ToUtf8(str);
	printf("            \"StatusRawValue\": ");
	cdi_free_string(str);

	n = cdi_get_dword(cdiSmart, nIndex, CDI_DWORD_ATTR_COUNT);
	for (INT j = 0; j < (INT)n; j++)
	{
		id = cdi_get_smart_id(cdiSmart, nIndex, j);
		if (id == 0x00)
			continue;
		str = cdi_get_smart_value(cdiSmart, nIndex, j, FALSE);
		printf("\t%02X %7s %-24s",
			id,
			cdi_get_health_status(cdi_get_smart_status(cdiSmart, nIndex, j)),
			Ucs2ToUtf8(str));
		printf(" %s\n", Ucs2ToUtf8(cdi_get_smart_name(cdiSmart, nIndex, id)));
		cdi_free_string(str);
	}
}

int main(int argc, char* argv[])
{
	DWORD dwCount;
	CDI_SMART* cdiSmart;
	PHY_DRIVE_INFO* pdInfo;

	(void)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	cdiSmart = cdi_create_smart();

	dwCount = GetDriveInfoList(FALSE, &pdInfo);

	cJSON *diskInfo = cJSON_CreateObject();
	cJSON_AddNumberToObject(diskInfo, "DiskCount", dwCount);
	cJSON_AddStringToObject(diskInfo, "Version", cdi_get_version());

	cdi_init_smart(cdiSmart, CDI_FLAG_DEFAULT);

	// 创建物理硬盘列表
	cJSON *physicalDriveList = cJSON_CreateArray();
	cJSON_AddItemToObject(diskInfo, "PhysicalDriveList", physicalDriveList);

	for (DWORD i = 0; i < dwCount; i++)
	{
		cJSON *physicalDriveItem = cJSON_CreateObject();
		cJSON_AddItemToArray(physicalDriveList, physicalDriveItem);

		cJSON_AddNumberToObject(physicalDriveItem, "Index", pdInfo[i].Index);
		cJSON_AddStringToObject(physicalDriveItem, "HWID", Ucs2ToUtf8(pdInfo[i].HwID));
		cJSON_AddStringToObject(physicalDriveItem, "Model", Ucs2ToUtf8(pdInfo[i].HwName));
		cJSON_AddStringToObject(physicalDriveItem, "Size", GetHumanSize(pdInfo[i].SizeInBytes, 1024));
		cJSON_AddStringToObject(physicalDriveItem, "RemovableMedia", pdInfo[i].RemovableMedia ? "Yes" : "No");
		cJSON_AddStringToObject(physicalDriveItem, "VendorId", pdInfo[i].VendorId);
		cJSON_AddStringToObject(physicalDriveItem, "ProductId", pdInfo[i].ProductId);
		cJSON_AddStringToObject(physicalDriveItem, "ProductRev", pdInfo[i].ProductRev);
		cJSON_AddStringToObject(physicalDriveItem, "BusType", GetBusTypeName(pdInfo[i].BusType));
		cJSON_AddStringToObject(physicalDriveItem, "PartitionTable", GetPartMapName(pdInfo[i].PartMap));

		switch(pdInfo[i].PartMap)
		{
		case PARTITION_STYLE_MBR:
			XString mbrSignature = xs_new("");
			xs_append_format(&mbrSignature, "%02X %02X %02X %02X",
				pdInfo[i].MbrSignature[0],
				pdInfo[i].MbrSignature[1],
				pdInfo[i].MbrSignature[2],
				pdInfo[i].MbrSignature[3]
			);
			cJSON_AddStringToObject(physicalDriveItem, "MbrSignature", mbrSignature.data);
			xs_free(&mbrSignature);
			break;
		case PARTITION_STYLE_GPT:
			XString gptGuid = xs_new("");
			xs_append_format(&gptGuid, "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
				pdInfo[i].GptGuid[0],
				pdInfo[i].GptGuid[1],
				pdInfo[i].GptGuid[2],
				pdInfo[i].GptGuid[3],
				pdInfo[i].GptGuid[4],
				pdInfo[i].GptGuid[5],
				pdInfo[i].GptGuid[6],
				pdInfo[i].GptGuid[7],
				pdInfo[i].GptGuid[8],
				pdInfo[i].GptGuid[9],
				pdInfo[i].GptGuid[10],
				pdInfo[i].GptGuid[11],
				pdInfo[i].GptGuid[12],
				pdInfo[i].GptGuid[13],
				pdInfo[i].GptGuid[14],
				pdInfo[i].GptGuid[15]
			);
			cJSON_AddStringToObject(physicalDriveItem, "GptGuid", gptGuid.data);
			xs_free(&gptGuid);
			break;
		}

		PrintSmartInfo(cdiSmart, &pdInfo[i], GetSmartIndex(cdiSmart, pdInfo[i].Index));

		cJSON *volumeList = cJSON_CreateArray();

		cJSON_AddItemToObject(physicalDriveItem, "VolumeList", volumeList);

		for (DWORD j = 0; j < pdInfo[i].VolCount; j++)
		{
			DISK_VOL_INFO* p = &pdInfo[i].VolInfo[j];

			// char startLba[21];
			// uint64_to_string(p->StartLba, startLba, sizeof(startLba));

			cJSON *volumeItem = cJSON_CreateObject();
			cJSON_AddItemToArray(volumeList, volumeItem);

			cJSON_AddStringToObject(volumeItem, "Volume", Ucs2ToUtf8(p->VolPath));
			cJSON_AddNumberToObject(volumeItem, "StartLba", p->StartLba);
			cJSON_AddNumberToObject(volumeItem, "PartitionNumber", p->PartNum);
			cJSON_AddStringToObject(volumeItem, "PartitionType", Ucs2ToUtf8(p->PartType));
			cJSON_AddStringToObject(volumeItem, "PartitionID", Ucs2ToUtf8(p->PartId));
			cJSON_AddStringToObject(volumeItem, "BootIndicator", p->BootIndicator ? "Yes" : "No");
			cJSON_AddStringToObject(volumeItem, "PartitionFlag", Ucs2ToUtf8(p->PartFlag));
			cJSON_AddStringToObject(volumeItem, "Label", Ucs2ToUtf8(p->VolLabel));
			cJSON_AddStringToObject(volumeItem, "FS", Ucs2ToUtf8(p->VolFs));
			cJSON_AddStringToObject(volumeItem, "FreeSpace", GetHumanSize(p->VolFreeSpace.QuadPart, 1024));
			cJSON_AddStringToObject(volumeItem, "TotalSpace", GetHumanSize(p->VolTotalSpace.QuadPart, 1024));
			cJSON_AddNumberToObject(volumeItem, "Usage", p->VolUsage);

			XString mountPoint = xs_new("");
			for (WCHAR* q = p->VolNames; q[0] != L'\0'; q += wcslen(q) + 1)
			{
				xs_append_format(&mountPoint, "%s", Ucs2ToUtf8(q));
			}
			cJSON_AddStringToObject(volumeItem, "MountPoint", mountPoint.data);
			xs_free(&mountPoint);
		}
	}

	cdi_destroy_smart(cdiSmart);
	DestoryDriveInfoList(pdInfo, dwCount);
	CoUninitialize();

	// 将 JSON 结构转换为格式化的字符串
	char *diskInfoJson = cJSON_Print(diskInfo);
	if (diskInfoJson == NULL) {
		fputs("format output exception\n", stderr);
		cJSON_Delete(diskInfo);
		return 1;
	}
	printf("%s", diskInfoJson);
	free(diskInfoJson);
	cJSON_Delete(diskInfo);

	return 0;
}
