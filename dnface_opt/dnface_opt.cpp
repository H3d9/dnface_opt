#include <Windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include "kdriver.h"
#include "win32utility.h"

KernelDriver& driver = KernelDriver::getInstance();
win32SystemManager& systemMgr = win32SystemManager::getInstance();


bool patch(DWORD pid) {

	bool isWow64 = false;
	std::vector<ULONG64> rips;
	std::vector<ULONG64> executeRange;

	printf("搜索ACE-GDPServer模块...");

	if (driver.searchVad(pid, executeRange, L"ACE-GDPServer64.dll") && !executeRange.empty()) {
		printf("x64\n");
		isWow64 = false;

	} else if (driver.searchVad(pid, executeRange, L"ACE-GDPServer32.dll") && !executeRange.empty()) {
		printf("x32\n");
		isWow64 = true;
	}
	if (executeRange.empty()) {
		printf("fail\n");
		systemMgr.panic(driver.errorCode, "搜索模块失败：%s", driver.errorMessage);

		return false;
	}


	for (size_t i = 0; i < executeRange.size(); i += 2) {
		auto moduleVABegin = executeRange[i];
		auto moduleVAEnd = executeRange[i + 1];
		for (auto moduleVA = moduleVABegin + 0x1000; moduleVA < moduleVAEnd; moduleVA += 0x1000) {
			rips.push_back(moduleVA);
		}
	}

	printf("搜索特征码...");
	auto vmbuf = new char[0x4000];
	bool patched = false;

	for (auto rip = rips.begin(); rip != rips.end(); ++rip) {

		ULONG64 vmStartAddress = (*rip & ~0xfff) - 0x1000;

		if (!driver.readVM(pid, vmbuf, (PVOID)vmStartAddress)) {
			continue;
		}

		for (LONG offset = 0; offset < 0x4000 - 100; offset++) {

			if (!isWow64) {

				const char pattern[] = "\x33\xC9\xFF\x15\xA5\xCC\x01\x00\xEB\xCD\x33\xC0\xEB\x05\xB8\x01\x00\x00\x00\x48\x83\xC4\x30\x5B\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC";

				if (0 == memcmp(pattern, vmbuf + offset, sizeof(pattern) - 1)) {
					printf("%llx + 0x%x\n", vmStartAddress, offset);

					printf("应用补丁到内存...");
					const char newcode[] = "\xEB\x17\xFF\x15\xA5\xCC\x01\x00\xEB\xCD\x33\xC0\xEB\x05\xB8\x01\x00\x00\x00\x48\x83\xC4\x30\x5B\xC3\xB9\x64\x00\x00\x00\xEB\xE2";
					memcpy(vmbuf + offset, newcode, sizeof(newcode) - 1);

					if (driver.writeVM(pid, vmbuf, (PVOID)vmStartAddress)) {
						printf("ok\n");

						printf("刷新指令缓存...");
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

			} else {

				const char pattern[] = "\x6a\x00\xff\xd7\xeb\xd9";

				if (0 == memcmp(pattern, vmbuf + offset, sizeof(pattern) - 1)) {
					printf("%llx + 0x%x\n", vmStartAddress, offset);

					printf("应用补丁到内存...");
					const char newcode[] = "\x6a\x64\xff\xd7\xeb\xd9";
					memcpy(vmbuf + offset, newcode, sizeof(newcode) - 1);

					if (driver.writeVM(pid, vmbuf, (PVOID)vmStartAddress)) {
						printf("ok\n");

						printf("刷新指令缓存...");
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
		}

		if (patched) {
			break;
		}
	}

	delete[] vmbuf;

	if (!patched) {
		printf("未找到\n");
	}

	return patched;
}



int main() {

	SetConsoleTitle("优化DNF内置ACE模块  by H3d9");

	if (!systemMgr.enableDebugPrivilege()) {
		return 0;
	}

	printf("初始化...");
	if (!driver.init()) {
		printf("fail\n");
		systemMgr.panic(driver.errorCode, "%s", driver.errorMessage);
		return 0;
	}
	printf("ok\n");

	printf("【使用效果】降低DNF的cpu在原来基础上降低10%%~20%%，并提升组队流畅度。\n");
	printf("【怎么用】开DNF的时候自己手动点开一下，等提示成功关了就行了。（为什么不弄自动的，因为我懒）\n");
	printf("【双开怎么用】先把游戏都打开再开这个\n");
	printf("【怎么恢复】重启游戏即可。\n\n");

	printf("【更新地址】https://www.bilibili.com/read/cv20499064\n");
	printf("【Github】https://github.com/H3d9/dnface_opt\n\n\n");

	char msg[1000] = "警告：修改DNF内置ACE不知道会不会出制裁/掉线/封号，请自行承担使用风险哦！！！\n\n";
	printf(msg);
	strcat(msg, "\n继续吗？？？");
	if (IDNO == MessageBox(0, msg, "警告", MB_YESNO)) {
		return 0;
	}

	win32ThreadManager threadMgr;
	DWORD pid = 0;

	printf("等待DNF启动...              \r");
	while (1) {
		if (!threadMgr.getTargetPid().empty()) {
#ifdef _DEBUG
			int t = 3;
#else 
			int t = 60;
#endif
			for (; t > 0; t--) {
				if (!threadMgr.getTargetPid().empty()) {
					printf("DNF已启动，%d 秒后开始操作...           \r", t);
					Sleep(1000);
				} else {
					break;
				}
			}
			if (t == 0) {
				break;
			}
		}
		Sleep(3000);
	}

	printf("\n\n加载驱动...");
	if (!driver.load()) {
		printf("fail\n");
		systemMgr.panic(driver.errorCode, "driver.load(): %s", driver.errorMessage);
		return 0;
	}
	printf("ok\n");

	auto pids = threadMgr.getTargetPid();
	for (auto pid : pids) {
		printf("正在处理DNF进程: pid = %d\n\n", pid);
		if (patch(pid)) {
			printf("操作成功完成！\n\n");
		} else {
			printf("操作失败！\n\n");
		}
	}

	driver.unload();

	system("pause");

	return 0;
}