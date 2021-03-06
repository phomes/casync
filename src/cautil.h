#ifndef foocautilhfoo
#define foocautilhfoo

#include <stdbool.h>

bool ca_is_url(const char *s);
bool ca_is_ssh_path(const char *s);

typedef enum CaLocatorClass {
        CA_LOCATOR_PATH,
        CA_LOCATOR_SSH,
        CA_LOCATOR_URL,
        _CA_LOCATOR_CLASS_INVALID = -1,
} CaLocatorClass;

CaLocatorClass ca_classify_locator(const char *s);

char *ca_strip_file_url(const char *p);
bool ca_locator_has_suffix(const char *p, const char *suffix);

#endif
