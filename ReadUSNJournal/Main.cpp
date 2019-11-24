#include <Windows.h>
#include <WinIoCtl.h>
#include <stdio.h>

#define BUF_LEN 4096

void FormatDateTime(PSYSTEMTIME ptm, LPTSTR szBuffer, int iBuffLen)
{
	SYSTEMTIME tmSys;
	if (ptm)
		tmSys = *ptm;
	else
		GetLocalTime(&tmSys);
	sprintf_s(szBuffer, iBuffLen,
		"%04d-%02d-%02d %02d:%02d:%02d.%03d",
		tmSys.wYear, tmSys.wMonth, tmSys.wDay,
		tmSys.wHour, tmSys.wMinute, tmSys.wSecond,
		tmSys.wMilliseconds);
}

int main()
{
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;

	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = &sd;

	//if (GetVolumeInformationA((std::string(drvname) + ":\\").c_str(), 0, 0, 0, &MaximumComponentLength, 0, FileSystemName, MAX_PATH + 1)
	//	&& 0 == strcmp(FileSystemName, "NTFS")) // �ж��Ƿ�Ϊ NTFS ��ʽ

	HANDLE hVol = INVALID_HANDLE_VALUE;
	hVol = CreateFileA(TEXT("\\\\.\\c:"),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&sa,
		OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING,
		NULL);

	if (hVol == INVALID_HANDLE_VALUE)
	{
		printf("CreateFile failed (%d)\n", GetLastError());
		return 0;
	}
	
	//DWORD dwRetBytes = 0;;
	//CREATE_USN_JOURNAL_DATA cujd;
	//BOOL bRET = DeviceIoControl(hVol,
	//	FSCTL_CREATE_USN_JOURNAL,
	//	&cujd,
	//	sizeof(cujd),
	//	NULL,// lpOutBuffer
	//	0,
	//	&dwRetBytes,
	//	NULL);

	//DELETE_USN_JOURNAL_DATA dusn;
	//dusn.UsnJournalID = UsnRecord->Usn;
	//dusn.DeleteFlags = USN_DELETE_FLAG_DELETE;
	//BOOL bDelRET = DeviceIoControl(hVol,
	//	FSCTL_DELETE_USN_JOURNAL,
	//	&dusn,
	//	sizeof(dusn),
	//	NULL,// lpOutBuffer
	//	0,
	//	&dwBytes,
	//	NULL);
	//printf("DELETE_USN %I64x  %s \n", UsnRecord->Usn, bDelRET ? "success" : "failed");

	USN_JOURNAL_DATA qujd = { 0 };
	DWORD dwBytes = 0;;
	BOOL bRet = DeviceIoControl(hVol,
		FSCTL_QUERY_USN_JOURNAL,
		NULL,
		0,
		&qujd,
		sizeof(qujd),
		&dwBytes,
		NULL);
	if (!bRet)
	{
		printf("Query journal failed (%d)\n", GetLastError());
		//GetTickCount();
		return 0;
	}

	
	//READ_USN_JOURNAL_DATA ReadData = {0};
	//ReadData.ReasonMask = 0xFFFFFFFF;
	//ReadData.UsnJournalID = JournalData.UsnJournalID;
	//READ_USN_JOURNAL_DATA_V1 ReadData = { 0 };
	//ReadData.ReasonMask = 0xFFFFFFFF;
	//ReadData.UsnJournalID = JournalData.UsnJournalID;
	READ_USN_JOURNAL_DATA_V0 rujd = { 0 };
	rujd.ReasonMask = USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE |
		USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME;
	rujd.UsnJournalID = qujd.UsnJournalID;

	CHAR Buffer[BUF_LEN];
	DWORD BytesReturned = 0;
	while (TRUE)
	{
		memset(Buffer, 0, BUF_LEN);
		bRet = DeviceIoControl(hVol,
			FSCTL_READ_USN_JOURNAL,
			&rujd,
			sizeof(rujd),
			Buffer,
			BUF_LEN,
			&BytesReturned,
			NULL);
		if (!bRet)
		{
			printf("Read journal failed (%d)\n", GetLastError());
			break;
		}
			
		DWORD dwRetBytes = BytesReturned - sizeof(USN);
		// Find the first record
		PUSN_RECORD UsnRecord = (PUSN_RECORD)(((PUCHAR)Buffer) + sizeof(USN));
		// This loop could go on for a long time, given the current buffer size.
		while (dwRetBytes > 0)
		{
			printf("RecordLength: %d\n", UsnRecord->RecordLength);
			printf("MajorVersion: %d\n", UsnRecord->MajorVersion);
			printf("MinorVersion: %d\n", UsnRecord->MinorVersion);
			printf("FileReferenceNumber: %I64x\n", UsnRecord->FileReferenceNumber);
			printf("ParentFileReferenceNumber: %I64x\n", UsnRecord->ParentFileReferenceNumber);
			printf("USN: %I64x\n", UsnRecord->Usn);
			FILETIME filetime;
			filetime.dwLowDateTime = UsnRecord->TimeStamp.LowPart;
			filetime.dwHighDateTime = UsnRecord->TimeStamp.HighPart;
			SYSTEMTIME sysTime;
			FileTimeToSystemTime(&filetime, &sysTime);
			CHAR szBuff[64] = { 0 };
			FormatDateTime(&sysTime, szBuff, 64);
			printf("TimeStamp: %s\n", szBuff);
			printf("Reason: 0x%x\n", UsnRecord->Reason);
			printf("File name: %.*S\n", UsnRecord->FileNameLength / 2, UsnRecord->FileName);
			printf("-------------------------------------------------------------------------------------\n");

			dwRetBytes -= UsnRecord->RecordLength;
			// Find the next record
			UsnRecord = (PUSN_RECORD)( (PCHAR)UsnRecord + UsnRecord->RecordLength);
		}
		// Update starting USN for next call
		rujd.StartUsn = *(USN *)&Buffer;
		Sleep(2000);
	}

	CloseHandle(hVol);

	return 0;
}


