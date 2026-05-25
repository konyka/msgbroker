#ifndef MB_SURVEY_H_INCLUDED
#define MB_SURVEY_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PROTO_SURVEY 4

#define MB_SURVEYOR   (MB_PROTO_SURVEY * 16 + 0)
#define MB_RESPONDENT (MB_PROTO_SURVEY * 16 + 1)
#define MB_XSURVEYOR   (MB_PROTO_SURVEY * 16 + 8)
#define MB_XRESPONDENT (MB_PROTO_SURVEY * 16 + 9)

#define MB_SURVEYOR_DEADLINE 1

#ifdef __cplusplus
}
#endif

#endif
