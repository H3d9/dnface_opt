#include <Windows.h>
#include <vector>
#include <string>
#include "kdriver.h"
#include "win32utility.h"

KernelDriver& driver = KernelDriver::getInstance();
win32SystemManager& systemMgr = win32SystemManager::getInstance();


bool patch(DWORD pid) {

	std::vector<ULONG64> rips;
	std::vector<ULONG64> executeRange;

	printf("����ACE-GDPServer64.dllģ��...");
	if (!driver.searchVad(pid, executeRange, L"ACE-GDPServer64.dll")) {
		printf("fail\n");
		systemMgr.panic(driver.errorCode, "searchVad ʧ��: %s", driver.errorMessage);
		return false;
	}
	if (executeRange.empty()) {
		printf("fail\n");
		systemMgr.panic("�޷���Ŀ��������ҵ�ģ��");
		return false;
	}
	printf("ok\n");


	for (size_t i = 0; i < executeRange.size(); i += 2) {
		auto moduleVABegin = executeRange[i];
		auto moduleVAEnd = executeRange[i + 1];
		for (auto moduleVA = moduleVABegin + 0x1000; moduleVA < moduleVAEnd; moduleVA += 0x1000) {
			rips.push_back(moduleVA);
		}
	}

	auto vmbuf = new char[0x4000];
	auto vmalloc = new char[0x4000];


	printf("����������...");
	bool patched = false;

	for (auto rip = rips.begin(); rip != rips.end(); ++rip) {

		ULONG64 vmStartAddress = (*rip & ~0xfff) - 0x1000;

		if (!driver.readVM(pid, vmbuf, (PVOID)vmStartAddress)) {
			continue;
		}

		for (LONG offset = 0; offset < 0x4000 - 0x100; offset++) {

			const char pattern[] = "\x33\xC9\xFF\x15\xA5\xCC\x01\x00\xEB\xCD\x33\xC0\xEB\x05\xB8\x01\x00\x00\x00\x48\x83\xC4\x30\x5B\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC";
			if (0 == memcmp(pattern, vmbuf + offset, sizeof(pattern) - 1)) {

				printf("%p + 0x%x\n", vmbuf, offset);
				printf("Ӧ�ò������ڴ�...");
				const char newcode[] = "\xEB\x17\xFF\x15\xA5\xCC\x01\x00\xEB\xCD\x33\xC0\xEB\x05\xB8\x01\x00\x00\x00\x48\x83\xC4\x30\x5B\xC3\xB9\x64\x00\x00\x00\xEB\xE2";
				memcpy(vmbuf + offset, newcode, sizeof(newcode) - 1);

				if (driver.writeVM(pid, vmbuf, (PVOID)vmStartAddress)) {
					printf("ok\n");
					printf("ˢ��ָ���...");
					auto hProc = OpenProcess(PROCESS_ALL_ACCESS, NULL, pid);
					if (hProc) {
						if (!FlushInstructionCache(hProc, (PVOID)vmStartAddress, 0x4000)) {
							printf("fail\n");
						}
						CloseHandle(hProc);
					}
					printf("ok\n");
					patched = true;
				} else {
					printf("fail\n");
					systemMgr.panic(driver.errorCode, "writeVM failed at 0x%llx : %s", vmStartAddress, driver.errorMessage);
				}
				
				break;
			}
		}

		if (patched) {
			break;
		}
	}

	delete[] vmbuf;
	delete[] vmalloc;

	if (!patched) {
		printf("δ�ҵ�\n");
	}

	return patched;
}

int main() {

	SetConsoleTitle("�Ż�DNF����ACEģ��  by H3d9");

	if (!systemMgr.enableDebugPrivilege()) {
		return 0;
	}

	printf("��ʼ��...");
	if (!driver.init()) {
		printf("fail\n");
		systemMgr.panic(driver.errorCode, "%s", driver.errorMessage);
		return 0;
	}
	printf("ok\n");

	printf("��ʹ��Ч��������DNF��cpu��ԭ�������Ͻ���10%%~20%%����������������ȡ�\n");
	printf("����ô�á���DNF��ʱ���Լ��ֶ��㿪һ�£�����ʾ�ɹ����˾����ˡ���Ϊʲô��Ū�Զ��ģ���Ϊ������\n");
	printf("����ô�ָ���������Ϸ��\n\n");

	char msg[1000] = "���棺�޸�DNF����ACE��֪���᲻����Ʋ�/����/��ţ������ге�ʹ�÷���Ŷ������\n\n";
	printf(msg);
	strcat(msg, "\n�����𣿣���");
	if (IDNO == MessageBox(0, msg, "����", MB_YESNO)) {
		return 0;
	}

	win32ThreadManager threadMgr;
	DWORD pid = 0;

	printf("�ȴ�DNF����...              \r");
	while (1) {
		if (pid = threadMgr.getTargetPid()) {
			int t = 60;
			for (; t > 0; t--) {
				if (pid = threadMgr.getTargetPid()) {
					printf("DNF��������%d ���ʼ����...           \r", t);
					Sleep(1000);
				} else {
					break;
				}
			}
			if (t == 0 && (pid = threadMgr.getTargetPid())) {
				break;
			}
		}
		Sleep(3000);
	}

	printf("\n\n��������...");
	if (!driver.load()) {
		printf("fail\n");
		systemMgr.panic(driver.errorCode, "driver.load(): %s", driver.errorMessage);
		return 0;
	}
	printf("ok\n");

	if (patch(pid)) {
		printf("�����ɹ���ɣ�\n\n");
	} else {
		printf("����ʧ�ܣ�\n\n");
	}

	driver.unload();

	system("pause");

	return 0;
}