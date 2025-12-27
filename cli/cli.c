
#define VC_EXTRALEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "../cdi/cdi.h"
#include "disklib.h"
#include "cJSON.h"
#include "u_string.h"

#pragma comment(lib, "cdi.lib")
#pragma comment(lib, "setupapi.lib")

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
		printf("            \"SSD\"", pdInfo->Ssd ? "\"Yes\"" : "\"No\"");
		printf("            \"Serial\"", pdInfo->SerialNumber);
		return;
	}

	cdi_update_smart(cdiSmart, nIndex);

	ssd = cdi_get_bool(cdiSmart, nIndex, CDI_BOOL_SSD);
	printf("            \"SSD\"", ssd ? "\"Yes\"" : "\"No\"");

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_SN);
	printf("            \"Serial\"", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_FIRMWARE);
	printf("            \"Firmware\"", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_INTERFACE);
	printf("            \"Interface\"", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_TRANSFER_MODE_CUR);
	printf("            \"CurrentTransferMode\"", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_TRANSFER_MODE_MAX);
	printf("            \"MaxTransferMode\"", Ucs2ToUtf8(str));
	cdi_free_string(str);

	str = cdi_get_string(cdiSmart, nIndex, CDI_STRING_FORM_FACTOR);
	printf("            \"FormFactor\"", Ucs2ToUtf8(str));
	cdi_free_string(str);

	d = cdi_get_int(cdiSmart, nIndex, CDI_INT_LIFE);
	if (d >= 0)
		printf("            \"HealthStatus\": \"%s (%d%%)\",\n", cdi_get_health_status(cdi_get_int(cdiSmart, nIndex, CDI_INT_DISK_STATUS)), d);
	else
		printf("            \"HealthStatus\"", cdi_get_health_status(cdi_get_int(cdiSmart, nIndex, CDI_INT_DISK_STATUS)));

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

	// 创建一个json对象
	cJSON *diskInfo = cJSON_CreateObject();
	if (diskInfo == NULL) {
		fputs("create info object exception\n", stderr);
		return 1;
	}
    cJSON_AddStringToObject(diskInfo, "Version", cdi_get_version());

	dwCount = GetDriveInfoList(FALSE, &pdInfo);
	cJSON_AddNumberToObject(diskInfo, "DiskCount", dwCount);

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

		printf("        {\n");
		printf("            \"Index\": %lu,\n", pdInfo[i].Index);
		printf("            \"HWID\"", Ucs2ToUtf8(pdInfo[i].HwID));
		printf("            \"Model\"", Ucs2ToUtf8(pdInfo[i].HwName));
		printf("            \"Size\"", GetHumanSize(pdInfo[i].SizeInBytes, 1024));
		printf("            \"RemovableMedia\"", pdInfo[i].RemovableMedia ? "Yes" : "No");
		printf("            \"VendorId\"", pdInfo[i].VendorId);
		printf("            \"ProductId\"", pdInfo[i].ProductId);
		printf("            \"ProductRev\"", pdInfo[i].ProductRev);
		printf("            \"BusType\"", GetBusTypeName(pdInfo[i].BusType));
		printf("            \"PartitionTable\"", GetPartMapName(pdInfo[i].PartMap));
		switch(pdInfo[i].PartMap)
		{
		case PARTITION_STYLE_MBR:
			char mbr_signature[32] = {0};
			sprintf_s(mbr_signature, sizeof(mbr_signature), "%02X %02X %02X %02X",
				pdInfo[i].MbrSignature[0],
				pdInfo[i].MbrSignature[1],
				pdInfo[i].MbrSignature[2],
				pdInfo[i].MbrSignature[3]
			);
			cJSON_AddStringToObject(physicalDriveItem, "MbrSignature", mbr_signature);
			break;
		case PARTITION_STYLE_GPT:
			char gpt_guid[64] = {0};
			sprintf_s(gpt_guid, sizeof(gpt_guid), "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
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
			cJSON_AddStringToObject(physicalDriveItem, "GptGuid", gpt_guid);
			break;
		}

		PrintSmartInfo(cdiSmart, &pdInfo[i], GetSmartIndex(cdiSmart, pdInfo[i].Index));

		printf("            \"VolumeList\": [\n");
		cJSON *volumeList = cJSON_CreateArray();

		for (DWORD j = 0; j < pdInfo[i].VolCount; j++)
		{
			DISK_VOL_INFO* p = &pdInfo[i].VolInfo[j];

			cJSON *volumeItem = cJSON_CreateObject();
			cJSON_AddItemToArray(volumeList, volumeItem);
			cJSON_AddStringToObject(volumeItem, "Volume", Ucs2ToUtf8(p->VolPath));
			cJSON_AddNumberToObject(volumeItem, "StartLBA",  cJSON_CreateNumber(p->StartLba));
			cJSON_AddNumberToObject(volumeItem, "PartitionNumber", cJSON_CreateNumber(p->PartNum));
			cJSON_AddStringToObject(volumeItem, "PartitionType", Ucs2ToUtf8(p->PartType));
			cJSON_AddStringToObject(volumeItem, "PartitionID", Ucs2ToUtf8(p->PartId));
			cJSON_AddStringToObject(volumeItem, "BootIndicator", p->BootIndicator ? "Yes" : "No");
			cJSON_AddStringToObject(volumeItem, "PartitionFlag", Ucs2ToUtf8(p->PartFlag));
			cJSON_AddStringToObject(volumeItem, "Label", Ucs2ToUtf8(p->VolLabel));
			cJSON_AddStringToObject(volumeItem, "FS", Ucs2ToUtf8(p->VolFs));
			cJSON_AddStringToObject(volumeItem, "FreeSpace", GetHumanSize(p->VolFreeSpace.QuadPart, 1024));
			cJSON_AddStringToObject(volumeItem, "TotalSpace", GetHumanSize(p->VolTotalSpace.QuadPart, 1024));
			cJSON_AddStringToObject(volumeItem, "Usage", p->VolUsage);

			UString *mountPointValue;
			u_string_new(mountPointValue);
			for (WCHAR* q = p->VolNames; q[0] != L'\0'; q += wcslen(q) + 1)
			{
				u_string_join(mountPointValue, "%s", Ucs2ToUtf8(q));
				// printf("%s", Ucs2ToUtf8(q));
			}
			cJSON_AddStringToObject(volumeItem, "MountPoint", u_string_body(mountPointValue));
			u_string_free(mountPointValue);
		}
	}
	printf("    ]\n");
	printf("}\n");

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
