#ifndef service_mne
#define service_mne

#ifdef __cplusplus
extern "C" {
#endif

void report_log ( const char *name, DWORD t, const char *str);
int create_service(const char *name, const char *path, int sauto,
                    const char *user, const char* passwd, const char *rootdir );
void remove_service(const char *name);
void start_service(const char *name);
void stop_service(const char *name);

#ifdef __cplusplus
}
#endif

#endif

