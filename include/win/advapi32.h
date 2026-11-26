// windows.h: advapi32 -- the registry, the user's name.
#pragma once

#define HKEY_CLASSES_ROOT ((HKEY)(ULONG_PTR)((LONG)0x80000000))
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)((LONG)0x80000001))
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
#define HKEY_USERS ((HKEY)(ULONG_PTR)((LONG)0x80000003))
#define HKEY_CURRENT_CONFIG ((HKEY)(ULONG_PTR)((LONG)0x80000005))
#define KEY_QUERY_VALUE 0x0001
#define KEY_SET_VALUE 0x0002
#define KEY_CREATE_SUB_KEY 0x0004
#define KEY_ENUMERATE_SUB_KEYS 0x0008
#define KEY_NOTIFY 0x0010
#define KEY_WOW64_64KEY 0x0100
#define KEY_WOW64_32KEY 0x0200
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define KEY_EXECUTE KEY_READ
#define KEY_ALL_ACCESS 0xF003F
#define REG_NONE 0
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4
#define REG_MULTI_SZ 7
#define REG_QWORD 11
#define REG_OPTION_NON_VOLATILE 0x00000000L
#define REG_OPTION_VOLATILE 0x00000001L
#define REG_CREATED_NEW_KEY 0x00000001L
#define REG_OPENED_EXISTING_KEY 0x00000002L
#define RRF_RT_REG_SZ 0x00000002
#define RRF_RT_REG_DWORD 0x00000010
#define RRF_RT_REG_QWORD 0x00000040
#define RRF_RT_ANY 0x0000ffff

LSTATUS RegOpenKeyExA(HKEY key, LPCSTR sub, DWORD options, REGSAM sam, PHKEY out);
LSTATUS RegOpenKeyExW(HKEY key, LPCWSTR sub, DWORD options, REGSAM sam, PHKEY out);
LSTATUS RegCreateKeyExA(HKEY key, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD options, REGSAM sam,
                        const LPSECURITY_ATTRIBUTES sa, PHKEY out, LPDWORD disposition);
LSTATUS RegCreateKeyExW(HKEY key, LPCWSTR sub, DWORD reserved, LPWSTR cls, DWORD options, REGSAM sam,
                        const LPSECURITY_ATTRIBUTES sa, PHKEY out, LPDWORD disposition);
LSTATUS RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
LSTATUS RegQueryValueExW(HKEY key, LPCWSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
LSTATUS RegGetValueA(HKEY key, LPCSTR sub, LPCSTR name, DWORD flags, LPDWORD type, PVOID data, LPDWORD size);
LSTATUS RegGetValueW(HKEY key, LPCWSTR sub, LPCWSTR name, DWORD flags, LPDWORD type, PVOID data, LPDWORD size);
LSTATUS RegSetValueExA(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE *data, DWORD size);
LSTATUS RegSetValueExW(HKEY key, LPCWSTR name, DWORD reserved, DWORD type, const BYTE *data, DWORD size);
LSTATUS RegDeleteValueA(HKEY key, LPCSTR name);
LSTATUS RegDeleteValueW(HKEY key, LPCWSTR name);
LSTATUS RegDeleteKeyA(HKEY key, LPCSTR sub);
LSTATUS RegDeleteKeyW(HKEY key, LPCWSTR sub);
LSTATUS RegEnumKeyExA(HKEY key, DWORD index, LPSTR name, LPDWORD name_size, LPDWORD reserved, LPSTR cls,
                      LPDWORD cls_size, PFILETIME written);
LSTATUS RegEnumKeyExW(HKEY key, DWORD index, LPWSTR name, LPDWORD name_size, LPDWORD reserved, LPWSTR cls,
                      LPDWORD cls_size, PFILETIME written);
LSTATUS RegEnumValueA(HKEY key, DWORD index, LPSTR name, LPDWORD name_size, LPDWORD reserved, LPDWORD type,
                      LPBYTE data, LPDWORD size);
LSTATUS RegEnumValueW(HKEY key, DWORD index, LPWSTR name, LPDWORD name_size, LPDWORD reserved, LPDWORD type,
                      LPBYTE data, LPDWORD size);
LSTATUS RegCloseKey(HKEY key);

// process and thread tokens (am I elevated, who am I)
#define TOKEN_ASSIGN_PRIMARY 0x0001
#define TOKEN_DUPLICATE 0x0002
#define TOKEN_IMPERSONATE 0x0004
#define TOKEN_QUERY 0x0008
#define TOKEN_QUERY_SOURCE 0x0010
#define TOKEN_ADJUST_PRIVILEGES 0x0020
#define TOKEN_ADJUST_GROUPS 0x0040
#define TOKEN_ADJUST_DEFAULT 0x0080
#define TOKEN_ADJUST_SESSIONID 0x0100
#define TOKEN_READ 0x00020008
#define TOKEN_WRITE 0x000200E0
#define TOKEN_ALL_ACCESS 0x000F01FF

typedef enum _TOKEN_INFORMATION_CLASS {
    TokenUser = 1, TokenGroups, TokenPrivileges, TokenOwner, TokenPrimaryGroup, TokenDefaultDacl, TokenSource,
    TokenType, TokenImpersonationLevel, TokenStatistics, TokenRestrictedSids, TokenSessionId,
    TokenGroupsAndPrivileges, TokenSessionReference, TokenSandBoxInert, TokenAuditPolicy, TokenOrigin,
    TokenElevationType, TokenLinkedToken, TokenElevation, TokenHasRestrictions, TokenAccessInformation,
    TokenVirtualizationAllowed, TokenVirtualizationEnabled, TokenIntegrityLevel, TokenUIAccess,
    TokenMandatoryPolicy, TokenLogonSid, TokenIsAppContainer
} TOKEN_INFORMATION_CLASS, *PTOKEN_INFORMATION_CLASS;

typedef enum _TOKEN_ELEVATION_TYPE {
    TokenElevationTypeDefault = 1, TokenElevationTypeFull, TokenElevationTypeLimited
} TOKEN_ELEVATION_TYPE, *PTOKEN_ELEVATION_TYPE;

typedef struct _TOKEN_ELEVATION { DWORD TokenIsElevated; } TOKEN_ELEVATION, *PTOKEN_ELEVATION;

BOOL OpenProcessToken(HANDLE process, DWORD access, PHANDLE token);
BOOL OpenThreadToken(HANDLE thread, DWORD access, BOOL as_self, PHANDLE token);
BOOL GetTokenInformation(HANDLE token, TOKEN_INFORMATION_CLASS cls, LPVOID info, DWORD size, PDWORD needed);

BOOL GetUserNameA(LPSTR buf, LPDWORD size);
BOOL GetUserNameW(LPWSTR buf, LPDWORD size);

#define RegOpenKeyEx __AW(RegOpenKeyEx)
#define RegCreateKeyEx __AW(RegCreateKeyEx)
#define RegQueryValueEx __AW(RegQueryValueEx)
#define RegGetValue __AW(RegGetValue)
#define RegSetValueEx __AW(RegSetValueEx)
#define RegDeleteValue __AW(RegDeleteValue)
#define RegDeleteKey __AW(RegDeleteKey)
#define RegEnumKeyEx __AW(RegEnumKeyEx)
#define RegEnumValue __AW(RegEnumValue)
#define GetUserName __AW(GetUserName)
