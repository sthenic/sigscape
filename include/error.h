#ifndef ERROR_H_E94S9C
#define ERROR_H_E94S9C

#define ADQR_EOK (0) /* OK */
#define ADQR_EINVAL (-1) /* Invalid argument */
#define ADQR_EAGAIN (-2) /* Resource temporarily unavailable */
#define ADQR_EOVERFLOW (-3) /* Overflow */
#define ADQR_ENOTREADY (-4) /* Resource not ready */
#define ADQR_EINTERRUPTED (-5) /* Operation interrupted */
#define ADQR_EIO (-6) /* I/O error */
#define ADQR_EEXTERNAL (-7) /* External errors, e.g. from OS-level operations */
#define ADQR_EUNSUPPORTED (-8) /* Operation not supported by the device. */
#define ADQR_EINTERNAL (-9) /* Internal errors, cannot be addressed by the user. */

#endif
