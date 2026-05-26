#ifndef MB_SURVEY_H_INCLUDED
#define MB_SURVEY_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Protocol identifier for SURVEYOR/RESPONDENT pattern. */
#define MB_PROTO_SURVEY 4

/** SURVEY socket types. */
#define MB_SURVEYOR   (MB_PROTO_SURVEY * 16 + 0)
#define MB_RESPONDENT (MB_PROTO_SURVEY * 16 + 1)
/** XP variants for SURVEYOR/RESPONDENT. */
#define MB_XSURVEYOR   (MB_PROTO_SURVEY * 16 + 8)
#define MB_XRESPONDENT (MB_PROTO_SURVEY * 16 + 9)

/** SURVEYOR_DEADLINE defines a deadline option. */
#define MB_SURVEYOR_DEADLINE 1

#ifdef __cplusplus
}
#endif

#endif
