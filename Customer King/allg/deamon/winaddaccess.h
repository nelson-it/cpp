#ifndef winaddaccess_mne
#define winaddaccess_mne

#ifdef __cplusplus
extern "C" {
#endif

BOOL add_access_right(TCHAR *lpszFileName, TCHAR *lpszAccountName,
      DWORD dwAccessMask, DWORD dwFlags );
int  add_access(const char *root, const char *user, int start);

#ifdef __cplusplus
}
#endif

#endif
