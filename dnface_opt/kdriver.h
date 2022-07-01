#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <memory>


// driver io module (sington)
class KernelDriver {

private:
	static KernelDriver  kernelDriver;

private:
	KernelDriver();
	~KernelDriver();
	KernelDriver(const KernelDriver&)               = delete;
	KernelDriver(KernelDriver&&)                    = delete;
	KernelDriver& operator= (const KernelDriver&)   = delete;
	KernelDriver& operator= (KernelDriver&&)        = delete;

public:
	static KernelDriver& getInstance();

public:
	bool     init();
	bool     load();
	void     unload();
	bool     readVM(DWORD pid, PVOID out, PVOID targetAddress);
	bool     writeVM(DWORD pid, PVOID in, PVOID targetAddress);
	bool     searchVad(DWORD pid, std::vector<ULONG64>& out, const wchar_t* moduleName);

private:
	bool     _startService();
	void     _endService();

private:
	struct VMIO_REQUEST {
		HANDLE   pid;

		PVOID    address           = NULL;
		CHAR     data   [0x1000]   = {};

		ULONG    errorCode         = 0;
		CHAR     errorFunc [128]   = {};

		VMIO_REQUEST(DWORD pid) : pid(reinterpret_cast<HANDLE>(static_cast<LONG64>(pid))) {}
	};

	static constexpr DWORD   VMIO_VERSION  = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0700, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	static constexpr DWORD   VMIO_READ     = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0701, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	static constexpr DWORD   VMIO_WRITE    = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0702, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	static constexpr DWORD	 VM_VADSEARCH  = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0706, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

	std::string     sysfile;
	SC_HANDLE       hSCManager;
	SC_HANDLE       hService;
	HANDLE          hDriver;


public:
	volatile DWORD  errorCode;     // module's errors recorded here (if method returns false).
	const CHAR*     errorMessage;  // caller can decide to log, panic, or ignore.

private:
	void            _resetError();
	void            _recordError(DWORD errorCode, const CHAR* msg, ...);

private:
	std::unique_ptr<CHAR[]>  errorMessage_ptr;
};