//typedef struct {
//
//	DWORD RecordLength;                //����USN��¼����
//	WORD   MajorVersion;                //���汾
//	WORD   MinorVersion;                //�ΰ汾
//	DWORDLONG FileReferenceNumber;    //�ļ�������
//	DWORDLONG ParentFileReferenceNumber;//���ļ�������
//	USN Usn;                        //USN��һ��Ϊint64���ͣ�
//	LARGE_INTEGER TimeStamp;            //ʱ���
//	DWORD Reason;                    //ԭ��
//	DWORD SourceInfo;                //Դ��Ϣ
//	DWORD SecurityId;                //��ȫ
//	DWORD FileAttributes;                //�ļ����ԣ��ļ���Ŀ¼��
//	WORD   FileNameLength;            //�ļ�������
//	WORD   FileNameOffset;                //�ļ���ƫ����
//	WCHAR FileName[1];                //�ļ�����һλ��ָ��
//
//} USN_RECORD, *PUSN_RECORD;

//typedef struct {
//
//	DWORDLONG UsnJournalID;        //USN��־ID
//	USN FirstUsn;                //��һ��USN��¼��λ��
//	USN NextUsn;                //��һ��USN��¼��Ҫд���λ��
//	USN LowestValidUsn;            //��С����Ч��USN��FistUSNС�ڸ�ֵ��
//	USN MaxUsn;                    //USN���ֵ
//	DWORDLONG MaximumSize;        //USN��־����С����Byte�㣩
//	DWORDLONG AllocationDelta;        //USN��־ÿ�δ������ͷŵ��ڴ��ֽ���
//
//} USN_JOURNAL_DATA, *PUSN_JOURNAL_DATA;

//typedef struct {
//
//	DWORDLONG StartFileReferenceNumber;//��ʼ�ļ�����������һ�ε��ñ���Ϊ0
//	USN LowUsn;    //��СUSN����һ�ε��ã����Ϊ0
//	USN HighUsn;//���USN
//
//} MFT_ENUM_DATA, *PMFT_ENUM_DATA;

//typedef struct {
//
//	USN StartUsn;//�����USN��¼��ʼλ�ã�����һ�ζ�ȡUSN��־��LastUsnֵ��
//	DWORD ReasonMask;    //ԭ���ʶ
//	DWORD ReturnOnlyOnClose;    //ֻ���ڼ�¼�ر�ʱ�ŷ���
//	DWORDLONG Timeout;        //�ӳ�ʱ��
//	DWORDLONG BytesToWaitFor;//��USN��־��С���ڸ�ֵʱ����
//	DWORDLONG UsnJournalID;    //USN��־ID
//
//} READ_USN_JOURNAL_DATA, *PREAD_USN_JOURNAL_DATA;

//typedef struct {
//
//	DWORDLONG MaximumSize;//NTFS�ļ�ϵͳ�����USN��־������С���ֽڣ�
//	DWORDLONG AllocationDelta;    //USN��־ÿ�δ������ͷŵ��ڴ��ֽ���
//
//} CREATE_USN_JOURNAL_DATA, *PCREATE_USN_JOURNAL_DATA;

//typedef struct {
//
//	DWORDLONG UsnJournalID;//USN��־ID
//	DWORD DeleteFlags;        //ɾ����־
//
//} DELETE_USN_JOURNAL_DATA, *PDELETE_USN_JOURNAL_DATA;