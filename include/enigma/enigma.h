#ifndef ENIGMA_H
#define ENIGMA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Version macros (optional) */
#ifndef ENIGMA_VERSION_MAJOR
#define ENIGMA_VERSION_MAJOR 0
#endif
#ifndef ENIGMA_VERSION_MINOR
#define ENIGMA_VERSION_MINOR 0
#endif
#ifndef ENIGMA_VERSION_PATCH
#define ENIGMA_VERSION_PATCH 0
#endif

/* Public API */
int enigma_add(int a, int b);
const char* enigma_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* ENIGMA_H */
