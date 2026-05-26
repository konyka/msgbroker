#ifndef MB_TLS_H_INCLUDED
#define MB_TLS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/** TLS transport identifier. */
#define MB_TLS -5

/** TLS configuration option selectors. */
#define MB_TLS_CONFIG_CERT   1
#define MB_TLS_CONFIG_KEY    2
#define MB_TLS_CONFIG_CA     3
#define MB_TLS_CONFIG_VERIFY 4

#ifdef __cplusplus
}
#endif

#endif
