
#define VC_EXTRALEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "../cdi/cdi.h"
#include "disklib.h"
#include "cJSON.h"

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

	for (DWORD i = 0; i < dwCount; i++)
	{
		cJSON *physicalDriveItem = cJSON_CreateObject();
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
		printf("            \"HWID\": \"%s\",\n", Ucs2ToUtf8(pdInfo[i].HwID));
		printf("            \"Model\": \"%s\",\n", Ucs2ToUtf8(pdInfo[i].HwName));
		printf("            \"Size\": \"%s\",\n", GetHumanSize(pdInfo[i].SizeInBytes, 1024));
		printf("            \"RemovableMedia\": \"%s\",\n", pdInfo[i].RemovableMedia ? "Yes" : "No");
		printf("            \"VendorId\": \"%s\",\n", pdInfo[i].VendorId);
		printf("            \"ProductId\": \"%s\",\n", pdInfo[i].ProductId);
		printf("            \"ProductRev\": \"%s\",\n", pdInfo[i].ProductRev);
		printf("            \"BusType\": \"%s\",\n", GetBusTypeName(pdInfo[i].BusType));
		printf("            \"PartitionTable\": \"%s\",\n", GetPartMapName(pdInfo[i].PartMap));
		switch(pdInfo[i].PartMap)
		{
		case PARTITION_STYLE_MBR:
			char mbr_signature[32] = {0};
			sprintf(mbr_signature, "%02X %02X %02X %02X",
				pdInfo[i].MbrSignature[0],
				pdInfo[i].MbrSignature[1],
				pdInfo[i].MbrSignature[2],
				pdInfo[i].MbrSignature[3]
			);
			cJSON_AddStringToObject(physicalDriveItem, "MbrSignature", mbr_signature);
			break;
		case PARTITION_STYLE_GPT:
			char gpt_guid[64] = {0};
			sprintf(gpt_guid, "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
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
		for (DWORD j = 0; j < pdInfo[i].VolCount; j++)
		{
			printf("                {\n");

			DISK_VOL_INFO* p = &pdInfo[i].VolInfo[j];
			// printf("\t%s\n", Ucs2ToUtf8(p->VolPath));
			printf("                    \"Volume\": \"%s\",\n", Ucs2ToUtf8(p->VolPath));
			printf("                    \"StartLBA\": %llu,\n", p->StartLba);
			printf("                    \"PartitionNumber\": %lu\n", p->PartNum);
			printf("                    \"PartitionType\": \"%s\",\n", Ucs2ToUtf8(p->PartType));
			printf("                    \"PartitionID\": \"%s\",\n", Ucs2ToUtf8(p->PartId));
			printf("                    \"BootIndicator\": \"%s\",\n", p->BootIndicator ? "Yes" : "No");
			printf("                    \"PartitionFlag\": \"%s\",\n", Ucs2ToUtf8(p->PartFlag));
			printf("                    \"Label\": \"%s\",\n", Ucs2ToUtf8(p->VolLabel));
			printf("                    \"FS\": \"%s\",\n", Ucs2ToUtf8(p->VolFs));
			printf("                    \"FreeSpace\": \"%s\",\n", GetHumanSize(p->VolFreeSpace.QuadPart, 1024));
			printf("                    \"TotalSpace\": \"%s\",\n", GetHumanSize(p->VolTotalSpace.QuadPart, 1024));
			printf("                    \"Usage\": \"%.2f%%\",\n", p->VolUsage);
			printf("                    \"MountPoint\": \"");
			for (WCHAR* q = p->VolNames; q[0] != L'\0'; q += wcslen(q) + 1)
			{
				printf("%s", Ucs2ToUtf8(q));
			}
			printf("\"\n");
			printf("                }%s\n", (j + 1) < pdInfo[i].VolCount ? "," : "");
		}

		printf("            ]\n");
		printf("%s", (i + 1) < dwCount ? "        },\n" : "        }\n");
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
